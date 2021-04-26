# GEODIFF (MIT License)
# Copyright (C) 2019 Peter Petrik

FIND_PACKAGE(Threads REQUIRED)
FIND_PACKAGE(GTest)

IF (NOT GTEST_FOUND)
    # Download and unpack googletest at configure time
    FILE(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/googletest-download")
    CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/GTestCMakeLists.in" "${CMAKE_BINARY_DIR}/googletest-download/CMakeLists.txt")
    EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" .
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/googletest-download" )
    EXECUTE_PROCESS(COMMAND "${CMAKE_COMMAND}" --build .
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/googletest-download" )

    # Prevent GoogleTest from overriding our compiler/linker options
    # when building with Visual Studio
    SET(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    # Add googletest directly to our build. This adds
    # the following targets: gtest, gtest_main, gmock
    # and gmock_main
    ADD_SUBDIRECTORY("${CMAKE_BINARY_DIR}/googletest-src"
                     "${CMAKE_BINARY_DIR}/googletest-build"
                     EXCLUDE_FROM_ALL)
ELSE(NOT GTEST_FOUND)
    include_directories(${GTEST_INCLUDE_DIR})
ENDIF(NOT GTEST_FOUND)


#########################################################################
#########################################################################
# This define is used for tests that need to locate the test
# data under tests/testdata in the qgis source tree.
# the TEST_DATA_DIR variable is set in the top level CMakeLists.txt
SET(TESTDIR ${CMAKE_SOURCE_DIR}/tests/testdata/)
ADD_DEFINITIONS(-DTEST_DATA_DIR="${TESTDIR}")

#########################################################################
#########################################################################

INCLUDE_DIRECTORIES(  
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_BINARY_DIR}/src
  ${CMAKE_SOURCE_DIR}/src
)

ADD_LIBRARY(geodifftestutils OBJECT geodiff_testutils.hpp geodiff_testutils.cpp)
TARGET_COMPILE_DEFINITIONS(geodifftestutils PUBLIC -DGEODIFF_STATIC)

MACRO (ADD_GEODIFF_TEST TESTSRC)
  SET (TESTNAME  ${TESTSRC})
  STRING(REPLACE "test_" "" TESTNAME ${TESTNAME})
  STRING(REPLACE ".cpp" "" TESTNAME ${TESTNAME})
  SET (TESTNAME  "geodiff_${TESTNAME}_test")
  ADD_EXECUTABLE(${TESTNAME} ${TESTSRC} $<TARGET_OBJECTS:geodifftestutils>)
  TARGET_LINK_LIBRARIES(${TESTNAME} gtest gmock geodiff_a)
  IF (NOT WIN32 AND NOT ANDROID AND NOT IOS)
    TARGET_LINK_LIBRARIES(${TESTNAME} dl)
  ENDIF ()
  IF (POSTGRES_FOUND)
    TARGET_LINK_LIBRARIES(${TESTNAME} ${POSTGRES_LIBRARY})
  ENDIF ()
  ADD_TEST(${TESTNAME} ${CMAKE_CURRENT_BINARY_DIR}/${TESTNAME})
ENDMACRO (ADD_GEODIFF_TEST)

SET(TESTS
    test_single_commit.cpp
    test_concurrent_commits.cpp
    test_modified_scheme.cpp
    test_changeset_reader.cpp
    test_changeset_utils.cpp
    test_driver_sqlite.cpp
    test_utils.cpp
)

FOREACH(TESTSRC ${TESTS})
    ADD_GEODIFF_TEST(${TESTSRC})
ENDFOREACH(TESTSRC)

IF (POSTGRES_FOUND)
  ADD_GEODIFF_TEST(test_driver_postgres.cpp)
  TARGET_INCLUDE_DIRECTORIES(geodiff_driver_postgres_test PRIVATE ${POSTGRES_INCLUDE_DIR})
  TARGET_LINK_LIBRARIES(geodiff_driver_postgres_test ${POSTGRES_LIBRARY})
ENDIF()


# geodiffcmd utility test
ADD_TEST(NAME geodiffinfo_test COMMAND $<TARGET_FILE:geodiffinfo> version )
