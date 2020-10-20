# GEODIFF (MIT License)
# Copyright (C) 2019 Peter Petrik

CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
PROJECT(geodiffproject)
SET(CMAKE_CXX_VISIBILITY_PRESET hidden)
SET(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
SET(CMAKE_CXX_STANDARD 11)

SET(ENABLE_TESTS TRUE CACHE BOOL "Build tests?")
SET(ENABLE_COVERAGE FALSE CACHE BOOL "Enable GCOV code coverage?")
SET(BUILD_TOOLS TRUE CACHE BOOL "Build tool executables?")
SET(BUILD_STATIC_LIBS FALSE CACHE BOOL "Build only static libraries?")
SET(WITH_INTERNAL_SQLITE3 TRUE CACHE BOOL "Use internal copy of SQLITE3")

IF(SKBUILD)
  MESSAGE(STATUS "The geodiff is built using scikit-build for pygeodiff Python package")
  FIND_PACKAGE(PythonExtensions REQUIRED)
  SET(GEODIFF_NAME "pygeodiff-${PYGEODIFFVERSION}-python")
  IF (MSVC AND NOT "${CMAKE_GENERATOR}" MATCHES "(Win64|IA64)")
    SET(GEODIFF_NAME "${GEODIFF_NAME}-win32")
  ENDIF (MSVC AND NOT "${CMAKE_GENERATOR}" MATCHES "(Win64|IA64)")
ELSE(SKBUILD)
  SET(GEODIFF_NAME geodiff)
ENDIF(SKBUILD)

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

IF(WITH_INTERNAL_SQLITE3)
  # Get SQLITE3 dependency
  # We want to build it from source
  # Since on some platforms/distributions
  # Sqlite3 is not compiled with sessions module
  IF(EXISTS "${CMAKE_BINARY_DIR}/sqlite3.tar.gz")
    MESSAGE("sqlite3 already downloaded")
  ELSE()
    # the cannonical download URL is https://www.sqlite.org/2019/sqlite-autoconf-3300100.tar.gz
    # but for an unknown reason Travis CI on macOS at some point stopped being able to download
    # anything from sqlite.org, with any connections timing out without getting any response.
    # A copy hosted on Lutra Dropbox seems to work...
    FILE(DOWNLOAD "https://www.dropbox.com/s/8x1ese8mx0sif3c/sqlite-autoconf-3300100.tar.gz?dl=1" ${CMAKE_BINARY_DIR}/sqlite3.tar.gz )
    FILE(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/external")
    EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E tar xfz ${CMAKE_BINARY_DIR}/sqlite3.tar.gz
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/external
      RESULT_VARIABLE rv)
  ENDIF()

  SET(sqlite3_dir ${CMAKE_BINARY_DIR}/external/sqlite-autoconf-3300100)
  SET(sqlite_src
    ${sqlite3_dir}/sqlite3.c
    ${sqlite3_dir}/sqlite3.h
  )

  SET(geodiff_src ${geodiff_src} ${sqlite_src})
ELSE(WITH_INTERNAL_SQLITE3)
  CMAKE_POLICY(SET CMP0074 NEW)
  FIND_PACKAGE(SQLite3 REQUIRED)
ENDIF(WITH_INTERNAL_SQLITE3)

# Get libgpkg dependency
IF(EXISTS "${CMAKE_BINARY_DIR}/libgpkg.tar.gz")
  MESSAGE("libgpkg already downloaded")
ELSE()
  FILE(DOWNLOAD "https://github.com/benstadin/libgpkg/archive/0822c5cba7e1ac2c2806e445e5f5dd2f0d0a18b4.tar.gz" ${CMAKE_BINARY_DIR}/libgpkg.tar.gz )
  FILE(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/external")
  EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E tar xfz ${CMAKE_BINARY_DIR}/libgpkg.tar.gz
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/external
    RESULT_VARIABLE rv)
ENDIF()

SET(libgpkg_dir ${CMAKE_BINARY_DIR}/external/libgpkg-0822c5cba7e1ac2c2806e445e5f5dd2f0d0a18b4)
SET(libgpkg_src
  ${libgpkg_dir}/gpkg/binstream.c
  ${libgpkg_dir}/gpkg/blobio.c
  ${libgpkg_dir}/gpkg/error.c
  ${libgpkg_dir}/gpkg/fp.c
  ${libgpkg_dir}/gpkg/geomio.c
  ${libgpkg_dir}/gpkg/gpkg.c
  ${libgpkg_dir}/gpkg/gpkg_db.c
  ${libgpkg_dir}/gpkg/gpkg_geom.c
  ${libgpkg_dir}/gpkg/i18n.c
  ${libgpkg_dir}/gpkg/sql.c
  ${libgpkg_dir}/gpkg/spatialdb.c
  ${libgpkg_dir}/gpkg/spl_db.c
  ${libgpkg_dir}/gpkg/spl_geom.c
  ${libgpkg_dir}/gpkg/strbuf.c
  ${libgpkg_dir}/gpkg/wkb.c
  ${libgpkg_dir}/gpkg/wkt.c
  ${libgpkg_dir}/gpkg/atomic_ops.h
)
SET(geodiff_src ${geodiff_src} ${libgpkg_src})

IF (NOT WIN32)
  SET_SOURCE_FILES_PROPERTIES(${libgpkg_src} PROPERTIES COMPILE_FLAGS "-Wno-deprecated-declarations")
ENDIF (NOT WIN32)

INCLUDE(CheckIncludeFile)
INCLUDE( ${libgpkg_dir}/gpkg/cmake/UseTLS.cmake )
CHECK_TLS()

INCLUDE( ${libgpkg_dir}/gpkg/cmake/UseLocale.cmake )
CHECK_LOCALE()

# check the version in the libgpkg main CMakeLists.txt
SET(gpkg_VERSION_MAJOR 0)
SET(gpkg_VERSION_MINOR 10)
SET(gpkg_VERSION_PATCH 0)
ADD_DEFINITIONS( -DGPKG_HAVE_CONFIG_H )
CONFIGURE_FILE( "${libgpkg_dir}/gpkg/config.h.in" "${CMAKE_CURRENT_BINARY_DIR}/config.h" )
INCLUDE_DIRECTORIES( ${CMAKE_CURRENT_BINARY_DIR} )

# build
SET(geodiff_src
  ${geodiff_src}
  src/geodiff.cpp
  src/geodiff.h
  src/geodiffutils.cpp
  src/geodiffutils.hpp
  src/geodiffrebase.cpp
  src/geodiffrebase.hpp
  src/geodiffexporter.cpp
  src/geodiffexporter.hpp
  src/geodifflogger.cpp
  src/geodifflogger.hpp
)

IF ( BUILD_STATIC_LIBS )
  ADD_LIBRARY(${GEODIFF_NAME} STATIC ${geodiff_src} )
ELSE ( BUILD_STATIC_LIBS )
  ADD_LIBRARY(${GEODIFF_NAME} SHARED ${geodiff_src} )
  IF (NOT WIN32 AND NOT ANDROID AND NOT IOS)
      TARGET_LINK_LIBRARIES(${GEODIFF_NAME} PUBLIC dl pthread)
  ENDIF (NOT WIN32 AND NOT ANDROID AND NOT IOS)

  # command line tool
  IF (BUILD_TOOLS)
      ADD_EXECUTABLE(geodiffinfo src/geodiffinfo.cpp)
      TARGET_LINK_LIBRARIES(geodiffinfo PUBLIC geodiff )
  ENDIF (BUILD_TOOLS)

ENDIF (BUILD_STATIC_LIBS)

ADD_DEFINITIONS( -DSQLITE_CORE )
IF(WITH_INTERNAL_SQLITE3)
  TARGET_COMPILE_DEFINITIONS(${GEODIFF_NAME} PUBLIC -DSQLITE_ENABLE_RTREE -DSQLITE_ENABLE_SESSION -DSQLITE_ENABLE_PREUPDATE_HOOK -DSQLITE_ENABLE_COLUMN_METADATA=1 -DSQLITE_MAX_VARIABLE_NUMBER=250000 -DSQLITE_ENABLE_FTS3=1 -DSQLITE_ENABLE_FTS3_PARENTHESIS=1 -DSQLITE_ENABLE_JSON1=1)
  TARGET_INCLUDE_DIRECTORIES(${GEODIFF_NAME} PRIVATE ${sqlite3_dir})
ELSE(WITH_INTERNAL_SQLITE3)
  ADD_DEFINITIONS( -DSQLITE_ENABLE_SESSION )
  TARGET_INCLUDE_DIRECTORIES( ${GEODIFF_NAME} PRIVATE ${SQLite3_INCLUDE_DIRS})
  IF ( NOT BUILD_STATIC_LIBS )
      TARGET_LINK_LIBRARIES(${GEODIFF_NAME} PUBLIC ${SQLite3_LIBRARIES})
      IF (BUILD_TOOLS)
          TARGET_LINK_LIBRARIES(geodiffinfo PUBLIC ${SQLite3_LIBRARIES} )
      ENDIF (BUILD_TOOLS)
  ENDIF ( NOT BUILD_STATIC_LIBS )
ENDIF(WITH_INTERNAL_SQLITE3)

TARGET_INCLUDE_DIRECTORIES(${GEODIFF_NAME} PRIVATE ${libgpkg_dir}/gpkg)

# win32 libs
IF ( WIN32 )
  TARGET_LINK_LIBRARIES( ${GEODIFF_NAME} PUBLIC shlwapi )
ENDIF ( WIN32 )

# tests
IF (ENABLE_TESTS)
    INCLUDE (CTest)
    ADD_DEFINITIONS(-DENABLE_TESTS)
    ENABLE_TESTING()
    ADD_SUBDIRECTORY(tests)
ENDIF(ENABLE_TESTS)

# install
IF(SKBUILD)
    IF(WIN32)
        INSTALL(TARGETS ${GEODIFF_NAME} RUNTIME DESTINATION pygeodiff)
    ELSE(WIN32)
        INSTALL(TARGETS ${GEODIFF_NAME} LIBRARY DESTINATION pygeodiff)
    ENDIF(WIN32)
ELSE(SKBUILD)
    IF(WIN32)
        INSTALL(TARGETS ${GEODIFF_NAME} 
		  RUNTIME DESTINATION bin
		  ARCHIVE DESTINATION lib
	    )
    ELSE(WIN32)
        INSTALL(TARGETS ${GEODIFF_NAME} 
		  LIBRARY DESTINATION lib
		  ARCHIVE DESTINATION lib
		)
    ENDIF(WIN32)
    INSTALL(FILES src/geodiff.h DESTINATION include)
ENDIF(SKBUILD)
