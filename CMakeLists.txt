cmake_minimum_required(VERSION 3.22)
project(MaxiMeter VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Statically link the MSVC C++ runtime so the exe runs on machines
# without Visual Studio or the VC++ Redistributable installed.
# Debug  -> /MTd   Release -> /MT
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Add JUCE
add_subdirectory(JUCE)

# ── libprojectM 4 (Milkdrop visualizer engine) ───────────────────────────────
# Built as a separate ExternalProject so it never conflicts with MaxiMeter's
# /MT runtime or CMake structure.  libprojectM is LGPL-2.1; we link statically
# and bundle the source URL so the license terms are met (no DLL needed).
include(ExternalProject)

# ── GLEW (required by projectM on Windows) ───────────────────────────────────
# projectM's Windows/OpenGL-Core path calls find_package(GLEW REQUIRED).
# We build GLEW as another ExternalProject and pass its install dir to projectM
# so no system-wide GLEW install is needed.
set(GLEW_INSTALL_DIR "${CMAKE_BINARY_DIR}/glew_install")
file(MAKE_DIRECTORY "${GLEW_INSTALL_DIR}/include")

if(WIN32)
    set(GLEW_LIB "${GLEW_INSTALL_DIR}/lib/libglew32.lib")
else()
    set(GLEW_LIB "${GLEW_INSTALL_DIR}/lib/libGLEW.a")
endif()

ExternalProject_Add(glew_external
    URL            "https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.tgz"
    URL_HASH       SHA256=d4fc82893cfb00109578d0a1a2337fb8ca335b3ceccf97b97e5cc7f08e4353e1
    SOURCE_SUBDIR  "build/cmake"
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=${GLEW_INSTALL_DIR}
        -DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}
        -DBUILD_UTILS=OFF
        -DBUILD_SHARED_LIBS=OFF
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    BUILD_BYPRODUCTS ${GLEW_LIB}
    INSTALL_COMMAND ${CMAKE_COMMAND} --build . --target install --config Release
)

set(PROJECTM_INSTALL_DIR "${CMAKE_BINARY_DIR}/projectm_install")
set(PROJECTM_INCLUDE_DIR "${PROJECTM_INSTALL_DIR}/include")

# CMake validates INTERFACE_INCLUDE_DIRECTORIES at configure time, so the
# directory must exist even before the ExternalProject has built.
file(MAKE_DIRECTORY "${PROJECTM_INCLUDE_DIR}")

# Lib names differ by config; Release libs are always used (projectM is
# build-type-neutral for audio visualisation — no debug symbols needed).
if(WIN32)
    set(PROJECTM_LIB          "${PROJECTM_INSTALL_DIR}/lib/libprojectM-4.lib")
    set(PROJECTM_PLAYLIST_LIB "${PROJECTM_INSTALL_DIR}/lib/libprojectM-4-playlist.lib")
else()
    set(PROJECTM_LIB          "${PROJECTM_INSTALL_DIR}/lib/libprojectM-4.a")
    set(PROJECTM_PLAYLIST_LIB "${PROJECTM_INSTALL_DIR}/lib/libprojectM-4-playlist.a")
endif()

ExternalProject_Add(projectm_external
    GIT_REPOSITORY "https://github.com/projectM-visualizer/projectm.git"
    GIT_TAG        "v4.1.1"
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
    DEPENDS        glew_external
    CMAKE_ARGS
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=${PROJECTM_INSTALL_DIR}
        -DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY}
        -DENABLE_SDL_UI=OFF
        -DENABLE_EMSCRIPTEN=OFF
        -DENABLE_PLAYLIST=ON
        -DENABLE_TESTING=OFF
        -DBUILD_SHARED_LIBS=OFF
        -DCMAKE_CXX_STANDARD=17
        # Point projectM to our GLEW install via CMAKE_PREFIX_PATH so
        # find_package(GLEW REQUIRED) finds the glew-config.cmake we installed
        "-DCMAKE_PREFIX_PATH=${GLEW_INSTALL_DIR}"
    BUILD_BYPRODUCTS
        ${PROJECTM_LIB}
        ${PROJECTM_PLAYLIST_LIB}
    # Only run install step to avoid polluting the build tree
    INSTALL_COMMAND ${CMAKE_COMMAND} --build . --target install --config Release
)

# Imported targets so MaxiMeter can link against the built libraries.
# These targets become valid once projectm_external has built.
add_library(projectm::projectm STATIC IMPORTED GLOBAL)
set_target_properties(projectm::projectm PROPERTIES
    IMPORTED_LOCATION             "${PROJECTM_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${PROJECTM_INCLUDE_DIR}"
)
add_dependencies(projectm::projectm projectm_external)

add_library(projectm::playlist STATIC IMPORTED GLOBAL)
set_target_properties(projectm::playlist PROPERTIES
    IMPORTED_LOCATION             "${PROJECTM_PLAYLIST_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${PROJECTM_INCLUDE_DIR}"
)
add_dependencies(projectm::playlist projectm_external)

# MaxiMeter also needs to link against GLEW (since projectM's static lib
# pulls in GLEW symbols that must be resolved at final link time).
add_library(glew::glew STATIC IMPORTED GLOBAL)
set_target_properties(glew::glew PROPERTIES
    IMPORTED_LOCATION             "${GLEW_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${GLEW_INSTALL_DIR}/include"
)
add_dependencies(glew::glew glew_external)
file(MAKE_DIRECTORY "${GLEW_INSTALL_DIR}/include")

