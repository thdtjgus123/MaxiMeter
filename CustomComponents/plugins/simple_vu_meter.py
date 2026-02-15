"""
Example: Simple VU Meter bar
A minimal custom component that renders a stereo VU meter with peak hold.
"""

from __future__ import annotations

import math

# Adjust import path — plugins are loaded by the registry which already
# has CustomComponents on sys.path.
from CustomComponents import (
    BaseComponent, Manifest, Category,
    Property, PropertyType,
    RenderContext, AudioData,
    Color, Gradient, GradientStop,
    Font, TextAlign,
)


class SimpleVUMeter(BaseComponent):
    """Stereo VU bar meter with customisable colours and peak hold."""

    @staticmethod
    def get_manifest() -> Manifest:
        return Manifest(
            id="com.maximeter.example.simple_vu",
            name="Simple VU Meter",
            version="1.0.0",
            author="MaxiMeter",
            description="A basic stereo VU bar meter with peak indicators.",
            category=Category.METER,
            default_size=(200, 300),
            min_size=(80, 120),
            tags=("vu", "meter", "level", "example"),
        )

    def get_properties(self):
        return [
            Property("bar_color", "Bar Colour", PropertyType.COLOR,
                     default=Color(0x3A, 0x7B, 0xFF), group="Appearance"),
            Property("peak_color", "Peak Colour", PropertyType.COLOR,
                     default=Color(0xFF, 0x44, 0x44), group="Appearance"),
            Property("bg_color", "Background", PropertyType.COLOR,
                     default=Color(0x12, 0x12, 0x1A), group="Appearance"),
            Property("show_scale", "Show dB Scale", PropertyType.BOOL,
                     default=True, group="Display"),
            Property("peak_hold_ms", "Peak Hold (ms)", PropertyType.RANGE,
                     default=2000.0, min_value=100, max_value=10000, step=100,
                     group="Behaviour"),
            Property("smoothing", "Smoothing", PropertyType.RANGE,
                     default=0.15, min_value=0.01, max_value=0.5, step=0.01,
                     group="Behaviour"),
        ]

    def on_init(self):
        self.state.set("smooth_l", 0.0)
        self.state.set("smooth_r", 0.0)
        self.state.set("peak_l", 0.0)
        self.state.set("peak_r", 0.0)
        self.state.set("peak_l_time", 0.0)
        self.state.set("peak_r_time", 0.0)
        self._frame = 0

    def on_render(self, ctx: RenderContext, audio: AudioData):
        bg = self.get_property("bg_color", Color(0x12, 0x12, 0x1A))
        bar_col = self.get_property("bar_color", Color(0x3A, 0x7B, 0xFF))
        peak_col = self.get_property("peak_color", Color(0xFF, 0x44, 0x44))
        show_scale = self.get_property("show_scale", True)
        smoothing = self.get_property("smoothing", 0.15)

        ctx.clear(bg)

        w, h = ctx.width, ctx.height
        margin = 10
        bar_gap = 6
        bar_w = (w - margin * 2 - bar_gap) / 2
        bar_top = margin + (20 if show_scale else 0)
        bar_h = h - bar_top - margin

        # Smooth levels
        smooth_l = self.state.get("smooth_l", 0.0)
        smooth_r = self.state.get("smooth_r", 0.0)
        target_l = audio.left.rms_linear
        target_r = audio.right.rms_linear
        smooth_l += (target_l - smooth_l) * smoothing
        smooth_r += (target_r - smooth_r) * smoothing
        self.state.set("smooth_l", smooth_l)
        self.state.set("smooth_r", smooth_r)

        # Peak hold
        peak_l = self.state.get("peak_l", 0.0)
        peak_r = self.state.get("peak_r", 0.0)
        if smooth_l > peak_l:
            peak_l = smooth_l
            self.state.set("peak_l_time", self._frame)
        elif self._frame - self.state.get("peak_l_time", 0) > 120:  # ~2s at 60fps
            peak_l *= 0.95
        if smooth_r > peak_r:
            peak_r = smooth_r
            self.state.set("peak_r_time", self._frame)
        elif self._frame - self.state.get("peak_r_time", 0) > 120:
            peak_r *= 0.95
        self.state.set("peak_l", peak_l)
        self.state.set("peak_r", peak_r)
        self._frame += 1

        # Draw bars
        for i, (level, peak) in enumerate([(smooth_l, peak_l), (smooth_r, peak_r)]):
            bx = margin + i * (bar_w + bar_gap)
            fill_h = bar_h * min(1.0, level)

            # Bar background
            ctx.fill_rounded_rect(bx, bar_top, bar_w, bar_h, 3,
                                  bg.with_alpha(0.3) if isinstance(bg, Color) else Color(30, 30, 40))

            # Level fill (bottom-up)
            if fill_h > 0:
                ctx.fill_rounded_rect(
                    bx, bar_top + bar_h - fill_h,
                    bar_w, fill_h, 3, bar_col,
                )

            # Peak marker
            peak_y = bar_top + bar_h - bar_h * min(1.0, peak)
            ctx.fill_rect(bx, peak_y, bar_w, 2, peak_col)

        # Scale labels
        if show_scale:
            for db in [0, -6, -12, -24, -48]:
                linear = 10 ** (db / 20.0) if db > -100 else 0
                y_pos = bar_top + bar_h - bar_h * linear
                ctx.draw_text(
                    f"{db}", margin - 2, y_pos - 6, w - margin * 2, 12,
                    Color(180, 180, 180), Font(size=9.0), TextAlign.RIGHT,
                )

        # Channel labels
        ctx.draw_text("L", margin + bar_w * 0.5 - 4, h - margin + 2, 20, 12,
                      Color(180, 180, 180), Font(size=10.0))
        ctx.draw_text("R", margin + bar_w + bar_gap + bar_w * 0.5 - 4, h - margin + 2, 20, 12,
                      Color(180, 180, 180), Font(size=10.0))
