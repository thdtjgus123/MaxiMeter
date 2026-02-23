#include "ThreeDViewport.h"

using namespace juce::gl;

// ─── Math helpers ────────────────────────────────────────────────────────────
static constexpr float kPi  = 3.14159265359f;
static constexpr float kDeg = kPi / 180.0f;

// Simple mat4 stored column-major (like OpenGL expects)
struct Mat4
{
    float m[16] {};

    static Mat4 identity()
    {
        Mat4 r;
        r.m[0]=1; r.m[5]=1; r.m[10]=1; r.m[15]=1;
        return r;
    }

    static Mat4 perspective(float fovY, float aspect, float near, float far)
    {
        Mat4 r;
        float f = 1.0f / std::tan(fovY * 0.5f);
        r.m[0]  =  f / aspect;
        r.m[5]  =  f;
        r.m[10] = (far + near) / (near - far);
        r.m[11] = -1.0f;
        r.m[14] = (2.0f * far * near) / (near - far);
        return r;
    }

    // forward, right, up must be orthonormal
    static Mat4 lookAt(float* eye, float* fwd, float* right, float* up)
    {
        Mat4 r;
        r.m[0]  =  right[0];
        r.m[4]  =  right[1];
        r.m[8]  =  right[2];
        r.m[1]  =  up[0];
        r.m[5]  =  up[1];
        r.m[9]  =  up[2];
        r.m[2]  = -fwd[0];
        r.m[6]  = -fwd[1];
        r.m[10] = -fwd[2];
        r.m[12] = -(right[0]*eye[0]+right[1]*eye[1]+right[2]*eye[2]);
        r.m[13] = -(  up[0]*eye[0]+  up[1]*eye[1]+  up[2]*eye[2]);
        r.m[14] =   (fwd[0]*eye[0]+ fwd[1]*eye[1]+ fwd[2]*eye[2]);
        r.m[15] =  1.0f;
        return r;
    }

    static Mat4 rotateY(float radians)
    {
        Mat4 r = identity();
        r.m[0]  =  std::cos(radians);
        r.m[2]  =  std::sin(radians);
        r.m[8]  = -std::sin(radians);
        r.m[10] =  std::cos(radians);
        return r;
    }
};

static float dot3(const float* a, const float* b)
{ return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }

static void cross3(const float* a, const float* b, float* out)
{
    out[0] = a[1]*b[2] - a[2]*b[1];
    out[1] = a[2]*b[0] - a[0]*b[2];
    out[2] = a[0]*b[1] - a[1]*b[0];
}

static void normalize3(float* v)
{
    float len = std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
    if (len > 0.00001f) { v[0]/=len; v[1]/=len; v[2]/=len; }
}

// ─────────────────────────────────────────────────────────────────────────────
ThreeDViewport::ThreeDViewport()
{
    waveformBuf_.resize(kAudioBufSize, 0.0f);
    spectrumBuf_.resize(kAudioBufSize, 0.0f);

    // Start the WASD movement timer
    startTimerHz(60);

    // Attach the GL context
    glContext_.setRenderer(this);
    glContext_.attachTo(*this);
    glContext_.setContinuousRepainting(true);

    setOpaque(true);
    setWantsKeyboardFocus(true);
}

ThreeDViewport::~ThreeDViewport()
{
    stopTimer();
    glContext_.setContinuousRepainting(false);
    glContext_.detach();
}

// ─── Public API ───────────────────────────────────────────────────────────────
void ThreeDViewport::setShader(const juce::String& vertSrc,
                               const juce::String& fragSrc,
                               std::function<void(const juce::String&)> callback)
{
    juce::ScopedLock sl(shaderLock_);
    pendingVertSrc_ = vertSrc;
    pendingFragSrc_ = fragSrc;
    pendingCallback_ = std::move(callback);
    programDirty_ = true;
}

void ThreeDViewport::setMeshPreset(MeshType mesh)
{
    pendingMesh_ = mesh;
    meshDirty_   = true;
}

