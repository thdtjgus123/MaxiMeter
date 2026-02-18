#include "CustomPluginComponent.h"
#include "PythonPluginBridge.h"
#include "PluginRenderReplayer.h"
#include "ShaderLibrary.h"
#include "../UI/DebugLogWindow.h"
#include <algorithm>

//==============================================================================
// PluginCleanupQueue — off-thread BridgeWorker destruction and bridge cleanup.
//
// CustomPluginComponent::~CustomPluginComponent() posts its BridgeWorker and
// instance ID here and returns immediately.  This thread then calls
// stopThread() and destroyInstance() without blocking the message thread.
//
// BridgeWorker is a private nested class, so we hold it as juce::Thread*
// (via unique_ptr) — juce::Thread has a virtual destructor, so deleting
// through the base pointer is safe and calls the correct ~BridgeWorker().
//==============================================================================
class PluginCleanupQueue : public juce::Thread
{
public:
    struct Task
    {
        std::unique_ptr<juce::Thread> worker;  // actually BridgeWorker*
        juce::String instanceId;
    };

    static PluginCleanupQueue& getInstance()
    {
        static PluginCleanupQueue inst;
        return inst;
    }

    // Called on the message thread — returns instantly.
    void push(std::unique_ptr<juce::Thread> worker, const juce::String& instanceId)
    {
        {
            const juce::ScopedLock sl(lock_);
            queue_.push_back({ std::move(worker), instanceId });
        }
        notify();
    }

    void run() override
    {
        while (!threadShouldExit())
        {
            wait(200);
            drain();
        }
        drain();  // flush remaining tasks before exit
    }

private:
    PluginCleanupQueue() : juce::Thread("PluginCleanup") { startThread(); }

    void drain()
    {
        std::vector<Task> tasks;
        {
            const juce::ScopedLock sl(lock_);
            tasks = std::move(queue_);
        }
        for (auto& t : tasks)
        {
            // stopThread is now off the message thread — no freeze.
            t.worker.reset();

            // Notify Python bridge to release the instance.
            if (t.instanceId.isNotEmpty())
            {
                try
                {
                    auto& bridge = PythonPluginBridge::getInstance();
                    if (bridge.isRunning())
                        bridge.destroyInstance(t.instanceId);
                }
                catch (...) {}
            }
        }
    }

    juce::CriticalSection lock_;
    std::vector<Task>     queue_;
};

//==============================================================================
// SharedGLRenderer — singleton off-screen GL context
//
// Owns a hidden 1×1 native window with an OpenGL context attached.
// All CustomPluginComponents register here and get their renderFrame_GL()
// called from the shared GL thread.  Result is read back to a juce::Image
// so the components remain lightweight (no per-component native HWND).
//==============================================================================

class SharedGLRenderer : private juce::OpenGLRenderer
{
public:
    static SharedGLRenderer& getInstance()
    {
        static SharedGLRenderer instance;
        return instance;
    }

    void registerComponent(CustomPluginComponent* comp)
    {
        {
            const juce::ScopedLock sl(lock_);
            if (std::find(components_.begin(), components_.end(), comp) == components_.end())
                components_.push_back(comp);
        }
        ensureStarted();
    }

    void unregisterComponent(CustomPluginComponent* comp)
    {
        const juce::ScopedLock sl(lock_);
        components_.erase(
            std::remove(components_.begin(), components_.end(), comp),
            components_.end());
    }

    juce::OpenGLContext& getContext() { return glContext_; }

private:
    SharedGLRenderer() = default;

    ~SharedGLRenderer()
    {
        glContext_.detach();
    }

    void ensureStarted()
    {
        if (started_) return;

        // Create a tiny off-screen native window to host the GL context.
        // Must remain "visible" (to the OS) so JUCE's continuous-repaint
        // timer keeps firing renderOpenGL().  Positioned off-screen so the
        // user never sees it.
        hostComp_.setSize(1, 1);
        hostComp_.setTopLeftPosition(-32000, -32000);
        hostComp_.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        hostComp_.setVisible(true);

        glContext_.setRenderer(this);
        glContext_.setContinuousRepainting(true);
        glContext_.setComponentPaintingEnabled(false);
        glContext_.attachTo(hostComp_);

        started_ = true;
    }

    //-- OpenGLRenderer callbacks (run on the shared GL thread) --------------

    void newOpenGLContextCreated() override
    {
        // Per-component init is lazy (inside renderFrame_GL).
    }

    void renderOpenGL() override
    {
        std::vector<CustomPluginComponent*> comps;
        {
            const juce::ScopedLock sl(lock_);
            comps = components_;
        }

        for (auto* comp : comps)
        {
            if (!comp->glInitialised)
                comp->initGLResources_GL(glContext_);

            comp->renderFrame_GL(glContext_);
        }

        // Yield CPU when the GL thread has nothing useful to do.
        // renderFrame_GL() early-outs if no new commands are pending,
        // so spinning here at thousands of fps just wastes power.
        juce::Thread::sleep(1);
    }

    void openGLContextClosing() override
    {
        std::vector<CustomPluginComponent*> comps;
        {
            const juce::ScopedLock sl(lock_);
            comps = components_;
        }
        for (auto* comp : comps)
            comp->releaseGLResources_GL();
    }

    //-- Data ----------------------------------------------------------------

    struct HostComp : juce::Component
    {
        void paint(juce::Graphics&) override {}
    };

    HostComp             hostComp_;
    juce::OpenGLContext  glContext_;
    juce::CriticalSection lock_;
    std::vector<CustomPluginComponent*> components_;
    bool                 started_ = false;
};

//==============================================================================
// ShaderCache
//==============================================================================

juce::OpenGLShaderProgram* ShaderCache::getOrCompile(
    juce::OpenGLContext& ctx,
    const juce::String& shaderId,
    const juce::String& vertexSrc,
    const juce::String& fragmentSrc)
{
    auto key = shaderId.toStdString();
    auto it = cache.find(key);
    if (it != cache.end() && it->second.compiled)
        return it->second.program.get();

    auto prog = std::make_unique<juce::OpenGLShaderProgram>(ctx);
    if (prog->addVertexShader(vertexSrc) && prog->addFragmentShader(fragmentSrc) && prog->link())
    {
        auto* raw = prog.get();
        cache[key] = { std::move(prog), true };
        return raw;
    }

    DBG("Shader compile failed for: " + shaderId);
    lastError = "Shader compile failed: " + shaderId;
    MAXIMETER_LOG("SHADER", "Compile FAILED for '" + shaderId + "': " + prog->getLastError());
    return nullptr;
}

