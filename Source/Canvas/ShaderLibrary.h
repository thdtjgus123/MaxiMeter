#pragma once

/**
 * @file ShaderLibrary.h
 * @brief Built-in GLSL fragment shaders for Python custom plugins.
 *
 * Each shader receives standard uniforms:
 *   - u_time       : float  (seconds since component creation)
 *   - u_resolution : vec2   (component width, height)
 *   - u_frame      : int    (frame counter)
 *   - u_audioData  : sampler2D (row 0 = spectrum, row 1 = waveform)
 *   - u_texture    : sampler2D (back-buffer with 2D rendered content)
 *   - u_spectrumSize : int
 *   - u_waveformSize : int
 *
 * Python plugins can also pass custom uniforms via draw_shader(uniforms={}).
 */

#include <JuceHeader.h>

namespace ShaderLibrary
{

//==============================================================================
// Passthrough — just renders the back-buffer texture as-is
//==============================================================================
inline juce::String passthroughShader()
{
    return R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D u_texture;
void main() {
    FragColor = texture(u_texture, vTexCoord);
}
)";
}

//==============================================================================
// Mandelbrot Set
//==============================================================================
inline juce::String mandelbrotShader()
{
    return R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;

uniform float u_time;
uniform vec2 u_resolution;
uniform sampler2D u_audioData;

uniform float u_zoom;       // default 1.0
uniform float u_centerX;    // default -0.5
uniform float u_centerY;    // default 0.0
uniform float u_maxIter;    // default 256
uniform float u_colorShift; // default 0.0

void main() {
    float zoom = max(u_zoom, 0.001);
    float cx = u_centerX;
    float cy = u_centerY;
    int maxIter = max(int(u_maxIter), 64);
    if (maxIter == 0) maxIter = 256;

    vec2 uv = (vTexCoord - 0.5) * vec2(u_resolution.x / u_resolution.y, 1.0);
    float scale = 3.0 / zoom;
    vec2 c = uv * scale + vec2(cx, cy);

    // Audio reactivity: modulate color using bass
    float bass = texture(u_audioData, vec2(0.05, 0.25)).r;

    vec2 z = vec2(0.0);
    int i;
    for (i = 0; i < maxIter; i++) {
        if (dot(z, z) > 4.0) break;
        z = vec2(z.x * z.x - z.y * z.y + c.x, 2.0 * z.x * z.y + c.y);
    }

    float t = float(i) / float(maxIter);

    // Smooth coloring
    if (i < maxIter) {
        float log_zn = log(dot(z, z)) / 2.0;
        float nu = log(log_zn / log(2.0)) / log(2.0);
        t = (float(i) + 1.0 - nu) / float(maxIter);
    }

    // Color palette with shift
    float shift = u_colorShift + u_time * 0.1 + bass * 2.0;
    vec3 col = 0.5 + 0.5 * cos(6.28318 * (t + shift + vec3(0.0, 0.33, 0.67)));

    if (i == maxIter) col = vec3(0.0);

    FragColor = vec4(col, 1.0);
}
)";
}

//==============================================================================
// Julia Set
//==============================================================================
inline juce::String juliaShader()
{
    return R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;

uniform float u_time;
uniform vec2 u_resolution;
uniform sampler2D u_audioData;

uniform float u_zoom;      // default 1.0
uniform float u_cReal;     // default -0.7
uniform float u_cImag;     // default 0.27015
uniform float u_maxIter;   // default 256
uniform float u_animate;   // default 1.0

void main() {
    float zoom = max(u_zoom, 0.001);
    int maxIter = max(int(u_maxIter), 64);
    if (maxIter == 0) maxIter = 256;

    vec2 uv = (vTexCoord - 0.5) * vec2(u_resolution.x / u_resolution.y, 1.0);
    float scale = 3.0 / zoom;
    vec2 z = uv * scale;

    // Audio-reactive Julia constant
    float bass = texture(u_audioData, vec2(0.05, 0.25)).r;
    float mid  = texture(u_audioData, vec2(0.3, 0.25)).r;

    vec2 c;
    if (u_animate > 0.5) {
        c = vec2(
            u_cReal + sin(u_time * 0.3) * 0.1 + bass * 0.05,
            u_cImag + cos(u_time * 0.4) * 0.1 + mid * 0.05
        );
    } else {
        c = vec2(u_cReal, u_cImag);
    }

    int i;
    for (i = 0; i < maxIter; i++) {
        if (dot(z, z) > 4.0) break;
        z = vec2(z.x * z.x - z.y * z.y + c.x, 2.0 * z.x * z.y + c.y);
    }

    float t = float(i) / float(maxIter);
    if (i < maxIter) {
        float log_zn = log(dot(z, z)) / 2.0;
        float nu = log(log_zn / log(2.0)) / log(2.0);
        t = (float(i) + 1.0 - nu) / float(maxIter);
    }

    vec3 col = 0.5 + 0.5 * cos(6.28318 * (t * 0.8 + u_time * 0.05 + vec3(0.0, 0.1, 0.2)));
    if (i == maxIter) col = vec3(0.0);

    FragColor = vec4(col, 1.0);
}
)";
}

