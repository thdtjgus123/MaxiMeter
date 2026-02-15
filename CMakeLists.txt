cmake_minimum_required(VERSION 3.22)
project(MaxiMeter VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add JUCE
add_subdirectory(JUCE)

# Main application target
juce_add_gui_app(MaxiMeter
    PRODUCT_NAME "MaxiMeter"
    COMPANY_NAME "MaxiMeter"
    BUNDLE_ID "com.maximeter.app"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/ico.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/ico.png"
)

# Generate JuceHeader.h
juce_generate_juce_header(MaxiMeter)

# Binary data (fonts + app icon)
juce_add_binary_data(FontAwesomeData SOURCES
    ThirdParty/FontAwesome/fa-solid-900.ttf
    ico.png
)

# Source files
target_sources(MaxiMeter PRIVATE
    # Main entry
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/Utils/CrashHandler.cpp

    # Audio engine
    Source/Audio/AudioEngine.cpp
    Source/Audio/FFTProcessor.cpp
    Source/Audio/LevelAnalyzer.cpp

    # UI components
    Source/UI/MainWindow.cpp
    Source/UI/TransportBar.cpp
    Source/UI/WaveformView.cpp
    Source/UI/StatusBar.cpp

    # UI: Stage 3 — skinned meters & renderers
    Source/UI/BitmapFontRenderer.cpp
    Source/UI/WinampSkinRenderer.cpp
    Source/UI/SkinnedPlayerPanel.cpp
    Source/UI/SkinnedSpectrumAnalyzer.cpp
    Source/UI/SkinnedVUMeter.cpp
    Source/UI/SkinnedOscilloscope.cpp
    Source/UI/PeakMeter.cpp

    # Audio: Stage 4 — advanced analyzers
    Source/Audio/LoudnessAnalyzer.cpp
    Source/Audio/StereoFieldAnalyzer.cpp

    # UI: Stage 4 — advanced meters
    Source/UI/Spectrogram.cpp
    Source/UI/Goniometer.cpp
    Source/UI/CorrelationMeter.cpp
    Source/UI/LoudnessMeter.cpp
    Source/UI/LevelHistogram.cpp
    Source/UI/LissajousScope.cpp
    Source/UI/MultiBandAnalyzer.cpp

    # Skin engine (Stage 2)
    Source/Skin/stb_image_impl.cpp
    Source/Skin/BitmapLoader.cpp
    Source/Skin/SkinSpriteTable.cpp
    Source/Skin/SkinTextParsers.cpp
    Source/Skin/SkinParser.cpp

    # Canvas: Stage 5
    Source/Canvas/CanvasModel.cpp
    Source/Canvas/MeterFactory.cpp
    Source/Canvas/CanvasView.cpp
    Source/Canvas/CanvasToolbox.cpp
    Source/Canvas/CanvasPropertyPanel.cpp
    Source/Canvas/MeterSettingsPanel.cpp
    Source/Canvas/LayerPanel.cpp
    Source/Canvas/MiniMap.cpp
    Source/Canvas/AlignmentToolbar.cpp
    Source/Canvas/CanvasEditor.cpp

    # Canvas: Custom Plugin / GPU pipeline
    Source/Canvas/PythonPluginBridge.cpp
    Source/Canvas/CustomPluginComponent.cpp

    # Export: Stage 6
    Source/Export/FFmpegProcess.cpp
    Source/Export/OfflineRenderer.cpp
    Source/Export/ExportDialog.cpp
    Source/Export/ExportProgressWindow.cpp
    Source/Export/BatchExporter.cpp

    # Stage 7: Polish & Release
    Source/UI/ThemeManager.cpp
    Source/UI/KeyboardShortcutManager.cpp
    Source/Project/ProjectSerializer.cpp
    Source/Project/PresetTemplates.cpp
)

# Include directories
target_include_directories(MaxiMeter PRIVATE
    Source
    ThirdParty/stb
)

# JUCE modules
target_link_libraries(MaxiMeter
    PRIVATE
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_opengl
        FontAwesomeData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
        juce::juce_recommended_lto_flags
)

# Preprocessor definitions
target_compile_definitions(MaxiMeter PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:MaxiMeter,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:MaxiMeter,JUCE_VERSION>"
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

# Copy CustomComponents Python package next to executable after each build
add_custom_command(TARGET MaxiMeter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/CustomComponents"
        "$<TARGET_FILE_DIR:MaxiMeter>/CustomComponents"
    COMMENT "Copying CustomComponents Python package to output directory"
)