void ShaderCache::clear()
{
    cache.clear();
}

//==============================================================================
// CustomPluginComponent
//==============================================================================

CustomPluginComponent::CustomPluginComponent()
{
    // Not opaque: post-shader 2D overlay in paint() needs transparent areas
    // so GL shader content shows through from renderOpenGL().
    setOpaque(false);
    startTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    backBufferTexture = std::make_unique<juce::OpenGLTexture>();
}

CustomPluginComponent::~CustomPluginComponent()
{
    // 1. Unregister from GL renderer first so no further renderFrame_GL calls
    //    hit this component while we're mid-destruction.
    auto& renderer = SharedGLRenderer::getInstance();
    renderer.unregisterComponent(this);

    // 2. Release GL resources asynchronously — do NOT block the message thread
    //    waiting for the GL render loop to finish its current frame.
    //    We transfer ownership of all GL objects into a shared "tomb" and post
    //    a non-blocking lambda that destroys the tomb on the GL thread.
    if (glInitialised)
    {
        glInitialised = false;  // stop initGLResources_GL from re-initialising

        // Move heap-owned objects into shared_ptrs so the lambda can outlive us.
        auto cachePtr = std::make_shared<ShaderCache>(std::move(shaderCache));
        auto texPtr   = std::shared_ptr<juce::OpenGLTexture>(std::move(backBufferTexture));

        // Capture plain GLuint handles by value.
        GLuint cap_fbo            = std::exchange(fbo_, 0);
        GLuint cap_fboColorTex    = std::exchange(fboColorTex_, 0);
        GLuint cap_quadVAO        = std::exchange(quadVAO, 0);
        GLuint cap_quadVBO        = std::exchange(quadVBO, 0);
        GLuint cap_audioTexture   = std::exchange(audioTexture, 0);
        GLuint cap_particleSSBO   = std::exchange(particleSSBO, 0);
        GLuint cap_computeProgram = std::exchange(computeProgram, 0);
        GLuint cap_customSSBO     = std::exchange(customComputeSSBO, 0);
        GLuint cap_customComputeProg = std::exchange(customComputeProgram, 0);

        auto& ctx = renderer.getContext();
        if (ctx.isAttached())
        {
            ctx.executeOnGLThread([
                cachePtr, texPtr,
                cap_fbo, cap_fboColorTex, cap_quadVAO, cap_quadVBO,
                cap_audioTexture, cap_particleSSBO, cap_computeProgram,
                cap_customSSBO, cap_customComputeProg
            ](juce::OpenGLContext&) mutable
            {
                using namespace juce::gl;
                cachePtr->clear();  // glDeleteProgram for each cached shader
                texPtr.reset();     // ~OpenGLTexture() → release() on GL thread ✓
                if (cap_fbo)             glDeleteFramebuffers(1, &cap_fbo);
                if (cap_fboColorTex)     glDeleteTextures(1, &cap_fboColorTex);
                if (cap_quadVAO)         glDeleteVertexArrays(1, &cap_quadVAO);
                if (cap_quadVBO)         glDeleteBuffers(1, &cap_quadVBO);
                if (cap_audioTexture)    glDeleteTextures(1, &cap_audioTexture);
                if (cap_particleSSBO)    glDeleteBuffers(1, &cap_particleSSBO);
                if (cap_computeProgram)  glDeleteProgram(cap_computeProgram);
                if (cap_customSSBO)      glDeleteBuffers(1, &cap_customSSBO);
                if (cap_customComputeProg) glDeleteProgram(cap_customComputeProg);
            }, false);  // false = non-blocking, returns immediately
        }
    }

    // 3. Hand the BridgeWorker and instance ID to the async cleanup queue.
    //    The queue thread calls stopThread() and destroyInstance() off the
    //    message thread, so we return immediately without any freeze.
    if (bridgeWorker_)
        PluginCleanupQueue::getInstance().push(
            std::unique_ptr<juce::Thread>(bridgeWorker_.release()), instanceId_);
}

//==============================================================================
void CustomPluginComponent::setPluginId(const juce::String& manifestId,
                                         const juce::String& instanceId)
{
    manifestId_ = manifestId;
    instanceId_ = instanceId;
    MAXIMETER_LOG("INSTANCE", "setPluginId: manifest=" + manifestId + " instance=" + instanceId);

    // Start background worker for non-blocking Python IPC
    bridgeWorker_ = std::make_unique<BridgeWorker>(*this);

    // Register with the shared off-screen GL renderer.
    // No per-component GL context / native HWND — this component stays
    // lightweight and participates in normal JUCE z-ordering.
    SharedGLRenderer::getInstance().registerComponent(this);
}

