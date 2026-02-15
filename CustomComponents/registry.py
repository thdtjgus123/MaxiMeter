"""
ComponentRegistry — discovers, loads, validates, and manages custom components.

The registry scans a ``plugins/`` directory for Python files containing
classes that inherit from ``BaseComponent``.  It validates each component's
manifest and exposes them to the C++ host through a simple JSON-based IPC
protocol.

Typical host workflow:
    1. ``registry = ComponentRegistry("plugins/")``
    2. ``registry.scan()``                         — discover all plugins
    3. ``registry.get_manifest_list()``            — list for TOOLBOX
    4. ``inst = registry.create("com.example.x")`` — instantiate by id
    5. Each frame: call ``inst.on_render(ctx, audio)``
"""

from __future__ import annotations

import importlib
import importlib.util
import inspect
import json
import logging
import os
import sys
import traceback
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Type

from .base_component import BaseComponent
from .manifest import Manifest
from .dependencies import ensure_packages, check_plugin_dependencies

logger = logging.getLogger("maximeter.registry")


@dataclass
class PluginInfo:
    """Metadata about a discovered plugin module."""

    module_path: str                    # absolute .py file path
    module_name: str                    # Python module name
    class_name: str                     # class within the module
    manifest: Manifest                  # validated manifest
    component_class: Type[BaseComponent] = field(repr=False, default=None)  # type: ignore[assignment]
    load_error: Optional[str] = None    # error message if loading failed
    enabled: bool = True

    @property
    def id(self) -> str:
        return self.manifest.id


