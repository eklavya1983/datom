cmake_minimum_required(VERSION 3.2)
project("store")


# set default build to debug if not set
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Common flags
set(CMAKE_COMMON_FLAGS "-Wall -Werror -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${CMAKE_COMMON_FLAGS}")

set (CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Common variables
set(TOPDIR ${CMAKE_SOURCE_DIR})
message("topdir is: ${TOPDIR}")

# 3rd party artifacts includes
set(ARTIFACTS_INCLUDE_DIRS ${TOPDIR}/artifacts/include)
set(ARTIFACTS_LIB_DIRS ${TOPDIR}/artifacts/lib)

# fbthrift related common variables
set(FBTHRIFT_INCLUDE_DIRS
	${ARTIFACTS_INCLUDE_DIRS})

set(FBTHRIFT_LIB_DIRS
	${ARTIFACTS_LIB_DIRS})

# thrift related variables
set(THRIFT_INCLUDE_DIRS ${TOPDIR}/thrift-0.9.3/lib/cpp/src)
set(THRIFT_LIB_DIRS ${TOPDIR}/thrift-0.9.3/lib/cpp/.libs)

# gtest related common variables
set(GTEST_INCLUDE_DIRS ${TOPDIR}/gtest-1.7.0/include)
set(GTEST_LIB_DIRS ${TOPDIR}/gtest-1.7.0/lib/.libs)

# zookeeper related variables
set(ZOOKEEPER_INCLUDE_DIRS ${TOPDIR}/zookeeper-3.4.9/src/c/include ${TOPDIR}/zookeeper-3.4.9/src/c/generated)
set(ZOOKEEPER_LIB_DIRS ${TOPDIR}/zookeeper-3.4.9/src/c/.libs)

# Common set of libraries to link for binaries
#set(COMMON_LIBS thriftcpp2 thrift folly glog gflags pthread boost_system double-conversion z)
set(COMMON_LIBS glog gflags pthread boost_system double-conversion z)

# Common set of libraries to link for tests 
set(COMMON_TEST_LIBS gtest ${COMMON_LIBS})

# enable testing
enable_testing()

# common include directories
#include_directories(${FBTHRIFT_INCLUDE_DIRS} ${TOPDIR}/src)
include_directories(${TOPDIR}/src)

# common library directories
#link_directories(${FBTHRIFT_LIB_DIRS})

add_subdirectory(src)
add_subdirectory(examples)
#add_subdirectory(config)


#add_test(actormsg_test actormsg_test) 
#add_test(actormsg_test actormsg_test) 