void CustomPluginComponent::feedAudioData(const juce::String& audioJson, bool shmActive,
                                          const float* pSpectrum, int spectrumLen,
                                          const float* pWaveform, int waveformLen)
{
    {
        const juce::SpinLock::ScopedLockType sl(audioLock);
        lastAudioJson_ = audioJson;

        // Update GPU texture data from raw pointers if provided.
        // This is preferred over JSON parsing for performance.
        if (pSpectrum != nullptr && spectrumLen > 0)
        {
            spectrumData.assign(pSpectrum, pSpectrum + spectrumLen);
        }
        
        if (pWaveform != nullptr && waveformLen > 0)
        {
            waveformData.assign(pWaveform, pWaveform + waveformLen);
        }
    }

    // Parse spectrum/waveform from JSON only if raw pointers weren't provided AND we aren't using SHM.
    // (If SHM is active, JSON likely doesn't contain the arrays anyway).
    if (!shmActive && (pSpectrum == nullptr || pWaveform == nullptr))
    {
        auto parsed = juce::JSON::parse(audioJson);
        if (auto* obj = parsed.getDynamicObject())
        {
            const juce::SpinLock::ScopedLockType sl(audioLock);

            if (pSpectrum == nullptr)
            {
                if (auto* specArr = obj->getProperty("spectrum").getArray())
                {
                    spectrumData.resize(specArr->size());
                    for (int i = 0; i < (int)specArr->size(); ++i)
                        spectrumData[i] = (float)(*specArr)[i];
                }
            }
            if (pWaveform == nullptr)
            {
                if (auto* waveArr = obj->getProperty("waveform").getArray())
                {
                    waveformData.resize(waveArr->size());
                    for (int i = 0; i < (int)waveArr->size(); ++i)
                        waveformData[i] = (float)(*waveArr)[i];
                }
            }
        }
    }

    // Post render request to background worker (non-blocking).
    // The worker thread calls bridge.renderInstance() off the message thread
    // so mouse clicks / UI events are never blocked by Python pipe I/O.
    if (instanceId_.isNotEmpty() && bridgeWorker_)
    {
        // Pick up any completed result from the previous frame
        std::vector<PluginRender::RenderCommand> result;
        if (bridgeWorker_->fetchResult(result))
        {
            if (!result.empty())
            {
                const juce::SpinLock::ScopedLockType sl(commandLock);
                cachedCommands = std::move(result);
                commandsPending = true;
                consecutiveErrors_ = 0;
            }
            else
            {
                consecutiveErrors_++;

                // AUTO-RECOVERY: if we keep getting empty responses, the Python
                // instance may have been lost (bridge restart, timeout during
                // initial create, etc.).  Post a recreate request to the
                // BridgeWorker so it happens off the message thread (avoids
                // blocking the UI on pipeLock).
                if (consecutiveErrors_ == 10 && manifestId_.isNotEmpty())
                {
                    DBG("CustomPluginComponent: " + instanceId_
                        + " — 10 consecutive empty frames, requesting re-create");
                    MAXIMETER_LOG("INSTANCE", instanceId_ + " — 10 consecutive empty frames, requesting re-create for " + manifestId_);
                    bridgeWorker_->postRecreateRequest(manifestId_, instanceId_);
                }
                // Retry periodically (every 60 frames ~ 1 second)
                else if (consecutiveErrors_ > 10 && (consecutiveErrors_ % 60) == 0)
                {
                    bridgeWorker_->postRecreateRequest(manifestId_, instanceId_);
                }
            }
        }

        // Post new request (worker will skip if still busy)
        bridgeWorker_->postRenderRequest(
            instanceId_, getWidth(), getHeight(), audioJson);
        workerBusy_.store(bridgeWorker_->busy.load());
    }
}

//==============================================================================
// BridgeWorker — background thread for Python IPC
//==============================================================================

CustomPluginComponent::BridgeWorker::BridgeWorker(CustomPluginComponent& o)
    : juce::Thread("PluginBridgeWorker"), owner_(o)
{
    startThread(juce::Thread::Priority::normal);
}

CustomPluginComponent::BridgeWorker::~BridgeWorker()
{
    signalThreadShouldExit();
    wakeUp_.signal();
    stopThread(2000);
}

void CustomPluginComponent::BridgeWorker::postRenderRequest(
    const juce::String& instanceId, int w, int h, const juce::String& audioJson)
{
    // If the worker is still processing the last request, skip this frame
    if (busy.load())
        return;

    {
        const juce::ScopedLock sl(requestLock_);
        reqInstanceId_ = instanceId;
        reqWidth_      = w;
        reqHeight_     = h;
        reqAudioJson_  = audioJson;
        hasRequest_    = true;
    }
    wakeUp_.signal();
}

void CustomPluginComponent::BridgeWorker::postRecreateRequest(
    const juce::String& manifestId, const juce::String& instanceId)
{
    {
        const juce::ScopedLock sl(requestLock_);
        recreateManifestId_ = manifestId;
        recreateInstanceId_ = instanceId;
        hasRecreateRequest_ = true;
    }
    wakeUp_.signal();
}

bool CustomPluginComponent::BridgeWorker::fetchResult(
    std::vector<PluginRender::RenderCommand>& out)
{
    const juce::SpinLock::ScopedLockType sl(resultLock_);
    if (!hasResult_) return false;
    out = std::move(result_);
    hasResult_ = false;
    return true;
}

void CustomPluginComponent::BridgeWorker::run()
{
    while (!threadShouldExit())
    {
        wakeUp_.wait(-1);  // sleep until signalled
        if (threadShouldExit()) break;

        // Handle recreate requests first (off the message thread)
        {
            juce::String manifestId, instanceId;
            bool doRecreate = false;
            {
                const juce::ScopedLock sl(requestLock_);
                if (hasRecreateRequest_)
                {
                    manifestId = recreateManifestId_;
                    instanceId = recreateInstanceId_;
                    hasRecreateRequest_ = false;
                    doRecreate = true;
                }
            }
            if (doRecreate)
            {
                try
                {
                    auto& bridge = PythonPluginBridge::getInstance();
                    if (bridge.isRunning())
                        bridge.createInstance(manifestId, instanceId);
                }
                catch (const std::exception& e)
                {
                    juce::Logger::writeToLog("CustomPlugin Create Error: " + juce::String(e.what()));
                }
                catch (...)
                {
                    juce::Logger::writeToLog("CustomPlugin Create Error: Unknown Exception");
                }
            }
        }

        // Grab the pending render request
        juce::String instanceId, audioJson;
        int w = 0, h = 0;
        {
            const juce::ScopedLock sl(requestLock_);
            if (!hasRequest_) continue;
            instanceId = reqInstanceId_;
            audioJson  = reqAudioJson_;
            w          = reqWidth_;
            h          = reqHeight_;
            hasRequest_ = false;
        }

        busy.store(true);

        // Call bridge on this background thread — this is the blocking call
        // that previously froze the message thread.
        std::vector<PluginRender::RenderCommand> commands;
        try
        {
            auto& bridge = PythonPluginBridge::getInstance();
            if (bridge.isRunning())
                commands = bridge.renderInstance(instanceId, w, h, audioJson);
        }
        catch (...)
        {
            DBG("BridgeWorker: renderInstance threw an exception");
        }

        // Store result for pickup by feedAudioData on the message thread
        {
            const juce::SpinLock::ScopedLockType sl(resultLock_);
            result_    = std::move(commands);
            hasResult_ = true;
        }

        busy.store(false);
    }
}

