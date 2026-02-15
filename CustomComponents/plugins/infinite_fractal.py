"""
Custom Component: infinite_fractal
An infinitely zooming Mandelbrot fractal whose zoom speed reacts to music loudness.
Uses a custom GLSL fragment shader for GPU-accelerated rendering.
"""

from __future__ import annotations
import math

from CustomComponents import (
    BaseComponent, Manifest, Category,
    Property, PropertyType,
    RenderContext, AudioData,
    Color, Font, TextAlign,
)

# ── Custom GLSL fragment shader ─────────────────────────────────────────────
# Uses smooth iteration count + palette cycling for rich colors.
# The host provides u_time, u_resolution, u_audioData automatically.
FRACTAL_SHADER = """
#version 330 core
in  vec2 vTexCoord;
out vec4 fragColor;

uniform float u_time;
uniform vec2  u_resolution;
uniform sampler2D u_audioData;

// Custom uniforms set from Python every frame
uniform float u_zoom;          // current zoom level (exponential)
uniform float u_centerX;       // camera center X in fractal space
uniform float u_centerY;       // camera center Y in fractal space
uniform float u_maxIter;       // iteration cap
uniform float u_paletteSpeed;  // palette animation speed
uniform float u_palR;          // palette hue offset R
uniform float u_palG;          // palette hue offset G
uniform float u_palB;          // palette hue offset B
uniform float u_glowIntensity; // inner glow strength on beats
uniform float u_warpAmount;    // audio-driven UV warp

void main() {
    int maxIter = max(int(u_maxIter), 64);
    float aspect = u_resolution.x / u_resolution.y;

    // Map pixel to fractal coordinates
    vec2 uv = (vTexCoord - 0.5) * vec2(aspect, 1.0);

    // Apply subtle audio-driven UV warp
    float bass = texture(u_audioData, vec2(0.02, 0.25)).r;
    float mid  = texture(u_audioData, vec2(0.15, 0.25)).r;
    uv += u_warpAmount * vec2(
        sin(uv.y * 8.0 + u_time * 2.0) * bass,
        cos(uv.x * 8.0 + u_time * 1.5) * mid
    ) * 0.02;

    // Scale by zoom and translate to center
    float scale = 3.0 / max(u_zoom, 0.0001);
    vec2 c = uv * scale + vec2(u_centerX, u_centerY);

    // ── Mandelbrot iteration ───────────────────────────────────────────
    vec2 z = vec2(0.0);
    int  i;
    for (i = 0; i < maxIter; i++) {
        if (dot(z, z) > 256.0) break;       // escape radius² = 256 for smoother coloring
        z = vec2(z.x * z.x - z.y * z.y + c.x,
                 2.0 * z.x * z.y + c.y);
    }

    // ── Smooth iteration count ─────────────────────────────────────────
    float t = float(i);
    if (i < maxIter) {
        float log_zn = log(dot(z, z)) / 2.0;
        float nu = log(log_zn / log(2.0)) / log(2.0);
        t = t + 1.0 - nu;
    }
    t = t / float(maxIter);

    // ── Color palette ──────────────────────────────────────────────────
    // Animated rainbow palette with per-channel offsets
    float phase = t * 6.0 + u_time * u_paletteSpeed;
    vec3 col = 0.5 + 0.5 * cos(6.28318 * (phase + vec3(u_palR, u_palG, u_palB)));

    // Interior = black with subtle glow on beats
    if (i == maxIter) {
        col = vec3(u_glowIntensity * 0.15, 0.0, u_glowIntensity * 0.08);
    }

    // Bright edge glow near the boundary
    if (i < maxIter) {
        float edgeFactor = 1.0 - t;
        col += vec3(0.1, 0.05, 0.15) * pow(edgeFactor, 8.0) * (1.0 + u_glowIntensity);
    }

    // Subtle vignette
    vec2 vig = vTexCoord - 0.5;
    float vigFactor = 1.0 - dot(vig, vig) * 1.2;
    col *= clamp(vigFactor, 0.3, 1.0);

    fragColor = vec4(col, 1.0);
}
"""


