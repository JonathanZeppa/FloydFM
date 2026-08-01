cmake_minimum_required(VERSION 3.22)

# Mac deployment target -- CRITICAL (FLOYD-FM-HANDOFF.md "Build").
# Without 10.13 set explicitly, `minos` defaults to the build OS version and
# the plugin silently fails on older Macs (auval reports `Cannot open
# component: -1`, no crash, passes Gatekeeper, no quarantine).
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS target" FORCE)
endif()

project(FloydFM VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.12
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# PRODUCT_NAME has NO whitespace (Phase 1 gate -- whitespace causes host
# caching bugs). PLUGIN_CODE confirmed with Jonathan 2026-07-26: Flfm.
juce_add_plugin(FloydFM
    COMPANY_NAME "Zedtronics"
    BUNDLE_ID net.zedtronics.FloydFM
    PLUGIN_MANUFACTURER_CODE Zedt
    PLUGIN_CODE Flfm
    # LV2 alongside VST3/AU so Linux users get the full interface. JUCE
    # has exported LV2 natively since JUCE 7; this is the same editor,
    # not a headless build.
    #
    # The separate Rust LV2 (FloydFM-Pi, URI .../floydfm-pi) remains the
    # Zynthian/Raspberry Pi build: headless, tiny, cross-compilable.
    # Distinct URIs, so both can be installed at once.
    FORMATS VST3 AU LV2
    LV2URI "https://zedtronics.net/plugins/floydfm"
    PRODUCT_NAME "FloydFM"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    # TRUE so the editor receives arrow keys for preset / algorithm
    # navigation. keyPressed() consumes ONLY the four arrows and returns
    # false for everything else, so DAW transport shortcuts still work.
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Instrument" "Synth"
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

# JUCE 8: do NOT include juce_recommended_lto_flags (CG7).
# Do NOT define JUCE_DISPLAY_SPLASH_SCREEN (CG6).
target_compile_definitions(FloydFM PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

# Montserrat -- the family LVGL ships as its built-in font. Replaced
# VT323 on 2026-07-26 for visual consistency with an LVGL hardware panel.
juce_add_binary_data(FloydFMData SOURCES
    Resources/Fonts/Montserrat-Regular.ttf
    Resources/Fonts/Montserrat-Medium.ttf
    Resources/Fonts/Montserrat-SemiBold.ttf)

target_sources(FloydFM PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/UI/FloydColours.h
    Source/UI/FloydFonts.h
    Source/UI/FloydLayout.h
    Source/UI/TextHelpers.h
    Source/UI/OperatorTab.h
    Source/UI/Controls.h
    Source/UI/VSlider.h
    Source/UI/RoutingDiagram.h
    Source/UI/Brightness.h
    Source/UI/EnvelopeCanvas.h
    Source/UI/PresetBar.h
    Source/DSP/Algorithm.h
    Source/DSP/RatioTable.h
    Source/DSP/EnvelopeShape.h
    Source/DSP/Operator.h
    Source/DSP/Voice.h
    Source/DSP/VoiceManager.h
    Source/DSP/Presets.h)

target_include_directories(FloydFM PRIVATE Source)

target_link_libraries(FloydFM PRIVATE
    FloydFMData
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_graphics
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------
#  Dev-only headless screenshot harness. NOT part of the plugin and NOT
#  built by default -- it is excluded from ALL_BUILD and must be asked
#  for by name:
#     cmake --build build --config Release --target FloydFMRender
#     build\Release\FloydFMRender.exe panel.png
#  Rationale: catch layout and typography drift by self-screenshot.
# ---------------------------------------------------------------------
juce_add_console_app(FloydFMRender PRODUCT_NAME "FloydFMRender")

target_sources(FloydFMRender PRIVATE Tools/RenderPanel.cpp)

target_compile_definitions(FloydFMRender PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STANDALONE_APPLICATION=0)

# FloydFM links its JUCE modules PRIVATE, so the module include paths do
# not propagate to a consumer -- the harness must name them itself.
target_link_libraries(FloydFMRender PRIVATE
    FloydFM
    FloydFMData
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_graphics
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

set_target_properties(FloydFMRender PROPERTIES EXCLUDE_FROM_ALL TRUE)
