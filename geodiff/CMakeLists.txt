# GEODIFF (MIT License)
# Copyright (C) 2019 Peter Petrik

CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
PROJECT(geodiffproject)
SET(CMAKE_CXX_VISIBILITY_PRESET hidden)
SET(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
SET (CMAKE_CXX_STANDARD 11)

SET (ENABLE_TESTS TRUE CACHE BOOL "Build tests?")
SET (ENABLE_COVERAGE FALSE CACHE BOOL "Enable GCOV code coverage?")
SET (BUILD_TOOLS TRUE CACHE BOOL "Build tool executables?")

FIND_PACKAGE( Boost REQUIRED COMPONENTS filesystem)
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )

#############################################################
# Setup code coverage
IF(ENABLE_COVERAGE)
  IF (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    MESSAGE(FATAL_ERROR "gcov coverage must be run in debug build type")
  ENDIF(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")

  IF(MSVC)
    MESSAGE(FATAL_ERROR "gcov coverage is not implemented for Windows")
  ENDIF(MSVC)

  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
ENDIF(ENABLE_COVERAGE)

# Get SQLITE3 dependency
# We want to build it from source
# Since on some platforms/distributions
# Sqlite3 is not compiled with sessions module
IF(EXISTS "${CMAKE_BINARY_DIR}/sqlite3.tar.gz")
  MESSAGE("sqlite3 already downloaded")
ELSE()
  FILE(DOWNLOAD "https://sqlite.org/2019/sqlite-autoconf-3280000.tar.gz" ${CMAKE_BINARY_DIR}/sqlite3.tar.gz SHOW_PROGRESS )
  FILE(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/external")
  EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E tar xfz ${CMAKE_BINARY_DIR}/sqlite3.tar.gz
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/external
    RESULT_VARIABLE rv)
ENDIF()

SET(sqlite3_dir ${CMAKE_BINARY_DIR}/external/sqlite-autoconf-3280000)
SET(geodiff_src
  src/geodiff.cpp
  src/geodiff.h
  src/geodiffutils.cpp
  src/geodiffutils.hpp
  src/geodiffrebase.cpp
  src/geodiffrebase.hpp
  ${sqlite3_dir}/sqlite3.c
  ${sqlite3_dir}/sqlite3.h
)
ADD_LIBRARY(geodiff SHARED ${geodiff_src} )
TARGET_COMPILE_DEFINITIONS(geodiff PUBLIC -DSQLITE_ENABLE_RTREE -DSQLITE_ENABLE_SESSION -DSQLITE_ENABLE_PREUPDATE_HOOK -DSQLITE_ENABLE_COLUMN_METADATA=1 -DSQLITE_MAX_VARIABLE_NUMBER=250000 -DSQLITE_ENABLE_FTS3=1 -DSQLITE_ENABLE_FTS3_PARENTHESIS=1 -DSQLITE_ENABLE_JSON1=1)
TARGET_INCLUDE_DIRECTORIES(geodiff PRIVATE ${sqlite3_dir})
TARGET_LINK_LIBRARIES(geodiff PRIVATE ${Boost_LIBRARIES} dl)

# command line tool
IF (BUILD_TOOLS)
  ADD_EXECUTABLE(geodiffinfo src/geodiffinfo.cpp)
  TARGET_LINK_LIBRARIES(geodiffinfo PUBLIC geodiff ${Boost_LIBRARIES} dl )
ENDIF (BUILD_TOOLS)

# tests
IF (ENABLE_TESTS)
  INCLUDE (CTest)
  ADD_DEFINITIONS(-DENABLE_TESTS)
  ENABLE_TESTING()
  ADD_SUBDIRECTORY(tests)
ENDIF(ENABLE_TESTS)

# install
IF(WIN32)
  INSTALL(TARGETS geodiff RUNTIME DESTINATION lib)
ELSE(WIN32)
  INSTALL(TARGETS geodiff LIBRARY DESTINATION lib)
ENDIF(WIN32)
INSTALL(FILES src/geodiff.h DESTINATION include)
