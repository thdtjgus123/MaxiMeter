"""
Fractal Visualizer — Mandelbrot & Julia set with audio reactivity.

Showcases: GPU shaders, fractal rendering, audio-reactive parameters.
"""

from __future__ import annotations

from CustomComponents import (
    BaseComponent, Manifest, Category,
    Property, PropertyType,
    RenderContext, AudioData,
    Color, Font, TextAlign,
)


class FractalVisualizer(BaseComponent):
    """Interactive Mandelbrot/Julia fractal renderer with audio reactivity."""

    @staticmethod
    def get_manifest() -> Manifest:
        return Manifest(
            id="com.maximeter.builtin.fractal_visualizer",
            name="Fractal Visualizer",
            version="1.0.0",
            author="MaxiMeter",
            description="GPU-rendered Mandelbrot/Julia fractals with audio-reactive coloring.",
            category=Category.VISUALIZER,
            default_size=(400, 400),
            min_size=(200, 200),
            tags=("fractal", "mandelbrot", "julia", "gpu", "shader"),
        )

    def get_properties(self):
        return [
            Property("fractal_type", "Fractal Type", PropertyType.ENUM,
                     default="mandelbrot",
                     choices=[("mandelbrot", "Mandelbrot"), ("julia", "Julia Set")],
                     group="Fractal"),
            Property("zoom", "Zoom", PropertyType.FLOAT,
                     default=1.0, min_value=0.01, max_value=100000.0,
                     group="Fractal"),
            Property("center_x", "Center X", PropertyType.FLOAT,
                     default=-0.5, min_value=-3.0, max_value=3.0,
                     group="Fractal"),
            Property("center_y", "Center Y", PropertyType.FLOAT,
                     default=0.0, min_value=-3.0, max_value=3.0,
                     group="Fractal"),
            Property("max_iter", "Max Iterations", PropertyType.INT,
                     default=256, min_value=64, max_value=2048,
                     group="Fractal"),
            Property("color_shift", "Color Shift", PropertyType.FLOAT,
                     default=0.0, min_value=0.0, max_value=1.0,
                     group="Appearance"),
            Property("animate", "Animate Julia", PropertyType.BOOL,
                     default=True, group="Animation"),
            Property("c_real", "Julia C (Real)", PropertyType.FLOAT,
                     default=-0.7, min_value=-2.0, max_value=2.0,
                     group="Julia"),
            Property("c_imag", "Julia C (Imag)", PropertyType.FLOAT,
                     default=0.27015, min_value=-2.0, max_value=2.0,
                     group="Julia"),
        ]

    def on_render(self, ctx: RenderContext, audio: AudioData):
        fractal = self.get_property("fractal_type", "mandelbrot")

        if fractal == "mandelbrot":
            ctx.mandelbrot(
                zoom=self.get_property("zoom", 1.0),
                center_x=self.get_property("center_x", -0.5),
                center_y=self.get_property("center_y", 0.0),
                max_iter=self.get_property("max_iter", 256),
                color_shift=self.get_property("color_shift", 0.0),
            )
        else:
            ctx.julia_set(
                zoom=self.get_property("zoom", 1.0),
                c_real=self.get_property("c_real", -0.7),
                c_imag=self.get_property("c_imag", 0.27015),
                max_iter=self.get_property("max_iter", 256),
                animate=self.get_property("animate", True),
            )

        # Title overlay
        label = "Mandelbrot" if fractal == "mandelbrot" else "Julia Set"
        ctx.save_state()
        ctx.set_opacity(0.7)
        ctx.fill_rounded_rect(8, 8, 140, 24, 4, Color(0, 0, 0, 160))
        ctx.draw_text(label, 14, 10, 130, 20,
                      Color(220, 220, 220), Font(size=12.0), TextAlign.LEFT)
        ctx.restore_state()
