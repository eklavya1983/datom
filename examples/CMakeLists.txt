cmake_minimum_required(VERSION 3.2)
project("examples")

# External dependency include directories
include_directories(${ARTIFACTS_INCLUDE_DIRS} ${GTEST_INCLUDE_DIRS} ${ZOOKEEPER_INCLUDE_DIRS} .)
link_directories(${ARTIFACTS_LIB_DIRS} ${GTEST_LIB_DIRS} ${ZOOKEEPER_LIB_DIRS}) 

#add_executable(zk ZookeeperClient.cpp zk.cpp)
#target_link_libraries(zk zookeeper_mt ${COMMON_LIBS})

add_executable(future_gtest future_test.cpp)
target_link_libraries(future_gtest folly ${COMMON_TEST_LIBS})

add_executable(consumer_ex consumer_ex.cpp)
target_link_libraries(consumer_ex rdkafka++ ${COMMON_TEST_LIBS})
