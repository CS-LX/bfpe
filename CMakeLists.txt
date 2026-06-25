cmake_minimum_required(VERSION 3.20)
project(bfpe VERSION 0.5.0 LANGUAGES CXX)

enable_testing()

add_subdirectory(src/codegen)
add_subdirectory(src/cli)
