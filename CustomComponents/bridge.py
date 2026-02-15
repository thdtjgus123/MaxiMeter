"""
IPC bridge — communication protocol between the C++ host and the Python plugin runtime.

The bridge uses a JSON-line protocol over stdin/stdout:
    - Host → Python:  JSON messages with ``{ "type": "...", ... }``
    - Python → Host:  JSON responses with ``{ "type": "...", ... }``

This module implements the Python side of the bridge.  The C++ side spawns
a ``python bridge_runner.py`` subprocess and communicates via pipes.

Message types (Host → Python):
    scan             — scan plugins directory
    list             — get manifest list for TOOLBOX
    create           — create a component instance
    render           — request render commands for a frame
    set_property     — update a property value
    resize           — notify component of size change
    mouse_event      — forward mouse interaction
    destroy          — destroy an instance
    save             — serialize all instances
    load             — deserialize instances
    reload           — hot-reload a plugin
    shutdown         — graceful shutdown

Message types (Python → Host):
    scan_result      — list of PluginInfo
    manifest_list    — JSON manifest list
    created          — instance created (with property descriptors)
    render_commands  — serialised draw commands
    properties       — property descriptors for panel
    save_data        — serialised state
    error            — error message
    ok               — success acknowledgement
"""

from __future__ import annotations

import json
import logging
import sys
import traceback
from typing import Any, Callable, Dict, Optional

from .audio_data import AudioData, ChannelData
from .registry import ComponentRegistry
from .render_context import RenderContext
from .shared_memory import AudioSharedMemoryReader

logger = logging.getLogger("maximeter.bridge")