# Main application target
juce_add_gui_app(MaxiMeter
    PRODUCT_NAME "MaxiMeter"
    COMPANY_NAME "MaxiMeter"
    BUNDLE_ID "com.maximeter.app"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/ico.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/ico.png"
    NEEDS_WEBVIEW2 TRUE
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
    Source/UI/EqualizerPanel.cpp
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
    Source/Canvas/WaterReflectionComponent.cpp

    # Canvas: projectM Milkdrop visualizer
    Source/Canvas/ProjectMComponent.cpp

    # ThreeD: 3D workflow (Three.js WebView)
    Source/ThreeD/ThreeDWebView.cpp
    Source/ThreeD/ThreeDEditor.cpp
    Source/ThreeD/ThreeDOfflineRenderer.cpp
    Source/ThreeD/ThreeDExportProgressWindow.cpp

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

    # VJ Mode
    Source/Audio/BPMDetector.cpp
    Source/VJ/VJTransitionEngine.cpp
    Source/VJ/VJBPMSync.cpp
    Source/VJ/VJSceneManager.cpp
    Source/VJ/VJControlPanel.cpp
    Source/VJ/VJEditor.cpp
    Source/VJ/VJGLSLTransition.cpp
    Source/VJ/VJShaderEditorDialog.cpp
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
        projectm::projectm
        projectm::playlist
        glew::glew
        opengl32
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
        juce::juce_recommended_lto_flags
)

# Preprocessor definitions
# MaxiMeter must depend on the external build so projectM is ready before linking
add_dependencies(MaxiMeter projectm_external glew_external)

target_compile_definitions(MaxiMeter PRIVATE
    JUCE_WEB_BROWSER=1
    PROJECTM_AVAILABLE=1
    PROJECTM_STATIC_DEFINE=1
    PROJECTM_PLAYLIST_STATIC_DEFINE=1
    GLEW_STATIC=1
    JUCE_USE_WIN_WEBVIEW2=1
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:MaxiMeter,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:MaxiMeter,JUCE_VERSION>"
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

# Allow duplicate stb_image symbols between MaxiMeter's stb_image_impl.cpp
# and projectM's embedded SOIL2 (both define STB_IMAGE_IMPLEMENTATION).
if(MSVC)
    target_link_options(MaxiMeter PRIVATE /FORCE:MULTIPLE)
endif()

# Copy assets (Three.js web app) next to executable after each build
add_custom_command(TARGET MaxiMeter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:MaxiMeter>/assets"
    COMMENT "Copying assets to output directory"
)

# Copy CustomComponents Python package next to executable after each build
add_custom_command(TARGET MaxiMeter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/CustomComponents"
        "$<TARGET_FILE_DIR:MaxiMeter>/CustomComponents"
    COMMENT "Copying CustomComponents Python package to output directory"
)

# ── FFmpeg auto-download (Windows, LGPL build) ───────────────────────────────
# Downloads ffmpeg.exe from gyan.dev at configure time (once only) and copies
# it next to MaxiMeter.exe at build time.  VideoLayerComponent requires
# ffmpeg.exe for video playback; audio/image layers work without it.
if(WIN32)
    set(FFMPEG_DL_DIR "${CMAKE_BINARY_DIR}/ffmpeg_download")
    set(FFMPEG_EXE    "${FFMPEG_DL_DIR}/ffmpeg.exe")

    if(NOT EXISTS "${FFMPEG_EXE}")
        message(STATUS "FFmpeg not found in build cache — downloading LGPL build...")
        file(MAKE_DIRECTORY "${FFMPEG_DL_DIR}")
        file(DOWNLOAD
            "https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip"
            "${FFMPEG_DL_DIR}/ffmpeg.zip"
            SHOW_PROGRESS
            TIMEOUT 120
            STATUS FFMPEG_DL_STATUS
        )
        list(GET FFMPEG_DL_STATUS 0 FFMPEG_DL_CODE)
        if(FFMPEG_DL_CODE EQUAL 0)
            file(ARCHIVE_EXTRACT
                INPUT      "${FFMPEG_DL_DIR}/ffmpeg.zip"
                DESTINATION "${FFMPEG_DL_DIR}"
                PATTERNS   "*/bin/ffmpeg.exe"
            )
            file(GLOB_RECURSE FFMPEG_EXE_FOUND "${FFMPEG_DL_DIR}/*/bin/ffmpeg.exe")
            if(FFMPEG_EXE_FOUND)
                list(GET FFMPEG_EXE_FOUND 0 _ffexe)
                file(COPY "${_ffexe}" DESTINATION "${FFMPEG_DL_DIR}")
                message(STATUS "FFmpeg downloaded and extracted successfully.")
            else()
                message(WARNING "FFmpeg zip downloaded but ffmpeg.exe not found inside. "
                                "Video playback will require manual FFmpeg installation.")
            endif()
        else()
            message(WARNING "FFmpeg download failed (${FFMPEG_DL_STATUS}). "
                            "Video playback will require manual FFmpeg installation next to MaxiMeter.exe.")
        endif()
    endif()

    if(EXISTS "${FFMPEG_EXE}")
        add_custom_command(TARGET MaxiMeter POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${FFMPEG_EXE}"
                "$<TARGET_FILE_DIR:MaxiMeter>/ffmpeg.exe"
            COMMENT "Copying ffmpeg.exe to output directory"
            VERBATIM
        )
    endif()
endif()
