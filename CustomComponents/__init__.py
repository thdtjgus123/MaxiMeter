"""
MaxiMeter Custom Component Framework
=====================================

Python framework for creating custom audio visualization components
that integrate with the MaxiMeter canvas editor and TOOLBOX.

Architecture Overview:
    - BaseComponent: Abstract base class all custom components must inherit from.
    - RenderContext: Drawing API (rects, circles, lines, text, gradients, images).
    - AudioData:     Read-only snapshot of current audio analysis data.
    - ComponentRegistry: Discovers, validates, and manages custom components.
    - PropertyDescriptor: Declarative system for user-editable properties.

Usage:
    1. Create a .py file in the `plugins/` directory.
    2. Define a class that inherits from `BaseComponent`.
    3. Implement required methods: `on_render()`, `get_manifest()`.
    4. The component will automatically appear in the TOOLBOX.

Example:
    from maximeter import BaseComponent, Manifest, Property

    class MyMeter(BaseComponent):
        @staticmethod
        def get_manifest():
            return Manifest(
                id="com.example.my_meter",
                name="My Meter",
                version="1.0.0",
                default_size=(300, 200),
            )

        def on_render(self, ctx, audio):
            ctx.clear(Color(0x16, 0x21, 0x3E))
            spectrum = audio.spectrum
            # ... draw visualisation ...
"""

__version__ = "0.1.0"

from .manifest import Manifest, Category
from .properties import Property, PropertyType, PropertyGroup
from .color import Color, Gradient, GradientStop
from .render_context import RenderContext, BlendMode, TextAlign, Font
from .audio_data import AudioData, ChannelData
from .base_component import BaseComponent, ComponentState
from .registry import ComponentRegistry
from .shared_memory import AudioSharedMemoryReader
from .dependencies import ensure_packages, get_numpy, is_available

__all__ = [
    # Core
    "BaseComponent",
    "ComponentState",
    "Manifest",
    "Category",
    # Properties
    "Property",
    "PropertyType",
    "PropertyGroup",
    # Rendering
    "RenderContext",
    "BlendMode",
    "TextAlign",
    "Font",
    "Color",
    "Gradient",
    "GradientStop",
    # Audio
    "AudioData",
    "ChannelData",
    # Registry
    "ComponentRegistry",
    # Performance
    "AudioSharedMemoryReader",
    # Dependencies
    "ensure_packages",
    "get_numpy",
    "is_available",
]
