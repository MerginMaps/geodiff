# GEODIFF (MIT License)
# Copyright (C) 2019 Peter Petrik

CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
PROJECT(geodiffproject)
SET(CMAKE_CXX_VISIBILITY_PRESET hidden)
SET(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
SET(CMAKE_CXX_STANDARD 11)

# set path to additional CMake modules
SET(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

SET(ENABLE_TESTS TRUE CACHE BOOL "Build tests?")
SET(ENABLE_COVERAGE FALSE CACHE BOOL "Enable GCOV code coverage?")
SET(BUILD_TOOLS TRUE CACHE BOOL "Build tool executables?")
SET(BUILD_STATIC FALSE CACHE BOOL "Build static libraries?")
SET(WITH_INTERNAL_SQLITE3 TRUE CACHE BOOL "Use internal copy of SQLITE3")
SET(WITH_POSTGRESQL FALSE CACHE BOOL "Whether to build with PostgreSQL driver")

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
    FILE(DOWNLOAD "https://www.sqlite.org/2019/sqlite-autoconf-3300100.tar.gz" ${CMAKE_BINARY_DIR}/sqlite3.tar.gz )
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

IF (WITH_POSTGRESQL)
  FIND_PACKAGE(Postgres REQUIRED)
  IF (POSTGRES_FOUND)
    SET(HAVE_POSTGRES TRUE)   # used in geodiff_config.hpp
  ENDIF()
ENDIF()

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
INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/3rdparty
  ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers
  ${CMAKE_CURRENT_BINARY_DIR}
)

# create geodiff_config.h
CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/cmake_templates/geodiff_config.hpp.in ${CMAKE_BINARY_DIR}/geodiff_config.hpp)


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

  src/changeset.h
  src/changesetreader.cpp
  src/changesetreader.h
  src/changesetutils.cpp
  src/changesetutils.h
  src/changesetvarint.h
  src/changesetwriter.cpp
  src/changesetwriter.h
  src/driver.cpp
  src/driver.h
  src/tableschema.cpp
  src/tableschema.h

  src/3rdparty/base64utils.cpp
  src/3rdparty/base64utils.h
  src/3rdparty/portableendian.h

  src/drivers/sqlitedriver.cpp
  src/drivers/sqlitedriver.h
)

IF (POSTGRES_FOUND)
  SET(geodiff_src
    ${geodiff_src}
    src/drivers/postgresdriver.cpp
    src/drivers/postgresdriver.h
    src/drivers/postgresutils.cpp
    src/drivers/postgresutils.h
  )
ENDIF()

IF ( ENABLE_TESTS )
  # tests need statically built library in order to use symbols that are not exported
  SET ( BUILD_STATIC TRUE )
ENDIF ()

IF ( BUILD_STATIC )
  ADD_LIBRARY(${GEODIFF_NAME}_a STATIC ${geodiff_src} )
  TARGET_COMPILE_DEFINITIONS(${GEODIFF_NAME}_a PUBLIC -DGEODIFF_STATIC)
ENDIF ()

ADD_LIBRARY(${GEODIFF_NAME} SHARED ${geodiff_src} )
IF (NOT WIN32 AND NOT ANDROID AND NOT IOS)
    TARGET_LINK_LIBRARIES(${GEODIFF_NAME} PUBLIC dl pthread)
ENDIF (NOT WIN32 AND NOT ANDROID AND NOT IOS)

# command line tool
IF (BUILD_TOOLS)
    ADD_EXECUTABLE(geodiffinfo src/geodiffinfo.cpp)
    TARGET_LINK_LIBRARIES(geodiffinfo PUBLIC geodiff )
ENDIF (BUILD_TOOLS)

ADD_DEFINITIONS( -DSQLITE_CORE )
IF(WITH_INTERNAL_SQLITE3)
  SET (INTERNAL_SQLITE3_DEFINITIONS
        -DSQLITE_ENABLE_RTREE -DSQLITE_ENABLE_SESSION -DSQLITE_ENABLE_PREUPDATE_HOOK
        -DSQLITE_ENABLE_COLUMN_METADATA=1 -DSQLITE_MAX_VARIABLE_NUMBER=250000 -DSQLITE_ENABLE_FTS3=1
        -DSQLITE_ENABLE_FTS3_PARENTHESIS=1 -DSQLITE_ENABLE_JSON1=1)
  TARGET_COMPILE_DEFINITIONS(${GEODIFF_NAME} PUBLIC ${INTERNAL_SQLITE3_DEFINITIONS})
  TARGET_INCLUDE_DIRECTORIES(${GEODIFF_NAME} PRIVATE ${sqlite3_dir})
  IF ( BUILD_STATIC )
    TARGET_COMPILE_DEFINITIONS(${GEODIFF_NAME}_a PUBLIC ${INTERNAL_SQLITE3_DEFINITIONS})
    TARGET_INCLUDE_DIRECTORIES(${GEODIFF_NAME}_a PRIVATE ${sqlite3_dir})
  ENDIF ()
ELSE(WITH_INTERNAL_SQLITE3)
  ADD_DEFINITIONS( -DSQLITE_ENABLE_SESSION )
  TARGET_LINK_LIBRARIES(${GEODIFF_NAME} PUBLIC ${SQLite3_LIBRARIES})
  IF (BUILD_TOOLS)
      TARGET_LINK_LIBRARIES(geodiffinfo PUBLIC ${SQLite3_LIBRARIES} )
  ENDIF (BUILD_TOOLS)
ENDIF(WITH_INTERNAL_SQLITE3)

TARGET_INCLUDE_DIRECTORIES(${GEODIFF_NAME} PRIVATE ${libgpkg_dir}/gpkg)
IF ( BUILD_STATIC )
  TARGET_INCLUDE_DIRECTORIES(${GEODIFF_NAME}_a PRIVATE ${libgpkg_dir}/gpkg)
ENDIF ()

IF (POSTGRES_FOUND)
  TARGET_INCLUDE_DIRECTORIES(${GEODIFF_NAME} PRIVATE ${POSTGRES_INCLUDE_DIR})
  TARGET_LINK_LIBRARIES(${GEODIFF_NAME} PUBLIC ${POSTGRES_LIBRARY})
  IF ( BUILD_STATIC )
    TARGET_INCLUDE_DIRECTORIES(${GEODIFF_NAME}_a PRIVATE ${POSTGRES_INCLUDE_DIR})
  ENDIF ()
ENDIF ()

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
