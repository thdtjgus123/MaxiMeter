MaxiMeter
The Ultimate Winamp-Styled Audio Visualization Studio

MaxiMeter is a canvas-based audio visualization editor that brings the legendary aesthetic of Winamp into the modern age. Upload your audio, arrange professional metering tools with classic Winamp skins, and export stunning visualization videos.

🎯 Overview
MaxiMeter combines three powerful concepts into one application:

Professional Audio Metering: Industry-standard meters (VU, Peak, Spectrum, Spectrogram, etc.)

Winamp Classic Skin Support: Full parsing and rendering of .wsz skin files from the golden era

Video Export Pipeline: Render your audio visualizations to high-quality video files

Why MaxiMeter?
For Music Producers: Create analysis videos showcasing your mix's frequency spectrum, stereo field, and dynamics

For Content Creators: Generate eye-catching visualizations for YouTube, Twitch, or VRChat worlds

For Nostalgic Souls: Experience the Y2K aesthetic with authentic Winamp skins applied to modern audio analysis

For VJs and Live Performers: Design custom visual layouts that sync perfectly with your audio

✨ Core Features
Audio Input System
Supported Formats: MP3, WAV, FLAC, OGG, AAC, AIFF

Real-time Analysis Engine:

FFT (Fast Fourier Transform) with configurable window sizes (512, 1024, 2048, 4096, 8192 samples)

Multi-band frequency analysis (configurable band count: 8, 16, 31, 64 bands)

RMS (Root Mean Square) level calculation

True Peak detection with oversampling

LUFS (Loudness Units relative to Full Scale) measurement

Sample-accurate Playback: Synchronize all meters to exact audio timeline

Waveform Overview: Visual timeline with zoom and pan controls

Meter Library
MaxiMeter provides a comprehensive toolbox of professional-grade meters:

Level Meters
VU Meter

Analog-style average level measurement

Configurable decay rate (300ms - 3000ms)

Ballistic characteristics: VU, PPM Type I/II, Nordic

Needle or bar display modes

Clip indicator with hold time

Peak Meter

True peak or sample peak modes

Peak hold with configurable duration (1s - infinite)

Configurable range: -60dB to 0dB, -48dB to +12dB, etc.

Color zones: Green (-∞ to -18dB), Yellow (-18dB to -6dB), Red (-6dB to 0dB)

Overload/clip warning

Frequency Analyzers
Spectrum Analyzer

Real-time frequency bars (8, 16, 31, 64 bands)

Frequency range: 20Hz - 20kHz

Display modes: Linear, Logarithmic, Octave-based

Peak hold dots option

Smoothing and decay controls

Stereo: L/R separate, Mid/Side, or summed mono

Spectrogram

Waterfall-style time-frequency display

Scrolling direction: Horizontal or vertical

Color maps: Rainbow, Heat, Grayscale, Custom

Dynamic range: -60dB to 0dB

Resolution: Time vs frequency trade-off slider

Oscilloscope

Traditional waveform display

Lissajous mode for stereo phase visualization

Trigger modes: Auto, Normal, Single

Time scale: 10ms - 1000ms per division

XY mode for stereo correlation

Spatial Meters
Goniometer (Stereo Phase Scope)

Polar display of L/R relationship

Phase correlation indicator (-1 to +1)

Mono compatibility warning

Persistence/trail effect

Mid/Side visualization mode

Correlation Meter

Real-time stereo phase correlation

Bar or numeric display

Integration time: 50ms - 5000ms

Warning zones for mono incompatibility

Statistical Meters
Loudness Meter

ITU-R BS.1770-4 / EBU R128 compliant

Momentary, Short-term, and Integrated LUFS

Loudness Range (LRA) measurement

Target loudness presets: Streaming (-14 LUFS), Broadcast (-23 LUFS), Cinema (-24 LKFS)

Histogram

Level distribution over time

Separate displays for L/R channels

Bin resolution: 0.1dB - 3dB

Cumulative or reset modes

Headroom analysis

Winamp Classic Skin Engine
MaxiMeter features a complete implementation of the Winamp Classic Skin format (.wsz).

Skin File Structure
Format: ZIP archive with .wsz extension

Contents:

Bitmap files (.bmp): UI elements, buttons, sliders, displays

viscolor.txt: Color palette for spectrum analyzer (24 colors)

region.txt: Window transparency and shape definitions

pledit.txt: Playlist editor font and colors (optional)

readme.txt: Skin credits and information

Supported Components
Main Window (main.bmp):

Play/Pause/Stop/Previous/Next buttons with 3 states (normal, pressed, disabled)

Volume slider (28 positions)

Balance slider (28 positions)

Position slider (customizable length)

