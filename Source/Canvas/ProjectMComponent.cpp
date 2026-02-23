#include "ProjectMComponent.h"

#ifdef PROJECTM_AVAILABLE
#include <projectM-4/projectM.h>
#include <projectM-4/playlist.h>
#endif

#include <juce_opengl/juce_opengl.h>

// We can't #include <GL/glew.h> because JUCE already pulled in gl.h.
// Instead, forward-declare the GLEW init functions we need — they are
// resolved at link time via the static libglew32.lib we already link.
#ifdef _WIN32
extern "C" {
    typedef unsigned int GLenum_glew;
    extern GLenum_glew __stdcall glewInit(void);
    extern unsigned char glewExperimental;
    extern const unsigned char* __stdcall glewGetErrorString(GLenum_glew error);
}
#define GLEW_OK 0
#endif

//==============================================================================
// ProjectMGLRenderer — singleton off-screen GL context
//
// Owns a hidden native window with an OpenGL context.  projectM v4
// hardcodes its final composite to FBO 0, so the host window must be
// sized to match the render target.  All ProjectMComponent instances
// register here and get their renderFrame_GL() called from the shared
// GL thread.  Result is read back to a juce::Image so the components
// remain lightweight (no per-component native HWND).
//==============================================================================

class ProjectMGLRenderer : private juce::OpenGLRenderer
{
public:
    static ProjectMGLRenderer& getInstance()
    {
        static ProjectMGLRenderer instance;
        return instance;
    }

    void registerComponent(ProjectMComponent* comp)
    {
        {
            const juce::ScopedLock sl(lock_);
            if (std::find(components_.begin(), components_.end(), comp) == components_.end())
                components_.push_back(comp);
        }
        ensureStarted();
    }

    /// Resize the hidden host window so that its default framebuffer (FBO 0)
    /// is large enough for projectM to render into.  Must be called on the
    /// message thread (e.g. from Component::resized()).
    void updateHostSize(int w, int h)
    {
        if (w <= 0 || h <= 0) return;
        // Grow to accommodate the largest registered component.
        int curW = hostComp_.getWidth();
        int curH = hostComp_.getHeight();
        if (w > curW || h > curH)
            hostComp_.setSize(juce::jmax(w, curW), juce::jmax(h, curH));
    }

    void unregisterComponent(ProjectMComponent* comp)
    {
        const juce::ScopedLock sl(lock_);
        components_.erase(
            std::remove(components_.begin(), components_.end(), comp),
            components_.end());
    }

    juce::OpenGLContext& getContext() { return glContext_; }

private:
    ProjectMGLRenderer() = default;

    ~ProjectMGLRenderer()
    {
        glContext_.detach();
    }

    void ensureStarted()
    {
        if (started_) return;

        // Create an off-screen native window to host the GL context.
        // Size must be >= the largest ProjectM render target, because
        // projectM v4 hard-codes its final composite to FBO 0.
        hostComp_.setSize(960, 540);
        hostComp_.setTopLeftPosition(-32000, -32000);
        hostComp_.addToDesktop(juce::ComponentPeer::windowIsTemporary);
        hostComp_.setVisible(true);

        glContext_.setRenderer(this);
        glContext_.setContinuousRepainting(true);
        glContext_.setComponentPaintingEnabled(false);
        glContext_.attachTo(hostComp_);

        started_ = true;
    }

    void newOpenGLContextCreated() override
    {
#ifdef _WIN32
        // projectM uses GLEW internally for GL function pointers.
        // Must be initialised once per context before any projectM call.
        glewExperimental = 1;  // GL_TRUE
        GLenum_glew err = glewInit();
        if (err != GLEW_OK)
            DBG("ProjectMGLRenderer: glewInit failed: " + juce::String((const char*)glewGetErrorString(err)));
#endif
    }

    void renderOpenGL() override
    {
        std::vector<ProjectMComponent*> comps;
        {
            const juce::ScopedLock sl(lock_);
            comps = components_;
        }

        for (auto* comp : comps)
        {
            if (!comp->glInitialised)
                comp->initGLResources_GL();

            comp->renderFrame_GL();
        }

        juce::Thread::sleep(1);
    }