class ComponentRegistry:
    """Discover, validate, and manage custom component plugins."""

    def __init__(self, plugins_dir: str | Path):
        self._plugins_dir = Path(plugins_dir).resolve()
        self._plugins: Dict[str, PluginInfo] = {}   # manifest.id → PluginInfo
        self._instances: Dict[str, BaseComponent] = {}  # instance_id → live instance

    # ── Discovery ───────────────────────────────────────────────────────────

    def scan(self) -> List[PluginInfo]:
        """Scan the plugins directory and register all valid components.

        Returns list of all PluginInfo (including ones with load errors).
        """
        self._plugins.clear()

        if not self._plugins_dir.exists():
            logger.warning("Plugins directory does not exist: %s", self._plugins_dir)
            return []

        # Scan both .py files at the top level and packages (dirs with __init__.py)
        entries: List[Path] = []

        for item in sorted(self._plugins_dir.iterdir()):
            if item.is_file() and item.suffix == ".py" and item.name != "__init__.py":
                entries.append(item)
            elif item.is_dir() and (item / "__init__.py").exists():
                entries.append(item / "__init__.py")

        for entry in entries:
            self._try_load_module(entry)

        logger.info("Scan complete: %d plugins found (%d valid)",
                     len(self._plugins),
                     sum(1 for p in self._plugins.values() if p.load_error is None))
        return list(self._plugins.values())

    def _try_load_module(self, path: Path):
        """Attempt to load a single plugin module and register its components."""
        mod_name = f"maximeter_plugin_{path.stem}"
        try:
            spec = importlib.util.spec_from_file_location(mod_name, str(path))
            if spec is None or spec.loader is None:
                logger.warning("Cannot create module spec for %s", path)
                return

            module = importlib.util.module_from_spec(spec)
            sys.modules[mod_name] = module
            spec.loader.exec_module(module)

            # Find all BaseComponent subclasses in the module
            for name, obj in inspect.getmembers(module, inspect.isclass):
                if (
                    issubclass(obj, BaseComponent)
                    and obj is not BaseComponent
                    and not inspect.isabstract(obj)
                ):
                    self._register_class(obj, name, path)

        except Exception:
            err = traceback.format_exc()
            logger.error("Failed to load plugin %s:\n%s", path, err)
            # Register with error so UI can show diagnostic
            info = PluginInfo(
                module_path=str(path),
                module_name=mod_name,
                class_name="?",
                manifest=Manifest(id=f"error.{path.stem}", name=path.stem),
                load_error=err,
            )
            self._plugins[info.id] = info

    def _register_class(self, cls: Type[BaseComponent], cls_name: str, path: Path):
        """Validate and register a single component class."""
        try:
            manifest = cls.get_manifest()
            if not isinstance(manifest, Manifest):
                raise TypeError(f"{cls_name}.get_manifest() must return a Manifest instance")

            if manifest.id in self._plugins:
                logger.warning(
                    "Duplicate manifest id '%s' — %s overrides %s",
                    manifest.id,
                    path,
                    self._plugins[manifest.id].module_path,
                )

            info = PluginInfo(
                module_path=str(path),
                module_name=cls.__module__ or "",
                class_name=cls_name,
                manifest=manifest,
                component_class=cls,
            )
            self._plugins[manifest.id] = info
            logger.info("Registered: %s (%s) from %s", manifest.name, manifest.id, path.name)

        except Exception:
            err = traceback.format_exc()
            logger.error("Failed to register %s from %s:\n%s", cls_name, path, err)
            info = PluginInfo(
                module_path=str(path),
                module_name=cls.__module__ or "",
                class_name=cls_name,
                manifest=Manifest(id=f"error.{cls_name}", name=cls_name),
                load_error=err,
            )
            self._plugins[info.id] = info

    # ── Querying ────────────────────────────────────────────────────────────

    def get_all_plugins(self) -> List[PluginInfo]:
        """Return all registered plugins (including errored ones)."""
        return list(self._plugins.values())

    def get_valid_plugins(self) -> List[PluginInfo]:
        """Return only successfully loaded and enabled plugins."""
        return [p for p in self._plugins.values() if p.load_error is None and p.enabled]

    def get_plugin(self, manifest_id: str) -> Optional[PluginInfo]:
        """Look up a plugin by manifest id."""
        return self._plugins.get(manifest_id)

    def get_manifest_list(self) -> List[Dict[str, Any]]:
        """Return serialised manifest list for the TOOLBOX UI.

        Each dict has: id, name, category, default_size, description, author, tags.
        """
        result = []
        for plugin in self.get_valid_plugins():
            m = plugin.manifest
            result.append({
                "id": m.id,
                "name": m.name,
                "category": m.category.name,
                "default_size": list(m.default_size),
                "description": m.description,
                "author": m.author,
                "version": m.version,
                "tags": list(m.tags),
                "icon": m.icon,
                "source_file": Path(plugin.module_path).stem,
            })
        return result

    # ── Instance management ─────────────────────────────────────────────────

    def create(self, manifest_id: str, instance_id: Optional[str] = None) -> Optional[BaseComponent]:
        """Create a new instance of a custom component by manifest id.

        Args:
            manifest_id: The ``Manifest.id`` of the component.
            instance_id: Optional unique id to track this instance.
                        If None, one is generated.

        Returns:
            The new component instance, or None if the plugin is not found.
        """
        plugin = self._plugins.get(manifest_id)
        if plugin is None:
            raise ValueError(f"Plugin '{manifest_id}' not found")
        
        if plugin.load_error is not None:
             raise RuntimeError(f"Plugin '{manifest_id}' failed to load: {plugin.load_error}")
             
        if plugin.component_class is None:
             raise RuntimeError(f"Plugin '{manifest_id}' has no valid component class")

        try:
            # Auto-install required packages if specified in manifest
            if plugin.manifest.requires:
                missing = check_plugin_dependencies(plugin.manifest)
                if missing:
                    logger.info("Installing missing dependencies for %s: %s",
                                manifest_id, missing)
                    results = ensure_packages(missing, auto_install=True)
                    failed = [pkg for pkg, ok in results.items() if not ok]
                    if failed:
                        logger.error("Failed to install dependencies: %s", failed)
                        raise ImportError(f"Failed to install dependencies: {', '.join(failed)}")

            instance = plugin.component_class()
            instance._init_defaults()
            instance.on_init()

            if instance_id is None:
                import uuid
                instance_id = str(uuid.uuid4())
            self._instances[instance_id] = instance

            logger.info("Created instance %s of %s", instance_id, manifest_id)
            return instance

        except Exception as e:
            # If it's already one of our known errors, just re-raise
            if isinstance(e, (ValueError, RuntimeError, ImportError)):
                raise e
            
            # Otherwise log and wrap the runtime error from the plugin code
            logger.error("Error creating instance of '%s':\n%s",
                         manifest_id, traceback.format_exc())
            raise RuntimeError(f"Error initializing '{manifest_id}': {e}")

    def get_instance(self, instance_id: str) -> Optional[BaseComponent]:
        """Retrieve a live instance by its id."""
        return self._instances.get(instance_id)

    def destroy_instance(self, instance_id: str):
        """Destroy and remove an instance."""
        inst = self._instances.pop(instance_id, None)
        if inst is not None:
            try:
                inst.on_destroy()
            except Exception:
                logger.error("Error in on_destroy for %s:\n%s",
                             instance_id, traceback.format_exc())

    def destroy_all(self):
        """Destroy all live instances."""
        for iid in list(self._instances.keys()):
            self.destroy_instance(iid)

    # ── Hot reload ──────────────────────────────────────────────────────────

    def reload_plugin(self, manifest_id: str) -> bool:
        """Hot-reload a single plugin from disk.

        Existing instances of the old class are NOT automatically migrated.
        Returns True if reload succeeded.
        """
        plugin = self._plugins.get(manifest_id)
        if plugin is None:
            return False

        path = Path(plugin.module_path)
        mod_name = plugin.module_name

        # Remove old module
        sys.modules.pop(mod_name, None)

        # Re-scan just this one file
        old_plugins = dict(self._plugins)
        self._try_load_module(path)

        new_plugin = self._plugins.get(manifest_id)
        if new_plugin and new_plugin.load_error is None:
            logger.info("Hot-reloaded: %s", manifest_id)
            return True

        # Restore old entry if reload failed
        if manifest_id in old_plugins:
            self._plugins[manifest_id] = old_plugins[manifest_id]
        return False

    # ── Serialisation ───────────────────────────────────────────────────────

    def serialise_instances(self) -> Dict[str, Any]:
        """Serialise all live instances to a dict (for project save)."""
        return {
            iid: inst.serialise()
            for iid, inst in self._instances.items()
        }

    def deserialise_instances(self, data: Dict[str, Any]):
        """Restore instances from serialised data (project load)."""
        for iid, inst_data in data.items():
            mid = inst_data.get("manifest_id")
            if not mid:
                continue
            instance = self.create(mid, instance_id=iid)
            if instance:
                instance.deserialise(inst_data)

    def to_json(self) -> str:
        """Export manifest list as JSON string (for C++ IPC)."""
        return json.dumps(self.get_manifest_list(), indent=2)
