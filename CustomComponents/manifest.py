"""
Manifest descriptor for custom components.

Every custom component must provide a static `get_manifest()` method returning
a `Manifest` instance.  The manifest tells the host app how to present the
component in the TOOLBOX and what its default configuration looks like.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Tuple, Optional, Sequence


class Category(Enum):
    """Logical grouping shown in the TOOLBOX sidebar."""

    METER = auto()         # Audio level / loudness meters
    ANALYZER = auto()      # Spectrum, FFT-based visualisations
    SCOPE = auto()         # Oscilloscope / Lissajous / Goniometer
    STEREO = auto()        # Stereo field visualisations
    DECORATION = auto()    # Non-audio visual elements (shapes, images)
    UTILITY = auto()       # Utility / helper widgets
    VISUALIZER = auto()    # GPU-accelerated visual effects (shaders, particles)
    OTHER = auto()


@dataclass(frozen=True)
class Manifest:
    """Describes a custom component to the host.

    Attributes:
        id:            Reverse-domain unique identifier (e.g. "com.example.my_meter").
        name:          Human-readable display name shown in the TOOLBOX.
        version:       Semantic version string (e.g. "1.0.0").
        author:        Author name (optional).
        description:   One-line description shown as tooltip (optional).
        category:      TOOLBOX grouping category.
        default_size:  Default (width, height) when first placed on canvas.
        min_size:      Minimum allowed resize (width, height).
        max_size:      Maximum allowed resize (width, height) — None = unlimited.
        icon:          Icon identifier or path (optional, for future use).
        tags:          Searchable tags for filtering in the TOOLBOX.
        url:           Project / documentation URL (optional).
        requires:      Python package dependencies (e.g. ("numpy", "scipy")).
                       These will be auto-installed if missing.
    """

    id: str
    name: str
    version: str = "1.0.0"
    author: str = ""
    description: str = ""
    category: Category = Category.OTHER
    default_size: Tuple[int, int] = (300, 200)
    min_size: Tuple[int, int] = (50, 50)
    max_size: Optional[Tuple[int, int]] = None
    icon: str = ""
    tags: Tuple[str, ...] = ()
    url: str = ""
    requires: Tuple[str, ...] = ()

    def __post_init__(self):
        if not self.id or not self.name:
            raise ValueError("Manifest 'id' and 'name' are required.")
        parts = self.id.split(".")
        if len(parts) < 2:
            raise ValueError(
                f"Manifest id must be reverse-domain style (e.g. 'com.example.my_meter'), "
                f"got '{self.id}'"
            )
