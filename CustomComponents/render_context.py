"""
RenderContext — drawing API exposed to custom components.

This is a *command-buffer* renderer: each draw call appends a serialisable
command to an internal list.  After ``on_render()`` returns, the host reads
the command buffer and replays it through the JUCE Graphics pipeline.

GPU shader commands (v3) are executed via the JUCE OpenGL backend.  The
host detects the GPU's OpenGL version at startup:

- **OpenGL 4.3+**: compute-shader path with SSBO (e.g. persistent particles).
- **OpenGL 3.3**:  fragment-shader fallback for wide compatibility.

Audio data reaches the renderer through one of two transports:

- **SharedMemory** (preferred, ~5 µs/frame): zero-copy mmap region
  written by C++ and read by Python.  See ``shared_memory.py``.
- **JSON over pipe** (fallback, ~500 µs/frame): used when shared memory
  is unavailable.

If the Python plugin process crashes, the host will automatically restart
it (up to 5 retries) with exponential back-off.

This design keeps custom Python code fully decoupled from the C++ rendering
backend and makes it easy to serialize, profile, or remote-debug rendering.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Any, Dict, List, Optional, Sequence, Tuple

from .color import Color, Gradient


# ── Enums ───────────────────────────────────────────────────────────────────

class BlendMode(Enum):
    NORMAL = auto()
    ADDITIVE = auto()
    MULTIPLY = auto()
    SCREEN = auto()


class TextAlign(Enum):
    LEFT = auto()
    CENTER = auto()
    RIGHT = auto()


@dataclass(frozen=True, slots=True)
class Font:
    """Describes a font for text rendering."""
    family: str = "Arial"
    size: float = 14.0
    bold: bool = False
    italic: bool = False


# ── Render Command types ────────────────────────────────────────────────────

class _CmdType(Enum):
    CLEAR = "clear"
    FILL_RECT = "fill_rect"
    STROKE_RECT = "stroke_rect"
    FILL_ROUNDED_RECT = "fill_rounded_rect"
    STROKE_ROUNDED_RECT = "stroke_rounded_rect"
    FILL_ELLIPSE = "fill_ellipse"
    STROKE_ELLIPSE = "stroke_ellipse"
    DRAW_LINE = "draw_line"
    DRAW_PATH = "draw_path"      # list of (x,y) points, closed flag
    FILL_PATH = "fill_path"
    DRAW_TEXT = "draw_text"
    DRAW_IMAGE = "draw_image"
    FILL_GRADIENT_RECT = "fill_gradient_rect"
    SET_CLIP = "set_clip"
    RESET_CLIP = "reset_clip"
    PUSH_TRANSFORM = "push_transform"
    POP_TRANSFORM = "pop_transform"
    SET_OPACITY = "set_opacity"
    SET_BLEND_MODE = "set_blend_mode"
    DRAW_ARC = "draw_arc"
    FILL_ARC = "fill_arc"
    # ── Extended commands (v2) ──
    DRAW_BEZIER = "draw_bezier"       # quadratic/cubic bezier curve
    FILL_CIRCLE = "fill_circle"       # convenience: cx, cy, radius, color
    STROKE_CIRCLE = "stroke_circle"   # convenience: cx, cy, radius, color, thickness
    DRAW_POLYLINE = "draw_polyline"   # multi-segment line (not closed)
    SAVE_STATE = "save_state"         # push graphics state
    RESTORE_STATE = "restore_state"   # pop graphics state
    # ── GPU shader commands (v3) ──
    DRAW_SHADER = "draw_shader"       # execute a GLSL shader pass
    DRAW_CUSTOM_SHADER = "draw_custom_shader"  # user-provided GLSL source


@dataclass(slots=True)
class _RenderCmd:
    """A single render command in the command buffer."""
    type: _CmdType
    params: Dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> Dict[str, Any]:
        """Serialise for IPC / debugging."""
        d = {"cmd": self.type.value}
        for k, v in self.params.items():
            if isinstance(v, Color):
                d[k] = v.to_argb()
            elif isinstance(v, Gradient):
                d[k] = {
                    "stops": [(s.position, s.color.to_argb()) for s in v.stops],
                    "x1": v.x1, "y1": v.y1, "x2": v.x2, "y2": v.y2,
                    "radial": v.is_radial,
                }
            elif isinstance(v, Font):
                d[k] = {"family": v.family, "size": v.size, "bold": v.bold, "italic": v.italic}
            elif isinstance(v, BlendMode):
                d[k] = v.name.lower()
            elif isinstance(v, TextAlign):
                d[k] = v.name.lower()
            else:
                d[k] = v
        return d


# ── RenderContext ───────────────────────────────────────────────────────────

class RenderContext:
    """Drawing API available inside ``BaseComponent.on_render()``.

    All coordinates are in *local* component space (0,0 = top-left of the
    component, width×height = bottom-right).  The host transforms to canvas
    space automatically.

    Example::

        def on_render(self, ctx: RenderContext, audio: AudioData):
            ctx.clear(Color.black())
            ctx.set_opacity(0.8)
            ctx.fill_rect(10, 10, 100, 50, Color.red())
            ctx.draw_text("Hello", 10, 70, font=Font("Arial", 16, bold=True))
    """

    def __init__(self, width: float, height: float):
        self._width = width
        self._height = height
        self._commands: List[_RenderCmd] = []

    # ── Read-only geometry ──────────────────────────────────────────────────

    @property
    def width(self) -> float:
        return self._width

    @property
    def height(self) -> float:
        return self._height

    # ── Command buffer access (used by host) ────────────────────────────────

    def _get_commands(self) -> List[Dict[str, Any]]:
        """Return serialised command list and clear the buffer."""
        cmds = [c.to_dict() for c in self._commands]
        self._commands.clear()
        return cmds

    # ── Drawing commands ────────────────────────────────────────────────────

    def clear(self, color: Color = Color.black()):
        """Fill the entire component area with a solid colour."""
        self._commands.append(_RenderCmd(_CmdType.CLEAR, {"color": color}))

    # -- Rectangles --

    def fill_rect(
        self,
        x: float,
        y: float,
        w: float,
        h: float,
        color: Color = Color.white(),
    ):
        self._commands.append(
            _RenderCmd(_CmdType.FILL_RECT, {"x": x, "y": y, "w": w, "h": h, "color": color})
        )

    def stroke_rect(
        self,
        x: float,
        y: float,
        w: float,
        h: float,
        color: Color = Color.white(),
        thickness: float = 1.0,
    ):
        self._commands.append(
            _RenderCmd(
                _CmdType.STROKE_RECT,
                {"x": x, "y": y, "w": w, "h": h, "color": color, "thickness": thickness},
            )
        )

    def fill_rounded_rect(
        self,
        x: float,
        y: float,
        w: float,
        h: float,
        radius: float,
        color: Color = Color.white(),
    ):
        self._commands.append(
            _RenderCmd(
                _CmdType.FILL_ROUNDED_RECT,
                {"x": x, "y": y, "w": w, "h": h, "radius": radius, "color": color},
            )
        )

    def stroke_rounded_rect(
        self,
        x: float,
        y: float,
        w: float,
        h: float,
        radius: float,
        color: Color = Color.white(),
        thickness: float = 1.0,
    ):
        self._commands.append(
            _RenderCmd(
                _CmdType.STROKE_ROUNDED_RECT,
                {
                    "x": x, "y": y, "w": w, "h": h,
                    "radius": radius, "color": color, "thickness": thickness,
                },
            )
        )

    # -- Ellipses --

    def fill_ellipse(self, cx: float, cy: float, rx: float, ry: float, color: Color = Color.white()):
        self._commands.append(
            _RenderCmd(_CmdType.FILL_ELLIPSE, {"cx": cx, "cy": cy, "rx": rx, "ry": ry, "color": color})
        )

    def stroke_ellipse(
        self,
        cx: float,
        cy: float,
        rx: float,
        ry: float,
        color: Color = Color.white(),
        thickness: float = 1.0,
    ):
        self._commands.append(
            _RenderCmd(
                _CmdType.STROKE_ELLIPSE,
                {"cx": cx, "cy": cy, "rx": rx, "ry": ry, "color": color, "thickness": thickness},
            )
        )

    # -- Lines --

    def draw_line(
        self,
        x1: float, y1: float, x2: float, y2: float,
        color: Color = Color.white(),
        thickness: float = 1.0,
    ):
        self._commands.append(
            _RenderCmd(
                _CmdType.DRAW_LINE,
                {"x1": x1, "y1": y1, "x2": x2, "y2": y2, "color": color, "thickness": thickness},
            )
        )

    # -- Paths --

    def draw_path(
        self,
        points: Sequence[Tuple[float, float]],
        color: Color = Color.white(),
        thickness: float = 1.0,
        closed: bool = False,
    ):
        self._commands.append(
            _RenderCmd(
                _CmdType.DRAW_PATH,
                {"points": list(points), "color": color, "thickness": thickness, "closed": closed},
            )
        )

    def fill_path(
        self,
        points: Sequence[Tuple[float, float]],
        color: Color = Color.white(),
    ):
        self._commands.append(
            _RenderCmd(_CmdType.FILL_PATH, {"points": list(points), "color": color})
        )

    # -- Arcs --

    def draw_arc(
        self,
        cx: float, cy: float, radius: float,
        start_angle: float, end_angle: float,
        color: Color = Color.white(),
        thickness: float = 1.0,
    ):
        """Draw an arc (angles in radians, 0 = right, clockwise)."""
        self._commands.append(
            _RenderCmd(
                _CmdType.DRAW_ARC,
                {
                    "cx": cx, "cy": cy, "radius": radius,
                    "start": start_angle, "end": end_angle,
                    "color": color, "thickness": thickness,
                },
            )
        )

    def fill_arc(
        self,
        cx: float, cy: float, radius: float,
        start_angle: float, end_angle: float,
        color: Color = Color.white(),
    ):
        """Fill a pie-slice arc."""
        self._commands.append(
            _RenderCmd(
                _CmdType.FILL_ARC,
                {
                    "cx": cx, "cy": cy, "radius": radius,
                    "start": start_angle, "end": end_angle,
                    "color": color,
                },
            )
        )

    # -- Text --

    def draw_text(
        self,
        text: str,
        x: float,
        y: float,
        width: float = 0,
        height: float = 0,
        color: Color = Color.white(),
        font: Font = Font(),
        align: TextAlign = TextAlign.LEFT,
    ):
        """Draw text.  If width/height are 0, text is unbounded."""
        self._commands.append(
            _RenderCmd(
                _CmdType.DRAW_TEXT,
                {
                    "text": text, "x": x, "y": y, "w": width, "h": height,
                    "color": color, "font": font, "align": align,
                },
            )
        )

    # -- Images --

    def draw_image(
        self,
        image_key: str,
        x: float,
        y: float,
        width: float = 0,
        height: float = 0,
        opacity: float = 1.0,
    ):
        """Draw a cached image. *image_key* is registered via ``load_image()``
        in ``on_init()``."""
        self._commands.append(
            _RenderCmd(
                _CmdType.DRAW_IMAGE,
                {"key": image_key, "x": x, "y": y, "w": width, "h": height, "opacity": opacity},
            )
        )

    # -- Gradients --

    def fill_gradient_rect(
        self,
        x: float,
        y: float,
        w: float,
        h: float,
        gradient: Gradient,
    ):
        self._commands.append(
            _RenderCmd(
                _CmdType.FILL_GRADIENT_RECT,
                {"x": x, "y": y, "w": w, "h": h, "gradient": gradient},
            )
        )

    # -- State --

    def set_clip(self, x: float, y: float, w: float, h: float):
        """Restrict drawing to rectangle. Call ``reset_clip()`` to restore."""
        self._commands.append(_RenderCmd(_CmdType.SET_CLIP, {"x": x, "y": y, "w": w, "h": h}))

    def reset_clip(self):
        self._commands.append(_RenderCmd(_CmdType.RESET_CLIP))

    def push_transform(
        self,
        translate: Optional[Tuple[float, float]] = None,
        rotate: float = 0.0,
        scale: Optional[Tuple[float, float]] = None,
    ):
        """Push a transformation onto the stack."""
        self._commands.append(
            _RenderCmd(
                _CmdType.PUSH_TRANSFORM,
                {"translate": translate, "rotate": rotate, "scale": scale},
            )
        )

    def pop_transform(self):
        self._commands.append(_RenderCmd(_CmdType.POP_TRANSFORM))

    def set_opacity(self, opacity: float):
        """Set global opacity (0.0 – 1.0) for subsequent draw calls."""
        self._commands.append(_RenderCmd(_CmdType.SET_OPACITY, {"opacity": opacity}))

    def set_blend_mode(self, mode: BlendMode):
        self._commands.append(_RenderCmd(_CmdType.SET_BLEND_MODE, {"mode": mode}))

    # ── Extended drawing (v2) ───────────────────────────────────────────────

    def fill_circle(self, cx: float, cy: float, radius: float,
                    color: Color = Color.white()):
        """Shorthand for fill_ellipse with equal radii."""
        self._commands.append(
            _RenderCmd(_CmdType.FILL_CIRCLE,
                       {"cx": cx, "cy": cy, "radius": radius, "color": color})
        )

    def stroke_circle(self, cx: float, cy: float, radius: float,
                      color: Color = Color.white(), thickness: float = 1.0):
        """Shorthand for stroke_ellipse with equal radii."""
        self._commands.append(
            _RenderCmd(_CmdType.STROKE_CIRCLE,
                       {"cx": cx, "cy": cy, "radius": radius,
                        "color": color, "thickness": thickness})
        )

    def draw_bezier(self, x1: float, y1: float,
                    cx1: float, cy1: float,
                    cx2: float, cy2: float,
                    x2: float, y2: float,
                    color: Color = Color.white(),
                    thickness: float = 1.0):
        """Draw a cubic bezier curve from (x1,y1) to (x2,y2)
        with control points (cx1,cy1) and (cx2,cy2)."""
        self._commands.append(
            _RenderCmd(_CmdType.DRAW_BEZIER, {
                "x1": x1, "y1": y1,
                "cx1": cx1, "cy1": cy1,
                "cx2": cx2, "cy2": cy2,
                "x2": x2, "y2": y2,
                "color": color, "thickness": thickness,
            })
        )

    def draw_polyline(self, points: Sequence[Tuple[float, float]],
                      color: Color = Color.white(),
                      thickness: float = 1.0):
        """Draw a multi-segment open polyline (not closed).
        More efficient than multiple draw_line calls."""
        self._commands.append(
            _RenderCmd(_CmdType.DRAW_POLYLINE, {
                "points": list(points),
                "color": color, "thickness": thickness,
            })
        )

    def save_state(self):
        """Save the current graphics state (clip, transform, opacity).
        Must be paired with ``restore_state()``."""
        self._commands.append(_RenderCmd(_CmdType.SAVE_STATE))

    def restore_state(self):
        """Restore a previously saved graphics state."""
        self._commands.append(_RenderCmd(_CmdType.RESTORE_STATE))

    # ── Convenience helpers ─────────────────────────────────────────────────

    def draw_centered_text(self, text: str, y: float,
                           color: Color = Color.white(),
                           font: Font = Font()):
        """Draw text horizontally centered in the component."""
        self.draw_text(text, 0, y, self._width, font.size + 4,
                       color, font, TextAlign.CENTER)

    def draw_fps(self, fps: float, x: float = 5, y: float = 5,
                 color: Color = Color(180, 180, 180)):
        """Draw an FPS counter at the given position."""
        self.draw_text(f"{fps:.0f} fps", x, y, 80, 14,
                       color, Font("Consolas", 10.0))

    # ── GPU Shader commands (v3) ────────────────────────────────────────────

    def draw_shader(self, shader_id: str,
                    uniforms: Optional[Dict[str, float]] = None,
                    **kwargs: float):
        """Execute a built-in GLSL shader as a fullscreen pass.

        The shader is rendered over the entire component area using OpenGL.
        Standard uniforms (u_time, u_resolution, u_audioData) are set
        automatically by the host.

        Args:
            shader_id: Built-in shader name. Available shaders:
                - ``"mandelbrot"`` — Mandelbrot fractal (interactive zoom)
                - ``"julia"``      — Julia set (animated, audio-reactive)
                - ``"bloom"``      — Bloom/glow post-process effect
                - ``"blur"``       — Gaussian blur post-process
                - ``"glitch"``     — Digital glitch/distortion effect
                - ``"particles"``  — GPU particle system (1000+ particles)
                - ``"waveform3d"`` — 3D waveform visualizer
                - ``"spectrum3d"`` — 3D spectrum bar visualizer
                - ``"plasma"``     — Animated plasma effect
                - ``"voronoi"``    — Voronoi cell pattern
                - ``"chromatic"``  — Chromatic aberration post-process
            uniforms: Dict of uniform name → float value.
                Each shader has its own set of custom uniforms.
            **kwargs: Additional uniforms as keyword arguments.

        Example::

            # Render a Mandelbrot set zoomed into a specific region
            ctx.draw_shader("mandelbrot", {
                "u_zoom": 50.0,
                "u_centerX": -0.748,
                "u_centerY": 0.1,
                "u_maxIter": 512,
            })

            # Apply bloom glow to previously drawn 2D content
            ctx.fill_rect(50, 50, 100, 100, Color.red())
            ctx.draw_shader("bloom", u_intensity=2.0, u_threshold=0.5)

            # Render 1000 audio-reactive particles
            ctx.draw_shader("particles", u_count=1000, u_size=3.0,
                            u_colorR=0.3, u_colorG=0.6, u_colorB=1.0)
        """
        all_uniforms = dict(uniforms or {})
        all_uniforms.update(kwargs)
        self._commands.append(
            _RenderCmd(_CmdType.DRAW_SHADER, {
                "shader_id": shader_id,
                "uniforms": all_uniforms,
            })
        )

    def draw_custom_shader(self, fragment_source: str,
                           compute_source: Optional[str] = None,
                           buffer_size: int = 4096,
                           num_groups: Tuple[int, int, int] = (1, 1, 1),
                           cache_key: Optional[str] = None,
                           uniforms: Optional[Dict[str, float]] = None,
                           **kwargs: float):
        """Compile and execute a user-provided GLSL fragment shader.

        Optionally provide a **compute shader** that runs before the fragment
        stage.  The compute shader can write to an SSBO (binding 0) which the
        fragment shader can then read.  This enables persistent GPU state
        across frames (particles, physics simulations, etc.).

        The shader source must be a complete GLSL shader including the
        ``#version`` directive.  Fragment-only shaders should use
        ``#version 330 core``; when using a compute stage, both the compute
        and fragment sources must use ``#version 430 core``.

        Shaders are compiled once and cached by ``cache_key`` for subsequent
        frames.

        The host automatically provides these uniforms to **both** compute and
        fragment stages:

        - ``u_time`` (float) — elapsed time in seconds
        - ``u_resolution`` (vec2) — viewport width/height in pixels
        - ``u_audioData`` (sampler2D) — row 0 = spectrum, row 1 = waveform

        Additionally, the **fragment** stage receives:

        - ``u_frame`` (int) — frame counter
        - ``u_spectrumSize`` (int) — number of spectrum bins
        - ``u_waveformSize`` (int) — number of waveform samples
        - ``u_texture`` (sampler2D) — back-buffer (previous 2D draws)

        And the **compute** stage receives:

        - ``u_deltaTime`` (float) — seconds since last frame

        Any uniforms passed via ``uniforms`` dict or ``**kwargs`` are set
        as ``float`` uniforms on **both** compute and fragment programs.

        Args:
            fragment_source: Complete GLSL fragment shader source string.
                Must include ``#version 330 core`` (fragment-only) or
                ``#version 430 core`` (when combined with compute) and
                define ``out vec4 fragColor`` for output.
            compute_source: Optional GLSL compute shader source.
                Must include ``#version 430 core`` and use
                ``layout(local_size_x = N) in;``.  The host provides an
                SSBO at ``binding = 0`` that persists across frames.
            buffer_size: Size of the compute SSBO in bytes (default 4096).
                Only used when ``compute_source`` is provided.  The buffer
                is zero-initialised on creation and persists across frames.
            num_groups: Compute dispatch group counts ``(x, y, z)``.
                Default ``(1, 1, 1)``.  For N work items with
                ``local_size_x = 256``, use ``((N+255)//256, 1, 1)``.
            cache_key: Optional cache key for the compiled shader.
                If omitted, a hash of the source code is used.
                Provide a stable key if you update uniforms each frame
                but keep the same source.
            uniforms: Dict of uniform name → float value.
            **kwargs: Additional uniforms as keyword arguments.

        Example (fragment-only)::

            MY_SHADER = '''
            #version 330 core
            out vec4 fragColor;
            in vec2 vTexCoord;
            uniform float u_time;
            uniform vec2  u_resolution;
            uniform float u_speed;

            void main() {
                vec2 uv = vTexCoord;
                float wave = sin(uv.x * 20.0 + u_time * u_speed) * 0.5 + 0.5;
                fragColor = vec4(uv.x, wave, uv.y, 1.0);
            }
            '''
            ctx.draw_custom_shader(MY_SHADER, cache_key="my_wave", u_speed=2.0)

        Example (compute + fragment)::

            COMPUTE_SRC = '''
            #version 430 core
            layout(local_size_x = 256) in;

            struct Ball { vec4 posVel; vec4 color; };
            layout(std430, binding = 0) buffer Buf { Ball balls[]; };

            uniform float u_time;
            uniform float u_deltaTime;
            uniform float u_count;

            void main() {
                uint i = gl_GlobalInvocationID.x;
                if (i >= uint(u_count)) return;
                Ball b = balls[i];
                // ... update physics ...
                balls[i] = b;
            }
            '''

            FRAG_SRC = '''
            #version 430 core
            in vec2 vTexCoord;
            out vec4 fragColor;
            uniform vec2 u_resolution;
            uniform float u_count;

            struct Ball { vec4 posVel; vec4 color; };
            layout(std430, binding = 0) readonly buffer Buf { Ball balls[]; };

            void main() {
                // ... render balls ...
                fragColor = vec4(0.0);
            }
            '''

            N = 1000
            ctx.draw_custom_shader(
                FRAG_SRC,
                compute_source=COMPUTE_SRC,
                buffer_size=N * 32,           # 2 × vec4 per ball
                num_groups=((N + 255) // 256, 1, 1),
                cache_key="my_balls",
                u_count=float(N),
            )
        """
        import hashlib
        hash_input = fragment_source + (compute_source or "")
        key = cache_key or ("_custom_" + hashlib.md5(
            hash_input.encode("utf-8")).hexdigest())

        all_uniforms = dict(uniforms or {})
        all_uniforms.update(kwargs)
        self._commands.append(
            _RenderCmd(_CmdType.DRAW_CUSTOM_SHADER, {
                "cache_key": key,
                "fragment_source": fragment_source,
                "compute_source": compute_source or "",
                "buffer_size": buffer_size,
                "num_groups_x": num_groups[0],
                "num_groups_y": num_groups[1],
                "num_groups_z": num_groups[2],
                "uniforms": all_uniforms,
            })
        )

    # ── Shader convenience helpers ──────────────────────────────────────────

    def mandelbrot(self, zoom: float = 1.0, center_x: float = -0.5,
                   center_y: float = 0.0, max_iter: int = 256,
                   color_shift: float = 0.0):
        """Render a Mandelbrot fractal.

        Audio-reactive: bass frequencies modulate the color palette.

        Args:
            zoom: Zoom level (higher = more zoomed in).
            center_x, center_y: Center point in fractal space.
            max_iter: Maximum iterations (higher = more detail but slower).
            color_shift: Shift the color palette (0.0 – 1.0).
        """
        self.draw_shader("mandelbrot", {
            "u_zoom": zoom,
            "u_centerX": center_x,
            "u_centerY": center_y,
            "u_maxIter": float(max_iter),
            "u_colorShift": color_shift,
        })

    def julia_set(self, zoom: float = 1.0, c_real: float = -0.7,
                  c_imag: float = 0.27015, max_iter: int = 256,
                  animate: bool = True):
        """Render a Julia set fractal.

        Audio-reactive: bass and mid frequencies modulate the Julia constant.

        Args:
            zoom: Zoom level.
            c_real, c_imag: Julia set constant (determines shape).
            max_iter: Maximum iterations.
            animate: Whether to animate the Julia constant over time.
        """
        self.draw_shader("julia", {
            "u_zoom": zoom,
            "u_cReal": c_real,
            "u_cImag": c_imag,
            "u_maxIter": float(max_iter),
            "u_animate": 1.0 if animate else 0.0,
        })

    def bloom(self, intensity: float = 1.5, threshold: float = 0.6,
              radius: float = 4.0):
        """Apply bloom/glow post-processing to previously drawn content.

        Audio-reactive: bass boosts bloom intensity.

        Args:
            intensity: Bloom strength multiplier.
            threshold: Brightness threshold for bloom (0.0 – 1.0).
            radius: Blur radius for the bloom effect.
        """
        self.draw_shader("bloom", {
            "u_intensity": intensity,
            "u_threshold": threshold,
            "u_radius": radius,
        })

    def blur(self, radius: float = 5.0, direction: Tuple[float, float] = (1.0, 1.0)):
        """Apply Gaussian blur post-processing.

        Audio-reactive: bass increases blur radius.

        Args:
            radius: Blur radius in pixels.
            direction: Blur direction vector (1,0)=horizontal, (0,1)=vertical.
        """
        self.draw_shader("blur", {
            "u_radius": radius,
            "u_dirX": direction[0],
            "u_dirY": direction[1],
        })

    def glitch(self, intensity: float = 0.5, block_size: float = 0.03,
               chromatic: float = 0.01):
        """Apply digital glitch effect.

        Audio-reactive: bass dramatically increases glitch intensity.

        Args:
            intensity: Overall glitch strength (0.0 – 1.0).
            block_size: Size of glitch blocks (smaller = finer).
            chromatic: Chromatic aberration amount.
        """
        self.draw_shader("glitch", {
            "u_intensity": intensity,
            "u_blockSize": block_size,
            "u_chromatic": chromatic,
        })

    def particles(self, count: int = 1000, speed: float = 1.0,
                  size: float = 3.0,
                  color: Optional[Tuple[float, float, float]] = None):
        """Render a GPU-accelerated particle system.

        Audio-reactive: particles respond to bass, mid and high spectrum data.

        The host automatically selects the best rendering path:

        - **OpenGL 4.3+** (compute shader): particles are simulated in a
          compute shader with persistent state stored in an SSBO.  Supports
          up to **100 000** particles at 60 fps on modern GPUs.
        - **OpenGL 3.3** (fragment fallback): stateless per-frame rendering
          capped at **500** particles for compatibility.

        Audio data is delivered via zero-copy SharedMemory (~5 µs per frame)
        with automatic JSON fallback when SHM is unavailable.

        Args:
            count: Number of particles.  GL 4.3+ allows up to 100 000;
                   GL 3.3 fallback clamps to 500.
            speed: Animation speed multiplier.
            size: Particle size in pixels.
            color: Base color as (r, g, b) floats 0–1.
        """
        r, g, b = color or (0.3, 0.6, 1.0)
        self.draw_shader("particles", {
            "u_count": float(count),
            "u_speed": speed,
            "u_size": size,
            "u_colorR": r,
            "u_colorG": g,
            "u_colorB": b,
        })

    def waveform_3d(self, rotate_x: float = 0.3, rotate_y: float = 0.0,
                    depth: float = 20.0, line_width: float = 2.0):
        """Render a 3D waveform visualizer.

        Args:
            rotate_x: Tilt angle (radians).
            rotate_y: Pan angle (auto-animated).
            depth: Number of waveform rows for depth effect.
            line_width: Line thickness.
        """
        self.draw_shader("waveform3d", {
            "u_rotateX": rotate_x,
            "u_rotateY": rotate_y,
            "u_depth": depth,
            "u_lineWidth": line_width,
        })

    def spectrum_3d(self, rotate_x: float = 0.4, bar_width: float = 0.8,
                    height: float = 1.5):
        """Render a 3D spectrum bar visualizer.

        Args:
            rotate_x: Tilt angle.
            bar_width: Width of frequency bars (0–1).
            height: Height scale for bars.
        """
        self.draw_shader("spectrum3d", {
            "u_rotateX": rotate_x,
            "u_barWidth": bar_width,
            "u_height": height,
        })

    def plasma(self, speed: float = 1.0, scale: float = 1.0,
               complexity: float = 3.0):
        """Render an animated plasma effect.

        Audio-reactive: bass and mid frequencies modulate the pattern.
        """
        self.draw_shader("plasma", {
            "u_speed": speed,
            "u_scale": scale,
            "u_complexity": complexity,
        })

    def voronoi(self, cells: float = 8.0, speed: float = 1.0):
        """Render a Voronoi cell pattern.

        Audio-reactive: cell points react to bass.
        """
        self.draw_shader("voronoi", {
            "u_cells": cells,
            "u_speed": speed,
        })

    def chromatic_aberration(self, intensity: float = 0.005,
                             angle: float = 0.0):
        """Apply chromatic aberration post-processing.

        Audio-reactive: bass increases aberration intensity.
        """
        self.draw_shader("chromatic", {
            "u_intensity": intensity,
            "u_angle": angle,
        })
