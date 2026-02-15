#pragma once

/**
 * @file CustomPluginComponent.h
 * @brief GPU-accelerated component for Python custom plugins.
 *
 * Uses a **shared off-screen OpenGL context** to render custom Python plugin
 * output.  Unlike a per-component GL context (which creates a native HWND that
 * breaks lightweight z-ordering), the shared context renders to FBOs and reads
 * back to juce::Image.  All visible output goes through paint(), so this
 * component behaves identically to every other lightweight JUCE meter.
 *
 * Supports:
 *   - 2D command-buffer replay (via PluginRenderReplayer through a back-buffer)
 *   - GLSL shader pipeline (Mandelbrot, bloom, blur, glitch, particles...)
 *   - Texture upload from Python (DrawImage)
 *   - Fullscreen-quad shader passes
 *
 * ARCHITECTURE:
 *   Python on_render() -> command buffer (JSON) -> C++ PythonPluginBridge
 *     -> CustomPluginComponent:
 *         1. "standard" commands -> PluginRenderReplayer -> juce::Graphics -> image
 *         2. DrawShader commands -> GLSL pipeline -> FBO -> readback
 *         3. Final composite to screen via paint()
 */

#include <JuceHeader.h>
#include "../UI/MeterBase.h"
#include "PythonPluginBridge.h"
#include "PluginRenderReplayer.h"
#include <unordered_map>

//==============================================================================
/// Built-in GLSL shader IDs that Python plugins can reference.
namespace ShaderIDs
{
    static constexpr const char* kMandelbrot     = "mandelbrot";
    static constexpr const char* kJuliaSet       = "julia";
    static constexpr const char* kBloom          = "bloom";
    static constexpr const char* kBlur           = "blur";
    static constexpr const char* kGlitch         = "glitch";
    static constexpr const char* kParticles      = "particles";
    static constexpr const char* kWaveform3D     = "waveform3d";
    static constexpr const char* kSpectrum3D     = "spectrum3d";
    static constexpr const char* kPlasma         = "plasma";
    static constexpr const char* kVoronoi        = "voronoi";
    static constexpr const char* kChromatic      = "chromatic";
}

//==============================================================================
/// Manages compilation and caching of GLSL shader programs.
class ShaderCache
{
public:
    struct ShaderEntry
    {
        std::unique_ptr<juce::OpenGLShaderProgram> program;
        bool compiled = false;
    };

    /// Get or compile a shader program.  Returns nullptr if compilation fails.
    juce::OpenGLShaderProgram* getOrCompile(
        juce::OpenGLContext& ctx,
        const juce::String& shaderId,
        const juce::String& vertexSrc,
        const juce::String& fragmentSrc);

    /// Clear all cached shaders (call when GL context is destroyed).
    void clear();

private:
    std::unordered_map<std::string, ShaderEntry> cache;

public:
    juce::String lastError;  ///< Last shader compilation error, if any
};

//==============================================================================
// Forward declaration -- implementation lives in the .cpp file.
class SharedGLRenderer;

//==============================================================================
/**
 * Component that renders Python custom plugin output.
 *
 * All GL rendering happens in a shared off-screen context managed by
 * SharedGLRenderer.  This component is a normal lightweight JUCE component
 * -- no native HWND, no z-ordering issues with sibling panels or overlays.
 *
 * Lifecycle:
 *   1. Created by MeterFactory::createMeter(MeterType::CustomPlugin)
 *   2. setPluginId() called with manifest id + instance UUID
 *   3. Each frame: feedAudioData() called from MeterFactory::feedMeter()
 *   4. SharedGLRenderer calls renderFrame_GL() on the GL thread
 *   5. Result is read back to a juce::Image
 *   6. paint() draws the readback image (lightweight, no native HWND)
 */