class InfiniteFractal(BaseComponent):
    """Infinitely zooming Mandelbrot fractal — zoom speed driven by music."""

    @staticmethod
    def get_manifest() -> Manifest:
        return Manifest(
            id="com.maximeter.custom.infinite_fractal",
            name="Infinite Fractal Zoom",
            version="1.0.0",
            author="MaxiMeter",
            description="A Mandelbrot fractal that zooms infinitely, "
                        "with speed controlled by audio loudness.",
            category=Category.VISUALIZER,
            default_size=(400, 400),
            min_size=(150, 150),
            tags=("fractal", "mandelbrot", "zoom", "shader", "gpu"),
        )

    def get_properties(self):
        return [
            Property("base_speed", "Base Zoom Speed", PropertyType.FLOAT,
                     default=0.3, min_value=0.0, max_value=2.0, step=0.05,
                     group="Zoom",
                     description="Zoom speed when audio is silent"),
            Property("audio_boost", "Audio Boost", PropertyType.FLOAT,
                     default=3.0, min_value=0.0, max_value=10.0, step=0.1,
                     group="Zoom",
                     description="How much loudness accelerates the zoom"),
            Property("max_iter", "Max Iterations", PropertyType.INT,
                     default=256, min_value=64, max_value=1024, step=32,
                     group="Quality",
                     description="Higher = sharper detail, slower render"),
            Property("palette_speed", "Color Speed", PropertyType.FLOAT,
                     default=0.15, min_value=0.0, max_value=2.0, step=0.01,
                     group="Color"),
            Property("palette_hue", "Palette Hue", PropertyType.FLOAT,
                     default=0.0, min_value=0.0, max_value=1.0, step=0.01,
                     group="Color",
                     description="Shift the color palette"),
            Property("uv_warp", "Audio Warp", PropertyType.FLOAT,
                     default=0.5, min_value=0.0, max_value=3.0, step=0.1,
                     group="Effects",
                     description="Audio-driven UV distortion amount"),
        ]

    # ── Interesting zoom targets in the Mandelbrot set ──────────────────
    # We cycle through these to keep the journey fresh.
    ZOOM_TARGETS = [
        (-0.74364388703,  0.13182590421),   # Seahorse valley spiral
        (-0.16,           1.0405),          # Elephant valley
        (-1.25066,        0.02012),         # Mini-brot tendril
        (-0.7463,         0.1102),          # Deep spiral arm
        ( 0.001643721971, 0.822467633298),  # Antenna double spiral
        (-0.749,          0.1005),          # Classic zoom spot
    ]

    def on_init(self):
        self.state.set("zoom_exp", 0.0)        # log-zoom accumulator
        self.state.set("smooth_loud", 0.0)      # smoothed loudness
        self.state.set("target_idx", 0)          # current zoom target
        self.state.set("time_at_target", 0.0)    # time spent on this target
        self.state.set("total_time", 0.0)        # total elapsed

    def on_render(self, ctx: RenderContext, audio: AudioData):
        # ── Read properties ─────────────────────────────────────────────
        base_speed    = self.get_property("base_speed", 0.3)
        audio_boost   = self.get_property("audio_boost", 3.0)
        max_iter      = self.get_property("max_iter", 256)
        palette_speed = self.get_property("palette_speed", 0.15)
        palette_hue   = self.get_property("palette_hue", 0.0)
        uv_warp       = self.get_property("uv_warp", 0.5)

        # ── Compute loudness ────────────────────────────────────────────
        # Use integrated short-term loudness, normalised to 0–1
        lufs = audio.lufs_short_term
        if lufs < -60.0:
            loudness = 0.0
        else:
            loudness = max(0.0, min(1.0, (lufs + 60.0) / 60.0))

        # Also check RMS as fallback if LUFS isn't available
        rms_level = max(audio.left.rms_linear, audio.right.rms_linear)
        loudness = max(loudness, rms_level)

        # Smooth the loudness for organic motion
        smooth_loud = self.state.get("smooth_loud", 0.0)
        smooth_loud += (loudness - smooth_loud) * 0.12
        self.state.set("smooth_loud", smooth_loud)

        # ── Zoom speed = base + loudness × boost ────────────────────────
        dt = 1.0 / 60.0  # assume ~60 fps
        zoom_speed = base_speed + smooth_loud * audio_boost
        zoom_exp = self.state.get("zoom_exp", 0.0) + zoom_speed * dt
        self.state.set("zoom_exp", zoom_exp)

        # Actual zoom = e^(zoom_exp), grows exponentially = infinite zoom
        zoom = math.exp(zoom_exp)

        # ── Cycle zoom targets ──────────────────────────────────────────
        # Switch target every ~30 seconds worth of zoom to keep it fresh.
        # At very deep zoom the float precision starts to pixelate anyway,
        # so we smoothly reset and move to the next target.
        MAX_ZOOM_EXP = 32.0  # ~e^32 ≈ 8.6 billion × zoom
        target_idx = int(self.state.get("target_idx", 0))

        if zoom_exp > MAX_ZOOM_EXP:
            # Reset zoom and go to next target
            zoom_exp = 0.0
            self.state.set("zoom_exp", 0.0)
            target_idx = (target_idx + 1) % len(self.ZOOM_TARGETS)
            self.state.set("target_idx", target_idx)
            zoom = 1.0

        cx, cy = self.ZOOM_TARGETS[target_idx]

        # ── Track total time for shader ─────────────────────────────────
        total_time = self.state.get("total_time", 0.0) + dt
        self.state.set("total_time", total_time)

        # ── Glow intensity on beats ─────────────────────────────────────
        glow = smooth_loud * 2.0

        # ── Render via custom GLSL shader ───────────────────────────────
        ctx.draw_custom_shader(
            FRACTAL_SHADER,
            cache_key="infinite_fractal_v1",
            u_zoom=zoom,
            u_centerX=cx,
            u_centerY=cy,
            u_maxIter=float(max_iter),
            u_paletteSpeed=palette_speed,
            u_palR=palette_hue,
            u_palG=palette_hue + 0.33,
            u_palB=palette_hue + 0.67,
            u_glowIntensity=glow,
            u_warpAmount=uv_warp,
        )

        # ── HUD overlay ─────────────────────────────────────────────────
        zoom_text = f"Zoom: {zoom:.1e}"
        ctx.draw_text(
            zoom_text, 8, ctx.height - 22, 200, 16,
            Color(200, 200, 200, 120), Font(size=10.0), TextAlign.LEFT,
        )
