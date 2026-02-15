"""
Declarative property system for custom component settings.

Properties declared in `get_properties()` are automatically surfaced in the
Property Panel when a custom component is selected on the canvas.  The host
serialises / deserialises property values with the project file.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Any, Dict, List, Optional, Sequence, Tuple, Union

from .color import Color


class PropertyType(Enum):
    """Supported property editor types."""

    INT = auto()
    FLOAT = auto()
    BOOL = auto()
    STRING = auto()
    COLOR = auto()         # opens colour picker
    ENUM = auto()          # drop-down with choices
    FILE_PATH = auto()     # file browser dialog
    RANGE = auto()         # slider (float) with min/max/step
    POINT = auto()         # (x, y) pair
    SIZE = auto()          # (w, h) pair


@dataclass
class Property:
    """Describes a single editable property.

    Attributes:
        key:          Unique identifier within this component (used for storage).
        label:        Human-readable label in the Property Panel.
        type:         Widget type to use in the panel.
        default:      Default value (must match *type*).
        description:  Tooltip text (optional).
        group:        Logical group name (for collapsible sections).
        min_value:    Minimum (for INT / FLOAT / RANGE).
        max_value:    Maximum (for INT / FLOAT / RANGE).
        step:         Step size (for RANGE).
        choices:      List of (value, label) pairs (for ENUM).
        file_filter:  Glob patterns for FILE_PATH (e.g. "*.png;*.jpg").
        visible_when: Dict mapping another property key → required value
                     for this property to be visible (simple conditional visibility).
    """

    key: str
    label: str
    type: PropertyType = PropertyType.STRING
    default: Any = None
    description: str = ""
    group: str = ""
    min_value: Optional[float] = None
    max_value: Optional[float] = None
    step: Optional[float] = None
    choices: Optional[List[Tuple[Any, str]]] = None
    file_filter: str = ""
    visible_when: Optional[Dict[str, Any]] = None

    def validate(self, value: Any) -> Any:
        """Validate and coerce *value* to the expected Python type.

        Raises ValueError on incompatible values.
        """
        if value is None:
            return self.default

        if self.type == PropertyType.INT:
            value = int(value)
            if self.min_value is not None:
                value = max(int(self.min_value), value)
            if self.max_value is not None:
                value = min(int(self.max_value), value)
            return value

        if self.type == PropertyType.FLOAT or self.type == PropertyType.RANGE:
            value = float(value)
            if self.min_value is not None:
                value = max(self.min_value, value)
            if self.max_value is not None:
                value = min(self.max_value, value)
            return value

        if self.type == PropertyType.BOOL:
            return bool(value)

        if self.type == PropertyType.STRING:
            return str(value)

        if self.type == PropertyType.COLOR:
            if isinstance(value, Color):
                return value
            if isinstance(value, int):
                return Color.from_hex(value)
            if isinstance(value, str):
                return Color.from_hex_str(value)
            raise ValueError(f"Cannot convert {type(value)} to Color")

        if self.type == PropertyType.ENUM:
            valid = [c[0] for c in (self.choices or [])]
            if value not in valid:
                raise ValueError(f"Invalid choice '{value}'; expected one of {valid}")
            return value

        if self.type in (PropertyType.POINT, PropertyType.SIZE):
            if isinstance(value, (list, tuple)) and len(value) == 2:
                return (float(value[0]), float(value[1]))
            raise ValueError(f"Expected (x,y) tuple, got {value}")

        return value


@dataclass
class PropertyGroup:
    """A named group of related properties (rendered as collapsible section)."""

    name: str
    properties: List[Property] = field(default_factory=list)

    def add(self, prop: Property) -> "PropertyGroup":
        self.properties.append(prop)
        return self

    def find(self, key: str) -> Optional[Property]:
        for p in self.properties:
            if p.key == key:
                return p
        return None


def group_properties(props: Sequence[Property]) -> List[PropertyGroup]:
    """Group a flat list of properties by their ``group`` field."""
    groups: Dict[str, PropertyGroup] = {}
    for p in props:
        name = p.group or "General"
        if name not in groups:
            groups[name] = PropertyGroup(name=name)
        groups[name].properties.append(p)
    return list(groups.values())
