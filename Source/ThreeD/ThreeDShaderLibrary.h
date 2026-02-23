#pragma once
#include <JuceHeader.h>

//==============================================================================
/// Mesh type used by the ThreeDViewport renderer.
enum class MeshType
{
    ShadertoyQuad = 0,  ///< Full-screen textured quad (shadertoy-style)
    Waveform3D,         ///< Audio waveform ribbon (N line-strip vertices)
    Spectrum3D,         ///< Frequency spectrum bars
    RotatingCube,       ///< Simple 3-D cube
    InfiniteGrid,       ///< XZ ground-plane grid
    Particles3D         ///< Point-sprite particles driven by audio RMS
};

//==============================================================================
/// One built-in shader preset.
struct ThreeDPreset
{
    juce::String name;
    MeshType     meshType;
    juce::String vertSrc;
    juce::String fragSrc;
};

//==============================================================================
namespace ThreeDShaderLibrary
{

// ── Helper macro to reduce boilerplate ────────────────────────────────────────
#define GLSL(src) "#version 330 core\n" #src

// ═══════════════════════════════════════════════════════════════════════════════
// 1.  Shadertoy Quad — fragment-shader playground, vertex is trivial
// ═══════════════════════════════════════════════════════════════════════════════
static const char* kShadertoyQuadVert = GLSL(
    layout(location = 0) in vec2 aPos;
    out vec2 vUV;
    void main() {
        vUV = aPos * 0.5 + 0.5;
        gl_Position = vec4(aPos, 0.0, 1.0);
    }
);

static const char* kShadertoyQuadFrag = GLSL(
    in vec2 vUV;
    out vec4 fragColor;

    uniform float u_time;
    uniform vec2  u_resolution;
    uniform float u_audioData[512];
    uniform int   u_waveformSize;

    void main() {
        vec2 uv = vUV;
        // Simple plasma / audio-reactive background
        float bass = 0.0;
        if (u_waveformSize > 0) {
            for (int i = 0; i < 8 && i < u_waveformSize; i++)
                bass += abs(u_audioData[i]);
            bass /= 8.0;
        }
        float t = u_time * 0.5;
        float r = 0.5 + 0.5 * sin(t + uv.x * 6.28 * (1.0 + bass));
        float g = 0.5 + 0.5 * sin(t * 1.3 + uv.y * 6.28);
        float b = 0.5 + 0.5 * cos(t * 0.7 + (uv.x + uv.y) * 6.28);
        fragColor = vec4(r, g, b, 1.0);
    }
);

// ═══════════════════════════════════════════════════════════════════════════════
// 2.  Waveform 3D — a ribbon strip following the audio waveform along X
// ═══════════════════════════════════════════════════════════════════════════════
static const char* kWaveform3DVert = GLSL(
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in float aIndex;

    uniform mat4 u_view;
    uniform mat4 u_proj;
    uniform float u_audioData[512];
    uniform int   u_waveformSize;
    uniform float u_time;

    out float vAmplitude;

    void main() {
        vec3 pos = aPos;
        int idx = int(aIndex);
        float amp = 0.0;
        if (idx >= 0 && idx < u_waveformSize)
            amp = u_audioData[idx];
        pos.y += amp * 2.0;
        vAmplitude = amp;
        gl_Position = u_proj * u_view * vec4(pos, 1.0);
    }
);

static const char* kWaveform3DFrag = GLSL(
    in float vAmplitude;
    out vec4 fragColor;
    uniform float u_time;
    void main() {
        float t = abs(vAmplitude);
        vec3 col = mix(vec3(0.0, 0.4, 1.0), vec3(1.0, 0.2, 0.0), t);
        fragColor = vec4(col, 1.0);
    }
);

// ═══════════════════════════════════════════════════════════════════════════════
// 3.  Spectrum 3D — frequency bars rising from ground plane
// ═══════════════════════════════════════════════════════════════════════════════
static const char* kSpectrum3DVert = GLSL(
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in float aIndex;

    uniform mat4 u_view;
    uniform mat4 u_proj;
    uniform float u_audioData[512];
    uniform int   u_spectrumSize;
    uniform float u_time;

    out float vHeight;

    void main() {
        vec3 pos = aPos;
        int idx = int(aIndex);
        float h = 0.0;
        if (idx >= 0 && idx < u_spectrumSize)
            h = clamp(u_audioData[idx], 0.0, 1.0);
        // Top vertices (aPos.y == 1) are lifted by spectrum value
        if (aPos.y > 0.5)
            pos.y = h * 4.0;
        vHeight = h;
        gl_Position = u_proj * u_view * vec4(pos, 1.0);
    }
);

static const char* kSpectrum3DFrag = GLSL(
    in float vHeight;
    out vec4 fragColor;
    void main() {
        vec3 low  = vec3(0.05, 0.3, 0.8);
        vec3 high = vec3(1.0, 0.1, 0.0);
        fragColor = vec4(mix(low, high, vHeight), 1.0);
    }
);

// ═══════════════════════════════════════════════════════════════════════════════
// 4.  Rotating Cube — classic spinning box with audio-driven colour
// ═══════════════════════════════════════════════════════════════════════════════
static const char* kRotatingCubeVert = GLSL(
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in vec3 aNormal;

    uniform mat4 u_model;
    uniform mat4 u_view;
    uniform mat4 u_proj;

    out vec3 vNormal;
    out vec3 vFragPos;

    void main() {
        vec4 world = u_model * vec4(aPos, 1.0);
        vFragPos  = world.xyz;
        vNormal   = mat3(transpose(inverse(u_model))) * aNormal;
        gl_Position = u_proj * u_view * world;
    }
);

static const char* kRotatingCubeFrag = GLSL(
    in vec3 vNormal;
    in vec3 vFragPos;
    out vec4 fragColor;

    uniform vec3  u_cameraPos;
    uniform float u_time;
    uniform float u_audioData[512];
    uniform int   u_waveformSize;

    void main() {
        float bass = 0.0;
        if (u_waveformSize > 0) {
            for (int i = 0; i < 4 && i < u_waveformSize; i++)
                bass += abs(u_audioData[i]);
            bass /= 4.0;
        }
        vec3 lightDir = normalize(vec3(1.0, 1.5, 2.0));
        vec3 norm = normalize(vNormal);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 baseCol = vec3(0.2 + bass, 0.5, 0.9);
        vec3 col = baseCol * (0.3 + 0.7 * diff);
        fragColor = vec4(col, 1.0);
    }
);

// ═══════════════════════════════════════════════════════════════════════════════
// 5.  Infinite Grid — XZ plane with fade-to-horizon
// ═══════════════════════════════════════════════════════════════════════════════
static const char* kInfiniteGridVert = GLSL(
    layout(location = 0) in vec3 aPos;

    uniform mat4 u_view;
    uniform mat4 u_proj;

    out vec3 vWorldPos;

    void main() {
        vWorldPos = aPos;
        gl_Position = u_proj * u_view * vec4(aPos, 1.0);
    }
);

static const char* kInfiniteGridFrag = GLSL(
    in vec3 vWorldPos;
    out vec4 fragColor;

    uniform float u_time;

    void main() {
        float scale = 1.0;
        vec2 coord = vWorldPos.xz / scale;
        vec2 grid  = abs(fract(coord - 0.5) - 0.5) / fwidth(coord);
        float line = min(grid.x, grid.y);
        float alpha = 1.0 - min(line, 1.0);
        // Fade with distance
        float dist = length(vWorldPos.xz);
        alpha *= smoothstep(60.0, 5.0, dist);
        fragColor = vec4(0.4, 0.6, 1.0, alpha * 0.8);
    }
);

// ═══════════════════════════════════════════════════════════════════════════════
// 6.  Particles 3D — point sprites driven by audio RMS
//     NOTE: avoids gl_PointCoord / discard for max driver compatibility.
//     Points are rendered as solid glowing squares; shape is handled CPU-side
//     via gl_PointSize only.
// ═══════════════════════════════════════════════════════════════════════════════
static const char* kParticles3DVert = GLSL(
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in float aPhase;

    uniform mat4  u_view;
    uniform mat4  u_proj;
    uniform float u_time;
    uniform float u_audioData[512];
    uniform int   u_waveformSize;

    out float vBrightness;

    void main() {
        float rms = 0.0;
        int   lim = u_waveformSize < 32 ? u_waveformSize : 32;
        for (int i = 0; i < lim; i++)
            rms += u_audioData[i] * u_audioData[i];
        if (lim > 0) rms = sqrt(rms / float(lim));

        float t = u_time + aPhase;
        vec3 p   = aPos;
        p.y += sin(t * 1.2 + aPhase * 5.0) * (0.5 + rms * 3.0);
        p.x += cos(t * 0.7 + aPhase * 3.0) * 0.3;
        gl_Position  = u_proj * u_view * vec4(p, 1.0);
        gl_PointSize = 4.0 + rms * 18.0;
        vBrightness  = rms;
    }
);

static const char* kParticles3DFrag = GLSL(
    in  float vBrightness;
    out vec4  fragColor;
    uniform float u_time;
    void main() {
        // Use gl_PointCoord for a smooth disc; fall back gracefully if coords
        // are degenerate (center = 0.5,0.5).
        vec2  uv    = gl_PointCoord - vec2(0.5);
        float r     = length(uv);
        float alpha = 1.0 - smoothstep(0.3, 0.5, r);
        if (alpha < 0.01) discard;
        vec3 cold = vec3(0.15, 0.45, 1.0);
        vec3 hot  = vec3(1.0,  0.35, 0.05);
        vec3 col  = mix(cold, hot, clamp(vBrightness * 4.0, 0.0, 1.0));
        float core = 1.0 - smoothstep(0.0, 0.2, r);
        col += vec3(0.5) * core;
        fragColor = vec4(col, alpha * 0.85);
    }
);

// ─── Blank template (must be defined while GLSL macro is still active) ───────
static const char* kBlankVert = GLSL(
    layout(location = 0) in vec2 aPos;
    void main() {
        gl_Position = vec4(aPos, 0.0, 1.0);
    }
);
static const char* kBlankFrag = GLSL(
    out vec4 fragColor;
    uniform float u_time;
    uniform vec2  u_resolution;
    void main() {
        vec2 uv = gl_FragCoord.xy / u_resolution;
        fragColor = vec4(uv, 0.5 + 0.5 * sin(u_time), 1.0);
    }
);

#undef GLSL

// ═══════════════════════════════════════════════════════════════════════════════
/// Return all built-in presets.
// ═══════════════════════════════════════════════════════════════════════════════
static inline std::vector<ThreeDPreset> getAllPresets()
{
    return {
        { "Blank",          MeshType::ShadertoyQuad, kBlankVert,         kBlankFrag         },
        { "Shadertoy Quad", MeshType::ShadertoyQuad, kShadertoyQuadVert, kShadertoyQuadFrag },
        { "Waveform 3D",    MeshType::Waveform3D,    kWaveform3DVert,    kWaveform3DFrag    },
        { "Spectrum 3D",    MeshType::Spectrum3D,    kSpectrum3DVert,    kSpectrum3DFrag    },
        { "Rotating Cube",  MeshType::RotatingCube,  kRotatingCubeVert,  kRotatingCubeFrag  },
        { "Infinite Grid",  MeshType::InfiniteGrid,  kInfiniteGridVert,  kInfiniteGridFrag  },
        { "Particles 3D",   MeshType::Particles3D,   kParticles3DVert,   kParticles3DFrag   },
    };
}

} // namespace ThreeDShaderLibrary