//==============================================================================
// Bloom (post-process)
//   1. Extract bright areas (luminance > threshold)  → B/W mask
//   2. Gaussian-blur those bright pixels (two-pass: H then V in one shader)
//   3. Additive composite on top of the original
//==============================================================================
inline juce::String bloomShader()
{
    return R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D u_texture;
uniform vec2 u_resolution;
uniform sampler2D u_audioData;

uniform float u_intensity;   // default 1.5
uniform float u_threshold;   // default 0.6
uniform float u_radius;      // default 4.0

void main() {
    vec2 texel = 1.0 / u_resolution;
    vec4 original = texture(u_texture, vTexCoord);

    float intensity = max(u_intensity, 0.1);
    float threshold = clamp(u_threshold, 0.0, 1.0);
    float radius = max(u_radius, 1.0);
    float sigma = radius * 0.5;
    float sigSq2 = 2.0 * sigma * sigma;

    // Audio reactivity — bass drives extra intensity
    float bass = texture(u_audioData, vec2(0.05, 0.25)).r;
    intensity *= (1.0 + bass * 2.0);

    int iRadius = int(min(radius, 16.0));   // cap for perf

    // ── Pass 1: horizontal blur of thresholded bright pixels ──
    vec3 hBlur = vec3(0.0);
    float hWeight = 0.0;
    for (int x = -iRadius; x <= iRadius; x++) {
        vec2 uv = vTexCoord + vec2(float(x), 0.0) * texel;
        vec3 s = texture(u_texture, uv).rgb;
        float lum = dot(s, vec3(0.2126, 0.7152, 0.0722));
        // Soft-threshold: extract only the excess above threshold
        float bright = smoothstep(threshold - 0.05, threshold + 0.05, lum);
        float w = exp(-float(x * x) / sigSq2);
        hBlur += s * bright * w;
        hWeight += w;
    }
    hBlur /= max(hWeight, 0.001);

    // ── Pass 2: vertical blur of the horizontally-blurred bright result ──
    // We approximate by sampling vertically around the current point and
    // repeating the horizontal gather at each vertical offset.
    vec3 bloom = vec3(0.0);
    float vWeight = 0.0;
    for (int y = -iRadius; y <= iRadius; y++) {
        float wy = exp(-float(y * y) / sigSq2);

        // Horizontal gather at this vertical offset
        vec3 row = vec3(0.0);
        float rw = 0.0;
        for (int x = -iRadius; x <= iRadius; x++) {
            vec2 uv = vTexCoord + vec2(float(x), float(y)) * texel;
            vec3 s = texture(u_texture, uv).rgb;
            float lum = dot(s, vec3(0.2126, 0.7152, 0.0722));
            float bright = smoothstep(threshold - 0.05, threshold + 0.05, lum);
            float wx = exp(-float(x * x) / sigSq2);
            row += s * bright * wx;
            rw += wx;
        }
        row /= max(rw, 0.001);

        bloom += row * wy;
        vWeight += wy;
    }
    bloom /= max(vWeight, 0.001);

    // ── Additive composite ──
    FragColor = vec4(original.rgb + bloom * intensity, original.a);
}
)";
}