void ThreeDViewport::setAudioData(const std::vector<float>& waveformMono,
                                  const std::vector<float>& spectrumMag)
{
    juce::ScopedLock sl(audioLock_);
    int wn = (int)std::min(waveformMono.size(), (size_t)kAudioBufSize);
    int sn = (int)std::min(spectrumMag.size(),  (size_t)kAudioBufSize);
    std::copy(waveformMono.begin(), waveformMono.begin() + wn, waveformBuf_.begin());
    std::copy(spectrumMag.begin(),  spectrumMag.begin()  + sn, spectrumBuf_.begin());
}

void ThreeDViewport::resetCamera()
{
    juce::ScopedLock sl(cameraLock_);
    cam_.pos  = { 0.0f, 2.0f, 8.0f };
    cam_.yaw   = -90.0f;
    cam_.pitch = -15.0f;
    cam_.fovDeg = 60.0f;
}

// ─── Component overrides ──────────────────────────────────────────────────────
void ThreeDViewport::resized() {}

void ThreeDViewport::visibilityChanged()
{
    if (isVisible())
        glContext_.setContinuousRepainting(true);
    else
        glContext_.setContinuousRepainting(false);
}

void ThreeDViewport::mouseDown(const juce::MouseEvent& e)
{
    mouseDragging_ = true;
    lastMousePos_  = e.getPosition();
    setMouseCursor(juce::MouseCursor::NoCursor);
}

void ThreeDViewport::mouseDrag(const juce::MouseEvent& e)
{
    if (!mouseDragging_) return;
    auto pos = e.getPosition();
    float dx = (float)(pos.x - lastMousePos_.x);
    float dy = (float)(pos.y - lastMousePos_.y);
    lastMousePos_ = pos;

    juce::ScopedLock sl(cameraLock_);
    cam_.yaw   += dx * cam_.mouseSens;
    cam_.pitch -= dy * cam_.mouseSens;
    cam_.pitch  = juce::jlimit(-89.0f, 89.0f, cam_.pitch);
}

void ThreeDViewport::mouseUp(const juce::MouseEvent&)
{
    mouseDragging_ = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void ThreeDViewport::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    juce::ScopedLock sl(cameraLock_);
    cam_.fovDeg -= w.deltaY * 5.0f;
    cam_.fovDeg = juce::jlimit(10.0f, 120.0f, cam_.fovDeg);
}

bool ThreeDViewport::keyPressed(const juce::KeyPress& key)
{
    // Consume WASD / QE so they don't bubble to parent
    int kc = key.getKeyCode();
    if (kc == 'w' || kc == 'W' || kc == 'a' || kc == 'A' ||
        kc == 's' || kc == 'S' || kc == 'd' || kc == 'D' ||
        kc == 'q' || kc == 'Q' || kc == 'e' || kc == 'E')
        return true;
    return false;
}

void ThreeDViewport::timerCallback()
{
    double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    float dt = (float)(now - lastTimerSec_);
    lastTimerSec_ = now;
    dt = juce::jlimit(0.0f, 0.1f, dt);

    bool w = juce::KeyPress::isKeyCurrentlyDown('w') || juce::KeyPress::isKeyCurrentlyDown('W');
    bool a = juce::KeyPress::isKeyCurrentlyDown('a') || juce::KeyPress::isKeyCurrentlyDown('A');
    bool s = juce::KeyPress::isKeyCurrentlyDown('s') || juce::KeyPress::isKeyCurrentlyDown('S');
    bool d = juce::KeyPress::isKeyCurrentlyDown('d') || juce::KeyPress::isKeyCurrentlyDown('D');
    bool q = juce::KeyPress::isKeyCurrentlyDown('q') || juce::KeyPress::isKeyCurrentlyDown('Q');
    bool e = juce::KeyPress::isKeyCurrentlyDown('e') || juce::KeyPress::isKeyCurrentlyDown('E');

    if (!(w || a || s || d || q || e)) return;

    // Compute camera front direction from yaw/pitch
    float yawR   = cam_.yaw   * kDeg;
    float pitchR = cam_.pitch * kDeg;
    float fwd[3] = {
        std::cos(pitchR) * std::cos(yawR),
        std::sin(pitchR),
        std::cos(pitchR) * std::sin(yawR)
    };
    float worldUp[3] = { 0.0f, 1.0f, 0.0f };
    float right[3];
    cross3(fwd, worldUp, right);
    normalize3(right);

    float spd = cam_.moveSpeed * dt;
    juce::ScopedLock sl(cameraLock_);
    if (w) { cam_.pos.x += fwd[0]*spd; cam_.pos.y += fwd[1]*spd; cam_.pos.z += fwd[2]*spd; }
    if (s) { cam_.pos.x -= fwd[0]*spd; cam_.pos.y -= fwd[1]*spd; cam_.pos.z -= fwd[2]*spd; }
    if (a) { cam_.pos.x -= right[0]*spd; cam_.pos.y -= right[1]*spd; cam_.pos.z -= right[2]*spd; }
    if (d) { cam_.pos.x += right[0]*spd; cam_.pos.y += right[1]*spd; cam_.pos.z += right[2]*spd; }
    if (q) cam_.pos.y -= spd;
    if (e) cam_.pos.y += spd;
}

