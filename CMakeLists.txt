cmake_minimum_required(VERSION 3.28.3)

project(Edward VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
include(cmake/Dependencies.cmake)

enable_testing()
add_subdirectory(tests/contracts)
