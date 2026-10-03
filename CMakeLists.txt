cmake_minimum_required(VERSION 3.22)

# Mac: binario universal (Apple Silicon + Intel), macOS 11 o superior
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(MixRefStrip VERSION 1.2.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: se baja solo de GitHub (o se usa una copia local con -DJUCE_LOCAL_PATH=...)
if(DEFINED JUCE_LOCAL_PATH)
    add_subdirectory(${JUCE_LOCAL_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(MIXREF_FORMATS VST3)
if(APPLE)
    list(APPEND MIXREF_FORMATS AU)
endif()

juce_add_plugin(MixRefStrip
    COMPANY_NAME "Lucas Audio"
    PRODUCT_NAME "MixRef Strip"
    PLUGIN_MANUFACTURER_CODE Lcas
    PLUGIN_CODE MxRf
    FORMATS ${MIXREF_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics EQ
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(MixRefStrip PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/EqGraph.cpp)

target_compile_definitions(MixRefStrip PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(MixRefStrip
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