// ─── OpenGL Renderer ──────────────────────────────────────────────────────────
void ThreeDViewport::newOpenGLContextCreated()
{
    startTimeSec_ = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    buildAllMeshes();
    buildDefaultProgram();
}

void ThreeDViewport::openGLContextClosing()
{
    currentProgram_.reset();
    for (auto& m : meshes_)
        freeMeshBuffers(m);
}

void ThreeDViewport::renderOpenGL()
{
    // ── Recompile shader if requested ────────────────────────────────────────
    {
        juce::ScopedLock sl(shaderLock_);
        if (programDirty_)
        {
            programDirty_ = false;
            recompileProgram();
        }
    }

    // ── Switch mesh if requested ─────────────────────────────────────────────
    if (meshDirty_)
    {
        meshDirty_    = false;
        activeMesh_   = pendingMesh_;
    }

    // ── Clear ────────────────────────────────────────────────────────────────
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);

    if (!currentProgram_) return;

    auto& mesh = meshes_[(int)activeMesh_];
    if (!mesh.ready)  return;

    // Use physical pixels (logical size × display scale) so the viewport
    // covers the full framebuffer on HiDPI / Windows display-scaled screens.
    auto scale = (float)glContext_.getRenderingScale();
    int  vpW   = juce::roundToInt(getWidth()  * scale);
    int  vpH   = juce::roundToInt(getHeight() * scale);
    if (vpW <= 0 || vpH <= 0) return;
    float aspect = (float)vpW / (float)vpH;

    glViewport(0, 0, vpW, vpH);

    // ── Camera matrices ──────────────────────────────────────────────────────
    Camera cam;
    { juce::ScopedLock sl(cameraLock_); cam = cam_; }

    float yawR   = cam.yaw   * kDeg;
    float pitchR = cam.pitch * kDeg;
    float fwd[3] = {
        std::cos(pitchR) * std::cos(yawR),
        std::sin(pitchR),
        std::cos(pitchR) * std::sin(yawR)
    };
    normalize3(fwd);
    float eye[3] = { cam.pos.x, cam.pos.y, cam.pos.z };
    float ctr[3] = { eye[0]+fwd[0], eye[1]+fwd[1], eye[2]+fwd[2] };
    float worldUp[3] = { 0.0f, 1.0f, 0.0f };
    float right[3];
    cross3(fwd, worldUp, right); normalize3(right);
    float up[3];
    cross3(right, fwd, up); normalize3(up);

    Mat4 view = Mat4::lookAt(eye, fwd, right, up);
    Mat4 proj = Mat4::perspective(cam.fovDeg * kDeg, aspect, 0.1f, 500.0f);

    // ── Audio data ───────────────────────────────────────────────────────────
    std::vector<float> wave, spec;
    {
        juce::ScopedLock sl(audioLock_);
        wave = waveformBuf_;
        spec = spectrumBuf_;
    }

    // ── Upload uniforms ──────────────────────────────────────────────────────
    currentProgram_->use();

    double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    float  t   = (float)(now - startTimeSec_);

    auto setUniformF = [&](const char* name, float v)
    {
        GLuint loc = (GLuint)glGetUniformLocation(currentProgram_->getProgramID(), name);
        if ((GLint)loc >= 0) glUniform1f(loc, v);
    };
    auto setUniformI = [&](const char* name, int v)
    {
        GLuint loc = (GLuint)glGetUniformLocation(currentProgram_->getProgramID(), name);
        if ((GLint)loc >= 0) glUniform1i(loc, v);
    };
    auto setUniformMat4 = [&](const char* name, const Mat4& mat)
    {
        GLuint loc = (GLuint)glGetUniformLocation(currentProgram_->getProgramID(), name);
        if ((GLint)loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, mat.m);
    };
    auto setUniformV2 = [&](const char* name, float x, float y)
    {
        GLuint loc = (GLuint)glGetUniformLocation(currentProgram_->getProgramID(), name);
        if ((GLint)loc >= 0) glUniform2f(loc, x, y);
    };
    auto setUniformV3 = [&](const char* name, float x, float y, float z)
    {
        GLuint loc = (GLuint)glGetUniformLocation(currentProgram_->getProgramID(), name);
        if ((GLint)loc >= 0) glUniform3f(loc, x, y, z);
    };

    setUniformF("u_time", t);
    setUniformV2("u_resolution", (float)vpW, (float)vpH);
    setUniformMat4("u_view", view);
    setUniformMat4("u_proj", proj);
    setUniformV3("u_cameraPos", eye[0], eye[1], eye[2]);

    // Rotating cube model matrix
    if (activeMesh_ == MeshType::RotatingCube)
    {
        Mat4 model = Mat4::rotateY(t * 0.7f);
        setUniformMat4("u_model", model);
    }
    else
    {
        setUniformMat4("u_model", Mat4::identity());
    }

    // Audio uniforms — use whichever buffer matches the preset
    const std::vector<float>* audioBuf = &wave;
    if (activeMesh_ == MeshType::Spectrum3D)
        audioBuf = &spec;

    {
        auto loc = glGetUniformLocation(currentProgram_->getProgramID(), "u_audioData");
        if (loc >= 0)
        {
            int sz = (int)std::min(audioBuf->size(), (size_t)kAudioBufSize);
            glUniform1fv(loc, sz, audioBuf->data());
        }
    }
    setUniformI("u_waveformSize", (int)wave.size());
    setUniformI("u_spectrumSize", (int)spec.size());

    // ── Draw ─────────────────────────────────────────────────────────────────
    bool isParticles = (activeMesh_ == MeshType::Particles3D);
    if (isParticles)
    {
        // Additive blending + no depth writes for nice glowing particles
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);
        // Fallback point size if GL_PROGRAM_POINT_SIZE is ignored by driver
        glPointSize(6.0f);
    }
    glBindVertexArray(mesh.vao);
    glDrawArrays(mesh.drawMode, 0, mesh.count);
    glBindVertexArray(0);
    if (isParticles)
    {
        glDepthMask(GL_TRUE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glPointSize(1.0f);
    }
}

