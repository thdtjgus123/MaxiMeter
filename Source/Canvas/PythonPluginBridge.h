#pragma once

/**
 * @file PythonPluginBridge.h
 * @brief C++ side of the Python custom component bridge.
 *
 * This header defines the interface between the C++ host (MaxiMeter / JUCE) and
 * the Python plugin runtime running as a child process.
 *
 * Communication uses JSON-line protocol over stdin/stdout pipes:
 *   Host  → Python:  { "type": "render", "instance_id": ..., "audio": {...}, "width": ..., "height": ... }
 *   Python → Host:   { "type": "render_commands", "commands": [...] }
 *
 * INTEGRATION STEPS:
 *   1. Add this header + PythonPluginBridge.cpp to your CMakeLists.txt
 *   2. On startup, call PythonPluginBridge::getInstance().start("path/to/plugins");
 *   3. Query getAvailablePlugins() to populate the TOOLBOX
 *   4. When user adds a custom component, call createInstance(manifestId)
 *   5. Each frame, call renderInstance(id, width, height, audioJson) →
 *      returns a list of draw commands to replay through juce::Graphics
 *   6. On shutdown, call stop()
 *
 * Implementation: PythonPluginBridge.cpp using native Win32 pipes
 * (CreateProcess + anonymous pipes for stdin/stdout IPC).
 */

#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <functional>

#if JUCE_WINDOWS
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#endif

//==============================================================================
/// Forward-declared render command types matching the Python RenderContext output.
namespace PluginRender
{
    /// Matches Python _CmdType enum values.
    enum class CmdType
    {
        Clear,
        FillRect, StrokeRect,
        FillRoundedRect, StrokeRoundedRect,
        FillEllipse, StrokeEllipse,
        DrawLine,
        DrawPath, FillPath,
        DrawText,
        DrawImage,
        FillGradientRect,
        SetClip, ResetClip,
        PushTransform, PopTransform,
        SetOpacity, SetBlendMode,
        DrawArc, FillArc,
        // ── Extended commands (v2) ──
        DrawBezier,
        FillCircle, StrokeCircle,
        DrawPolyline,
        SaveState, RestoreState,
        // ── GPU shader commands (v3) ──
        DrawShader,          ///< Execute a GLSL shader pass
        DrawCustomShader,    ///< Execute user-provided GLSL source
    };

    /// A single render command with parameters stored as a juce::var (JSON-like).
    struct RenderCommand
    {
        CmdType type;
        juce::var params;  // DynamicObject with command-specific keys
    };
}

//==============================================================================
/// Metadata for a custom plugin, as reported by the Python registry.
struct CustomPluginManifest
{
    juce::String id;            ///< e.g. "com.maximeter.example.simple_vu"
    juce::String name;          ///< e.g. "Simple VU Meter"
    juce::String category;      ///< e.g. "METER"
    juce::String description;
    juce::String author;
    juce::String version;
    juce::String sourceFile;    ///< e.g. "my_component_01" (filename stem for matching)
    int          defaultWidth  = 300;
    int          defaultHeight = 200;
    juce::StringArray tags;
};

//==============================================================================
/// Property descriptor reported by Python plugins.
struct CustomPluginProperty
{
    juce::String key;
    juce::String label;
    juce::String type;      ///< "INT", "FLOAT", "BOOL", "STRING", "COLOR", "ENUM", "RANGE"…
    juce::var    defaultVal;
    juce::String group;
    float        minVal  = 0;
    float        maxVal  = 1;
    float        step    = 0;
    juce::Array<std::pair<juce::var, juce::String>> choices;
    juce::String description;
};

//==============================================================================
/**
 * Singleton bridge to the Python plugin subprocess.
 *
 * Thread safety:  All public methods are meant to be called from the
 * message thread.  The bridge internally guards pipe I/O.
 */
class PythonPluginBridge
{
public:
    static PythonPluginBridge& getInstance();

    //-- Lifecycle -----------------------------------------------------------

    /// Start the Python bridge subprocess.
    /// @param pluginsDir  Absolute path to the plugins/ directory.
    /// @param pythonExe   Optional custom Python executable path.
    /// @return true if the process launched successfully.
    bool start(const juce::File& pluginsDir,
               const juce::String& pythonExe = "python");

    /// Gracefully shut down the subprocess.
    void stop();

    /// @return true if the subprocess is running.
    bool isRunning() const;

    //-- Discovery -----------------------------------------------------------

    /// Ask the Python side to (re-)scan the plugins directory.
    /// Blocks briefly; call from message thread.
    void scanPlugins();

