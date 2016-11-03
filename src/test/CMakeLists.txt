cmake_minimum_required(VERSION 3.2)
project("test")

# Here add_unit_test is coming actor/test/CMakefiles.txt
macro(add_unit_test name)
  add_executable(${name}Test ${name}Test.cpp ${ARGN})
  target_link_libraries(${name}Test
                        testlib config volumeserver infra zookeeper_mt rdkafka++ thriftcpp2 thriftprotocol thrift zstd folly crypto ${COMMON_TEST_LIBS})
  add_test(${name} ${EXECUTABLE_OUTPUT_PATH}/${name}Test --tryfromenv=toolsdir)
  install (TARGETS ${name}Test DESTINATION test)
  #add_dependencies(${name}Test all_unit_tests)
endmacro()

include_directories(${FBTHRIFT_INCLUDE_DIRS} ${ZOOKEEPER_INCLUDE_DIRS} ${ARTIFACTS_INCLUDE_DIRS} ${GTEST_INCLUDE_DIRS})
link_directories(${FBTHRIFT_LIB_DIRS} ${GTEST_LIB_DIRS} ${ZOOKEEPER_LIB_DIRS} ${ARTIFACTS_LIB_DIRS})

add_unit_test(Datom)
add_unit_test(Service)
