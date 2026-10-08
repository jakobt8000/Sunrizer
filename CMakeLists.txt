cmake_minimum_required(VERSION 3.22)

project(SUNRIZE VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Universal binary (Apple Silicon + Intel) on macOS
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

# JUCE: use a local checkout if JUCE_DIR is given, otherwise download it
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(SUNRIZE_FORMATS AU VST3)
else()
    set(SUNRIZE_FORMATS VST3)
endif()

# COMPANY_NAME "REZONANZA" is what puts the plugin in the REZONANZA folder in Ableton
juce_add_plugin(Sunrize
    COMPANY_NAME "REZONANZA"
    PLUGIN_MANUFACTURER_CODE Rznz
    PLUGIN_CODE Srzn
    FORMATS ${SUNRIZE_FORMATS}
    PRODUCT_NAME "SUNRIZE"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    BUNDLE_ID dk.rezonanza.sunrize)

juce_add_binary_data(SunrizeData SOURCES
    Resources/logo.png
    Resources/IBMPlexMono-Regular.ttf)

target_sources(Sunrize PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Sunrize PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Sunrize
    PRIVATE
        SunrizeData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