//==============================================================================
// GL-thread rendering (called by SharedGLRenderer)
//==============================================================================

void CustomPluginComponent::initGLResources_GL(juce::OpenGLContext& /*ctx*/)
{
    createQuadGeometry();

    // Create FBO for offscreen rendering
    using namespace juce::gl;
    glGenFramebuffers(1, &fbo_);
    glGenTextures(1, &fboColorTex_);

    // Create audio texture (1D, 2 rows: spectrum + waveform)
    using namespace juce::gl;
    glGenTextures(1, &audioTexture);
    glBindTexture(GL_TEXTURE_2D, audioTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    {
        const juce::SpinLock::ScopedLockType sl(audioLock);
        spectrumData.resize(2048, 0.0f);
        waveformData.resize(1024, 0.0f);
    }

    // Detect OpenGL 4.3+ for compute shader support
    computeSupported = false;
    const char* verStr = (const char*)glGetString(GL_VERSION);
    if (verStr != nullptr)
    {
        int major = 0, minor = 0;
        if (sscanf(verStr, "%d.%d", &major, &minor) == 2)
            computeSupported = (major > 4 || (major == 4 && minor >= 3));
    }
    DBG("OpenGL version: " + juce::String(verStr ? verStr : "unknown")
        + " — compute shaders " + (computeSupported ? "supported" : "NOT supported (need 4.3+)"));
    MAXIMETER_LOG("RENDER", "OpenGL init: " + juce::String(verStr ? verStr : "?")
        + " | compute=" + juce::String(computeSupported ? "YES" : "NO"));

    lastFrameTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;

    glInitialised = true;
    MAXIMETER_LOG("RENDER", "GL resources initialised for instance " + instanceId_);
}

void CustomPluginComponent::renderFrame_GL(juce::OpenGLContext& ctx)
{
    using namespace juce::gl;

    if (!glInitialised) return;

    // Use logical pixel dimensions (component bounds).
    // The FBO renders at logical resolution; paint() lets JUCE handle
    // any HiDPI scaling during the lightweight compositing pass.
    const int w = getWidth();
    const int h = getHeight();
    if (w <= 0 || h <= 0) return;

    // ── Throttle: skip full re-render when nothing changed ──
    // The GL thread runs continuously; with multiple components this wastes
    // CPU/GPU on duplicate glReadPixels and software back-buffer renders.
    // Only do the expensive work when new commands have arrived.
    bool hasNewCommands = false;
    {
        const juce::SpinLock::ScopedLockType sl(commandLock);
        if (commandsPending)
        {
            lastCommands_ = std::move(cachedCommands);
            commandsPending = false;
            hasNewCommands = true;
        }
    }

    if (!hasNewCommands)
        return;  // nothing new — reuse the image already in renderedFrame_

    frameCounter++;
    shaderCache.lastError = {};

    if (frameCounter <= 5 && !lastCommands_.empty())
    {
        int shaderCmds = 0, stdCmds = 0;
        for (auto& c : lastCommands_)
        {
            if (c.type == PluginRender::CmdType::DrawShader ||
                c.type == PluginRender::CmdType::DrawCustomShader)
                shaderCmds++;
            else
                stdCmds++;
        }
        MAXIMETER_LOG("RENDER", instanceId_ + " first render: "
            + juce::String((int)lastCommands_.size()) + " cmds ("
            + juce::String(stdCmds) + " 2D, " + juce::String(shaderCmds) + " shader) "
            + juce::String(w) + "x" + juce::String(h) + "px");
    }

    // ── Split commands into pre-shader 2D, shaders, and post-shader 2D ──
    pendingShaderPasses.clear();
    processRenderCommands(lastCommands_);

    int firstShaderIdx = -1;
    for (int i = 0; i < (int)lastCommands_.size(); ++i)
    {
        if (lastCommands_[i].type == PluginRender::CmdType::DrawShader ||
            lastCommands_[i].type == PluginRender::CmdType::DrawCustomShader)
        {
            firstShaderIdx = i;
            break;
        }
    }

    std::vector<PluginRender::RenderCommand> preShaderCmds;
    postShaderCmds_.clear();

    if (firstShaderIdx >= 0)
    {
        preShaderCmds.assign(lastCommands_.begin(),
                             lastCommands_.begin() + firstShaderIdx);

        for (int i = firstShaderIdx; i < (int)lastCommands_.size(); ++i)
        {
            auto& cmd = lastCommands_[i];
            if (cmd.type != PluginRender::CmdType::DrawShader &&
                cmd.type != PluginRender::CmdType::DrawCustomShader)
                postShaderCmds_.push_back(cmd);
        }
    }
    else
    {
        preShaderCmds = lastCommands_;
    }

    // Render pre-shader 2D commands to software back-buffer
    if (!preShaderCmds.empty())
        renderBackBuffer(preShaderCmds);

    // Upload back-buffer as GL texture
    if (backBuffer.isValid() && backBufferTexture)
        backBufferTexture->loadImage(backBuffer);

    // ── Resize FBO if needed ──
    if (fboWidth_ != w || fboHeight_ != h)
    {
        glBindTexture(GL_TEXTURE_2D, fboColorTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, fboColorTex_, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        fboWidth_ = w;
        fboHeight_ = h;
    }

    // ── Render to FBO ──
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, w, h);

    // Use meter BG colour as GL clear colour (transparent = default black)
    {
        auto bg = meterBg_;
        if (bg.getAlpha() > 0)
            glClearColor(bg.getFloatRed(), bg.getFloatGreen(),
                         bg.getFloatBlue(), bg.getFloatAlpha());
        else
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    }
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    uploadAudioTexture();

    if (pendingShaderPasses.empty())
    {
        drawBackBufferQuad(ctx, w, h);
    }
    else
    {
        for (auto& pass : pendingShaderPasses)
            executeShaderPass(ctx, pass, w, h);
    }

    hasError_ = shaderCache.lastError.isNotEmpty();
    glDisable(GL_BLEND);

    // ── Readback FBO → juce::Image ──
    std::vector<uint8_t> pixels((size_t)w * h * 4);
    glReadPixels(0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, pixels.data());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Build image with vertical flip (GL is bottom-up, JUCE is top-down)
    juce::Image frame(juce::Image::ARGB, w, h, false);
    {
        juce::Image::BitmapData bmp(frame, juce::Image::BitmapData::writeOnly);
        const size_t rowBytes = (size_t)w * 4;
        for (int y = 0; y < h; ++y)
            memcpy(bmp.getLinePointer(y),
                   pixels.data() + (size_t)(h - 1 - y) * rowBytes, rowBytes);
    }

    // Draw post-shader 2D overlay onto the readback image
    if (!postShaderCmds_.empty())
    {
        juce::Graphics g(frame);
        PluginRenderReplayer::replay(g, postShaderCmds_);
    }

    // Store for paint() on the message thread
    {
        const juce::SpinLock::ScopedLockType sl(frameLock_);
        renderedFrame_ = std::move(frame);
    }

    repaint();  // thread-safe in JUCE
}

void CustomPluginComponent::releaseGLResources_GL()
{
    using namespace juce::gl;

    shaderCache.clear();

    if (fbo_)          { glDeleteFramebuffers(1, &fbo_);          fbo_ = 0; }
    if (fboColorTex_)  { glDeleteTextures(1, &fboColorTex_);     fboColorTex_ = 0; }
    fboWidth_ = fboHeight_ = 0;

    if (quadVAO) { glDeleteVertexArrays(1, &quadVAO); quadVAO = 0; }
    if (quadVBO) { glDeleteBuffers(1, &quadVBO); quadVBO = 0; }
    if (audioTexture) { glDeleteTextures(1, &audioTexture); audioTexture = 0; }
    if (particleSSBO) { glDeleteBuffers(1, &particleSSBO); particleSSBO = 0; }
    if (computeProgram) { glDeleteProgram(computeProgram); computeProgram = 0; }
    computeShaderCompiled = false;
    particleCount = 0;

    // Custom compute cleanup
    if (customComputeSSBO) { glDeleteBuffers(1, &customComputeSSBO); customComputeSSBO = 0; }
    if (customComputeProgram) { glDeleteProgram(customComputeProgram); customComputeProgram = 0; }
    customSSBOSize = 0;
    customComputeKey = {};

    if (backBufferTexture) backBufferTexture->release();
    glInitialised = false;
}

//==============================================================================
// Offline rendering support (for video export)
//==============================================================================

void CustomPluginComponent::initForOfflineRendering()
{
    isOffline_ = true;

    // Register with SharedGLRenderer to ensure the GL context is active
    // and initGLResources_GL will be called on the GL thread.
    // renderFrame_GL will be called by the continuous loop but will
    // early-return because commandsPending is false.
    SharedGLRenderer::getInstance().registerComponent(this);
}

juce::Image CustomPluginComponent::renderOfflineFrame(
    std::vector<PluginRender::RenderCommand> commands,
    const std::vector<float>& spectrum,
    const std::vector<float>& waveform,
    int w, int h)
{
    if (w <= 0 || h <= 0)
        return {};

    // Set component bounds (renderFrame_GL reads getWidth/getHeight)
    setSize(w, h);

    // Inject render commands
    {
        const juce::SpinLock::ScopedLockType sl(commandLock);
        cachedCommands = std::move(commands);
        commandsPending = true;
    }

    // Inject audio data (used by GL audio texture upload)
    {
        const juce::SpinLock::ScopedLockType sl(audioLock);
        if (!spectrum.empty()) spectrumData = spectrum;
        if (!waveform.empty()) waveformData = waveform;
    }

    // Execute the full GL pipeline synchronously on the shared GL thread.
    // This handles pre-shader 2D, all shader passes, FBO readback, and
    // post-shader 2D overlay — the result is stored in renderedFrame_.
    auto& ctx = SharedGLRenderer::getInstance().getContext();
    if (ctx.isAttached())
    {
        ctx.executeOnGLThread([this](juce::OpenGLContext& glCtx) {
            if (!glInitialised)
                initGLResources_GL(glCtx);
            renderFrame_GL(glCtx);
        }, true);  // block until done
    }

    // Grab the composited image
    juce::Image result;
    {
        const juce::SpinLock::ScopedLockType sl(frameLock_);
        result = renderedFrame_;
    }
    return result;
}

void CustomPluginComponent::releaseOfflineRendering()
{
    SharedGLRenderer::getInstance().unregisterComponent(this);

    if (glInitialised)
    {
        auto& ctx = SharedGLRenderer::getInstance().getContext();
        if (ctx.isAttached())
        {
            ctx.executeOnGLThread([this](juce::OpenGLContext&) {
                releaseGLResources_GL();
            }, true);
        }
    }
}

//==============================================================================
void CustomPluginComponent::paint(juce::Graphics& g)
{
    // All rendering (shaders + 2D) has been composited into renderedFrame_
    // by the shared GL renderer.  Just blit it here as a normal JUCE image
    // — no native HWND, no z-ordering issues.
    juce::Image frameCopy;
    {
        const juce::SpinLock::ScopedLockType sl(frameLock_);
        frameCopy = renderedFrame_;
    }

    if (frameCopy.isValid())
    {
        g.drawImage(frameCopy, getLocalBounds().toFloat());
    }
    else if (instanceId_.isNotEmpty())
    {
        // Placeholder while waiting for first frame
        g.fillAll(hasCustomBg() ? meterBg_ : juce::Colour(0xFF1E1E2E));
        g.setColour(juce::Colour(0xFF888888));
        g.setFont(14.0f);
        g.drawText("Loading...",
                   getLocalBounds(), juce::Justification::centred);
    }
}

void CustomPluginComponent::resized()
{
    // Recreate back-buffer at logical resolution
    int w = getWidth(), h = getHeight();
    if (w > 0 && h > 0)
        backBuffer = juce::Image(juce::Image::ARGB, w, h, true);

    // Notify Python of resize (skip for offline instances — they are driven
    // directly by renderOfflineFrame() and don't need resize notifications;
    // skipping avoids unnecessary bridge pipe contention during export)
    if (instanceId_.isNotEmpty() && !isOffline_)
    {
        auto& bridge = PythonPluginBridge::getInstance();
        if (bridge.isRunning())
            bridge.notifyResize(instanceId_, w, h);
    }
}

//==============================================================================
// Internal helpers
//==============================================================================

void CustomPluginComponent::processRenderCommands(
    const std::vector<PluginRender::RenderCommand>& commands)
{
    for (auto& cmd : commands)
    {
        if (cmd.type == PluginRender::CmdType::DrawShader)
        {
            ShaderPass pass;
            pass.shaderId = cmd.params["shader_id"].toString();

            // Extract uniforms
            if (auto* unifObj = cmd.params["uniforms"].getDynamicObject())
            {
                for (auto& prop : unifObj->getProperties())
                    pass.uniforms[prop.name.toString().toStdString()] = (float)prop.value;
            }

            pendingShaderPasses.push_back(std::move(pass));
        }
        else if (cmd.type == PluginRender::CmdType::DrawCustomShader)
        {
            ShaderPass pass;
            // Prefix custom cache keys to avoid collision with built-in shader IDs
            pass.shaderId = "_usr_" + cmd.params["cache_key"].toString();
            pass.customFragmentSrc = cmd.params["fragment_source"].toString();
            pass.customComputeSrc = cmd.params["compute_source"].toString();
            pass.customBufferSize = (int)cmd.params["buffer_size"];
            pass.computeGroupsX = juce::jmax(1, (int)cmd.params["num_groups_x"]);
            pass.computeGroupsY = juce::jmax(1, (int)cmd.params["num_groups_y"]);
            pass.computeGroupsZ = juce::jmax(1, (int)cmd.params["num_groups_z"]);

            // Extract uniforms
            if (auto* unifObj = cmd.params["uniforms"].getDynamicObject())
            {
                for (auto& prop : unifObj->getProperties())
                    pass.uniforms[prop.name.toString().toStdString()] = (float)prop.value;
            }

            pendingShaderPasses.push_back(std::move(pass));
        }
    }
}

void CustomPluginComponent::renderBackBuffer(
    const std::vector<PluginRender::RenderCommand>& stdCommands)
{
    int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0) return;

    if (!backBuffer.isValid() || backBuffer.getWidth() != w || backBuffer.getHeight() != h)
        backBuffer = juce::Image(juce::Image::ARGB, w, h, true);

    backBuffer.clear(backBuffer.getBounds());
    juce::Graphics g(backBuffer);
    PluginRenderReplayer::replay(g, stdCommands);
}

