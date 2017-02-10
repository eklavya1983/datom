cmake_minimum_required(VERSION 3.2)
project("infra")

include_directories(${FBTHRIFT_INCLUDE_DIRS} ${ZOOKEEPER_INCLUDE_DIRS} ${ARTIFACTS_INCLUDE_DIRS})

# Thrift generated sources to include
file(GLOB_RECURSE thrift_cpp_sources "gen/*.cpp")
file(GLOB_RECURSE thrift_ignore_sources "gen/*.skeleton.cpp")
#list(REMOVE_ITEM thrift_cpp_sources ${thrift_ignore_sources})
file(GLOB_RECURSE thrift_ext_sources "gen-ext/*.cpp")
message("Thirft sources: ${thrift_cpp_sources}")

# Library
add_library(infra MessageUtils.cpp Service.cpp PropertiesCache.cpp ServiceServer.cpp ZooKafkaClient.cpp KafkaClient.cpp ConnectionCache.cpp PBMember.cpp Journal.cpp ${thrift_ext_sources} ${thrift_cpp_sources})
install (TARGETS infra DESTINATION lib)

# Tests
add_subdirectory(test)
