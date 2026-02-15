"""
Color utilities for the MaxiMeter custom component framework.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Tuple, List


@dataclass(frozen=True, slots=True)
class Color:
    """Immutable RGBA colour.

    Constructors:
        Color(r, g, b, a=255)          – from 0-255 components
        Color.from_hex(0xFF3A7BFF)     – from 32-bit ARGB integer
        Color.from_hex_str("#3A7BFF")  – from CSS-style hex string
    """

    r: int = 0
    g: int = 0
    b: int = 0
    a: int = 255

    # ── Alternate constructors ──────────────────────────────────────────────

    @classmethod
    def from_hex(cls, argb: int) -> "Color":
        """Create from 0xAARRGGBB integer."""
        return cls(
            r=(argb >> 16) & 0xFF,
            g=(argb >> 8) & 0xFF,
            b=argb & 0xFF,
            a=(argb >> 24) & 0xFF,
        )

    @classmethod
    def from_hex_str(cls, s: str) -> "Color":
        """Create from '#RRGGBB' or '#AARRGGBB' string."""
        s = s.lstrip("#")
        if len(s) == 6:
            return cls(int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16))
        elif len(s) == 8:
            return cls(
                int(s[2:4], 16),
                int(s[4:6], 16),
                int(s[6:8], 16),
                int(s[0:2], 16),
            )
        raise ValueError(f"Invalid hex colour string: #{s}")

    # ── Helpers ─────────────────────────────────────────────────────────────

    def with_alpha(self, a: int | float) -> "Color":
        """Return a copy with modified alpha. Float 0.0-1.0 or int 0-255."""
        if isinstance(a, float):
            a = int(a * 255)
        return Color(self.r, self.g, self.b, max(0, min(255, a)))

    def to_tuple(self) -> Tuple[int, int, int, int]:
        return (self.r, self.g, self.b, self.a)

    def to_argb(self) -> int:
        return (self.a << 24) | (self.r << 16) | (self.g << 8) | self.b

    def lerp(self, other: "Color", t: float) -> "Color":
        """Linear interpolation between two colours."""
        t = max(0.0, min(1.0, t))
        return Color(
            r=int(self.r + (other.r - self.r) * t),
            g=int(self.g + (other.g - self.g) * t),
            b=int(self.b + (other.b - self.b) * t),
            a=int(self.a + (other.a - self.a) * t),
        )

    def __repr__(self) -> str:
        return f"Color(#{self.a:02X}{self.r:02X}{self.g:02X}{self.b:02X})"

    # ── Predefined colours ──────────────────────────────────────────────────

    @staticmethod
    def white() -> "Color":
        return Color(255, 255, 255)

    @staticmethod
    def black() -> "Color":
        return Color(0, 0, 0)

    @staticmethod
    def transparent() -> "Color":
        return Color(0, 0, 0, 0)

    @staticmethod
    def red() -> "Color":
        return Color(255, 0, 0)

    @staticmethod
    def green() -> "Color":
        return Color(0, 255, 0)

    @staticmethod
    def blue() -> "Color":
        return Color(0, 0, 255)

    @staticmethod
    def yellow() -> "Color":
        return Color(255, 255, 0)

    @staticmethod
    def cyan() -> "Color":
        return Color(0, 255, 255)

    @staticmethod
    def magenta() -> "Color":
        return Color(255, 0, 255)


@dataclass(frozen=True, slots=True)
class GradientStop:
    """A single colour stop in a gradient."""

    position: float  # 0.0 to 1.0
    color: Color = field(default_factory=Color.white)


@dataclass(frozen=True, slots=True)
class Gradient:
    """Linear or radial gradient definition.

    Usage:
        grad = Gradient.linear(0, 0, 0, 100, [
            GradientStop(0.0, Color.red()),
            GradientStop(1.0, Color.blue()),
        ])
    """

    stops: Tuple[GradientStop, ...] = ()
    # Linear: (x1, y1) → (x2, y2)
    x1: float = 0.0
    y1: float = 0.0
    x2: float = 0.0
    y2: float = 0.0
    is_radial: bool = False

    @classmethod
    def linear(
        cls,
        x1: float,
        y1: float,
        x2: float,
        y2: float,
        stops: List[GradientStop],
    ) -> "Gradient":
        return cls(stops=tuple(stops), x1=x1, y1=y1, x2=x2, y2=y2, is_radial=False)

    @classmethod
    def radial(
        cls,
        cx: float,
        cy: float,
        radius: float,
        stops: List[GradientStop],
    ) -> "Gradient":
        return cls(
            stops=tuple(stops),
            x1=cx,
            y1=cy,
            x2=cx + radius,
            y2=cy,
            is_radial=True,
        )

    def color_at(self, t: float) -> Color:
        """Sample the gradient colour at position *t* (0.0–1.0)."""
        if not self.stops:
            return Color.white()
        t = max(0.0, min(1.0, t))
        prev = self.stops[0]
        for stop in self.stops:
            if stop.position >= t:
                if stop.position == prev.position:
                    return stop.color
                frac = (t - prev.position) / (stop.position - prev.position)
                return prev.color.lerp(stop.color, frac)
            prev = stop
        return self.stops[-1].color