void CustomPluginComponent::executeShaderPass(juce::OpenGLContext& ctx, const ShaderPass& pass, int width, int height)
{
    using namespace juce::gl;

    auto vertSrc = getBuiltinVertexShader();
    juce::String fragSrc;
    bool isCustomShader = pass.customFragmentSrc.isNotEmpty();

    if (isCustomShader)
    {
        fragSrc = pass.customFragmentSrc;
    }
    else
    {
        fragSrc = getBuiltinFragmentShader(pass.shaderId); // #version 330 fallback
    }

    if (fragSrc.isEmpty())
    {
        DBG("executeShaderPass: no fragment source for shader '" + pass.shaderId + "'");
        MAXIMETER_LOG("SHADER", "No fragment source for shader '" + pass.shaderId + "'");
        return;
    }

    // Check if this shader has a compute stage
    juce::String computeSrc;
    bool isCustomCompute = false;

    if (isCustomShader)
    {
        computeSrc = pass.customComputeSrc;   // empty if user didn't provide one
        isCustomCompute = computeSrc.isNotEmpty();
    }
    else
    {
        computeSrc = ShaderLibrary::getComputeShader(pass.shaderId);
    }

    bool useCompute = computeSrc.isNotEmpty() && computeSupported;

    // For compute shaders, use the 430 vertex shader
    juce::String cacheKey = pass.shaderId;
    if (useCompute)
    {
        if (!isCustomCompute)
        {
            // Built-in: swap to SSBO-reading fragment + 430 vertex
            auto computeFragSrc = ShaderLibrary::getComputeFragmentShader(pass.shaderId);
            if (computeFragSrc.isNotEmpty())
            {
                fragSrc = computeFragSrc;
                cacheKey = pass.shaderId + "_430";
            }
        }
        else
        {
            // Custom compute: fragment already provided by user (must be #version 430+)
            cacheKey = pass.shaderId + "_ccmp";
        }

        vertSrc = R"(
            #version 430 core
            layout (location = 0) in vec2 aPos;
            layout (location = 1) in vec2 aTexCoord;
            out vec2 vTexCoord;
            void main() {
                gl_Position = vec4(aPos, 0.0, 1.0);
                vTexCoord = aTexCoord;
            }
        )";
    }

    auto* shader = shaderCache.getOrCompile(ctx, cacheKey, vertSrc, fragSrc);
    if (!shader)
    {
        DBG("executeShaderPass: shader compile/link failed for '" + cacheKey
            + "': " + shaderCache.lastError);
        MAXIMETER_LOG("SHADER", "Compile/link FAILED for '" + cacheKey + "': " + shaderCache.lastError);
        return;
    }

    // Track which SSBO to bind for the fragment pass
    GLuint activeSSBO = 0;

    //----------------------------------------------------------------------
    // Compute shader dispatch
    //----------------------------------------------------------------------
    if (useCompute)
    {
        if (isCustomCompute)
        {
            //------------------------------------------------------------------
            // Custom compute shader path (generic SSBO, user-specified dispatch)
            //------------------------------------------------------------------
            int bufSize = pass.customBufferSize;
            if (bufSize <= 0) bufSize = 4096;  // sensible default: 4KB
            bufSize = juce::jmax(64, bufSize); // minimum 64 bytes

            // (Re-)create custom SSBO if size changed or not yet created
            if (customComputeSSBO == 0 || bufSize != customSSBOSize)
            {
                if (customComputeSSBO) glDeleteBuffers(1, &customComputeSSBO);
                glGenBuffers(1, &customComputeSSBO);
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, customComputeSSBO);
                std::vector<char> initData(bufSize, 0);
                glBufferData(GL_SHADER_STORAGE_BUFFER, bufSize,
                             initData.data(), GL_DYNAMIC_COPY);
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
                customSSBOSize = bufSize;
            }

            // Compile custom compute shader if source key changed
            if (customComputeKey != pass.shaderId || customComputeProgram == 0)
            {
                if (customComputeProgram) glDeleteProgram(customComputeProgram);
                customComputeProgram = 0;

                GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
                auto srcStd = computeSrc.toStdString();
                const char* srcPtr = srcStd.c_str();
                glShaderSource(cs, 1, &srcPtr, nullptr);
                glCompileShader(cs);

                GLint compiled = 0;
                glGetShaderiv(cs, GL_COMPILE_STATUS, &compiled);
                if (!compiled)
                {
                    char infoLog[512];
                    glGetShaderInfoLog(cs, 512, nullptr, infoLog);
                    DBG("Custom compute shader compile error: " + juce::String(infoLog));
                    shaderCache.lastError = "Custom compute compile failed: " + juce::String(infoLog);
                    glDeleteShader(cs);
                }
                else
                {
                    customComputeProgram = glCreateProgram();
                    glAttachShader(customComputeProgram, cs);
                    glLinkProgram(customComputeProgram);

                    GLint linked = 0;
                    glGetProgramiv(customComputeProgram, GL_LINK_STATUS, &linked);
                    if (!linked)
                    {
                        char infoLog[512];
                        glGetProgramInfoLog(customComputeProgram, 512, nullptr, infoLog);
                        DBG("Custom compute shader link error: " + juce::String(infoLog));
                        shaderCache.lastError = "Custom compute link failed: " + juce::String(infoLog);
                        glDeleteProgram(customComputeProgram);
                        customComputeProgram = 0;
                    }
                    glDeleteShader(cs);
                }
                customComputeKey = pass.shaderId;
            }

            // Dispatch custom compute
            if (customComputeProgram != 0)
            {
                double now2 = juce::Time::getMillisecondCounterHiRes() / 1000.0;
                float deltaTime = (float)(now2 - customLastFrameTime);
                deltaTime = juce::jmin(deltaTime, 0.05f);
                customLastFrameTime = now2;

                glUseProgram(customComputeProgram);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, customComputeSSBO);

                // Audio texture on unit 1
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, audioTexture);
                glUniform1i(glGetUniformLocation(customComputeProgram, "u_audioData"), 1);

                // Standard uniforms
                float timeNow = (float)(juce::Time::getMillisecondCounterHiRes() / 1000.0 - startTime);
                glUniform1f(glGetUniformLocation(customComputeProgram, "u_time"), timeNow);
                glUniform1f(glGetUniformLocation(customComputeProgram, "u_deltaTime"), deltaTime);
                glUniform2f(glGetUniformLocation(customComputeProgram, "u_resolution"),
                            (float)width, (float)height);

                // ALL user uniforms forwarded to compute program
                for (auto& [name, value] : pass.uniforms)
                    glUniform1f(glGetUniformLocation(customComputeProgram, name.c_str()), value);

                glDispatchCompute(pass.computeGroupsX, pass.computeGroupsY, pass.computeGroupsZ);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
                glUseProgram(0);

                activeSSBO = customComputeSSBO;
            }
        }
        else
        {
            //------------------------------------------------------------------
            // Built-in compute shader path (particle-specific)
            //------------------------------------------------------------------

            // Determine requested particle count
            int reqCount = 1000;
            auto it = pass.uniforms.find("u_count");
            if (it != pass.uniforms.end())
                reqCount = juce::jmax(10, (int)it->second);
            reqCount = juce::jmin(reqCount, 100000);  // cap at 100k

            // (Re-)create SSBO if particle count changed or not yet created
            if (particleSSBO == 0 || reqCount != particleCount)
            {
                if (particleSSBO) glDeleteBuffers(1, &particleSSBO);
                glGenBuffers(1, &particleSSBO);
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSSBO);

                // 2 vec4 per particle (posVel + color), zero-init
                std::vector<float> initData(reqCount * 8, 0.0f);
                glBufferData(GL_SHADER_STORAGE_BUFFER,
                             initData.size() * sizeof(float),
                             initData.data(), GL_DYNAMIC_COPY);
                glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
                particleCount = reqCount;
            }

            // Compile compute shader if needed
            if (!computeShaderCompiled || computeProgram == 0)
            {
                if (computeProgram) glDeleteProgram(computeProgram);

                GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
                auto srcStd = computeSrc.toStdString();
                const char* srcPtr = srcStd.c_str();
                glShaderSource(cs, 1, &srcPtr, nullptr);
                glCompileShader(cs);

                GLint compiled = 0;
                glGetShaderiv(cs, GL_COMPILE_STATUS, &compiled);
                if (!compiled)
                {
                    char infoLog[512];
                    glGetShaderInfoLog(cs, 512, nullptr, infoLog);
                    DBG("Compute shader compile error: " + juce::String(infoLog));
                    shaderCache.lastError = "Compute shader compile failed: " + juce::String(infoLog);
                    glDeleteShader(cs);
                    computeProgram = 0;
                    computeShaderCompiled = false;
                }
                else
                {
                    computeProgram = glCreateProgram();
                    glAttachShader(computeProgram, cs);
                    glLinkProgram(computeProgram);

                    GLint linked = 0;
                    glGetProgramiv(computeProgram, GL_LINK_STATUS, &linked);
                    if (!linked)
                    {
                        char infoLog[512];
                        glGetProgramInfoLog(computeProgram, 512, nullptr, infoLog);
                        DBG("Compute shader link error: " + juce::String(infoLog));
                        shaderCache.lastError = "Compute shader link failed: " + juce::String(infoLog);
                        glDeleteProgram(computeProgram);
                        computeProgram = 0;
                    }
                    glDeleteShader(cs);
                    computeShaderCompiled = (computeProgram != 0);
                }
            }

            // Dispatch compute shader
            if (computeProgram != 0)
            {
                double now2 = juce::Time::getMillisecondCounterHiRes() / 1000.0;
                float deltaTime = (float)(now2 - lastFrameTime);
                deltaTime = juce::jmin(deltaTime, 0.05f);  // cap at 50ms
                lastFrameTime = now2;

                glUseProgram(computeProgram);

                // Bind SSBO to binding 0
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleSSBO);

                // Bind audio texture to unit 1
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, audioTexture);
                glUniform1i(glGetUniformLocation(computeProgram, "u_audioData"), 1);

                // Set uniforms
                float timeNow = (float)(juce::Time::getMillisecondCounterHiRes() / 1000.0 - startTime);
                glUniform1f(glGetUniformLocation(computeProgram, "u_time"), timeNow);
                glUniform1f(glGetUniformLocation(computeProgram, "u_deltaTime"), deltaTime);
                glUniform1i(glGetUniformLocation(computeProgram, "u_count"), particleCount);

                // Custom uniforms from Python
                auto setComputeUniform = [&](const char* name, float defaultVal) {
                    float val = defaultVal;
                    auto uit = pass.uniforms.find(name);
                    if (uit != pass.uniforms.end()) val = uit->second;
                    glUniform1f(glGetUniformLocation(computeProgram, name), val);
                };
                setComputeUniform("u_speed", 1.0f);
                setComputeUniform("u_colorR", 0.3f);
                setComputeUniform("u_colorG", 0.6f);
                setComputeUniform("u_colorB", 1.0f);

                // Dispatch: 256 threads per group
                int numGroups = (particleCount + 255) / 256;
                glDispatchCompute(numGroups, 1, 1);

                // Memory barrier to ensure SSBO writes complete before fragment reads
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

                glUseProgram(0);

                activeSSBO = particleSSBO;
            }
        }
    }

    //----------------------------------------------------------------------
    // Fragment shader pass
    //----------------------------------------------------------------------
    shader->use();

    // Standard uniforms
    double now = juce::Time::getMillisecondCounterHiRes() / 1000.0 - startTime;
    shader->setUniform("u_time", (GLfloat)now);
    shader->setUniform("u_resolution", (GLfloat)width, (GLfloat)height);
    shader->setUniform("u_frame", (GLint)frameCounter);

    // Audio texture on unit 1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, audioTexture);
    shader->setUniform("u_audioData", 1);
    shader->setUniform("u_spectrumSize", (GLint)(int)spectrumData.size());
    shader->setUniform("u_waveformSize", (GLint)(int)waveformData.size());

    // Back-buffer on unit 0 (for post-processing shaders like bloom/blur)
    if (backBuffer.isValid())
    {
        glActiveTexture(GL_TEXTURE0);
        backBufferTexture->bind();
        shader->setUniform("u_texture", 0);
    }

    // Custom uniforms from Python
    for (auto& [name, value] : pass.uniforms)
        shader->setUniform(name.c_str(), (GLfloat)value);

    // Override u_count for built-in compute path to match actual SSBO size
    // (prevents out-of-bounds SSBO reads if Python passed count > cap)
    if (useCompute && !isCustomCompute && particleCount > 0)
        shader->setUniform("u_count", (GLfloat)particleCount);

    // Bind SSBO for compute-enabled shaders (fragment reads compute data)
    if (useCompute && activeSSBO != 0)
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, activeSSBO);

    drawFullscreenQuad();

    if (backBuffer.isValid())
        backBufferTexture->unbind();

    // Unbind SSBO
    if (useCompute && activeSSBO != 0)
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
}

