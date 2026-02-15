"""
Particle Visualizer — GPU-accelerated audio-reactive particles.

Showcases: GPU particle system (compute shader on GL 4.3+, fragment fallback
on GL 3.3), audio reactivity via SharedMemory transport, 3D waveform,
spectrum 3D, plasma and voronoi effects.

Performance notes:
    - OpenGL 4.3+ (compute shader path): up to 100,000 particles using SSBO
      and compute shader dispatch. Particle state persists across frames.
    - OpenGL 3.3  (fragment fallback):   up to  500 particles, stateless
      per-frame rendering in the fragment shader.
    - Audio data is delivered via zero-copy SharedMemory (~5 µs/frame)
      with automatic JSON fallback when SHM is not available.
    - If the Python process crashes the host will auto-restart it
      (up to 5 retries).
"""

from __future__ import annotations

from CustomComponents import (
    BaseComponent, Manifest, Category,
    Property, PropertyType,
    RenderContext, AudioData,
    Color, Font, TextAlign,
)


class ParticleVisualizer(BaseComponent):
    """GPU-accelerated particle system and 3D audio visualizations.

    On GPUs supporting OpenGL 4.3+ the ``particles`` mode uses a compute
    shader backed by an SSBO for persistent particle state, enabling counts
    up to 100 000.  Older GPUs (GL 3.3) fall back to a fragment-only path
    capped at 500 particles.  Audio data reaches the shader via
    SharedMemory for minimal latency.
    """

    @staticmethod
    def get_manifest() -> Manifest:
        return Manifest(
            id="com.maximeter.builtin.particle_visualizer",
            name="Particle Visualizer",
            version="1.1.0",
            author="MaxiMeter",
            description="GPU compute-shader particles (100k on GL 4.3+), "
                        "3D waveform, 3D spectrum, plasma, voronoi — "
                        "all audio-reactive via SharedMemory.",
            category=Category.VISUALIZER,
            default_size=(400, 300),
            min_size=(200, 150),
            tags=("particles", "3d", "gpu", "compute", "shader",
                  "waveform", "spectrum"),
        )

    def get_properties(self):
        return [
            Property("mode", "Visualization", PropertyType.ENUM,
                     default="particles",
                     choices=[
                         ("particles", "Particles"),
                         ("waveform3d", "3D Waveform"),
                         ("spectrum3d", "3D Spectrum"),
                         ("plasma", "Plasma"),
                         ("voronoi", "Voronoi"),
                     ],
                     group="Mode"),
            Property("particle_count", "Particle Count", PropertyType.INT,
                     default=2000, min_value=100, max_value=100000,
                     group="Particles",
                     description="GL 4.3+ compute shader: up to 100 000. "
                                 "GL 3.3 fallback: capped at 500."),
            Property("particle_size", "Particle Size", PropertyType.FLOAT,
                     default=3.0, min_value=0.5, max_value=10.0,
                     group="Particles"),
            Property("speed", "Speed", PropertyType.FLOAT,
                     default=1.0, min_value=0.1, max_value=5.0,
                     group="Animation"),
            Property("color_r", "Color R", PropertyType.FLOAT,
                     default=0.3, min_value=0.0, max_value=1.0,
                     group="Color"),
            Property("color_g", "Color G", PropertyType.FLOAT,
                     default=0.6, min_value=0.0, max_value=1.0,
                     group="Color"),
            Property("color_b", "Color B", PropertyType.FLOAT,
                     default=1.0, min_value=0.0, max_value=1.0,
                     group="Color"),
        ]

    def on_render(self, ctx: RenderContext, audio: AudioData):
        mode = self.get_property("mode", "particles")
        speed = self.get_property("speed", 1.0)

        if mode == "particles":
            ctx.clear(Color(0x05, 0x05, 0x10))
            ctx.particles(
                count=self.get_property("particle_count", 1000),
                speed=speed,
                size=self.get_property("particle_size", 3.0),
                color=(
                    self.get_property("color_r", 0.3),
                    self.get_property("color_g", 0.6),
                    self.get_property("color_b", 1.0),
                ),
            )

        elif mode == "waveform3d":
            ctx.clear(Color(0x05, 0x08, 0x15))
            ctx.waveform_3d(
                rotate_x=0.3,
                depth=25.0,
                line_width=2.0,
            )

        elif mode == "spectrum3d":
            ctx.clear(Color(0x05, 0x05, 0x10))
            ctx.spectrum_3d(
                rotate_x=0.4,
                bar_width=0.8,
                height=1.5,
            )

        elif mode == "plasma":
            ctx.plasma(speed=speed, scale=1.2, complexity=4.0)

        elif mode == "voronoi":
            ctx.voronoi(cells=10.0, speed=speed)

        # Mode label overlay
        ctx.save_state()
        ctx.set_opacity(0.6)
        ctx.fill_rounded_rect(8, 8, 120, 22, 4, Color(0, 0, 0, 140))
        ctx.draw_text(mode.upper(), 14, 10, 110, 18,
                      Color(200, 200, 200), Font(size=10.0), TextAlign.LEFT)
        ctx.restore_state()
