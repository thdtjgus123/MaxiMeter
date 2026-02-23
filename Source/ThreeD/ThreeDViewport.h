#pragma once

#include <JuceHeader.h>
#include "ThreeDShaderLibrary.h"

//==============================================================================
/// OpenGL 3D viewport component.
/// Owns its own OpenGLContext, renders 3D geometry driven by audio and user shaders.
/// Camera: WASD movement + mouse-drag for yaw/pitch (FPS style).
class ThreeDViewport : public juce::Component,
                       public juce::OpenGLRenderer,
                       public juce::Timer
{
public:
    ThreeDViewport();
    ~ThreeDViewport() override;

    //==========================================================================
    /// Compile new vertex+fragment shaders. Returns empty string on success,
    /// or an error message on failure (called from message thread; compiles on
    /// GL thread asynchronously — callback delivers result on message thread).
    void setShader(const juce::String& vertSrc,
                   const juce::String& fragSrc,
                   std::function<void(const juce::String& errorOrEmpty)> callback);

    /// Switch the active mesh preset (safe to call from message thread).
    void setMeshPreset(MeshType mesh);

    /// Feed current audio waveform data (call each timer tick from message thread).
    void setAudioData(const std::vector<float>& waveformMono,
                      const std::vector<float>& spectrumMag);

    /// Reset camera to default position.
    void resetCamera();

    //==========================================================================
    // juce::Component overrides
    void resized() override;
    void visibilityChanged() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

    /// Allow keys to be consumed so parent doesn't intercept WASD.
    bool keyPressed(const juce::KeyPress& key) override;

    // juce::Timer — drives WASD movement on message thread
    void timerCallback() override;

    //==========================================================================
    // juce::OpenGLRenderer
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

private:
    //==========================================================================
    // GL state (accessed ONLY from GL thread unless noted)
    juce::OpenGLContext glContext_;

    // Shader programs
    std::unique_ptr<juce::OpenGLShaderProgram> currentProgram_;
    bool programDirty_ = false;  ///< Set on message thread; cleared on GL thread
    juce::String pendingVertSrc_;
    juce::String pendingFragSrc_;
    std::function<void(const juce::String&)> pendingCallback_;
    juce::CriticalSection shaderLock_;

    // Mesh geometry
    struct MeshBuffers
    {
        GLuint vao = 0, vbo = 0, vboAux = 0, ebo = 0;
        int    count = 0;
        GLenum drawMode = 0;  ///< GL_TRIANGLES=0x0004, GL_LINES=0x0001, GL_POINTS=0x0000
        bool   ready = false;
    };
    MeshBuffers meshes_[6];  ///< one per MeshType
    MeshType    activeMesh_ { MeshType::ShadertoyQuad };
    MeshType    pendingMesh_{ MeshType::ShadertoyQuad };
    bool        meshDirty_  = false;

    // Audio data (shared, protected by audioLock_)
    juce::CriticalSection audioLock_;
    std::vector<float> waveformBuf_;
    std::vector<float> spectrumBuf_;
    static constexpr int kAudioBufSize = 512;

    // Timing
    double startTimeSec_ = 0.0;

    // Camera — all on message thread except read in renderOpenGL (atomic copy)
    struct Camera
    {
        juce::Vector3D<float> pos  { 0.0f, 2.0f, 8.0f };
        float yaw   = -90.0f;  ///< degrees
        float pitch =  -15.0f; ///< degrees
        float fovDeg = 60.0f;
        float moveSpeed = 5.0f;
        float mouseSens = 0.2f;
    };
    Camera cam_;
    juce::CriticalSection cameraLock_;

    // Mouse drag state (message thread)
    bool      mouseDragging_ = false;
    juce::Point<int> lastMousePos_;
    double    lastTimerSec_ = 0.0;

    //==========================================================================
    // GL helpers
    void buildAllMeshes();
    void buildShadertoyQuad(MeshBuffers& m);
    void buildWaveform3D(MeshBuffers& m);
    void buildSpectrum3D(MeshBuffers& m);
    void buildRotatingCube(MeshBuffers& m);
    void buildInfiniteGrid(MeshBuffers& m);
    void buildParticles3D(MeshBuffers& m);
    void freeMeshBuffers(MeshBuffers& m);
    void buildDefaultProgram();
    void recompileProgram(); ///< called inside renderOpenGL when programDirty_

    // Matrix helpers
    static juce::Matrix3D<float> makePerspective(float fovDeg, float aspect, float near, float far);
    static juce::Matrix3D<float> makeLookAt(juce::Vector3D<float> eye,
                                             juce::Vector3D<float> centre,
                                             juce::Vector3D<float> up);
    juce::Vector3D<float> cameraFront() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThreeDViewport)
};