class BridgeProtocol:
    """JSON-line IPC protocol handler."""

    def __init__(self, registry: ComponentRegistry):
        self.registry = registry
        self._handlers: Dict[str, Callable] = {
            "scan": self._handle_scan,
            "list": self._handle_list,
            "create": self._handle_create,
            "render": self._handle_render,
            "set_property": self._handle_set_property,
            "resize": self._handle_resize,
            "mouse_event": self._handle_mouse_event,
            "destroy": self._handle_destroy,
            "save": self._handle_save,
            "load": self._handle_load,
            "reload": self._handle_reload,
            "shutdown": self._handle_shutdown,
        }
        self._running = True

        # Shared memory reader for zero-copy audio transport
        self._shm_reader = AudioSharedMemoryReader()
        self._shm_available = False

        # Per-instance error tracking for error overlay
        self._instance_errors: Dict[str, str] = {}  # instance_id → last error message

    # ── Main loop ───────────────────────────────────────────────────────────

    def run(self):
        """Read JSON messages from stdin, dispatch, write responses to stdout."""
        logger.info("Bridge protocol started")

        # Try to open shared memory for high-performance audio transport
        self._shm_available = self._shm_reader.open()
        if self._shm_available:
            logger.info("SharedMemory audio transport ACTIVE (zero-copy mode)")
        else:
            logger.info("SharedMemory not available, using JSON audio transport (fallback)")

        while self._running:
            try:
                line = sys.stdin.readline()
                if not line:
                    break  # EOF — host closed pipe

                msg = json.loads(line.strip())
                msg_type = msg.get("type", "")

                handler = self._handlers.get(msg_type)
                if handler:
                    response = handler(msg)
                else:
                    response = {"type": "error", "message": f"Unknown message type: {msg_type}"}

                if response:
                    self._send(response)

            except json.JSONDecodeError as e:
                self._send({"type": "error", "message": f"Invalid JSON: {e}"})
            except Exception as e:
                logger.exception("Bridge error")
                self._send({"type": "error", "message": str(e)})

        logger.info("Bridge protocol stopped")

    def _send(self, msg: Dict[str, Any]):
        """Write a JSON message to stdout."""
        try:
            line = json.dumps(msg, default=str)
            sys.stdout.write(line + "\n")
            sys.stdout.flush()
        except Exception:
            logger.exception("Failed to send message")

    # ── Handlers ────────────────────────────────────────────────────────────

    def _handle_scan(self, msg: Dict) -> Dict:
        plugins = self.registry.scan()
        return {
            "type": "scan_result",
            "count": len(plugins),
            "plugins": [
                {
                    "id": p.id,
                    "name": p.manifest.name,
                    "class": p.class_name,
                    "file": p.module_path,
                    "error": p.load_error,
                    "enabled": p.enabled,
                }
                for p in plugins
            ],
        }

    def _handle_list(self, msg: Dict) -> Dict:
        return {
            "type": "manifest_list",
            "manifests": self.registry.get_manifest_list(),
        }

    def _handle_create(self, msg: Dict) -> Dict:
        manifest_id = msg.get("manifest_id", "")
        instance_id = msg.get("instance_id")
        
        try:
            instance = self.registry.create(manifest_id, instance_id)
        except Exception as e:
            # Capture the full reason why creation failed so the host knows
            return {"type": "error", "message": str(e)}

        if instance is None:
             return {"type": "error", "message": f"Unknown error creating '{manifest_id}'"}

        # Return property descriptors so the host can build the property panel
        props = instance.get_properties()
        return {
            "type": "created",
            "instance_id": instance_id,
            "manifest_id": manifest_id,
            "properties": [
                {
                    "key": p.key,
                    "label": p.label,
                    "type": p.type.name,
                    "default": _serialise_value(p.default),
                    "group": p.group,
                    "min": p.min_value,
                    "max": p.max_value,
                    "step": p.step,
                    "choices": p.choices,
                    "description": p.description,
                }
                for p in props
            ],
            "images": instance.get_images(),
        }

    def _handle_render(self, msg: Dict) -> Dict:
        instance_id = msg.get("instance_id", "")
        width = msg.get("width", 300)
        height = msg.get("height", 200)

        instance = self.registry.get_instance(instance_id)
        if instance is None:
            return {"type": "error", "message": f"Instance not found: {instance_id}"}

        # Build AudioData — prefer shared memory, fall back to JSON
        if self._shm_available and self._shm_reader.is_open:
            shm_data = self._shm_reader.read_raw()
            if shm_data is not None:
                audio = _build_audio_data(shm_data)
            else:
                audio = _build_audio_data(msg.get("audio", {}))
        else:
            audio = _build_audio_data(msg.get("audio", {}))

        # Create render context and execute
        ctx = RenderContext(width, height)
        try:
            instance._tick_fps()
            instance.on_render(ctx, audio)
            # Clear any previous error for this instance
            self._instance_errors.pop(instance_id, None)
        except Exception as e:
            error_msg = traceback.format_exc()
            logger.exception("Render error in %s", instance_id)
            self._instance_errors[instance_id] = str(e)

            # Render an error overlay instead of crashing
            ctx = RenderContext(width, height)
            _render_error_overlay(ctx, width, height, str(e))

        return {
            "type": "render_commands",
            "instance_id": instance_id,
            "commands": ctx._get_commands(),
            "has_error": instance_id in self._instance_errors,
        }

    def _handle_set_property(self, msg: Dict) -> Dict:
        instance_id = msg.get("instance_id", "")
        key = msg.get("key", "")
        value = msg.get("value")

        instance = self.registry.get_instance(instance_id)
        if instance is None:
            return {"type": "error", "message": f"Instance not found: {instance_id}"}

        instance.set_property(key, value)
        return {"type": "ok"}

    def _handle_resize(self, msg: Dict) -> Dict:
        instance_id = msg.get("instance_id", "")
        width = msg.get("width", 0)
        height = msg.get("height", 0)

        instance = self.registry.get_instance(instance_id)
        if instance is None:
            return {"type": "error", "message": f"Instance not found: {instance_id}"}

        instance.on_resize(width, height)
        return {"type": "ok"}

    def _handle_mouse_event(self, msg: Dict) -> Dict:
        instance_id = msg.get("instance_id", "")
        event = msg.get("event", "")
        x = msg.get("x", 0.0)
        y = msg.get("y", 0.0)
        button = msg.get("button", 0)

        instance = self.registry.get_instance(instance_id)
        if instance is None:
            return {"type": "error", "message": f"Instance not found: {instance_id}"}

        if event == "down":
            instance.on_mouse_down(x, y, button)
        elif event == "move":
            instance.on_mouse_move(x, y)
        elif event == "up":
            instance.on_mouse_up(x, y, button)

        return {"type": "ok"}

    def _handle_destroy(self, msg: Dict) -> Dict:
        instance_id = msg.get("instance_id", "")
        self.registry.destroy_instance(instance_id)
        return {"type": "ok"}

    def _handle_save(self, msg: Dict) -> Dict:
        return {
            "type": "save_data",
            "data": self.registry.serialise_instances(),
        }

    def _handle_load(self, msg: Dict) -> Dict:
        data = msg.get("data", {})
        self.registry.deserialise_instances(data)
        return {"type": "ok"}

    def _handle_reload(self, msg: Dict) -> Dict:
        manifest_id = msg.get("manifest_id", "")
        ok = self.registry.reload_plugin(manifest_id)
        if ok:
            return {"type": "ok", "message": f"Reloaded {manifest_id}"}
        return {"type": "error", "message": f"Reload failed for {manifest_id}"}

    def _handle_shutdown(self, msg: Dict) -> Optional[Dict]:
        self.registry.destroy_all()
        self._shm_reader.close()
        self._running = False
        return {"type": "ok", "message": "Shutting down"}


