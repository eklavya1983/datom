cmake_minimum_required(VERSION 3.2)
project("src")

# Based on info from http://stackoverflow.com/questions/303983.2/cmake-rpath-not-working-could-not-find-shared-object-file
# When make install is executed all binaries will have rpath set to lib folder
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

#add_subdirectory(examples)
add_subdirectory(infra)
add_subdirectory(testlib)
add_subdirectory(configservice)
add_subdirectory(volumeserver)
add_subdirectory(test)