//==============================================================================
// Gaussian Blur (post-process)
//==============================================================================
inline juce::String blurShader()
{
    return R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D u_texture;
uniform vec2 u_resolution;
uniform sampler2D u_audioData;

uniform float u_radius;     // default 5.0
uniform float u_dirX;       // default 1.0
uniform float u_dirY;       // default 1.0

void main() {
    vec2 texel = 1.0 / u_resolution;
    float radius = max(u_radius, 0.5);

    // Audio-reactive radius
    float bass = texture(u_audioData, vec2(0.05, 0.25)).r;
    radius *= (1.0 + bass);

    vec4 result = vec4(0.0);
    float total = 0.0;

    vec2 dir = normalize(vec2(u_dirX, u_dirY));
    if (length(vec2(u_dirX, u_dirY)) < 0.01) dir = vec2(1.0, 0.0);

    for (float i = -radius; i <= radius; i += 1.0) {
        float w = exp(-(i*i) / (2.0 * radius * radius / 4.0));
        result += texture(u_texture, vTexCoord + dir * i * texel) * w;
        total += w;
    }

    FragColor = result / total;
}
)";
}

//==============================================================================
// Glitch effect (post-process)
//==============================================================================
inline juce::String glitchShader()
{
    return R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;

uniform float u_time;
uniform vec2 u_resolution;
uniform sampler2D u_texture;
uniform sampler2D u_audioData;

uniform float u_intensity;    // default 0.5
uniform float u_blockSize;    // default 0.03
uniform float u_chromatic;    // default 0.01

// Pseudo-random
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
    float intensity = u_intensity;
    float bass = texture(u_audioData, vec2(0.05, 0.25)).r;
    intensity *= (1.0 + bass * 3.0);

    vec2 uv = vTexCoord;

    // Block-based distortion
    float blockSize = max(u_blockSize, 0.005);
    vec2 block = floor(uv / blockSize) * blockSize;
    float noise = hash(block + floor(u_time * 10.0));

    if (noise > 1.0 - intensity * 0.3) {
        uv.x += (hash(block + 0.1) - 0.5) * intensity * 0.1;
    }

    // Scanline jitter
    float lineNoise = hash(vec2(floor(uv.y * u_resolution.y), floor(u_time * 15.0)));
    if (lineNoise > 1.0 - intensity * 0.1) {
        uv.x += (lineNoise - 0.5) * intensity * 0.05;
    }

    // Chromatic aberration
    float ca = u_chromatic * intensity;
    float r = texture(u_texture, uv + vec2(ca, 0.0)).r;
    float g = texture(u_texture, uv).g;
    float b = texture(u_texture, uv - vec2(ca, 0.0)).b;

    // Random color offset
    vec3 col = vec3(r, g, b);

    // Occasional white flash
    if (hash(vec2(floor(u_time * 5.0), 0.0)) > 0.97) {
        col = mix(col, vec3(1.0), intensity * 0.3);
    }

    FragColor = vec4(col, 1.0);
}
)";
}

