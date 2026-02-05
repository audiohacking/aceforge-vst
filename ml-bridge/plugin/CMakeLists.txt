# AceForge Bridge plugin — AU + VST3 (macOS target)
cmake_minimum_required(VERSION 3.22)

if(NOT APPLE)
  message(STATUS "AceForge Bridge plugin: skipping (target is macOS only for now)")
  return()
endif()

enable_language(ObjectiveCXX)

# Fetch JUCE
include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        7.0.10
  GIT_SHALLOW    TRUE
)
# Required for building without a JUCE commercial license (GPL/AGPL)
set(JUCE_ENABLE_GPL_MODE ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(JUCE)

# AceForgeClient static library (macOS implementation)
set(ACEFORGE_CLIENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../AceForgeClient)
add_library(AceForgeClient STATIC
  ${ACEFORGE_CLIENT_DIR}/AceForgeClientMac.mm
)
target_include_directories(AceForgeClient PUBLIC ${ACEFORGE_CLIENT_DIR})
target_link_libraries(AceForgeClient PUBLIC
  "-framework Foundation"
  "-framework Security"
)
target_compile_features(AceForgeClient PUBLIC cxx_std_17)

# Plugin target
juce_add_plugin(AceForgeBridge
  VERSION 0.1.0
  COMPANY_NAME "AceForge"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  PLUGIN_MANUFACTURER_CODE Acfg
  PLUGIN_CODE AfBr
  FORMATS AU VST3
  PRODUCT_NAME "AceForge Bridge"
  COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(AceForgeBridge
  PRIVATE
  PluginProcessor.cpp
  PluginEditor.cpp
)

target_compile_definitions(AceForgeBridge
  PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
)

target_include_directories(AceForgeBridge
  PRIVATE
  ${ACEFORGE_CLIENT_DIR}
)

target_link_libraries(AceForgeBridge
  PRIVATE
  AceForgeClient
  juce::juce_audio_utils
  juce::juce_audio_formats
  PUBLIC
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags
)