Time display (bitmap fonts for digits)

Mono/Stereo indicator

Title scrolling text area

VU meters or mini spectrum (5-band)

Equalizer (eqmain.bmp):

10-band graphic EQ sliders

Preamp slider

On/Off and Auto buttons

Presets display

Playlist Editor (pledit.bmp):

Background textures

Scrollbar elements

Action buttons

Spectrum/Oscilloscope Visualization:

20-band spectrum analyzer using viscolor.txt palette

Oscilloscope waveform display

Bar or line rendering modes

Parsing Engine
ZIP Extraction: Automatic decompression of .wsz files

Bitmap Sprite Sheet Parsing:

Fixed coordinate mapping for each UI element

Automatic extraction of button states (normal at offset 0, pressed at offset +H, disabled at offset +2H)

Slider position frame extraction from grid layouts

Color Palette Application: Parse viscolor.txt for 24-color gradient mapping

Region Masking: Apply region.txt for non-rectangular window shapes

Font Rendering: Bitmap font extraction from numbers.bmp, text.bmp

Skin Integration Workflow
Load Skin: Drag and drop .wsz file or browse Winamp Skin Museum (65,000+ skins)

Component Extraction: MaxiMeter automatically identifies and extracts:

Spectrum analyzer area → Spectrum Meter component

VU meter graphics → VU Meter component

Button states → Interactive controls (optional)

Color schemes → Global theme palette

Toolbox Population: Extracted components appear as draggable meter presets

Canvas Placement: Arrange skinned meters on your canvas

Live Preview: See authentic Winamp animation with your audio

Mix and Match: Combine elements from multiple skins for unique compositions

Skin Library Management
Built-in Browser: Search and preview from Winamp Skin Museum archive

Favorites System: Save your preferred skins for quick access

Metadata Display: Show skin author, year, and description from readme.txt

Thumbnail Generation: Auto-generate preview images for skin library

🔌 Custom Plugin System (Python)
MaxiMeter includes a full Python plugin framework for creating custom
audio-reactive visualisations and meters.

Plugin Architecture

Runtime: Python 3.10+ with automatic dependency installation

IPC: Win32 anonymous pipes + zero-copy SharedMemory (~5 µs per frame)

Rendering: Command-buffer pattern — Python emits draw commands, C++/OpenGL replays them

Error Recovery: Auto-restart on crash (up to 5 retries)

GPU Shader Integration

12 built-in GLSL shaders accessible from Python (particles, spectrum3d, waveform3d, plasma, voronoi, mandelbrot, julia, bloom, blur, glitch, chromatic)

Compute Shader Particles (OpenGL 4.3+): persistent SSBO-backed particle simulation supporting up to 100 000 particles

Fragment Shader Fallback (OpenGL 3.3): automatic downgrade capped at 500 particles for older GPUs

SharedMemory Audio Transport

12 KB memory-mapped region for binary audio data transfer

Fields: per-channel RMS / peak / true-peak, FFT spectrum, waveform, LUFS, BPM, beat phase, stereo correlation, and more

~100× faster than JSON serialisation

Plugin Development: See `CustomComponents/README.md` for the full developer guide, API reference, and example plugins.

🎨 Canvas Editor
Layout System
Infinite Canvas: Zoom (10% - 500%) and pan with mouse/trackpad

Grid and Guides:

Snap-to-grid with configurable spacing (1px, 5px, 10px, 25px, 50px)

Ruler guides (horizontal and vertical)

Smart guides showing alignment with other objects

Layer Management:

Z-order control (bring to front, send to back)

Group/ungroup meters

Lock/unlock individual meters

Show/hide layers

Multi-select: Shift+Click or drag-select for bulk operations

Meter Manipulation
Drag and Drop: Click and drag meters from toolbox to canvas

Resize: Drag corner/edge handles with aspect ratio lock option

Rotate: Free rotation or snap to 90° increments

Properties Panel: Right-click or double-click to configure:

Visual: Colors, opacity, border, shadow

Audio: Input channel (L, R, L+R, Mid, Side), frequency range, sensitivity

Behavior: Response time, decay rate, peak hold

Winamp Skin: Apply skin theme, choose component variant

Timeline Controls
Transport Bar: Play, Pause, Stop, Loop

Scrubbing: Click/drag timeline to preview specific moments

Markers: Add labeled markers for key moments in audio

Region Selection: Define export range (full track or custom selection)

Background Customization
Solid Color: Pick any color or transparent

Gradient: Linear or radial with multi-stop editor

Image: Upload PNG/JPG background with scaling options (fit, fill, tile, stretch)

Video: Use video file as animated background (advanced)