    /// Get the list of available custom plugin manifests.
    std::vector<CustomPluginManifest> getAvailablePlugins() const;

    //-- Instance management -------------------------------------------------

    /// Create a new instance of a custom component.
    /// @param manifestId  The Manifest.id (e.g. "com.example.my_meter").
    /// @param instanceId  A UUID string to identify this instance.
    /// @return Property descriptors for the Property Panel, or empty on error.
    std::vector<CustomPluginProperty> createInstance(const juce::String& manifestId,
                                                     const juce::String& instanceId);

    /// Destroy an instance.
    void destroyInstance(const juce::String& instanceId);

    //-- Rendering -----------------------------------------------------------

    /// Request render commands for one frame.
    /// @param instanceId  The instance UUID.
    /// @param width       Component width in pixels.
    /// @param height      Component height in pixels.
    /// @param audioJson   Serialised AudioData snapshot (see AudioDataSerialiser).
    /// @return List of render commands to replay through juce::Graphics.
    std::vector<PluginRender::RenderCommand> renderInstance(
        const juce::String& instanceId,
        int width, int height,
        const juce::String& audioJson);

    //-- Properties ----------------------------------------------------------

    /// Notify the Python side that a property value changed.
    void setProperty(const juce::String& instanceId,
                     const juce::String& key,
                     const juce::var& value);

    /// Notify the Python side that the component was resized.
    void notifyResize(const juce::String& instanceId, int width, int height);

    //-- Serialisation -------------------------------------------------------

    /// Serialise all Python instances for project save.
    juce::String serialiseAll();

    /// Restore instances from project load data.
    void deserialiseAll(const juce::String& jsonData);

    //-- Hot reload ----------------------------------------------------------

    /// Hot-reload a single plugin.
    bool reloadPlugin(const juce::String& manifestId);

    //-- Error callback ------------------------------------------------------

    /// Called on bridge errors (subprocess crash, protocol errors, etc.)
    std::function<void(const juce::String& errorMessage)> onError;

private:
    PythonPluginBridge() = default;
    ~PythonPluginBridge();

    PythonPluginBridge(const PythonPluginBridge&) = delete;
    PythonPluginBridge& operator=(const PythonPluginBridge&) = delete;

    /// Send a JSON message to the Python subprocess and wait for response.
    /// @param timeoutMs  Max wait time in milliseconds (0 = use default).
    juce::var sendMessage(const juce::var& msg, DWORD timeoutMs = 0);

    /// Parse a CustomPluginManifest from a JSON var.
    static CustomPluginManifest parseManifest(const juce::var& v);

    /// Parse a render command from a JSON var.
    static PluginRender::RenderCommand parseRenderCommand(const juce::var& v);

    //-- Members -------------------------------------------------------------
    std::vector<CustomPluginManifest>         cachedManifests;
    juce::CriticalSection                     pipeLock;
    bool                                      running = false;

    //-- Error recovery state ------------------------------------------------
    juce::File  lastPluginsDir_;    ///< Remembered for restart
    juce::String lastPythonExe_;   ///< Remembered for restart
    int          restartCount_ = 0;
    bool         insideScan_   = false;  ///< Guard against recursive scan during restart
    static constexpr int kMaxRestarts = 5;

    /// Attempt to restart the crashed subprocess.
    /// Returns true if restart succeeded.
    bool tryRestart();

#if JUCE_WINDOWS
    HANDLE hProcess       = nullptr;
    HANDLE hStdinWrite    = nullptr;   ///< write end of child's stdin
    HANDLE hStdoutRead    = nullptr;   ///< read end of child's stdout
#endif
};


//==============================================================================
/**
 * Shared memory audio transport for zero-copy data transfer to Python.
 *
 * Instead of serializing audio data as JSON every frame, the host writes
 * audio data into a shared memory region that Python reads directly.
 * This reduces per-frame IPC overhead from ~500µs to ~5µs.
 *
 * Layout matches the Python AudioSharedMemoryReader in shared_memory.py.
 */
class AudioSharedMemory
{
public:
    AudioSharedMemory() = default;
    ~AudioSharedMemory() { destroy(); }

    static constexpr uint32_t kMagic   = 0x4D584D41; // "MXMA"
    static constexpr uint32_t kVersion = 1;
    static constexpr int kHeaderSize   = 236;
    static constexpr int kMaxChannels  = 8;
    static constexpr int kChannelStride = 20; // 5 × float32
    static constexpr const char* kShmName = "MaxiMeter_AudioSHM";

