cmake_minimum_required(VERSION 3.2)
project("volumeserver")

include_directories(${FBTHRIFT_INCLUDE_DIRS} ${ZOOKEEPER_INCLUDE_DIRS} ${ARTIFACTS_INCLUDE_DIRS})
link_directories(${FBTHRIFT_LIB_DIRS} ${GTEST_LIB_DIRS} ${ZOOKEEPER_LIB_DIRS} ${ARTIFACTS_LIB_DIRS})

# Library
add_library(volumeserver VolumeServer.cpp VolumeMetaDb.cpp)
install (TARGETS volumeserver DESTINATION lib)

add_executable(volumeserver-main VolumeServerMain.cpp)
target_link_libraries(volumeserver-main volumeserver rocksdb snappy testlib infra zookeeper_mt rdkafka++ thriftcpp2 thriftprotocol thrift folly zstd ${COMMON_LIBS})

add_subdirectory(test)