//==============================================================================
// Particle system — Compute Shader + Fragment Shader (SSBO-based)
//
// Architecture:
//   1. Compute shader (particlesComputeShader) runs once per particle,
//      updating position/velocity/color in an SSBO.
//   2. Fragment shader (particlesShader) reads the SSBO and renders glow
//      by iterating only visible particles near each pixel using a grid.
//   However, since JUCE's ShaderProgram doesn't support compute shaders
//   natively, we use a spatial-hash approach in the fragment shader:
//   Particles are grouped into tiles, and each pixel only checks its
//   local tile, reducing O(pixels*particles) to O(pixels*~8).
//==============================================================================
inline juce::String particlesComputeShader()
{
    return R"(
#version 430 core
layout(local_size_x = 256) in;

struct Particle {
    vec4 posVel;   // xy = position, zw = velocity
    vec4 color;    // rgb = color, a = life
};

layout(std430, binding = 0) buffer ParticleBuffer {
    Particle particles[];
};

uniform float u_time;
uniform float u_deltaTime;
uniform float u_speed;
uniform int   u_count;
uniform float u_colorR;
uniform float u_colorG;
uniform float u_colorB;

layout(binding = 1) uniform sampler2D u_audioData;

float hash(float n) { return fract(sin(n) * 43758.5453); }

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= uint(u_count)) return;

    Particle p = particles[idx];
    float fi = float(idx);

    // First frame initialization
    if (p.color.a <= 0.001) {
        p.posVel.x = hash(fi * 1.17);
        p.posVel.y = hash(fi * 2.31);
        p.posVel.z = (hash(fi * 3.47) - 0.5) * 0.02;
        p.posVel.w = (hash(fi * 4.59) - 0.5) * 0.02;
        p.color = vec4(u_colorR, u_colorG, u_colorB, 1.0);
        // Per-particle color variation
        p.color.rgb = mix(p.color.rgb, vec3(1.0, 0.5, 0.2), hash(fi * 5.0) * 0.3);
    }

    float dt = u_deltaTime * u_speed;

    // Audio reactivity
    float freqBin = fi / float(max(u_count, 1));
    float audioVal = texture(u_audioData, vec2(freqBin, 0.25)).r;

    // Force field: gentle orbiting + audio displacement
    float angle = hash(fi * 6.71) * 6.28318;
    float orbitSpeed = 0.3 + hash(fi * 7.83) * 0.5;
    p.posVel.z += sin(u_time * orbitSpeed + angle) * 0.0005 * dt;
    p.posVel.w += cos(u_time * orbitSpeed + angle) * 0.0005 * dt;

    // Audio-reactive burst
    p.posVel.z += sin(angle) * audioVal * 0.003 * dt;
    p.posVel.w += cos(angle) * audioVal * 0.003 * dt;

    // Damping
    p.posVel.z *= (1.0 - 0.02 * dt);
    p.posVel.w *= (1.0 - 0.02 * dt);

    // Integrate position
    p.posVel.x += p.posVel.z * dt * 60.0;
    p.posVel.y += p.posVel.w * dt * 60.0;

    // Wrap around
    p.posVel.x = fract(p.posVel.x);
    p.posVel.y = fract(p.posVel.y);

    // Pulse color with audio
    p.color.rgb = mix(
        vec3(u_colorR, u_colorG, u_colorB),
        vec3(1.0, 0.5, 0.2),
        hash(fi * 5.0) * 0.3
    );
    p.color.rgb *= (0.7 + 0.3 * sin(fi * 0.1 + u_time));
    p.color.a = 0.5 + audioVal * 1.5;

    particles[idx] = p;
}
)";
}

/// Fragment shader for compute-shader path (reads SSBO, requires #version 430)
inline juce::String particlesShaderCompute()
{
    return R"(
#version 430 core
in vec2 vTexCoord;
out vec4 FragColor;

uniform float u_time;
uniform vec2 u_resolution;
uniform sampler2D u_audioData;

uniform float u_count;
uniform float u_speed;
uniform float u_size;
uniform float u_colorR;
uniform float u_colorG;
uniform float u_colorB;

struct Particle {
    vec4 posVel;
    vec4 color;
};

layout(std430, binding = 0) readonly buffer ParticleBuffer {
    Particle particles[];
};

void main() {
    vec2 uv = vTexCoord;
    float aspect = u_resolution.x / u_resolution.y;
    float particleSize = max(u_size, 0.5) / max(u_resolution.x, u_resolution.y);

    int count = max(int(u_count), 10);
    vec3 col = vec3(0.0);
    float totalGlow = 0.0;

    // Tile-based culling: only check particles near this pixel
    float tileSize = max(particleSize * 8.0, 0.02);
    int tileX = int(uv.x / tileSize);
    int tileY = int(uv.y / tileSize);

    for (int i = 0; i < count; i++) {
        Particle p = particles[i];
        vec2 pPos = p.posVel.xy;

        // Tile-based early rejection
        int ptX = int(pPos.x / tileSize);
        int ptY = int(pPos.y / tileSize);
        if (abs(ptX - tileX) > 1 || abs(ptY - tileY) > 1) continue;

        float d = length((uv - pPos) * vec2(aspect, 1.0));
        float glow = particleSize / max(d, 0.0001);
        glow = pow(glow, 1.5) * 0.01;
        glow *= p.color.a;

        col += p.color.rgb * glow;
        totalGlow += glow;
    }

    col = col / (1.0 + col);
    FragColor = vec4(col, min(totalGlow * 0.5, 1.0));
}
)";
}

