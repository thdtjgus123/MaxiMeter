"""
Example: Circular Spectrum Analyser
Renders FFT spectrum data as a radial / circular bar visualisation.
"""

from __future__ import annotations

import math

from CustomComponents import (
    BaseComponent, Manifest, Category,
    Property, PropertyType,
    RenderContext, AudioData,
    Color, Gradient, GradientStop,
    Font, TextAlign,
)


class CircularSpectrum(BaseComponent):
    """Radial spectrum analyser with beat-reactive ring."""

    @staticmethod
    def get_manifest() -> Manifest:
        return Manifest(
            id="com.maximeter.example.circular_spectrum",
            name="Circular Spectrum",
            version="1.0.0",
            author="MaxiMeter",
            description="Radial FFT spectrum bars arranged in a circle.",
            category=Category.ANALYZER,
            default_size=(300, 300),
            min_size=(150, 150),
            tags=("spectrum", "circular", "radial", "fft", "example"),
        )

    def get_properties(self):
        return [
            Property("num_bars", "Number of Bars", PropertyType.INT,
                     default=64, min_value=8, max_value=256, group="Display"),
            Property("inner_radius", "Inner Radius %", PropertyType.RANGE,
                     default=0.3, min_value=0.1, max_value=0.8, step=0.05,
                     group="Display"),
            Property("bar_gain", "Bar Length", PropertyType.RANGE,
                     default=1.0, min_value=0.2, max_value=5.0, step=0.1,
                     group="Display"),
            Property("bar_color_low", "Low Freq Colour", PropertyType.COLOR,
                     default=Color(0x00, 0xCC, 0xFF), group="Colour"),
            Property("bar_color_high", "High Freq Colour", PropertyType.COLOR,
                     default=Color(0xFF, 0x44, 0x88), group="Colour"),
            Property("bg_color", "Background", PropertyType.COLOR,
                     default=Color(0x0A, 0x0A, 0x14), group="Colour"),
            Property("smoothing", "Smoothing", PropertyType.RANGE,
                     default=0.2, min_value=0.01, max_value=0.5, step=0.01,
                     group="Behaviour"),
            Property("mirror", "Mirror Mode", PropertyType.BOOL,
                     default=False, group="Display"),
        ]

    def on_init(self):
        self._smooth_bins: list[float] = []

    def on_render(self, ctx: RenderContext, audio: AudioData):
        bg = self.get_property("bg_color", Color(0x0A, 0x0A, 0x14))
        col_low = self.get_property("bar_color_low", Color(0x00, 0xCC, 0xFF))
        col_high = self.get_property("bar_color_high", Color(0xFF, 0x44, 0x88))
        num_bars = self.get_property("num_bars", 64)
        inner_pct = self.get_property("inner_radius", 0.3)
        bar_gain = self.get_property("bar_gain", 1.0)
        smoothing = self.get_property("smoothing", 0.2)
        mirror = self.get_property("mirror", False)

        ctx.clear(bg)
        w, h = ctx.width, ctx.height
        cx, cy = w / 2, h / 2
        max_r = min(w, h) / 2 - 4
        inner_r = max_r * inner_pct

        # Build bar magnitudes from spectrum
        spectrum = audio.spectrum_linear or []
        bins = self._map_spectrum_to_bars(spectrum, num_bars)

        # Smooth
        if len(self._smooth_bins) != num_bars:
            self._smooth_bins = [0.0] * num_bars
        for i in range(num_bars):
            self._smooth_bins[i] += (bins[i] - self._smooth_bins[i]) * smoothing

        # Draw bars
        angle_step = (2 * math.pi) / num_bars
        bar_angle = angle_step * 0.7  # leave small gap

        for i in range(num_bars):
            mag = min(1.0, self._smooth_bins[i] * bar_gain)
            bar_len = (max_r - inner_r) * mag
            angle = i * angle_step - math.pi / 2  # start from top

            # Colour interpolation
            t = i / max(1, num_bars - 1)
            col = col_low.lerp(col_high, t)

            # Bar as two line endpoints
            cos_a = math.cos(angle)
            sin_a = math.sin(angle)
            x1 = cx + inner_r * cos_a
            y1 = cy + inner_r * sin_a
            x2 = cx + (inner_r + bar_len) * cos_a
            y2 = cy + (inner_r + bar_len) * sin_a

            thickness = max(1.0, (2 * math.pi * inner_r / num_bars) * 0.6)
            ctx.draw_line(x1, y1, x2, y2, col, thickness)

            # Mirror (inward)
            if mirror and bar_len > 0:
                mx2 = cx + max(0, inner_r - bar_len * 0.4) * cos_a
                my2 = cy + max(0, inner_r - bar_len * 0.4) * sin_a
                ctx.draw_line(x1, y1, mx2, my2, col.with_alpha(0.4), thickness * 0.5)

        # Centre circle
        ctx.fill_ellipse(cx, cy, inner_r * 0.3, inner_r * 0.3, bg)
        ctx.stroke_ellipse(cx, cy, inner_r * 0.3, inner_r * 0.3,
                           col_low.with_alpha(0.3), 1.0)

    def _map_spectrum_to_bars(self, spectrum: list[float], num_bars: int) -> list[float]:
        """Map linear spectrum bins to *num_bars* using logarithmic distribution."""
        if not spectrum:
            return [0.0] * num_bars

        n = len(spectrum)
        result = []
        for i in range(num_bars):
            # Log-spaced band boundaries
            lo = int(n * (2 ** (i / num_bars * 10) - 1) / (2 ** 10 - 1))
            hi = int(n * (2 ** ((i + 1) / num_bars * 10) - 1) / (2 ** 10 - 1))
            lo = max(0, min(n - 1, lo))
            hi = max(lo, min(n - 1, hi))

            band = spectrum[lo : hi + 1]
            avg = sum(band) / len(band) if band else 0.0
            result.append(avg)

        return result