    /// Create the shared memory region. Call once at startup.
    bool create(int fftSize = 4096, int waveformSize = 1024)
    {
        int spectrumCount = fftSize / 2 + 1;
        bufferSize = kHeaderSize + spectrumCount * 4 + waveformSize * 4;

#if JUCE_WINDOWS
        hMapFile = CreateFileMappingA(
            INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
            0, (DWORD)bufferSize, kShmName);
        if (hMapFile == nullptr) return false;

        pBuf = (uint8_t*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, bufferSize);
        if (pBuf == nullptr) { CloseHandle(hMapFile); hMapFile = nullptr; return false; }
#endif

        // Write header
        writeU32(0, kMagic);
        writeU32(4, kVersion);
        writeU32(8, 0);          // frame counter
        writeU32(12, (uint32_t)bufferSize);
        writeU32(24, (uint32_t)fftSize);
        writeU32(28, (uint32_t)waveformSize);

        this->fftSize = fftSize;
        this->waveformSize = waveformSize;
        ready = true;
        return true;
    }

    /// Destroy the shared memory region.
    void destroy()
    {
#if JUCE_WINDOWS
        if (pBuf) { UnmapViewOfFile(pBuf); pBuf = nullptr; }
        if (hMapFile) { CloseHandle(hMapFile); hMapFile = nullptr; }
#endif
        ready = false;
    }

    bool isReady() const { return ready; }

    /// Write per-channel levels.
    void writeChannelData(int ch, float rms, float peak, float truePeak,
                          float rmsLin, float peakLin)
    {
        if (!pBuf || ch >= kMaxChannels) return;
        int off = 76 + ch * kChannelStride;
        writeF32(off + 0,  rms);
        writeF32(off + 4,  peak);
        writeF32(off + 8,  truePeak);
        writeF32(off + 12, rmsLin);
        writeF32(off + 16, peakLin);
    }

    /// Write spectrum data (linear, fftSize/2+1 floats).
    void writeSpectrum(const float* data, int count)
    {
        if (!pBuf) return;
        int maxCount = fftSize / 2 + 1;
        count = juce::jmin(count, maxCount);
        memcpy(pBuf + kHeaderSize, data, count * sizeof(float));
    }

    /// Write waveform data.
    void writeWaveform(const float* data, int count)
    {
        if (!pBuf) return;
        count = juce::jmin(count, waveformSize);
        int spectrumBytes = (fftSize / 2 + 1) * sizeof(float);
        memcpy(pBuf + kHeaderSize + spectrumBytes, data, count * sizeof(float));
    }

    /// Write all scalar fields and increment frame counter.
    void writeFrame(float sampleRate, int numChannels, bool isPlaying,
                    float positionSec, float durationSec,
                    float correlation, float stereoAngle,
                    float lufsMom, float lufsShort, float lufsInteg, float luRange,
                    float bpm, float beatPhase)
    {
        if (!pBuf) return;
        writeF32(16, sampleRate);
        writeU32(20, (uint32_t)numChannels);
        writeU32(32, isPlaying ? 1u : 0u);
        writeF32(36, positionSec);
        writeF32(40, durationSec);
        writeF32(44, correlation);
        writeF32(48, stereoAngle);
        writeF32(52, lufsMom);
        writeF32(56, lufsShort);
        writeF32(60, lufsInteg);
        writeF32(64, luRange);
        writeF32(68, bpm);
        writeF32(72, beatPhase);

        // Increment frame counter (atomic from Python reader's perspective)
        frameCounter++;
        writeU32(8, frameCounter);
    }

private:
    void writeU32(int offset, uint32_t v)
    {
        memcpy(pBuf + offset, &v, 4);
    }
    void writeF32(int offset, float v)
    {
        memcpy(pBuf + offset, &v, 4);
    }

    uint8_t* pBuf = nullptr;
    int bufferSize = 0;
    int fftSize = 4096;
    int waveformSize = 1024;
    uint32_t frameCounter = 0;
    bool ready = false;

#if JUCE_WINDOWS
    HANDLE hMapFile = nullptr;
#endif
};


//==============================================================================
/**
 * Helper to serialise the current audio analysis state to a JSON string
 * compatible with the Python AudioData format.
 *
 * Call this once per frame and pass the result to renderInstance().
 */
namespace AudioDataSerialiser
{
    /// Build a JSON string from current analysis data.
    /// Implement in PythonPluginBridge.cpp using your AudioEngine / analyzers.
    juce::String buildAudioJson(/* your analyser refs here */);
}