/// Fallback fragment shader for GL < 4.3 (self-contained, no SSBO)
/// Particle count capped to 500 for acceptable performance.
inline juce::String particlesShader()
{
    return R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;

uniform float u_time;
uniform vec2 u_resolution;
uniform sampler2D u_audioData;

uniform float u_count;
uniform float u_speed;
uniform float u_size;
uniform float u_colorR;
uniform float u_colorG;
uniform float u_colorB;

float hash(float n) { return fract(sin(n) * 43758.5453); }

void main() {
    vec2 uv = vTexCoord;
    vec2 res = u_resolution;
    float aspect = res.x / res.y;

    int count = clamp(int(u_count), 10, 500); // capped for fragment-only path
    float particleSize = max(u_size, 0.5) / max(res.x, res.y);
    float speed = u_speed;

    // Audio reactivity
    float bass = texture(u_audioData, vec2(0.05, 0.25)).r;

    vec3 col = vec3(0.0);
    float totalGlow = 0.0;

    for (int i = 0; i < count; i++) {
        float fi = float(i);

        // Deterministic position per particle
        float px = hash(fi * 1.17);
        float py = hash(fi * 2.31);
        float pSpeed = hash(fi * 3.47) * 0.5 + 0.5;
        float pPhase = hash(fi * 4.59) * 6.28318;

        // Animated position
        float t = u_time * speed * pSpeed;
        float x = fract(px + sin(t * 0.3 + pPhase) * 0.2 + t * 0.05);
        float y = fract(py + cos(t * 0.4 + pPhase) * 0.15 + t * 0.03);

        // Audio-reactive displacement
        float freqBin = fi / float(count);
        float audioVal = texture(u_audioData, vec2(freqBin, 0.25)).r;
        y += audioVal * 0.15;
        x += audioVal * 0.05 * sin(pPhase);

        vec2 pPos = vec2(x, y);
        float d = length((uv - pPos) * vec2(aspect, 1.0));

        float glow = particleSize / max(d, 0.0001);
        glow = pow(glow, 1.5) * 0.01;
        glow *= (0.5 + audioVal * 2.0);

        vec3 pCol = vec3(u_colorR, u_colorG, u_colorB);
        pCol = mix(pCol, vec3(1.0, 0.5, 0.2), hash(fi * 5.0) * 0.3);
        pCol *= (0.7 + 0.3 * sin(fi * 0.1 + u_time));

        col += pCol * glow;
        totalGlow += glow;
    }

    col = col / (1.0 + col);
    FragColor = vec4(col, min(totalGlow * 0.5, 1.0));
}
)";
}

//==============================================================================
// 3D Waveform visualizer
//==============================================================================
inline juce::String waveform3dShader()
{
    return R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;

uniform float u_time;
uniform vec2 u_resolution;
uniform sampler2D u_audioData;
uniform int u_waveformSize;

uniform float u_rotateX;    // default 0.3
uniform float u_rotateY;    // default 0.0
uniform float u_depth;      // default 20.0
uniform float u_lineWidth;  // default 2.0

void main() {
    vec2 uv = vTexCoord * 2.0 - 1.0;
    uv.x *= u_resolution.x / u_resolution.y;

    float depth = max(u_depth, 1.0);
    float rotX = u_rotateX;
    float rotY = u_rotateY + u_time * 0.2;

    // Simple pseudo-3D: render multiple "rows" of the waveform
    vec3 col = vec3(0.0);
    int numRows = int(depth);

    for (int row = 0; row < numRows; row++) {
        float frow = float(row) / float(numRows);
        float z = frow * 2.0 - 1.0;

        // Perspective projection
        float perspective = 1.0 / (1.5 + z * 0.5);
        vec2 projected = uv * perspective;

        // Apply rotation
        float cosR = cos(rotX);
        float sinR = sin(rotX);
        projected.y = projected.y * cosR - z * sinR * 0.3;

        float cosY = cos(rotY);
        float sinY = sin(rotY);
        float px = projected.x * cosY - z * sinY * 0.3;

        // Map x to waveform sample
        float samplePos = (px + 1.5) / 3.0;
        if (samplePos < 0.0 || samplePos > 1.0) continue;

        // Read waveform from audio texture (row 1 = waveform)
        float waveVal = texture(u_audioData, vec2(samplePos, 0.75)).r;
        waveVal *= perspective * 0.5;

        // Check if this pixel is near the waveform line
        float dist = abs(projected.y - waveVal);
        float lineW = u_lineWidth / u_resolution.y * perspective;

        if (dist < lineW) {
            float intensity = 1.0 - dist / lineW;
            intensity *= intensity;
            intensity *= (1.0 - frow * 0.7); // fade with depth

            // Color based on depth
            vec3 lineCol = mix(
                vec3(0.2, 0.6, 1.0),
                vec3(1.0, 0.3, 0.5),
                frow
            );
            col += lineCol * intensity;
        }
    }

    FragColor = vec4(col, 1.0);
}
)";
}