void CustomPluginComponent::drawFullscreenQuad()
{
    using namespace juce::gl;

    if (quadVAO == 0) return;

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void CustomPluginComponent::drawBackBufferQuad(juce::OpenGLContext& ctx, int width, int height)
{
    using namespace juce::gl;

    if (!backBuffer.isValid()) return;

    auto* shader = shaderCache.getOrCompile(
        ctx, "passthrough",
        getBuiltinVertexShader(),
        ShaderLibrary::getFragmentShader("passthrough"));
    if (!shader) return;

    shader->use();

    glActiveTexture(GL_TEXTURE0);
    backBufferTexture->bind();
    shader->setUniform("u_texture", 0);

    drawFullscreenQuad();

    backBufferTexture->unbind();
}

void CustomPluginComponent::uploadAudioTexture()
{
    using namespace juce::gl;

    if (!audioTexture) return;

    // Thread-safe copy of audio data
    std::vector<float> specCopy, waveCopy;
    {
        const juce::SpinLock::ScopedLockType sl(audioLock);
        specCopy = spectrumData;
        waveCopy = waveformData;
    }

    // Pack spectrum + waveform into a 2D texture
    // Row 0: spectrum, Row 1: waveform (both padded to texWidth)
    int texWidth = juce::jmax((int)specCopy.size(), (int)waveCopy.size());
    if (texWidth < 1) texWidth = 2048;

    std::vector<float> texData(texWidth * 2, 0.0f);
    if (!specCopy.empty())
        memcpy(texData.data(), specCopy.data(),
               juce::jmin((int)specCopy.size(), texWidth) * sizeof(float));
    if (!waveCopy.empty())
        memcpy(texData.data() + texWidth, waveCopy.data(),
               juce::jmin((int)waveCopy.size(), texWidth) * sizeof(float));

    glBindTexture(GL_TEXTURE_2D, audioTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, texWidth, 2, 0,
                 GL_RED, GL_FLOAT, texData.data());
}

void CustomPluginComponent::createQuadGeometry()
{
    using namespace juce::gl;

    // Fullscreen quad: position (x,y) + texcoord (u,v)
    float quadVertices[] = {
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // Position attribute (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    // Texcoord attribute (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
}

//==============================================================================
juce::String CustomPluginComponent::getBuiltinVertexShader()
{
    return R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTexCoord;
        out vec2 vTexCoord;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
            vTexCoord = aTexCoord;
        }
    )";
}

juce::String CustomPluginComponent::getBuiltinFragmentShader(const juce::String& shaderId)
{
    return ShaderLibrary::getFragmentShader(shaderId);
}
