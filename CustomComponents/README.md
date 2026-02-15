# MaxiMeter Custom Components — Plugin Developer Guide

## Overview

MaxiMeter supports user-created **Python plugins** that render audio-reactive
visualisations and meters directly on the canvas.  Plugins are hot-loaded at
runtime and communicate with the C++ host via a high-performance IPC bridge.

### Architecture at a Glance

```
┌──────────────────┐      SharedMemory (12 KB mmap)       ┌──────────────────┐
│   C++ Host       │ ──────────────────────────────────► │  Python Plugin   │
│  (JUCE / OpenGL) │      Win32 Pipe (JSON fallback)      │  (bridge.py)     │
│                  │ ◄────────────────────────────────── │                  │
│  MeterFactory    │      Render Commands (JSON)          │  RenderContext   │
└──────────────────┘                                      └──────────────────┘
```

| Transport       | Latency    | Data                  |
|-----------------|------------|-----------------------|
| SharedMemory    | ~5 µs      | Audio frames (binary) |
| JSON over Pipe  | ~500 µs    | Audio frames (text)   |
| JSON over Pipe  | —          | Render commands (out)  |

SharedMemory is used automatically when available.  If the mmap region
cannot be opened the bridge falls back to JSON transparently.

---

## Quick Start

1. Create a `.py` file in `CustomComponents/plugins/`.
2. Subclass `BaseComponent` and implement `get_manifest()`, `get_properties()`,
   and `on_render()`.
3. Restart MaxiMeter (or click the toolbox **+** button) — your plugin
   appears in the Custom Components section.

### Minimal Example

```python
from CustomComponents import (
    BaseComponent, Manifest, Category,
    Property, PropertyType,
    RenderContext, AudioData,
    Color, Font, TextAlign,
)

class HelloMeter(BaseComponent):
    @staticmethod
    def get_manifest() -> Manifest:
        return Manifest(
            id="com.example.hello_meter",
            name="Hello Meter",
            version="1.0.0",
            author="You",
            description="A minimal custom meter.",
            category=Category.METER,
            default_size=(200, 100),
            min_size=(100, 50),
            tags=("example",),
        )

    def get_properties(self):
        return [
            Property("bg", "Background", PropertyType.COLOR,
                     default=Color(0x10, 0x10, 0x20)),
        ]

    def on_render(self, ctx: RenderContext, audio: AudioData):
        ctx.clear(self.get_property("bg", Color(0x10, 0x10, 0x20)))
        db = audio.left.rms  # dBFS
        ctx.fill_rect(10, 10, ctx.width - 20, 30, Color(0, 200, 100))
        ctx.draw_text(f"{db:.1f} dBFS", 10, 50, 180, 20,
                      Color.white(), Font(size=14.0), TextAlign.LEFT)
```

---

## GPU Shader System

### Built-in Shaders

| Shader ID      | Type          | Description                        |
|----------------|---------------|------------------------------------|
| `particles`    | Visualizer    | Audio-reactive particle system     |
| `waveform3d`   | Visualizer    | 3-D waveform with depth rows      |
| `spectrum3d`   | Visualizer    | 3-D frequency bar graph           |
| `plasma`       | Visualizer    | Animated plasma pattern            |
| `voronoi`      | Visualizer    | Voronoi cell pattern               |
| `mandelbrot`   | Visualizer    | Mandelbrot fractal                 |
| `julia`        | Visualizer    | Julia-set fractal                  |
| `bloom`        | Post-process  | Glow / bloom effect                |
| `blur`         | Post-process  | Gaussian blur                      |
| `glitch`       | Post-process  | Digital glitch / line displacement |
| `chromatic`    | Post-process  | Chromatic aberration               |

### Compute Shader Particles (OpenGL 4.3+)

When the host detects **OpenGL 4.3 or higher**, the `particles` shader uses
a **compute shader** backed by an **SSBO (Shader Storage Buffer Object)**.

| Feature                  | Compute (GL 4.3+)        | Fragment fallback (GL 3.3) |
|--------------------------|--------------------------|----------------------------|
| Max particles            | **100 000**              | 500                        |
| Particle state           | Persistent (SSBO)        | Stateless (per-frame hash) |
| Physics                  | Velocity + gravity + drag| Position-only approximation|
| Workgroup size           | 256 threads              | N/A                        |
| Audio reactivity         | bass / mid / high bands  | bass / mid / high bands    |

The path selection is **fully automatic** — your Python code just calls
`ctx.particles(count=50000)` and the host picks the best backend.

```python
# Works on all GPUs — host auto-selects compute or fragment path
ctx.particles(
    count=50000,      # GL 3.3 will clamp this to 500
    speed=1.5,
    size=2.0,
    color=(0.3, 0.8, 1.0),
)
```

### Using `draw_shader()` Directly

For full control you can pass raw uniforms:

```python
ctx.draw_shader("particles",
    u_count=10000,
    u_speed=2.0,
    u_size=4.0,
    u_colorR=1.0, u_colorG=0.4, u_colorB=0.1,
)
```

### Custom GLSL Shaders

You can write your **own GLSL fragment shader** and pass the source code as
a string.  The host compiles it once and caches the program for subsequent
frames.

#### Host-Provided Uniforms

| Uniform          | Type        | Description                           |
|------------------|-------------|---------------------------------------|
| `u_time`         | float       | Elapsed time in seconds               |
| `u_resolution`   | vec2        | Viewport width, height in pixels      |
| `u_frame`        | int         | Frame counter                         |
| `u_audioData`    | sampler2D   | Row 0 = spectrum, Row 1 = waveform    |
| `u_spectrumSize` | int         | Number of spectrum bins               |
| `u_waveformSize` | int         | Number of waveform samples            |
| `u_texture`      | sampler2D   | Back-buffer (previous 2D draws)       |

Any additional uniforms you pass via `uniforms={}` or `**kwargs` are set
as `float` uniforms.

#### Example

```python
MY_SHADER = '''
#version 330 core
out vec4 fragColor;
in vec2 vTexCoord;
uniform float u_time;
uniform vec2  u_resolution;
uniform sampler2D u_audioData;
uniform int   u_spectrumSize;
uniform float u_intensity;

void main() {
    vec2 uv = vTexCoord;
    // Read spectrum magnitude at this x position
    float bin = texture(u_audioData, vec2(uv.x, 0.0)).r;
    // Read waveform sample
    float wave = texture(u_audioData, vec2(uv.x, 1.0)).r;

    float v = bin * u_intensity;
    fragColor = vec4(v, v * 0.5, uv.y, 1.0);
}
'''

class MyShaderViz(BaseComponent):
    @staticmethod
    def get_manifest():
        return Manifest(
            id="com.example.my_shader_viz",
            name="My Shader Viz",
            category=Category.VISUALIZER,
            default_size=(400, 300),
        )

    def on_render(self, ctx, audio):
        ctx.draw_custom_shader(
            MY_SHADER,
            cache_key="my_audio_viz",
            u_intensity=2.0,
        )
```

#### Rules

- Fragment-only shaders: `#version 330 core`
- Compute + fragment shaders: `#version 430 core` for both
- Output to `out vec4 fragColor`
- Use `vTexCoord` (vec2, 0–1) for UV coordinates
- Provide a stable `cache_key` to avoid recompilation every frame
- Custom uniforms are limited to `float` type
- Custom compute shaders get an SSBO at `binding = 0` (persistent across frames)

---

## SharedMemory Audio Transport

Audio data is written by the C++ host into a **12 KB memory-mapped region**
(`MaxiMeter_AudioSHM`) every frame and read by Python with near-zero
overhead.

### Memory Layout (v1)

| Offset | Size       | Field                                      |
|--------|------------|--------------------------------------------|
| 0      | 4 B        | Magic `0x4D584D41` ("MXMA")               |
| 4      | 4 B        | Version (1)                                |
| 8      | 4 B        | Frame counter (uint32)                     |
| 12     | 4 B        | Total buffer size                          |
| 16–75  | 60 B       | Global scalars (sample rate, channels, LUFS, BPM, …) |
| 76–235 | 160 B      | Per-channel data × 8 (RMS, peak, true-peak)|
| 236    | N×4 B      | Spectrum (float32[], length = fft_size/2+1)|
| 236+N×4| M×4 B      | Waveform (float32[], length = waveform_size)|

For full field details see `shared_memory.py`.

### Python Side

The bridge opens SharedMemory automatically.  If you need direct access:

```python
from CustomComponents.shared_memory import AudioSharedMemoryReader

reader = AudioSharedMemoryReader()
if reader.open():
    data = reader.read_raw()  # returns dict with all fields
    print(data["lufs_momentary"])
    reader.close()
```

---

## Error Recovery & Auto-Restart

The C++ host monitors the Python bridge process:

- **Crash detection**: If `WriteFile` / `ReadFile` fails or the process
  handle becomes invalid the host triggers an automatic restart.
- **Max retries**: Up to **5** consecutive restarts.  The counter resets
  after a successful IPC round-trip.
- **Intentional stop**: Calling `stop()` blocks the restart mechanism so
  the process is not accidentally revived.
- **Cool-down**: 200 ms delay between restart attempts to avoid spin-loops.

No action required from plugin authors — this is handled entirely by the
host.

---

## AudioData Reference

The `audio` object passed to `on_render()` exposes:

