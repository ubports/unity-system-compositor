include(ExternalProject)
include(FindPackageHandleStandardArgs)

#gtest
if(EXISTS /usr/src/googletest)
  set(USING_GOOGLETEST_1_8 TRUE)
  set(GTEST_INSTALL_DIR /usr/src/googletest/googletest/include)
else()
  set(GTEST_INSTALL_DIR /usr/src/gmock/gtest/include)
endif()

find_path(GTEST_INCLUDE_DIR gtest/gtest.h
            HINTS ${GTEST_INSTALL_DIR})

#gmock
find_path(GMOCK_INSTALL_DIR CMakeLists.txt
          HINTS /usr/src/googletest /usr/src/gmock)
if(${GMOCK_INSTALL_DIR} STREQUAL "GMOCK_INSTALL_DIR-NOTFOUND")
    message(FATAL_ERROR "google-mock package not found")
endif()

find_path(GMOCK_INCLUDE_DIR gmock/gmock.h)

if (USING_GOOGLETEST_1_8)
  set(GMOCK_BASE_BINARY_DIR ${CMAKE_BINARY_DIR}/gmock/libs)
  set(GMOCK_BINARY_DIR ${GMOCK_BASE_BINARY_DIR}/googlemock)
  set(GTEST_BINARY_DIR ${GMOCK_BINARY_DIR}/gtest)
else()
  set(GMOCK_BASE_BINARY_DIR ${CMAKE_BINARY_DIR}/gmock/libs)
  set(GMOCK_BINARY_DIR ${GMOCK_BASE_BINARY_DIR})
  set(GTEST_BINARY_DIR ${GMOCK_BINARY_DIR}/gtest)
endif()

set(GTEST_CMAKE_ARGS "")

if (USING_GOOGLETEST_1_8)
  list(APPEND GTEST_CMAKE_ARGS -DBUILD_GTEST=ON)
endif()

ExternalProject_Add(
    GMock
    #where to build in source tree
    PREFIX ${GMOCK_PREFIX}
    #where the source is external to the project
    SOURCE_DIR ${GMOCK_INSTALL_DIR}
    #forward the compilers to the subproject so cross-arch builds work
    CMAKE_ARGS ${GTEST_CMAKE_ARGS}
    BINARY_DIR ${GMOCK_BASE_BINARY_DIR}

    #we don't need to install, so skip
    INSTALL_COMMAND ""
)

set(GMOCK_LIBRARY ${GMOCK_BINARY_DIR}/libgmock.a)
set(GMOCK_MAIN_LIBRARY ${GMOCK_BINARY_DIR}/libgmock_main.a)
set(GMOCK_BOTH_LIBRARIES ${GMOCK_LIBRARY} ${GMOCK_MAIN_LIBRARY})
set(GTEST_LIBRARY ${GTEST_BINARY_DIR}/libgtest.a)
set(GTEST_MAIN_LIBRARY ${GTEST_BINARY_DIR}/libgtest_main.a)
set(GTEST_BOTH_LIBRARIES ${GTEST_LIBRARY} ${GTEST_MAIN_LIBRARY})
set(GTEST_ALL_LIBRARIES ${GTEST_BOTH_LIBRARIES} ${GMOCK_BOTH_LIBRARIES})

find_package_handle_standard_args(GTest  DEFAULT_MSG
                                    GMOCK_INCLUDE_DIR
                                    GTEST_INCLUDE_DIR)