// ─── GL helpers ───────────────────────────────────────────────────────────────
void ThreeDViewport::recompileProgram()
{
    juce::String vertSrc, fragSrc;
    std::function<void(const juce::String&)> cb;
    {
        juce::ScopedLock sl(shaderLock_);
        vertSrc = pendingVertSrc_;
        fragSrc = pendingFragSrc_;
        cb      = pendingCallback_;
    }

    auto prog = std::make_unique<juce::OpenGLShaderProgram>(glContext_);
    bool ok = prog->addVertexShader(vertSrc)
           && prog->addFragmentShader(fragSrc)
           && prog->link();

    juce::String err = ok ? juce::String() : prog->getLastError();

    if (ok)
        currentProgram_ = std::move(prog);

    if (cb)
    {
        juce::MessageManager::callAsync([cb = std::move(cb), err]() mutable
        {
            cb(err);
        });
    }
}

void ThreeDViewport::buildDefaultProgram()
{
    auto presets = ThreeDShaderLibrary::getAllPresets();
    auto& p = presets[0]; // Shadertoy quad

    auto prog = std::make_unique<juce::OpenGLShaderProgram>(glContext_);
    bool ok = prog->addVertexShader(p.vertSrc)
           && prog->addFragmentShader(p.fragSrc)
           && prog->link();

    if (ok)
        currentProgram_ = std::move(prog);
}

