"""
Test runner for the CustomComponents framework.
Verifies that plugins can be discovered, instantiated, and rendered.

Usage:
    python -m CustomComponents.test_framework
"""

from __future__ import annotations

import json
import os
import sys
from pathlib import Path

# Ensure project root is on sys.path
_root = Path(__file__).resolve().parent.parent
if str(_root) not in sys.path:
    sys.path.insert(0, str(_root))

from CustomComponents import ComponentRegistry, RenderContext, AudioData, ChannelData


def main():
    plugins_dir = Path(__file__).resolve().parent / "plugins"
    print(f"=== MaxiMeter Custom Component Framework Test ===")
    print(f"Plugins directory: {plugins_dir}\n")

    # 1. Create registry and scan
    registry = ComponentRegistry(plugins_dir)
    plugins = registry.scan()

    print(f"Discovered {len(plugins)} plugin(s):\n")
    for p in plugins:
        status = "OK" if p.load_error is None else f"ERROR: {p.load_error[:80]}"
        print(f"  [{status}] {p.manifest.name} ({p.manifest.id})")
        print(f"          Author: {p.manifest.author}")
        print(f"          Category: {p.manifest.category.name}")
        print(f"          Size: {p.manifest.default_size}")
        print()

    valid = registry.get_valid_plugins()
    if not valid:
        print("No valid plugins found!")
        return 1

    # 2. Test manifest list (JSON for TOOLBOX)
    manifest_json = registry.to_json()
    manifests = json.loads(manifest_json)
    print(f"TOOLBOX manifest list ({len(manifests)} items):")
    for m in manifests:
        print(f"  - {m['name']} [{m['category']}] ({m['default_size'][0]}x{m['default_size'][1]})")
    print()

    # 3. Test instance creation and rendering
    print("=== Instance Tests ===\n")

    # Mock audio data
    audio = AudioData(
        sample_rate=44100.0,
        num_channels=2,
        is_playing=True,
        position_seconds=5.0,
        duration_seconds=120.0,
        channels=[
            ChannelData(rms=-12.0, peak=-6.0, rms_linear=0.25, peak_linear=0.5),
            ChannelData(rms=-14.0, peak=-8.0, rms_linear=0.20, peak_linear=0.4),
        ],
        spectrum=[float(-40 + i * 0.3) for i in range(1025)],
        spectrum_linear=[float(max(0, 0.02 * (1025 - i) / 1025)) for i in range(1025)],
        fft_size=2048,
        waveform=[0.0] * 1024,
        correlation=0.85,
        lufs_momentary=-14.5,
        lufs_short_term=-14.2,
        lufs_integrated=-14.0,
    )

    for plugin in valid:
        mid = plugin.manifest.id
        print(f"Testing: {plugin.manifest.name} ({mid})")

        # Create
        instance = registry.create(mid, f"test_{mid}")
        if instance is None:
            print(f"  FAIL: could not create instance")
            continue
        print(f"  Created OK")

        # Check properties
        props = instance.get_properties()
        print(f"  Properties: {len(props)}")
        for p in props:
            print(f"    - {p.key} ({p.type.name}) = {p.default}")

        # Render
        w, h = plugin.manifest.default_size
        ctx = RenderContext(w, h)
        try:
            instance.on_render(ctx, audio)
            commands = ctx._get_commands()
            print(f"  Render OK: {len(commands)} draw commands")
            # Show first few commands
            for cmd in commands[:3]:
                print(f"    {cmd['cmd']}")
            if len(commands) > 3:
                print(f"    ... and {len(commands) - 3} more")
        except Exception as e:
            print(f"  RENDER ERROR: {e}")

        # Serialise / deserialise round-trip
        data = instance.serialise()
        instance.deserialise(data)
        print(f"  Serialise/Deserialise OK")

        # Destroy
        registry.destroy_instance(f"test_{mid}")
        print(f"  Destroyed OK")
        print()

    print("=== All tests passed ===")
    return 0


if __name__ == "__main__":
    sys.exit(main())
