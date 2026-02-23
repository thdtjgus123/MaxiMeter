"""
Water Reflection — Audio-reactive water reflection effect.

Draws a spectrum analyzer in the top half and applies a water reflection
shader to the bottom half, creating a flowing water effect that reflects
the content above it.
"""

from __future__ import annotations

from CustomComponents import (
    BaseComponent, Manifest, Category,
    Property, PropertyType,
    RenderContext, AudioData,
    Color, Font, TextAlign,
)


class WaterReflection(BaseComponent):
    """Demonstrates a water reflection post-processing shader."""

    @staticmethod
    def get_manifest() -> Manifest:
        return Manifest(
            id="com.maximeter.builtin.water_reflection",
            name="Water Reflection",
            version="1.0.0",
            author="MaxiMeter",
            description="Audio-reactive water reflection effect.",
            category=Category.VISUALIZER,
            default_size=(400, 300),
            min_size=(200, 150),
            tags=("shader", "water", "reflection", "post-process"),
        )

    def get_properties(self):
        return [
            Property("speed", "Flow Speed", PropertyType.FLOAT,
                     default=1.0, min_value=0.0, max_value=5.0, step=0.1,
                     group="Water"),
            Property("intensity", "Ripple Intensity", PropertyType.FLOAT,
                     default=0.02, min_value=0.0, max_value=0.1, step=0.005,
                     group="Water"),
            Property("reflection_height", "Water Level", PropertyType.FLOAT,
                     default=0.5, min_value=0.1, max_value=0.9, step=0.05,
                     group="Water"),
            Property("bar_color", "Bar Color", PropertyType.COLOR,
                     default=Color(0x3A, 0x7B, 0xFF), group="Appearance"),
            Property("bg_color", "Background", PropertyType.COLOR,
                     default=Color(0x0A, 0x0A, 0x14), group="Appearance"),
        ]

    def on_init(self):
        self.state.set("smooth_levels", [0.0] * 32)

    def on_render(self, ctx: RenderContext, audio: AudioData):
        bg = self.get_property("bg_color", Color(0x0A, 0x0A, 0x14))
        bar_col = self.get_property("bar_color", Color(0x3A, 0x7B, 0xFF))
        speed = self.get_property("speed", 1.0)
        intensity = self.get_property("intensity", 0.02)
        reflection_height = self.get_property("reflection_height", 0.5)

        w, h = ctx.width, ctx.height
        ctx.clear(bg)

        # Draw a spectrum bar visualization in the top part
        num_bars = 32
        smooth = self.state.get("smooth_levels", [0.0] * num_bars)
        spectrum = getattr(audio, "spectrum", []) or []
        margin = 20
        bar_area_w = w - margin * 2
        bar_w = bar_area_w / num_bars * 0.8
        gap = bar_area_w / num_bars * 0.2
        
        # The area above the water
        top_h = h * (1.0 - reflection_height)

        for i in range(num_bars):
            # Get spectrum value (log-scale mapping)
            idx = int((i / num_bars) ** 2 * len(spectrum)) if spectrum else 0
            val = spectrum[idx] if idx < len(spectrum) else 0.0

            # Smoothing
            if i < len(smooth):
                smooth[i] += (val - smooth[i]) * 0.2
            else:
                smooth.append(val * 0.2)

            level = min(1.0, max(0.0, smooth[i] * 3.0))
            bar_h = (top_h - margin * 2) * level
            bx = margin + i * (bar_w + gap)
            by = top_h - bar_h

            # Color gradient from bottom to top
            brightness = 0.5 + level * 0.5
            r = int(bar_col.r * brightness)
            g_ = int(bar_col.g * brightness)
            b = int(bar_col.b * brightness)
            col = Color(min(255, r), min(255, g_), min(255, b))

            ctx.fill_rounded_rect(bx, by, bar_w, bar_h, 2, col)

        self.state.set("smooth_levels", smooth)

        # Draw title
        ctx.draw_text(
            "WATER REFLECTION", margin, 8, bar_area_w, 18,
            Color(200, 200, 200, 180), Font(size=11.0), TextAlign.LEFT,
        )

        # Apply the water reflection shader
        ctx.water(speed=speed, intensity=intensity, reflection_height=reflection_height)
