# MaxiMeter - A grid based music visualizer

## Overview
This software is brought from the idea of making a gird-based, more customizable MiniMeters that support video exportation.

You can freely add most of the Winamp (.wsz) skins directly to MaxiMeter. And every music visualization component can be put on a grid and customized with python script.

This software is entirely open-source and free, so you can modify it under the MIT license.

## Structure of the software
The structure of MaxiMeter is composed of five panels. Each does a customization centered around the viewport.

### 1. Player
The Player panel handles audio playback and sits at the top of the interface.
It has:
- Standard transport controls (Play, Pause, Stop)
- Open and load audio file (MP3, WAV, FLAC, OGG, AAC)
- Timeline scrubbing with sample-accurate positioning

### 2. Component Toolbox
The Toolbox panel on the left displays all available components you can add to your visualization.
Usage:
- Drag a component from the toolbox onto the Viewport
- Double-click to instantly create at canvas center

### 3. Properties, Settings, Layers Panel
The right sidebar contains three tabs that control component behavior and project settings.

#### Properties Tab
Change background grid properties.
Examples:
- Color pickers for meter colors
- Changes apply in real-time (no need to restart)

#### Settings Tab
Change and customize when you select a component on the canvas
- Sliders for sensitivity, decay rate, smoothing
- Dropdowns for display modes (bar, line, filled)

#### Layers Tab
Lists all components currently on the canvas
-Drag to reorder Z-index (bring to front / send to back)
-Toggle visibility
-Lock/unlock components to prevent accidental edits

### 4. Viewport
The Viewport is the main canvas where you arrange and preview your visualization.

### 5. Waveform Visualizer
The Waveform Visualizer sits below the Viewport and displays the full audio timeline.
Features:
- Full waveform overview (left/right channels)
- Click/drag to jump to any point in the track

## Winamp Skin Support
MaxiMeter supports Winamp Classic Skins (.wsz files).
How to Use:
1. Download a skin from Winamp Skin Museum
2. Go to File > Import Winamp Skin or from the Settings panel
3. Select your .wsz file
4. Drag them onto the canvas
5. You can double click to change to interaction mode.

## Installation
Download the latest release from Releases
Extract the archive
Run MaxiMeter.exe

## Python Scripting
Create your own custom components using Python 3.10+.
Example: Simple Beat-Reactive Box

```python
from CustomComponents import BaseComponent, Manifest, Category, RenderContext, AudioData, Color

class BeatingBox(BaseComponent):
    @staticmethod
    def get_manifest():
        return Manifest(
            id="com.example.beating_box",
            name="Beating Box",
            category=Category.VISUALIZER,
            default_size=(200, 200),
        )

    def on_render(self, ctx: RenderContext, audio: AudioData):
        level = max(audio.left.rms_linear, audio.right.rms_linear)
        size = 100 + (level * 100)
        ctx.clear(Color.black())
        ctx.fill_rect(50, 50, size, size, Color.cyan())
```
See F1 -> Documentation or Help -> Documentation for full API documentation.