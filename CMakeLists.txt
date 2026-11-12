cmake_minimum_required(VERSION 3.22)
project(NorthstarFX VERSION 2.2.0)

if(APPLE)
    set(TYAP_LYAP_FORMATS VST3 AU Standalone)
else()
    set(TYAP_LYAP_FORMATS VST3 Standalone)
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.12)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(NorthstarFX
    COMPANY_NAME "Northstar Audio"
    BUNDLE_ID "audio.northstar.fx"
    PLUGIN_MANUFACTURER_CODE Nstr
    PLUGIN_CODE Nsfx
    FORMATS ${TYAP_LYAP_FORMATS}
    PRODUCT_NAME "Tyap Lyap"
    PLUGIN_NAME "Tyap Lyap"
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Spatial)

juce_add_binary_data(TyapLyapSkin SOURCES Assets/cabinet.png Assets/knob.png Assets/telegram.png)
target_sources(NorthstarFX PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/TapeStop.h
    Source/BassImpact.h
    Source/PresetBank.cpp
    Source/PresetBank.h
    Source/ShelfScene.cpp
    Source/ShelfScene.h
    Source/SpeakerVisual.cpp
    Source/SpeakerVisual.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h)

target_compile_features(NorthstarFX PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(NorthstarFX PUBLIC /utf-8)
endif()
target_compile_definitions(NorthstarFX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(NorthstarFX PRIVATE
    TyapLyapSkin
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_warning_flags)

add_executable(OrbitTest Tests/OrbitTest.cpp)
target_compile_features(OrbitTest PRIVATE cxx_std_17)
add_executable(RenderTest Tests/RenderTest.cpp)
target_compile_features(RenderTest PRIVATE cxx_std_17)
target_include_directories(RenderTest PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/NorthstarFX_artefacts/JuceLibraryCode" "${juce_SOURCE_DIR}/modules")
target_compile_definitions(RenderTest PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
target_link_libraries(RenderTest PRIVATE NorthstarFX TyapLyapSkin)

add_executable(Vst3SignalTest Tests/Vst3SignalTest.cpp)
target_compile_features(Vst3SignalTest PRIVATE cxx_std_17)
target_compile_definitions(Vst3SignalTest PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_USE_CURL=0)
target_link_libraries(Vst3SignalTest PRIVATE juce::juce_audio_processors_headless)

juce_generate_juce_header(NorthstarFX)
