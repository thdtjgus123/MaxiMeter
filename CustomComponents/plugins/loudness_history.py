"""
Example: Loudness History Graph
Plots LUFS loudness over time as a scrolling line graph.
"""

from __future__ import annotations

from collections import deque

from CustomComponents import (
    BaseComponent, Manifest, Category,
    Property, PropertyType,
    RenderContext, AudioData,
    Color, Font, TextAlign,
)


class LoudnessHistory(BaseComponent):
    """Scrolling LUFS loudness graph with target reference line."""

    @staticmethod
    def get_manifest() -> Manifest:
        return Manifest(
            id="com.maximeter.example.loudness_history",
            name="Loudness History",
            version="1.0.0",
            author="MaxiMeter",
            description="Scrolling line graph of EBU R128 momentary / short-term loudness.",
            category=Category.METER,
            default_size=(500, 180),
            min_size=(200, 80),
            tags=("loudness", "lufs", "history", "graph", "example"),
        )

    def get_properties(self):
        return [
            Property("mode", "Loudness Mode", PropertyType.ENUM,
                     default="momentary",
                     choices=[("momentary", "Momentary"), ("short_term", "Short-Term")],
                     group="Display"),
            Property("target_lufs", "Target LUFS", PropertyType.RANGE,
                     default=-14.0, min_value=-30.0, max_value=0.0, step=0.5,
                     group="Display"),
            Property("range_db", "Display Range (dB)", PropertyType.RANGE,
                     default=40.0, min_value=20.0, max_value=80.0, step=5.0,
                     group="Display"),
            Property("history_seconds", "History (s)", PropertyType.RANGE,
                     default=30.0, min_value=5.0, max_value=120.0, step=5.0,
                     group="Display"),
            Property("line_color", "Line Colour", PropertyType.COLOR,
                     default=Color(0x00, 0xFF, 0x88), group="Colour"),
            Property("target_color", "Target Line", PropertyType.COLOR,
                     default=Color(0xFF, 0xCC, 0x00), group="Colour"),
            Property("bg_color", "Background", PropertyType.COLOR,
                     default=Color(0x0E, 0x0E, 0x18), group="Colour"),
        ]

    def on_init(self):
        self._history: deque[float] = deque(maxlen=1800)  # 30s * 60fps

    def on_render(self, ctx: RenderContext, audio: AudioData):
        mode = self.get_property("mode", "momentary")
        target = self.get_property("target_lufs", -14.0)
        range_db = self.get_property("range_db", 40.0)
        line_col = self.get_property("line_color", Color(0x00, 0xFF, 0x88))
        target_col = self.get_property("target_color", Color(0xFF, 0xCC, 0x00))
        bg = self.get_property("bg_color", Color(0x0E, 0x0E, 0x18))
        hist_sec = self.get_property("history_seconds", 30.0)

        # Update max history length
        max_len = int(hist_sec * 60)
        if self._history.maxlen != max_len:
            old = list(self._history)
            self._history = deque(old[-max_len:], maxlen=max_len)

        # Sample current loudness
        lufs = audio.lufs_momentary if mode == "momentary" else audio.lufs_short_term
        self._history.append(lufs)

        ctx.clear(bg)
        w, h = ctx.width, ctx.height
        margin_l, margin_r, margin_t, margin_b = 45, 10, 10, 20
        gx = margin_l
        gy = margin_t
        gw = w - margin_l - margin_r
        gh = h - margin_t - margin_b

        # Graph background
        ctx.fill_rounded_rect(gx, gy, gw, gh, 4, Color(20, 20, 30))

        ceil_db = target + range_db / 2
        floor_db = target - range_db / 2

        def db_to_y(db: float) -> float:
            norm = (db - floor_db) / (ceil_db - floor_db)
            return gy + gh * (1.0 - max(0.0, min(1.0, norm)))

        # Grid lines
        step = 6 if range_db <= 40 else 12
        db = int(floor_db // step) * step
        while db <= ceil_db:
            y = db_to_y(float(db))
            if gy <= y <= gy + gh:
                ctx.draw_line(gx, y, gx + gw, y, Color(255, 255, 255, 20), 1.0)
                ctx.draw_text(f"{db}", 0, y - 6, margin_l - 4, 12,
                              Color(140, 140, 140), Font(size=9.0), TextAlign.RIGHT)
            db += step

        # Target line
        ty = db_to_y(target)
        ctx.draw_line(gx, ty, gx + gw, ty, target_col.with_alpha(0.6), 1.5)
        ctx.draw_text(f"{target:.0f}", 0, ty - 6, margin_l - 4, 12,
                      target_col, Font(size=9.0, bold=True), TextAlign.RIGHT)

        # Data line
        n = len(self._history)
        if n > 1:
            points = []
            for i, val in enumerate(self._history):
                px = gx + gw * (i / (n - 1))
                py = db_to_y(val)
                points.append((px, py))

            ctx.set_clip(gx, gy, gw, gh)
            ctx.draw_path(points, line_col, 1.5)
            ctx.reset_clip()

        # Current value label
        current = self._history[-1] if self._history else -100.0
        label = f"{current:.1f} LUFS" if current > -100 else "--- LUFS"
        ctx.draw_text(label, gx + 6, gy + 4, gw, 14,
                      line_col, Font(size=11.0, bold=True))