# ── Helpers ─────────────────────────────────────────────────────────────────

def _build_audio_data(d: Dict) -> AudioData:
    """Construct an AudioData from a dict received from the C++ host."""
    channels = []
    for ch in d.get("channels", []):
        channels.append(ChannelData(
            rms=ch.get("rms", -100.0),
            peak=ch.get("peak", -100.0),
            true_peak=ch.get("true_peak", -100.0),
            rms_linear=ch.get("rms_linear", 0.0),
            peak_linear=ch.get("peak_linear", 0.0),
        ))
    if not channels:
        channels = [ChannelData(), ChannelData()]

    return AudioData(
        sample_rate=d.get("sample_rate", 44100.0),
        num_channels=d.get("num_channels", 2),
        is_playing=d.get("is_playing", False),
        position_seconds=d.get("position_seconds", 0.0),
        duration_seconds=d.get("duration_seconds", 0.0),
        channels=channels,
        spectrum=d.get("spectrum", []),
        spectrum_linear=d.get("spectrum_linear", []),
        fft_size=d.get("fft_size", 2048),
        waveform=d.get("waveform", []),
        correlation=d.get("correlation", 0.0),
        stereo_angle=d.get("stereo_angle", 0.0),
        lufs_momentary=d.get("lufs_momentary", -100.0),
        lufs_short_term=d.get("lufs_short_term", -100.0),
        lufs_integrated=d.get("lufs_integrated", -100.0),
        loudness_range=d.get("loudness_range", 0.0),
        bpm=d.get("bpm", 0.0),
        beat_phase=d.get("beat_phase", 0.0),
    )


def _serialise_value(v: Any) -> Any:
    """Make a property default JSON-safe."""
    from .color import Color
    if isinstance(v, Color):
        return v.to_argb()
    return v


def _render_error_overlay(ctx: RenderContext, w: float, h: float, error_msg: str):
    """Render a red error overlay on the component area instead of crashing.

    This provides immediate visual feedback when a Python plugin has a bug,
    without bringing down the entire application.
    """
    from .color import Color
    from .render_context import Font, TextAlign

    # Dark semi-transparent background
    ctx.clear(Color(0x20, 0x08, 0x08))

    # Red border
    border = 3.0
    ctx.stroke_rect(border / 2, border / 2, w - border, h - border,
                    Color(0xFF, 0x33, 0x33), border)

    # Error icon area
    icon_y = min(20.0, h * 0.1)
    ctx.draw_text(
        "ERROR", 0, icon_y, w, 24,
        Color(0xFF, 0x44, 0x44),
        Font("Arial", 18.0, bold=True),
        TextAlign.CENTER,
    )

    # Error message (truncated to fit)
    msg_lines = error_msg[:200]  # truncate very long messages
    msg_y = icon_y + 30
    available_h = h - msg_y - 10
    if available_h > 0:
        ctx.draw_text(
            msg_lines, 10, msg_y, w - 20, available_h,
            Color(0xFF, 0xAA, 0xAA),
            Font("Consolas", 11.0),
            TextAlign.LEFT,
        )

    # Hint at bottom
    if h > 100:
        ctx.draw_text(
            "Fix the script and reload", 0, h - 25, w, 20,
            Color(0x99, 0x66, 0x66),
            Font("Arial", 10.0),
            TextAlign.CENTER,
        )
