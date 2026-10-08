cmake_minimum_required(VERSION 3.24)
project(HazeVox VERSION 2.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# On Mac the plugin is copied into ~/Library/Audio/Plug-Ins automatically.
# On Windows that folder needs admin rights, so copying is off by default there.
if(APPLE)
    option(HAZE_COPY_PLUGIN "Copy plugin to the system plugin folder after building" ON)
else()
    option(HAZE_COPY_PLUGIN "Copy plugin to the system plugin folder after building" OFF)
endif()

include(FetchContent)

# JUCE (audio plugin framework)
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(juce)

# Signalsmith Stretch (MIT-licensed pitch + formant shifter)
FetchContent_Declare(signalsmith-stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG main
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(signalsmith-stretch)

juce_add_plugin(HazeVox
    COMPANY_NAME "HazeAudio"
    PLUGIN_MANUFACTURER_CODE Hzau
    PLUGIN_CODE Hzvx
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Haze Vox"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD ${HAZE_COPY_PLUGIN})

target_sources(HazeVox PRIVATE
    Source/DSP.h
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp)

target_compile_definitions(HazeVox PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(HazeVox
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        signalsmith-stretch
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)