    void openGLContextClosing() override
    {
        std::vector<ProjectMComponent*> comps;
        {
            const juce::ScopedLock sl(lock_);
            comps = components_;
        }
        for (auto* comp : comps)
            comp->releaseGLResources_GL();
    }

    struct HostComp : juce::Component { void paint(juce::Graphics&) override {} };

    HostComp              hostComp_;
    juce::OpenGLContext   glContext_;
    juce::CriticalSection lock_;
    std::vector<ProjectMComponent*> components_;
    bool started_ = false;
};

//==============================================================================
// ProjectMComponent
//==============================================================================

ProjectMComponent::ProjectMComponent(AudioEngine& ae)
    : audioEngine_(ae)
{
    setOpaque(true);
    readbackImage_ = juce::Image(juce::Image::RGB, 1, 1, false);
    ProjectMGLRenderer::getInstance().registerComponent(this);
}

ProjectMComponent::~ProjectMComponent()
{
    // Unregister first so the GL thread stops calling us.
    ProjectMGLRenderer::getInstance().unregisterComponent(this);

    // Give the GL thread time to finish any in-progress renderFrame_GL
    // that was already executing with a snapshot of the old component list.
    juce::Thread::sleep(50);

    // Release GL resources on the GL thread.
    auto& ctx = ProjectMGLRenderer::getInstance().getContext();
    if (ctx.isActive())
    {
        ctx.executeOnGLThread([this](juce::OpenGLContext&)
        {
            releaseGLResources_GL();
        }, true);   // block until done
    }
}

//==============================================================================
void ProjectMComponent::setPresetPath(const juce::String& folderPath)
{
    {
        const juce::ScopedLock sl(settingsLock_);
        presetPath_ = folderPath;
    }
    presetPathDirty_.store(true);
    ProjectMGLRenderer::getInstance().getContext().triggerRepaint();
}

juce::String ProjectMComponent::getPresetPath() const
{
    const juce::ScopedLock sl(const_cast<juce::CriticalSection&>(settingsLock_));
    return presetPath_;
}

void ProjectMComponent::nextPreset()
{
    nextPreset_.store(true);
    ProjectMGLRenderer::getInstance().getContext().triggerRepaint();
}

void ProjectMComponent::prevPreset()
{
    prevPreset_.store(true);
    ProjectMGLRenderer::getInstance().getContext().triggerRepaint();
}

void ProjectMComponent::setAutoPresetSeconds(int secs)
{
    autoPresetSecs_.store(secs);
    autoPresetDirty_.store(true);
    ProjectMGLRenderer::getInstance().getContext().triggerRepaint();
}

int ProjectMComponent::getAutoPresetSeconds() const
{
    return autoPresetSecs_.load();
}

juce::String ProjectMComponent::getCurrentPresetName() const
{
    const juce::ScopedLock sl(presetNameLock_);
    return currentPresetName_;
}

void ProjectMComponent::resized()
{
    int w = getWidth();
    int h = getHeight();
    pendingWidth_.store(w);
    pendingHeight_.store(h);
    sizeChanged_.store(true);
    ProjectMGLRenderer::getInstance().updateHostSize(w, h);
    ProjectMGLRenderer::getInstance().getContext().triggerRepaint();
}

void ProjectMComponent::paint(juce::Graphics& g)
{
    const juce::ScopedLock sl(imageLock_);
    if (readbackImage_.isValid())
        g.drawImage(readbackImage_, getLocalBounds().toFloat());
    else
        g.fillAll(juce::Colours::black);
}

//==============================================================================
// GL-thread methods
//==============================================================================