class CustomPluginComponent : public juce::Component,
                               public MeterBase
{
public:
    CustomPluginComponent();
    ~CustomPluginComponent() override;

    //-- Plugin identity -----------------------------------------------------
    void setPluginId(const juce::String& manifestId, const juce::String& instanceId);
    const juce::String& getManifestId() const { return manifestId_; }
    const juce::String& getInstanceId() const { return instanceId_; }

    //-- Plugin properties (populated after createInstance) -------------------
    void setPluginProperties(const std::vector<CustomPluginProperty>& props) { pluginProperties_ = props; }
    const std::vector<CustomPluginProperty>& getPluginProperties() const { return pluginProperties_; }

    /// Update a single property's current value (so export picks it up).
    void updatePropertyValue(const juce::String& key, const juce::var& value)
    {
        for (auto& p : pluginProperties_)
            if (p.key == key) { p.defaultVal = value; return; }
    }

    //-- Audio data feed (called 60fps from MeterFactory) --------------------
    /// Non-blocking: posts work to a background thread.
    /// @param audioJson   Serialized AudioData JSON (lightweight if SHM active).
    /// @param shmActive   Whether shared memory is being used for main data transfer.
    /// @param pSpectrum   Optional raw pointer to linear magnitude spectrum (for GPU texture).
    /// @param spectrumLen Number of bins in pSpectrum.
    /// @param pWaveform   Optional raw pointer to mono waveform (for GPU texture).
    /// @param waveformLen Number of samples in pWaveform.
    void feedAudioData(const juce::String& audioJson, bool shmActive,
                       const float* pSpectrum = nullptr, int spectrumLen = 0,
                       const float* pWaveform = nullptr, int waveformLen = 0);

    //-- Throttle query (called from MeterFactory before building JSON) ------
    bool isRenderThrottled() const { return workerBusy_.load(); }

    //-- Component overrides -------------------------------------------------
    void paint(juce::Graphics& g) override;
    void resized() override;

    //-- Error state ---------------------------------------------------------
    bool hasError() const { return hasError_; }

    //-- Offline rendering support (for video export) -------------------------
    /// Register with SharedGLRenderer so that GL resources are available.
    /// Does NOT start a BridgeWorker — the caller drives rendering manually.
    void initForOfflineRendering();

    /// True when this component is used for offline (video export) rendering.
    bool isOffline() const { return isOffline_; }

    /// Render a frame synchronously on the shared GL thread and return the
    /// composited image (pre-shader 2D + shaders + post-shader 2D overlay).
    /// Safe to call from any thread.
    juce::Image renderOfflineFrame(
        std::vector<PluginRender::RenderCommand> commands,
        const std::vector<float>& spectrum,
        const std::vector<float>& waveform,
        int width, int height);

    /// Unregister from SharedGLRenderer and release GL resources.
    void releaseOfflineRendering();

    //----------------------------------------------------------------------
    // GL-thread methods -- called exclusively by SharedGLRenderer.
    // Do NOT call from application code.
    //----------------------------------------------------------------------
    void initGLResources_GL(juce::OpenGLContext& ctx);
    void renderFrame_GL(juce::OpenGLContext& ctx);
    void releaseGLResources_GL();

    bool glInitialised = false;   ///< Set by initGLResources_GL, read by SharedGLRenderer

private:
    friend class SharedGLRenderer;

    juce::String manifestId_;
    juce::String instanceId_;
    std::vector<CustomPluginProperty> pluginProperties_;
    bool isOffline_ = false;  ///< True when used for offline (video export) rendering

    // Thread-safe audio data exchange (message thread -> GL thread)
    juce::SpinLock audioLock;
    juce::String lastAudioJson_;

    // Async bridge I/O: previous frame's commands used during GL render
    juce::SpinLock commandLock;
    std::vector<PluginRender::RenderCommand> cachedCommands;
    bool commandsPending = false;

    // Retained commands for flicker-free rendering
    std::vector<PluginRender::RenderCommand> lastCommands_;

    // Post-shader 2D overlay commands (text labels etc.)
    std::vector<PluginRender::RenderCommand> postShaderCmds_;

    ShaderCache shaderCache;

    // FBO for offscreen rendering (GL thread only)
    GLuint fbo_ = 0;
    GLuint fboColorTex_ = 0;
    int    fboWidth_ = 0;
    int    fboHeight_ = 0;

    // Rendered frame -- written by GL thread, read by paint() on message thread
    juce::Image renderedFrame_;
    juce::SpinLock frameLock_;

    // Back-buffer for 2D command replay (software rendering)
    juce::Image backBuffer;
    juce::OpenGLTexture backBufferTexture;

    // Shader-related state
    struct ShaderPass
    {
        juce::String shaderId;
        juce::String customFragmentSrc;
        juce::String customComputeSrc;
        int  customBufferSize = 0;
        int  computeGroupsX = 1;
        int  computeGroupsY = 1;
        int  computeGroupsZ = 1;
        std::unordered_map<std::string, float> uniforms;
    };
    std::vector<ShaderPass> pendingShaderPasses;

    // Fullscreen quad VAO for shader passes
    GLuint quadVAO = 0;
    GLuint quadVBO = 0;

    // Audio texture (1D) for shader access
    GLuint audioTexture = 0;
    std::vector<float> spectrumData;
    std::vector<float> waveformData;

    // Compute shader support (OpenGL 4.3+)
    bool computeSupported = false;
    GLuint particleSSBO = 0;
    GLuint computeProgram = 0;
    int    particleCount = 0;
    bool   computeShaderCompiled = false;
    double lastFrameTime = 0.0;

    // Custom compute shader state
    GLuint customComputeSSBO = 0;
    int    customSSBOSize = 0;
    GLuint customComputeProgram = 0;
    juce::String customComputeKey;
    double customLastFrameTime = 0.0;

    // Frame counter for time-based animations
    uint64_t frameCounter = 0;
    double startTime = 0.0;

    bool hasError_ = false;
    int  consecutiveErrors_ = 0;

    //-- Background worker for non-blocking Python IPC -----------------------
    class BridgeWorker : public juce::Thread
    {
    public:
        BridgeWorker(CustomPluginComponent& owner);
        ~BridgeWorker() override;

        void postRenderRequest(const juce::String& instanceId,
                               int w, int h,
                               const juce::String& audioJson);

        /// Post a non-blocking request to re-create the Python instance
        /// on the worker thread (avoids blocking the message thread).
        void postRecreateRequest(const juce::String& manifestId,
                                 const juce::String& instanceId);

        bool fetchResult(std::vector<PluginRender::RenderCommand>& out);

        std::atomic<bool> busy { false };

    private:
        void run() override;

        CustomPluginComponent& owner_;
        juce::CriticalSection  requestLock_;
        juce::String           reqInstanceId_;
        juce::String           reqAudioJson_;
        int                    reqWidth_  = 0;
        int                    reqHeight_ = 0;
        bool                   hasRequest_ = false;

        // Recreate request (posted from message thread, executed on worker)
        juce::String           recreateManifestId_;
        juce::String           recreateInstanceId_;
        bool                   hasRecreateRequest_ = false;

        juce::SpinLock         resultLock_;
        std::vector<PluginRender::RenderCommand> result_;
        bool                   hasResult_ = false;

        juce::WaitableEvent    wakeUp_ { true };
    };

    std::unique_ptr<BridgeWorker> bridgeWorker_;
    std::atomic<bool>             workerBusy_ { false };

    //-- Internal helpers ----------------------------------------------------
    void processRenderCommands(const std::vector<PluginRender::RenderCommand>& commands);
    void renderBackBuffer(const std::vector<PluginRender::RenderCommand>& stdCommands);
    void executeShaderPass(juce::OpenGLContext& ctx, const ShaderPass& pass, int width, int height);
    void drawFullscreenQuad();
    void drawBackBufferQuad(juce::OpenGLContext& ctx, int width, int height);
    void uploadAudioTexture();
    void createQuadGeometry();

    static juce::String getBuiltinVertexShader();
    static juce::String getBuiltinFragmentShader(const juce::String& shaderId);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomPluginComponent)
};