void ThreeDViewport::buildAllMeshes()
{
    buildShadertoyQuad(meshes_[(int)MeshType::ShadertoyQuad]);
    buildWaveform3D   (meshes_[(int)MeshType::Waveform3D]);
    buildSpectrum3D   (meshes_[(int)MeshType::Spectrum3D]);
    buildRotatingCube (meshes_[(int)MeshType::RotatingCube]);
    buildInfiniteGrid (meshes_[(int)MeshType::InfiniteGrid]);
    buildParticles3D  (meshes_[(int)MeshType::Particles3D]);
}

void ThreeDViewport::buildShadertoyQuad(MeshBuffers& mb)
{
    // 2 triangles: full-screen quad in NDC
    float verts[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f,
    };
    glGenVertexArrays(1, &mb.vao);
    glGenBuffers(1, &mb.vbo);
    glBindVertexArray(mb.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mb.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glBindVertexArray(0);
    mb.count = 6;
    mb.drawMode = GL_TRIANGLES;
    mb.ready = true;
}

void ThreeDViewport::buildWaveform3D(MeshBuffers& mb)
{
    // N points along X axis; Y driven by waveform in shader
    const int N = 512;
    std::vector<float> verts; // x, y(=0), z, index
    verts.reserve(N * 4);
    for (int i = 0; i < N; ++i)
    {
        float x = (float)i / (N - 1) * 10.0f - 5.0f;
        verts.push_back(x);
        verts.push_back(0.0f);
        verts.push_back(0.0f);
        verts.push_back((float)i);
    }
    glGenVertexArrays(1, &mb.vao);
    glGenBuffers(1, &mb.vbo);
    glBindVertexArray(mb.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mb.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(float)), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
    mb.count = N;
    mb.drawMode = GL_LINE_STRIP;
    mb.ready = true;
}

void ThreeDViewport::buildSpectrum3D(MeshBuffers& mb)
{
    // N bars (2 triangles each) along X axis
    const int N = 128;
    std::vector<float> verts; // x, y, z, index
    verts.reserve(N * 6 * 4);
    float barW = 10.0f / N;
    for (int i = 0; i < N; ++i)
    {
        float x0 = (float)i / N * 10.0f - 5.0f + barW * 0.05f;
        float x1 = x0 + barW * 0.9f;
        float fi  = (float)i;
        // bottom-left, bottom-right, top-right, bottom-left, top-right, top-left
        float quad[6][4] = {
            {x0, 0.0f, 0.0f, fi},
            {x1, 0.0f, 0.0f, fi},
            {x1, 1.0f, 0.0f, fi},
            {x0, 0.0f, 0.0f, fi},
            {x1, 1.0f, 0.0f, fi},
            {x0, 1.0f, 0.0f, fi},
        };
        for (auto& v : quad)
            for (float c : v) verts.push_back(c);
    }
    glGenVertexArrays(1, &mb.vao);
    glGenBuffers(1, &mb.vbo);
    glBindVertexArray(mb.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mb.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(float)), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
    mb.count = N * 6;
    mb.drawMode = GL_TRIANGLES;
    mb.ready = true;
}

void ThreeDViewport::buildRotatingCube(MeshBuffers& mb)
{
    // 6 faces × 2 triangles × 3 verts; pos + normal
    float v[] = {
        // front
        -1,-1, 1, 0,0,1,   1,-1, 1, 0,0,1,   1, 1, 1, 0,0,1,
        -1,-1, 1, 0,0,1,   1, 1, 1, 0,0,1,  -1, 1, 1, 0,0,1,
        // back
         1,-1,-1, 0,0,-1, -1,-1,-1, 0,0,-1, -1, 1,-1, 0,0,-1,
         1,-1,-1, 0,0,-1, -1, 1,-1, 0,0,-1,  1, 1,-1, 0,0,-1,
        // left
        -1,-1,-1,-1,0,0,  -1,-1, 1,-1,0,0,  -1, 1, 1,-1,0,0,
        -1,-1,-1,-1,0,0,  -1, 1, 1,-1,0,0,  -1, 1,-1,-1,0,0,
        // right
         1,-1, 1, 1,0,0,   1,-1,-1, 1,0,0,   1, 1,-1, 1,0,0,
         1,-1, 1, 1,0,0,   1, 1,-1, 1,0,0,   1, 1, 1, 1,0,0,
        // top
        -1, 1, 1, 0,1,0,   1, 1, 1, 0,1,0,   1, 1,-1, 0,1,0,
        -1, 1, 1, 0,1,0,   1, 1,-1, 0,1,0,  -1, 1,-1, 0,1,0,
        // bottom
        -1,-1,-1, 0,-1,0,  1,-1,-1, 0,-1,0,  1,-1, 1, 0,-1,0,
        -1,-1,-1, 0,-1,0,  1,-1, 1, 0,-1,0, -1,-1, 1, 0,-1,0,
    };
    glGenVertexArrays(1, &mb.vao);
    glGenBuffers(1, &mb.vbo);
    glBindVertexArray(mb.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mb.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
    mb.count = 36;
    mb.drawMode = GL_TRIANGLES;
    mb.ready = true;
}

void ThreeDViewport::buildInfiniteGrid(MeshBuffers& mb)
{
    const int N = 81;   // lines each direction
    const float ext = 40.0f;
    const float step = ext * 2.0f / (N - 1);
    std::vector<float> verts;
    // X-aligned lines
    for (int i = 0; i < N; ++i)
    {
        float z = -ext + step * i;
        verts.push_back(-ext); verts.push_back(0.0f); verts.push_back(z);
        verts.push_back( ext); verts.push_back(0.0f); verts.push_back(z);
    }
    // Z-aligned lines
    for (int i = 0; i < N; ++i)
    {
        float x = -ext + step * i;
        verts.push_back(x); verts.push_back(0.0f); verts.push_back(-ext);
        verts.push_back(x); verts.push_back(0.0f); verts.push_back( ext);
    }
    glGenVertexArrays(1, &mb.vao);
    glGenBuffers(1, &mb.vbo);
    glBindVertexArray(mb.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mb.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(float)), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glBindVertexArray(0);
    mb.count = N * 4;
    mb.drawMode = GL_LINES;
    mb.ready = true;
}

void ThreeDViewport::buildParticles3D(MeshBuffers& mb)
{
    const int N = 2000;
    std::vector<float> verts; // x y z phase
    verts.reserve(N * 4);
    juce::Random rng(42);
    for (int i = 0; i < N; ++i)
    {
        float x = rng.nextFloat() * 20.0f - 10.0f;
        float y = rng.nextFloat() * 5.0f;
        float z = rng.nextFloat() * 20.0f - 10.0f;
        float ph = rng.nextFloat() * 6.28318f;
        verts.push_back(x); verts.push_back(y); verts.push_back(z); verts.push_back(ph);
    }
    glGenVertexArrays(1, &mb.vao);
    glGenBuffers(1, &mb.vbo);
    glBindVertexArray(mb.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mb.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(float)), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
    mb.count = N;
    mb.drawMode = GL_POINTS;
    mb.ready = true;
}

void ThreeDViewport::freeMeshBuffers(MeshBuffers& mb)
{
    if (mb.vao) { glDeleteVertexArrays(1, &mb.vao); mb.vao = 0; }
    if (mb.vbo) { glDeleteBuffers(1, &mb.vbo);      mb.vbo = 0; }
    if (mb.ebo) { glDeleteBuffers(1, &mb.ebo);      mb.ebo = 0; }
    mb.ready = false;
}

// Matrix stubs (not used in ThreeDViewport directly, but required by header declarations)
juce::Matrix3D<float> ThreeDViewport::makePerspective(float, float, float, float)
{ return {}; }
juce::Matrix3D<float> ThreeDViewport::makeLookAt(juce::Vector3D<float>,
                                                  juce::Vector3D<float>,
                                                  juce::Vector3D<float>)
{ return {}; }
juce::Vector3D<float> ThreeDViewport::cameraFront() const
{
    float yr = cam_.yaw * kDeg, pr = cam_.pitch * kDeg;
    return { std::cos(pr)*std::cos(yr), std::sin(pr), std::cos(pr)*std::sin(yr) };
}
