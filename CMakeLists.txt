cmake_minimum_required(VERSION 3.22)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(Chor100 VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.9.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Chor100
    COMPANY_NAME "Chor100"
    PLUGIN_MANUFACTURER_CODE Chrz
    PLUGIN_CODE C1oo
    FORMATS VST3 Standalone
    PRODUCT_NAME "Chor100"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Chor100 PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(Chor100 PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(Chor100
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
