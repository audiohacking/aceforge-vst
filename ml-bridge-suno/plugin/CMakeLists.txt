# AceForge-Suno plugin — Suno API (Music Generation), AU + VST3, macOS
cmake_minimum_required(VERSION 3.22)

if(NOT APPLE)
  message(STATUS "AceForge-Suno plugin: skipping (macOS only)")
  return()
endif()

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        8.0.12
  GIT_SHALLOW    TRUE
)
set(JUCE_ENABLE_GPL_MODE ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(JUCE)

set(SUNO_CLIENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../SunoClient)
add_library(SunoClient STATIC
  ${SUNO_CLIENT_DIR}/SunoClientMac.mm
)
target_include_directories(SunoClient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(SunoClient PUBLIC "-framework Foundation" "-framework Security")
target_compile_features(SunoClient PUBLIC cxx_std_17)

juce_add_plugin(AceForgeSuno
  VERSION 0.1.0
  COMPANY_NAME "AudioHacking"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  PLUGIN_MANUFACTURER_CODE AuHk
  PLUGIN_CODE AfSn
  FORMATS AU VST3
  PRODUCT_NAME "AceForge-Suno"
  COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(AceForgeSuno
  PRIVATE
  PluginProcessor.cpp
  PluginEditor.cpp
)

target_compile_definitions(AceForgeSuno
  PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
)

target_include_directories(AceForgeSuno
  PRIVATE
  ${SUNO_CLIENT_DIR}
)

target_link_libraries(AceForgeSuno
  PRIVATE
  SunoClient
  juce::juce_audio_utils
  juce::juce_audio_formats
  PUBLIC
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags
)