void ProjectMComponent::initGLResources_GL()
{
#ifdef PROJECTM_AVAILABLE
    if (glInitialised) return;

    int w = pendingWidth_.load();
    int h = pendingHeight_.load();
    if (w <= 0) w = juce::jmax(1, getWidth());
    if (h <= 0) h = juce::jmax(1, getHeight());
    w = juce::jmax(1, w);
    h = juce::jmax(1, h);

    // Create projectM instance
    pmHandle_ = projectm_create();
    if (pmHandle_ == nullptr)
    {
        DBG("ProjectMComponent: projectm_create() failed");
        glInitialised = true;
        return;
    }

    auto* pm = static_cast<projectm_handle>(pmHandle_);

    projectm_set_window_size(pm, static_cast<size_t>(w), static_cast<size_t>(h));

    // Create playlist
    playlistHandle_ = projectm_playlist_create(pm);

    // Auto-cycle if configured
    int autoCycle = autoPresetSecs_.load();
    if (auto* pl = static_cast<projectm_playlist_handle>(playlistHandle_))
    {
        projectm_playlist_set_shuffle(pl, autoCycle > 0);
    }
    if (autoCycle > 0)
    {
        projectm_set_preset_duration(pm, static_cast<double>(autoCycle));
    }
    else
    {
        projectm_set_preset_duration(pm, 9999.0); // effectively manual mode
    }

    // Load presets if path is already set
    juce::String path;
    {
        const juce::ScopedLock sl(settingsLock_);
        path = presetPath_;
    }
    if (path.isNotEmpty())
        applyPresetPath_GL(path);

    fboWidth_  = w;
    fboHeight_ = h;

    glInitialised = true;
    presetPathDirty_.store(false);
#else
    glInitialised = true;
#endif
}

void ProjectMComponent::renderFrame_GL()
{
#ifdef PROJECTM_AVAILABLE
    if (!glInitialised || pmHandle_ == nullptr) return;

    auto* pm = static_cast<projectm_handle>(pmHandle_);

    // Handle size changes
    if (sizeChanged_.exchange(false))
    {
        int nw = pendingWidth_.load();
        int nh = pendingHeight_.load();
        if (nw > 0 && nh > 0 && (nw != fboWidth_ || nh != fboHeight_))
        {
            fboWidth_  = nw;
            fboHeight_ = nh;
            projectm_set_window_size(pm, static_cast<size_t>(nw), static_cast<size_t>(nh));
        }
    }

    // Reload presets if path changed
    if (presetPathDirty_.exchange(false))
    {
        juce::String path;
        {
            const juce::ScopedLock sl(settingsLock_);
            path = presetPath_;
        }
        applyPresetPath_GL(path);
    }

    // Sync auto-cycle settings if changed
    if (autoPresetDirty_.exchange(false))
    {
        int autoCycle = autoPresetSecs_.load();
        if (auto* pl = static_cast<projectm_playlist_handle>(playlistHandle_))
            projectm_playlist_set_shuffle(pl, autoCycle > 0);
        projectm_set_preset_duration(pm, autoCycle > 0 ? static_cast<double>(autoCycle) : 9999.0);
    }

    // Navigate playlist
    if (auto* pl = static_cast<projectm_playlist_handle>(playlistHandle_))
    {
        if (nextPreset_.exchange(false))
            projectm_playlist_play_next(pl, true);
        if (prevPreset_.exchange(false))
            projectm_playlist_play_previous(pl, true);
    }

    // Drain PCM from AudioEngine and feed to projectM
    {
        std::array<float, kPCMDrainFrames * 2> pcmBuf {};
        int frames = audioEngine_.drainStereoFrames(pcmBuf.data(), kPCMDrainFrames);
        if (frames > 0)
            projectm_pcm_add_float(pm, pcmBuf.data(), static_cast<unsigned int>(frames),
                                   PROJECTM_STEREO);
    }

    // projectM v4 hardcodes its final composite pass to FBO 0
    // (glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0)).  The host window
    // has been sized large enough for the render target, so FBO 0
    // is the correct size.  We just render and read back from FBO 0.
    juce::gl::glBindFramebuffer(juce::gl::GL_FRAMEBUFFER, 0);
    juce::gl::glViewport(0, 0, fboWidth_, fboHeight_);

    projectm_opengl_render_frame(pm);

    // Read back from FBO 0
    readbackFBO_GL();

    // Update current preset name for the settings panel
    if (auto* pl = static_cast<projectm_playlist_handle>(playlistHandle_))
    {
        uint32_t pos = projectm_playlist_get_position(pl);
        char* name = projectm_playlist_item(pl, pos);
        if (name != nullptr)
        {
            // Extract just the filename without extension
            juce::String fullPath = juce::String::fromUTF8(name);
            juce::String displayName = juce::File(fullPath).getFileNameWithoutExtension();
            {
                const juce::ScopedLock sl(presetNameLock_);
                currentPresetName_ = displayName;
            }
            projectm_playlist_free_string(name);
        }
    }

    // Trigger a repaint on the GUI thread so paint() picks up the new image
    juce::MessageManager::callAsync([safeComp = juce::Component::SafePointer<ProjectMComponent>(this)]
    {
        if (safeComp != nullptr)
            safeComp->repaint();
    });
#endif
}

