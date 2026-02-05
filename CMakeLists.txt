# AceStudioReverse — root CMake (builds AceForge Bridge plugin)
cmake_minimum_required(VERSION 3.22)
project(AceStudioReverse VERSION 0.1.0)

# Enable languages needed by the project (on macOS)
if(APPLE)
  enable_language(OBJCXX)
endif()

add_subdirectory(ml-bridge/plugin)