//==============================================================================
// 3D Spectrum visualizer
//==============================================================================
inline juce::String spectrum3dShader()
{
    return R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;

uniform float u_time;
uniform vec2 u_resolution;
uniform sampler2D u_audioData;
uniform int u_spectrumSize;

uniform float u_rotateX;    // default 0.4
uniform float u_barWidth;   // default 0.8
uniform float u_height;     // default 1.5

void main() {
    vec2 uv = vTexCoord * 2.0 - 1.0;
    uv.x *= u_resolution.x / u_resolution.y;

    float rotX = u_rotateX;
    float barW = max(u_barWidth, 0.1);
    float heightScale = max(u_height, 0.1);

    int numBars = 64;
    vec3 col = vec3(0.0);

    for (int i = 0; i < numBars; i++) {
        float fi = float(i) / float(numBars);

        // Log-scale frequency mapping
        float freq = pow(fi, 2.0);

        // Read spectrum value
        float specVal = texture(u_audioData, vec2(freq, 0.25)).r;
        specVal = max(specVal, 0.0);

        // 3D bar position
        float bx = (fi - 0.5) * 3.0;
        float bz = 0.0;

        // Simple 3D perspective
        float cosR = cos(rotX);
        float sinR = sin(rotX);
        float projY = -sinR * bz + cosR * (-1.0);
        float projScale = 1.0 / (2.0 + sinR * 0.5);

        float barLeft = (bx - barW / float(numBars) * 1.5) * projScale;
        float barRight = (bx + barW / float(numBars) * 1.5) * projScale;
        float barBottom = projY * projScale;
        float barTop = (projY + specVal * heightScale) * projScale;

        if (uv.x >= barLeft && uv.x <= barRight && uv.y >= barBottom && uv.y <= barTop) {
            float heightFrac = (uv.y - barBottom) / max(barTop - barBottom, 0.001);

            vec3 barCol = mix(
                vec3(0.1, 0.4, 1.0),
                vec3(1.0, 0.2, 0.3),
                heightFrac
            );
            barCol *= (0.6 + specVal * 0.8);

            col = barCol;
        }
    }

    FragColor = vec4(col, 1.0);
}
)";
}

//==============================================================================
// Plasma effect
//==============================================================================
inline juce::String plasmaShader()
{
    return R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;

uniform float u_time;
uniform vec2 u_resolution;
uniform sampler2D u_audioData;

uniform float u_speed;     // default 1.0
uniform float u_scale;     // default 1.0
uniform float u_complexity;// default 3.0

void main() {
    vec2 uv = vTexCoord * u_scale;
    float t = u_time * u_speed;
    float complexity = max(u_complexity, 1.0);

    float bass = texture(u_audioData, vec2(0.05, 0.25)).r;
    float mid  = texture(u_audioData, vec2(0.3, 0.25)).r;

    float v1 = sin(uv.x * complexity + t);
    float v2 = sin(uv.y * complexity + t * 0.7);
    float v3 = sin((uv.x + uv.y) * complexity * 0.7 + t * 1.3);
    float v4 = sin(length(uv - 0.5) * complexity * 2.0 - t);

    float v = (v1 + v2 + v3 + v4) * 0.25;
    v += bass * 0.3;

    vec3 col;
    col.r = sin(v * 3.14159 + t * 0.5 + 0.0) * 0.5 + 0.5;
    col.g = sin(v * 3.14159 + t * 0.5 + 2.09) * 0.5 + 0.5;
    col.b = sin(v * 3.14159 + t * 0.5 + 4.19) * 0.5 + 0.5;

    col *= (0.8 + mid * 0.5);

    FragColor = vec4(col, 1.0);
}
)";
}