🎬 Video Export System
Rendering Engine
Backend Options:

WebCodecs API: Browser-native encoding (Electron/Web builds)

FFmpeg: High-quality offline rendering with full codec support

Frame-by-frame Rendering:

Accurate audio-video synchronization

Progress bar with estimated time remaining

Pause/resume capability

Background rendering (continue working during export)

Export Settings
Resolution Presets:

1080p (1920×1080) @ 30/60fps

1440p (2560×1440) @ 30/60fps

4K (3840×2160) @ 30/60fps

Custom resolution

Portrait modes for social media (1080×1920)

Video Codecs:

H.264 (MP4) - Maximum compatibility

H.265/HEVC (MP4) - Better compression

VP9 (WebM) - Web-optimized

ProRes (MOV) - Lossless for editing

PNG sequence - Frame-by-frame export

Quality Settings:

Bitrate: VBR or CBR (1 Mbps - 100 Mbps)

Quality presets: Draft, Good, Best

Keyframe interval

Color space: sRGB, Rec.709, Rec.2020

Audio Settings:

Pass-through original audio

Re-encode: AAC, MP3, Opus, FLAC

Sample rate: Match source or resample (44.1kHz, 48kHz, 96kHz)

Bitrate: 128 kbps - 320 kbps

Batch Export
Multiple Configurations: Save different export presets

Queue System: Export same project to multiple formats sequentially

Automation: Apply template to multiple audio files

🛠️ Technical Architecture
Technology Stack Recommendations
Option 1: Electron + Web Technologies
Pros: Cross-platform, rich web ecosystem, easy UI development
Stack:

Framework: Electron + React/Vue/Svelte

Canvas Rendering: Fabric.js or Konva.js for interactive canvas

Audio Processing: Web Audio API (AnalyserNode, ScriptProcessorNode/AudioWorklet)

FFT Library: DSP.js or custom implementation

Video Encoding: WebCodecs API + WebAV library

File Handling: JSZip for .wsz parsing

Styling: CSS with Winamp-inspired themes

Option 2: Native C++ with JUCE
Pros: Best performance, professional audio tools foundation
Stack:

Framework: JUCE (audio application framework)

Audio I/O: JUCE AudioFormatReader/AudioDeviceManager

DSP: JUCE DSP module (FFT, filters, analyzers)

GUI: JUCE Graphics with custom component painting

Video Export: Integrate FFmpeg via subprocess or library

Skin Parsing: libzip + custom BMP parser

Option 3: Unity (Your Specialty!)
Pros: Familiar environment, powerful rendering, good for complex visuals
Stack:

Audio: NAudio library or Unity AudioSource + AudioListener

FFT: Unity Microphone class + custom FFT, or FMOD integration

UI: Unity UI Toolkit or uGUI with custom canvas system

Video: Unity Recorder package + FFmpeg integration

Skin Parsing: SharpZipLib + Texture2D for BMP loading

Core Systems Design
1. Audio Analysis Pipeline
text
Audio File → Decoder → Buffer → FFT → Frequency Bins
                      ↓
                  RMS/Peak → Level Meters
                      ↓
                  Resampler → Video Timeline
2. Skin Parser Module
text
.wsz File → ZIP Extract → BMP Files → Sprite Atlas
                       → viscolor.txt → Color Palette
                       → region.txt → Alpha Mask
                       → Component Registry
3. Render Pipeline
text
Audio Frame → Analysis → Meter State Update → Canvas Render → Frame Buffer
                                                              ↓
                                                          Video Encoder
Performance Optimization
Audio Processing: Run FFT in dedicated worker thread/AudioWorklet

Canvas Rendering: Use WebGL/GPU acceleration for meters when possible

Video Encoding: Utilize hardware encoding (NVENC, Quick Sync, VideoToolbox)

Caching: Pre-compute FFT for entire audio file for instant scrubbing

LOD: Reduce meter detail when zoomed out in canvas

📋 Development Roadmap
Phase 1: Core Foundation (Weeks 1-4)
 Audio file loading and playback

 Basic FFT implementation (1024/2048 samples)

 Simple spectrum analyzer (16-band)

 Canvas with drag-drop meter placement

 Timeline with scrubbing

 Basic VU meter implementation

Phase 2: Winamp Skin Engine (Weeks 5-8)
 .wsz file parser (ZIP extraction)

 BMP sprite sheet loader

 viscolor.txt parser

 Spectrum analyzer component extraction

 VU meter component extraction

 Skin preview and selection UI

 Apply skin to meter instances

Phase 3: Advanced Meters (Weeks 9-12)
 Peak meter with true peak detection

 Spectrogram with scrolling display

 Goniometer/stereo phase scope

 Correlation meter

 Oscilloscope with Lissajous mode

 Loudness meter (EBU R128)

 Histogram meter

