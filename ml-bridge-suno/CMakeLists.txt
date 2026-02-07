# suno-daw — AceForge-Suno AU/VST3 plugin (Suno API), macOS
cmake_minimum_required(VERSION 3.22)
project(suno-daw VERSION 0.1.0)

if(APPLE)
  enable_language(OBJCXX)
endif()

add_subdirectory(plugin)
