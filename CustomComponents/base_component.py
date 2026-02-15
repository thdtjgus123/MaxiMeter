"""
BaseComponent — abstract base class for all custom components.

Lifecycle:
    1. ``__init__()``    — Host creates instance (do minimal work here).
    2. ``on_init()``     — Called once after creation; load resources here.
    3. ``on_render()``   — Called every frame (~60 fps) with RenderContext + AudioData.
    4. ``on_resize()``   — Called when the component is resized on the canvas.
    5. ``on_property_changed()`` — Called when a property value changes in the panel.
    6. ``on_destroy()``  — Called when the item is removed from the canvas.
"""

from __future__ import annotations

import abc
import time
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional

from .audio_data import AudioData
from .color import Color
from .manifest import Manifest
from .properties import Property
from .render_context import RenderContext


@dataclass
class ComponentState:
    """Mutable key–value state bag persisted with the project.

    Use this instead of bare instance attributes so the host can
    serialise/restore your component's state when saving/loading projects.

    Example::

        def on_init(self):
            self.state.set("smoothed_level", 0.0)

        def on_render(self, ctx, audio):
            level = self.state.get("smoothed_level", 0.0)
            level += (audio.mono_rms - level) * 0.1
            self.state.set("smoothed_level", level)
    """

    _data: Dict[str, Any] = field(default_factory=dict)

    def get(self, key: str, default: Any = None) -> Any:
        return self._data.get(key, default)

    def set(self, key: str, value: Any):
        self._data[key] = value

    def to_dict(self) -> Dict[str, Any]:
        return dict(self._data)

    def from_dict(self, d: Dict[str, Any]):
        self._data.update(d)


class BaseComponent(abc.ABC):
    """Abstract base class for custom MaxiMeter components.

    Subclass this and implement at minimum:

    * ``get_manifest()``   — static, returns a :class:`Manifest`.
    * ``on_render(ctx, audio)`` — draw your visualisation each frame.

    Optionally override the other lifecycle hooks as needed.
    """

    def __init__(self):
        self.state = ComponentState()
        self._properties: Dict[str, Any] = {}
        self._property_defs: Dict[str, Property] = {}
        self._image_cache: Dict[str, str] = {}  # key → file path (resolved by host)
        self._frame_count: int = 0
        self._last_time: float = time.perf_counter()
        self._fps: float = 0.0

    # ── Manifest (REQUIRED) ─────────────────────────────────────────────────

    @staticmethod
    @abc.abstractmethod
    def get_manifest() -> Manifest:
        """Return the component's manifest describing its identity and defaults."""
        ...

    # ── Properties (optional) ───────────────────────────────────────────────

    def get_properties(self) -> List[Property]:
        """Return list of user-editable properties.

        Override this to expose settings in the Property Panel.
        Default implementation returns an empty list.
        """
        return []

    def get_property(self, key: str, default: Any = None) -> Any:
        """Read the current value of a property."""
        return self._properties.get(key, default)

    def set_property(self, key: str, value: Any):
        """Called by host when user changes a property. Also callable from code."""
        # Coerce value through the Property's validate() so that, e.g.,
        # an INT property received as float from the JSON bridge is cast to int.
        prop_def = self._property_defs.get(key)
        if prop_def is not None:
            try:
                value = prop_def.validate(value)
            except (ValueError, TypeError):
                pass  # keep raw value on unexpected types
        self._properties[key] = value
        self.on_property_changed(key, value)

    def _init_defaults(self):
        """Populate properties with declared default values."""
        for prop in self.get_properties():
            self._property_defs[prop.key] = prop
            if prop.key not in self._properties:
                self._properties[prop.key] = prop.default

    # ── Lifecycle hooks ─────────────────────────────────────────────────────

    def on_init(self):
        """Called once after creation.  Load resources, set initial state."""
        pass

    @abc.abstractmethod
    def on_render(self, ctx: RenderContext, audio: AudioData):
        """Called every frame.  Draw your visualisation here.

        Args:
            ctx:   Drawing API — all coordinates in local component space.
            audio: Current audio analysis snapshot.
        """
        ...

    def on_resize(self, width: float, height: float):
        """Called when the component is resized on the canvas."""
        pass

    def on_property_changed(self, key: str, value: Any):
        """Called when a property value changes (from the panel or programmatically)."""
        pass

    def on_mouse_down(self, x: float, y: float, button: int = 0):
        """Called on mouse press (when interactive mode is active)."""
        pass

    def on_mouse_move(self, x: float, y: float):
        """Called on mouse move within the component."""
        pass

    def on_mouse_up(self, x: float, y: float, button: int = 0):
        """Called on mouse release."""
        pass

    def on_destroy(self):
        """Called when the component is removed from the canvas.  Clean up here."""
        pass

    # ── Utility helpers ─────────────────────────────────────────────────────

    def load_image(self, key: str, path: str):
        """Register an image for use in ``ctx.draw_image()``.

        Call this in ``on_init()``.  *path* is relative to the plugin directory.
        """
        self._image_cache[key] = path

    def get_images(self) -> Dict[str, str]:
        """Return the image cache (used by host for loading)."""
        return dict(self._image_cache)

    @property
    def fps(self) -> float:
        """Approximate frames-per-second (updated every 30 frames)."""
        return self._fps

    def _tick_fps(self):
        """Internal FPS tracking — called by host each frame."""
        self._frame_count += 1
        if self._frame_count % 30 == 0:
            now = time.perf_counter()
            elapsed = now - self._last_time
            self._fps = 30.0 / elapsed if elapsed > 0 else 0.0
            self._last_time = now

    # ── Serialisation helpers ───────────────────────────────────────────────

    def serialise(self) -> Dict[str, Any]:
        """Serialise component state + property values to a dict."""
        return {
            "manifest_id": self.get_manifest().id,
            "properties": dict(self._properties),
            "state": self.state.to_dict(),
        }

    def deserialise(self, data: Dict[str, Any]):
        """Restore component from serialised data."""
        if "properties" in data:
            self._properties.update(data["properties"])
        if "state" in data:
            self.state.from_dict(data["state"])