| Field                    | Type           | Description                       |
|--------------------------|----------------|-----------------------------------|
| `audio.left.rms`         | float (dBFS)   | Left channel RMS level            |
| `audio.left.peak`        | float (dBFS)   | Left channel peak                 |
| `audio.left.rms_linear`  | float 0–1      | Linear RMS                        |
| `audio.right.*`          | same           | Right channel                     |
| `audio.spectrum_linear`  | list[float]    | FFT magnitude bins (linear)       |
| `audio.spectrum_db`      | list[float]    | FFT magnitude bins (dBFS)         |
| `audio.waveform`         | list[float]    | Time-domain samples (1024)        |
| `audio.lufs_momentary`   | float          | EBU R128 momentary loudness       |
| `audio.lufs_short_term`  | float          | EBU R128 short-term loudness      |
| `audio.lufs_integrated`  | float          | EBU R128 integrated loudness      |
| `audio.bpm`              | float          | Estimated BPM                     |
| `audio.beat_phase`       | float 0–1      | Current beat phase                |
| `audio.sample_rate`      | float          | Sample rate (Hz)                  |
| `audio.is_playing`       | bool           | Whether audio is playing          |
| `audio.correlation`      | float          | Stereo correlation (–1 … +1)     |

---

## RenderContext API Summary

### Drawing Primitives

| Method                      | Description                          |
|-----------------------------|--------------------------------------|
| `clear(color)`              | Fill entire area                     |
| `fill_rect(x,y,w,h,color)` | Solid rectangle                      |
| `stroke_rect(…)`           | Rectangle outline                    |
| `fill_rounded_rect(…)`     | Rounded rectangle                    |
| `fill_ellipse(…)`          | Ellipse / circle                     |
| `draw_line(…)`             | Single line                          |
| `draw_path(points,…)`      | Multi-segment path                   |
| `draw_text(…)`             | Text rendering                       |
| `draw_arc(…)`              | Arc segment                          |
| `draw_bezier(…)`           | Bézier curve                         |

### State Management

| Method              | Description                              |
|---------------------|------------------------------------------|
| `save_state()`      | Push graphics state                      |
| `restore_state()`   | Pop graphics state                       |
| `set_opacity(v)`    | Set global opacity 0–1                   |
| `set_clip(x,y,w,h)` | Set clip rectangle                      |
| `reset_clip()`      | Remove clip                              |

### GPU Shader Helpers

| Method                        | Shader        | Description                  |
|-------------------------------|---------------|------------------------------|
| `particles(count,speed,…)`    | `particles`   | Compute / fragment particles |
| `waveform_3d(…)`             | `waveform3d`  | 3-D waveform rows           |
| `spectrum_3d(…)`             | `spectrum3d`  | 3-D bar spectrum             |
| `plasma(speed,scale,…)`      | `plasma`      | Animated plasma              |
| `voronoi(cells,speed)`       | `voronoi`     | Voronoi cells                |
| `mandelbrot(zoom,…)`         | `mandelbrot`  | Mandelbrot fractal           |
| `julia_set(zoom,c_real,…)`   | `julia`       | Julia fractal                |
| `bloom(intensity,…)`         | `bloom`       | Glow post-process            |
| `blur(radius,…)`             | `blur`        | Gaussian blur                |
| `glitch(intensity,…)`        | `glitch`      | Digital glitch               |
| `chromatic_aberration(…)`    | `chromatic`   | Chromatic aberration         |
| `draw_shader(id, **kwargs)`  | any           | Raw built-in shader call     |
| `draw_custom_shader(src, …)` | user GLSL     | Custom fragment (+ optional compute) shader |

---

## Example Plugins

See `plugins/` for ready-to-run examples:

| File                       | Description                                                  |
|----------------------------|--------------------------------------------------------------|
| `simple_vu_meter.py`       | Classic stereo VU with peak hold (2-D drawing only)          |
| `particle_visualizer.py`   | Compute-shader particles + 3-D waveform / spectrum / plasma  |
| `shader_effects.py`        | Post-processing chain (bloom, glitch, blur, chromatic)       |
| `circular_spectrum.py`     | Radial FFT bar analyser                                      |
| `fractal_visualizer.py`    | Audio-reactive Mandelbrot / Julia fractals                   |
| `loudness_history.py`      | Scrolling LUFS loudness graph                                |

---

## Troubleshooting

| Symptom                          | Cause                                        | Fix |
|----------------------------------|----------------------------------------------|-----|
| Plugin not appearing in toolbox  | Syntax error or missing `get_manifest()`     | Check Python console / log for import errors |
| Particles capped at 500         | GPU only supports GL 3.3                     | This is the expected fallback — no fix needed |
| No audio data (all zeros)       | SharedMemory & JSON both failed              | Ensure `MaxiMeter_AudioSHM` mmap name matches; check pipe connection |
| Plugin keeps restarting          | Crash loop (5 retries then stops)            | Fix the exception in your `on_render()` code |
| `draw_shader()` has no effect   | Unknown shader ID                            | Use one of the built-in IDs listed above |
