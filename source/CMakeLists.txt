cmake_minimum_required(VERSION 3.25)

project(Seqwencer VERSION 1.3.24.0 LANGUAGES C CXX)

set(JUCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../_Tools/JUCE/_8.0.15")

if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt"
   AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../JUCE_8.0.15/CMakeLists.txt")
    set(JUCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../JUCE_8.0.15")
endif()

if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE 8.0.15 was not found. Expected: ${JUCE_DIR}/CMakeLists.txt")
endif()

add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")

juce_add_plugin(Seqwencer
    VERSION 1.3.24.0
    COMPANY_NAME "sl23"
    BUNDLE_ID "com.sl23.seqwencer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_AUTO_MANIFEST FALSE
    PLUGIN_MANUFACTURER_CODE sl23
    PLUGIN_CODE sqw1
    FORMATS VST3
    VST3_CATEGORIES Fx Modulation Tools Stereo
    PRODUCT_NAME "Seqwencer"
    PLUGIN_NAME "Seqwencer"
    DESCRIPTION "Dual Step Modulation and FX")

juce_generate_juce_header(Seqwencer)

target_sources(Seqwencer
    PRIVATE
        PluginEditor.cpp
        PluginEditor.h
        PluginProcessor.cpp
        PluginProcessor.h
        SeqwencerBridgeProtocol.h
        SequencerCore.h)

target_compile_features(Seqwencer PRIVATE cxx_std_17)

target_compile_definitions(Seqwencer
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Seqwencer
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

add_executable(SeqwencerCoreTests
    tests/SequencerCoreTests.cpp
    SeqwencerBridgeProtocol.h
    SequencerCore.h)
target_compile_features(SeqwencerCoreTests PRIVATE cxx_std_17)

juce_add_console_app(SeqwencerVST3ProbeTests
    PRODUCT_NAME "Seqwencer VST3 Probe Tests")
target_sources(SeqwencerVST3ProbeTests
    PRIVATE
        tests/VST3ProbeTests.cpp)
juce_generate_juce_header(SeqwencerVST3ProbeTests)
target_compile_features(SeqwencerVST3ProbeTests PRIVATE cxx_std_17)
target_compile_definitions(SeqwencerVST3ProbeTests
    PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
target_link_libraries(SeqwencerVST3ProbeTests
    PRIVATE
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
