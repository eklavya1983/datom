cmake_minimum_required(VERSION 3.2)
project("dc")

include_directories(${FBTHRIFT_INCLUDE_DIRS} ${ZOOKEEPER_INCLUDE_DIRS} ${ARTIFACTS_INCLUDE_DIRS})
link_directories(${FBTHRIFT_LIB_DIRS} ${GTEST_LIB_DIRS} ${ZOOKEEPER_LIB_DIRS} ${ARTIFACTS_LIB_DIRS})

# Library
add_library(dcserver DCServer.cpp)
install (TARGETS dcserver DESTINATION lib)

add_executable(dcserver-main DCServerMain.cpp)
target_link_libraries(dcserver-main dcserver testlib infra zookeeper_mt rdkafka++ thriftcpp2 thriftprotocol thrift folly zstd ${COMMON_LIBS})

add_subdirectory(test)