void ProjectMComponent::releaseGLResources_GL()
{
#ifdef PROJECTM_AVAILABLE
    if (playlistHandle_ != nullptr)
    {
        projectm_playlist_destroy(static_cast<projectm_playlist_handle>(playlistHandle_));
        playlistHandle_ = nullptr;
    }
    if (pmHandle_ != nullptr)
    {
        projectm_destroy(static_cast<projectm_handle>(pmHandle_));
        pmHandle_ = nullptr;
    }
    glInitialised = false;
#endif
}

//==============================================================================
// Private GL helpers
//==============================================================================

void ProjectMComponent::applyPresetPath_GL(const juce::String& path)
{
#ifdef PROJECTM_AVAILABLE
    if (playlistHandle_ == nullptr || path.isEmpty()) return;

    auto* pl = static_cast<projectm_playlist_handle>(playlistHandle_);
    projectm_playlist_clear(pl);
    projectm_playlist_add_path(pl, path.toRawUTF8(), true, false);

    if (projectm_playlist_size(pl) > 0)
        projectm_playlist_play_next(pl, false);
#else
    juce::ignoreUnused(path);
#endif
}

void ProjectMComponent::readbackFBO_GL()
{
    if (fboWidth_ <= 0 || fboHeight_ <= 0) return;

    // Read from the default framebuffer (FBO 0) where projectM rendered
    const int numPixels = fboWidth_ * fboHeight_;
    std::vector<juce::uint8> pixels(static_cast<size_t>(numPixels * 3));

    juce::gl::glBindFramebuffer(juce::gl::GL_FRAMEBUFFER, 0);
    juce::gl::glPixelStorei(juce::gl::GL_PACK_ALIGNMENT, 1);
    juce::gl::glReadPixels(0, 0, fboWidth_, fboHeight_,
                           juce::gl::GL_RGB, juce::gl::GL_UNSIGNED_BYTE,
                           pixels.data());

    // GL reads bottom-to-top; flip vertically into a juce::Image
    juce::Image img(juce::Image::ARGB, fboWidth_, fboHeight_, false);
    {
        juce::Image::BitmapData bmp(img, juce::Image::BitmapData::writeOnly);
        const int ps = bmp.pixelStride;
        for (int y = 0; y < fboHeight_; ++y)
        {
            const juce::uint8* srcRow = pixels.data() + static_cast<size_t>(fboHeight_ - 1 - y) * fboWidth_ * 3;
            juce::uint8* dstRow = bmp.getLinePointer(y);
            for (int x = 0; x < fboWidth_; ++x)
            {
                auto* px = reinterpret_cast<juce::PixelARGB*>(dstRow + x * ps);
                px->setARGB(255,
                            srcRow[x * 3],
                            srcRow[x * 3 + 1],
                            srcRow[x * 3 + 2]);
            }
        }
    }

    {
        const juce::ScopedLock sl(imageLock_);
        readbackImage_ = std::move(img);
    }
}