//==============================================================================
// Voronoi
//==============================================================================
inline juce::String voronoiShader()
{
    return R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;

uniform float u_time;
uniform vec2 u_resolution;
uniform sampler2D u_audioData;

uniform float u_cells;     // default 8.0
uniform float u_speed;     // default 1.0

vec2 hash2(vec2 p) {
    p = vec2(dot(p, vec2(127.1, 311.7)),
             dot(p, vec2(269.5, 183.3)));
    return fract(sin(p) * 43758.5453);
}

void main() {
    vec2 uv = vTexCoord * u_cells;
    float t = u_time * u_speed;

    float bass = texture(u_audioData, vec2(0.05, 0.25)).r;
    float mid  = texture(u_audioData, vec2(0.3, 0.25)).r;

    vec2 ip = floor(uv);
    vec2 fp = fract(uv);

    float minDist = 10.0;
    vec2 minPoint = vec2(0.0);

    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec2 neighbor = vec2(float(x), float(y));
            vec2 point = hash2(ip + neighbor);
            point = 0.5 + 0.5 * sin(t + 6.28318 * point + bass * 2.0);
            float d = length(neighbor + point - fp);
            if (d < minDist) {
                minDist = d;
                minPoint = point;
            }
        }
    }

    vec3 col = 0.5 + 0.5 * cos(6.28318 * (minDist * 2.0 + vec3(0.0, 0.33, 0.67) + u_time * 0.1));
    col *= (1.0 - minDist * 0.5);
    col *= (0.7 + mid * 0.6);

    FragColor = vec4(col, 1.0);
}
)";
}

//==============================================================================
// Chromatic Aberration (post-process)
//==============================================================================
inline juce::String chromaticShader()
{
    return R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D u_texture;
uniform vec2 u_resolution;
uniform sampler2D u_audioData;

uniform float u_intensity;   // default 0.005
uniform float u_angle;       // default 0.0

void main() {
    float intensity = u_intensity;
    float bass = texture(u_audioData, vec2(0.05, 0.25)).r;
    intensity *= (1.0 + bass * 3.0);

    float angle = u_angle;
    vec2 dir = vec2(cos(angle), sin(angle)) * intensity;

    float r = texture(u_texture, vTexCoord + dir).r;
    float g = texture(u_texture, vTexCoord).g;
    float b = texture(u_texture, vTexCoord - dir).b;

    FragColor = vec4(r, g, b, 1.0);
}
)";
}

//==============================================================================
// Registry: get shader source by ID
//==============================================================================
inline juce::String getFragmentShader(const juce::String& shaderId)
{
    if (shaderId == "passthrough")  return passthroughShader();
    if (shaderId == "mandelbrot")   return mandelbrotShader();
    if (shaderId == "julia")        return juliaShader();
    if (shaderId == "bloom")        return bloomShader();
    if (shaderId == "blur")         return blurShader();
    if (shaderId == "glitch")       return glitchShader();
    if (shaderId == "particles")    return particlesShader();
    if (shaderId == "waveform3d")   return waveform3dShader();
    if (shaderId == "spectrum3d")   return spectrum3dShader();
    if (shaderId == "plasma")       return plasmaShader();
    if (shaderId == "voronoi")      return voronoiShader();
    if (shaderId == "chromatic")    return chromaticShader();

    // Unknown shader
    DBG("Unknown shader ID: " + shaderId);
    return {};
}

/// Get compute shader source by ID. Returns empty string if shader has no compute stage.
inline juce::String getComputeShader(const juce::String& shaderId)
{
    if (shaderId == "particles") return particlesComputeShader();
    return {};
}

/// Get the SSBO-reading fragment shader for compute-enabled shaders.
/// Returns empty string if shader has no compute-specific fragment stage.
inline juce::String getComputeFragmentShader(const juce::String& shaderId)
{
    if (shaderId == "particles") return particlesShaderCompute();
    return {};
}

} // namespace ShaderLibrary
