"""
Shader Effects — Post-processing shader chain with audio reactivity.

Showcases: Bloom, blur, glitch, chromatic aberration.
Draw 2D content first, then apply GPU post-processing.
"""

from __future__ import annotations

from CustomComponents import (
    BaseComponent, Manifest, Category,
    Property, PropertyType,
    RenderContext, AudioData,
    Color, Font, TextAlign,
)


class ShaderEffects(BaseComponent):
    """Demonstrates chaining 2D drawing with GPU post-processing shaders."""

    @staticmethod
    def get_manifest() -> Manifest:
        return Manifest(
            id="com.maximeter.builtin.shader_effects",
            name="Shader Effects",
            version="1.0.0",
            author="MaxiMeter",
            description="Audio-reactive post-processing: bloom, glitch, blur.",
            category=Category.VISUALIZER,
            default_size=(400, 250),
            min_size=(200, 150),
            tags=("shader", "bloom", "glitch", "blur", "post-process"),
        )

    def get_properties(self):
        return [
            Property("effect", "Effect", PropertyType.ENUM,
                     default="bloom",
                     choices=[
                         ("bloom", "Bloom"),
                         ("glitch", "Glitch"),
                         ("blur", "Blur"),
                         ("chromatic", "Chromatic"),
                     ],
                     group="Effect"),
            Property("intensity", "Intensity", PropertyType.FLOAT,
                     default=1.0, min_value=0.0, max_value=5.0, step=0.1,
                     group="Effect"),
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
        effect = self.get_property("effect", "bloom")
        intensity = self.get_property("intensity", 1.0)

        w, h = ctx.width, ctx.height
        ctx.clear(bg)

        # Draw a spectrum bar visualization (2D)
        num_bars = 32
        smooth = self.state.get("smooth_levels", [0.0] * num_bars)
        spectrum = getattr(audio, "spectrum", []) or []
        margin = 20
        bar_area_w = w - margin * 2
        bar_w = bar_area_w / num_bars * 0.8
        gap = bar_area_w / num_bars * 0.2

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
            bar_h = (h - margin * 3) * level
            bx = margin + i * (bar_w + gap)
            by = h - margin - bar_h

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
            effect.upper(), margin, 8, bar_area_w, 18,
            Color(200, 200, 200, 180), Font(size=11.0), TextAlign.LEFT,
        )

        # Apply the selected post-processing shader
        if effect == "bloom":
            ctx.bloom(intensity=intensity * 1.5, threshold=0.4, radius=5.0)
        elif effect == "glitch":
            ctx.glitch(intensity=intensity * 0.4, block_size=0.03)
        elif effect == "blur":
            ctx.blur(radius=intensity * 3.0)
        elif effect == "chromatic":
            ctx.chromatic_aberration(intensity=intensity * 0.008)