Phase 4: Canvas Editor Polish (Weeks 13-14)
 Multi-select and grouping

 Snap-to-grid and guides

 Layer management panel

 Undo/redo system

 Copy/paste/duplicate

 Alignment tools

 Background image/gradient support

Phase 5: Video Export (Weeks 15-18)
 FFmpeg integration

 Frame-by-frame rendering

 Export settings UI (resolution, codec, quality)

 Progress tracking

 H.264 export

 WebM export

 ProRes export (optional)

 Batch export queue

Phase 6: Polish & Launch (Weeks 19-20)
 UI/UX refinement

 Preset templates (saved project layouts)

 Keyboard shortcuts

 Dark/light theme toggle

 Tutorial/onboarding flow

 Performance profiling and optimization

 Cross-platform testing (Windows/Mac/Linux)

 Documentation and website

🎮 User Workflow Example
Scenario: Music Producer Making a Mix Analysis Video
Import Audio

Drag my_master.wav onto canvas

Waveform appears on timeline

Choose Aesthetic

Click "Browse Skins" → Search "Skinner v1.0" (classic favorite)

Preview shows Winamp skin from 2001

Click "Apply"

Build Layout

Drag "Spectrum Analyzer" from skinned toolbox → Position top-center

Drag "VU Meter" → Position bottom-left

Drag "Goniometer" → Position bottom-right

Drag "Peak Meter" → Position as vertical bars on left/right edges

Customize

Double-click Spectrum → Set to 31-band, logarithmic scale

Double-click VU → Set decay to 300ms VU ballistics

Double-click Goniometer → Enable trail effect (500ms persistence)

Set canvas background to dark gradient

Preview

Hit Play → Meters animate with authentic Winamp look

Scrub timeline to check all sections look good

Export

Click Export → Choose 1080p @ 60fps, H.264, 15 Mbps

Hit "Render Video"

5 minutes later → my_master_visualization.mp4 ready for YouTube!

🎯 Differentiators
vs. Existing Audio Visualizers
Most visualizers (VSXU, ProjectM, plane9): Real-time music visualization for live playback, not video export, no custom layout control

MaxiMeter: Precise control over meter placement, Winamp aesthetic, professional metering, video export

vs. DAW Metering Plugins
DAW plugins (iZotope Insight, Waves PAZ): Professional meters but stuck inside DAW, no video export

MaxiMeter: Standalone, designed for video content, nostalgic branding

vs. After Effects/Motion Graphics
Motion graphics software: Requires manual animation, no real-time audio reactivity, steep learning curve

MaxiMeter: Automatic audio-reactive animation, one-click export, accessible to non-editors

🌟 Future Expansion Ideas
Community Features
Skin Marketplace: User-submitted skins and meter designs

Template Library: Pre-made layouts for different genres (EDM, podcast, lofi, etc.)

Cloud Sync: Save projects to cloud, work across devices

Advanced Capabilities
MIDI Control: Map hardware controllers to meter parameters

Live Mode: Real-time visualization from audio input (microphone/line-in) for live streaming

Plugin Support: Load VST/AU plugins for audio effects before visualization

3D Meters: Volumetric spectrum analyzers, 3D oscilloscopes (leveraging Unity/Three.js)

Platform Expansion
Web App: Browser-based lite version (limited export)

Mobile: iOS/Android viewer app for sharing visualizations

OBS Plugin: Direct integration for streamers

📖 Technical References
Winamp Skin Format
WSZ Specification - Complete format documentation

Winamp Developer Wiki - Official skin creation guide

Webamp Source Code - Reference parser implementation

Audio DSP
Web Audio API Specification - Browser audio processing

EBU R128 - Loudness normalization standard

JUCE DSP Module - Professional audio processing

Video Encoding
FFmpeg Documentation - Comprehensive encoding reference

WebCodecs API - Browser-native video encoding

🤝 Contributing
MaxiMeter is a passion project celebrating audio engineering and Y2K internet culture. Contributions welcome in:

Winamp skin parsing improvements

New meter types (K-System, Dolby meters, etc.)

Performance optimizations

Cross-platform compatibility fixes

UI/UX enhancements

📜 License
[Choose appropriate license: MIT, GPL-3.0, etc.]

🙏 Acknowledgments
Nullsoft/Winamp: For creating the iconic music player that defined an era

Winamp Skin Museum: For preserving 65,000+ skins

Audio engineering community: For developing metering standards

Y2K aesthetic revivalists: For keeping the dream alive

MaxiMeter - Where nostalgia meets professional audio visualization 🎵