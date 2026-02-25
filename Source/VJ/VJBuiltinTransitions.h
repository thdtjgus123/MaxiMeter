#pragma once

#include <JuceHeader.h>

//==============================================================================
/// Collection of built-in GLSL transition snippets in the GLTransitions format.
/// Each snippet defines a single function:
///   vec4 transition(vec2 uv) { ... }
/// with access to:  from, to (sampler2D), progress (float), ratio (float),
///                  getFromColor(vec2), getToColor(vec2)
///
/// These can be loaded directly into a VJGLSLTransition instance or pasted
/// into the GLSL editor as starting templates.
namespace VJBuiltinTransitions
{

//==============================================================================
struct Preset
{
    const char* name;
    const char* source;
};

//------------------------------------------------------------------------------
inline constexpr const char* kFade = R"(
vec4 transition(vec2 uv) {
    return mix(getFromColor(uv), getToColor(uv), progress);
}
)";

//------------------------------------------------------------------------------
inline constexpr const char* kDissolve = R"(
// Dissolve — random per-pixel threshold
float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}
vec4 transition(vec2 uv) {
    float r = rand(uv);
    return (r < progress) ? getToColor(uv) : getFromColor(uv);
}
)";

//------------------------------------------------------------------------------
inline constexpr const char* kPixelate = R"(
// Pixelate — blocky dissolve
vec4 transition(vec2 uv) {
    float cells = mix(1.0, 60.0, 1.0 - abs(progress - 0.5) * 2.0);
    vec2 pixel = floor(uv * cells) / cells;
    return mix(getFromColor(pixel), getToColor(pixel), progress);
}
)";

//------------------------------------------------------------------------------
inline constexpr const char* kRadialWipe = R"(
// Radial wipe (clock)
vec4 transition(vec2 uv) {
    float angle = atan(uv.y - 0.5, uv.x - 0.5) + 3.14159265;
    float norm  = angle / (2.0 * 3.14159265);
    return (norm < progress) ? getToColor(uv) : getFromColor(uv);
}
)";

//------------------------------------------------------------------------------
inline constexpr const char* kStarWipe = R"(
// Star wipe — 5-pointed star mask expanding from centre
vec4 transition(vec2 uv) {
    vec2 p = uv - 0.5;
    float angle = atan(p.y, p.x);
    float r = length(p);
    float star = 0.25 + 0.1 * cos(5.0 * angle);
    float threshold = progress * 0.8;
    return (r < star * threshold / 0.35) ? getToColor(uv) : getFromColor(uv);
}
)";

//------------------------------------------------------------------------------
inline constexpr const char* kCheckerboard = R"(
// Checkerboard reveal
vec4 transition(vec2 uv) {
    float cells = 10.0;
    vec2 cell = floor(uv * cells);
    float idx = mod(cell.x + cell.y, 2.0);
    float t = (idx < 0.5) ? progress * 2.0 : progress * 2.0 - 1.0;
    t = clamp(t, 0.0, 1.0);
    return mix(getFromColor(uv), getToColor(uv), t);
}
)";

//------------------------------------------------------------------------------
inline constexpr const char* kBurn = R"(
// Burn / flame edge
float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}
vec4 transition(vec2 uv) {
    float n = rand(uv * 20.0);
    float edge = progress * 1.3 - 0.15 + n * 0.3;
    edge = smoothstep(progress - 0.1, progress + 0.1, edge);
    vec4 from = getFromColor(uv);
    vec4 to   = getToColor(uv);
    // Orange glow at the burn edge
    float glow = smoothstep(0.0, 0.15, abs(edge - 0.5) < 0.1 ? 1.0 : 0.0);
    vec4 c = mix(from, to, edge);
    c.rgb += vec3(1.0, 0.4, 0.0) * glow * 0.5;
    return c;
}
)";

//------------------------------------------------------------------------------
inline constexpr const char* kRipple = R"(
// Ripple distortion
vec4 transition(vec2 uv) {
    vec2 center = vec2(0.5);
    float dist = distance(uv, center);
    float amp = (1.0 - progress) * 0.05;
    float freq = 20.0;
    vec2 offset = normalize(uv - center) * sin(dist * freq - progress * 10.0) * amp;
    return mix(getFromColor(uv + offset), getToColor(uv), smoothstep(0.0, 1.0, progress));
}
)";

//------------------------------------------------------------------------------
inline constexpr const char* kGlitch = R"(
// Glitch blocks
float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}
vec4 transition(vec2 uv) {
    float block = floor(uv.y * 20.0);
    float offset = (rand(vec2(block, progress)) - 0.5) * 0.2 * (1.0 - progress);
    vec2 uv2 = vec2(uv.x + offset, uv.y);
    return mix(getFromColor(uv2), getToColor(uv), smoothstep(0.3, 0.7, progress));
}
)";

//------------------------------------------------------------------------------
inline constexpr const char* kIrisOpen = R"(
// Iris open — circular reveal from centre
vec4 transition(vec2 uv) {
    float dist = distance(uv, vec2(0.5));
    float radius = progress * 0.72;  // sqrt(0.5) ≈ 0.707
    return (dist < radius) ? getToColor(uv) : getFromColor(uv);
}
)";

//------------------------------------------------------------------------------
inline constexpr const char* kRGBSplit = R"(
// RGB channel split
vec4 transition(vec2 uv) {
    float amount = 0.03 * sin(progress * 3.14159);
    vec4 from = getFromColor(uv);
    vec4 to   = getToColor(uv);
    vec4 c;
    c.r = mix(getFromColor(uv + vec2(amount, 0.0)).r, getToColor(uv).r, progress);
    c.g = mix(getFromColor(uv).g, getToColor(uv).g, progress);
    c.b = mix(getFromColor(uv - vec2(amount, 0.0)).b, getToColor(uv).b, progress);
    c.a = 1.0;
    return c;
}
)";

//==============================================================================
inline const std::vector<Preset>& getAll()
{
    static const std::vector<Preset> presets =
    {
        { "Fade (GLSL)",          kFade         },
        { "Dissolve",             kDissolve     },
        { "Pixelate",             kPixelate     },
        { "Radial Wipe",          kRadialWipe   },
        { "Star Wipe",            kStarWipe     },
        { "Checkerboard",         kCheckerboard },
        { "Burn",                 kBurn         },
        { "Ripple",               kRipple       },
        { "Glitch",               kGlitch       },
        { "Iris Open",            kIrisOpen     },
        { "RGB Split",            kRGBSplit     },
    };
    return presets;
}

} // namespace VJBuiltinTransitions
