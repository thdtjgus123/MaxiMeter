"""
AudioData — read-only snapshot of current audio analysis data.

An ``AudioData`` instance is passed to ``BaseComponent.on_render()`` every
frame.  All arrays are plain Python lists (not NumPy) so the framework has
zero external dependencies.

The host fills this from the C++ side before each render call.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import List, Optional


@dataclass(slots=True)
class ChannelData:
    """Per-channel audio measurements."""

    rms: float = 0.0
    """RMS level in dB (−∞ … 0 dB)."""

    peak: float = 0.0
    """Sample peak in dB."""

    true_peak: float = 0.0
    """True-peak (inter-sample) in dB."""

    rms_linear: float = 0.0
    """RMS level as linear 0.0–1.0."""

    peak_linear: float = 0.0
    """Sample peak as linear 0.0–1.0."""


@dataclass(slots=True)
class AudioData:
    """Snapshot of audio analysis data for the current frame.

    All frequency-domain arrays are magnitude values in dB unless stated
    otherwise.

    Attributes:
        sample_rate:       Current audio stream sample rate (Hz).
        num_channels:      Number of audio channels (1=mono, 2=stereo, etc.).
        channels:          Per-channel level data.
        spectrum:          Magnitude spectrum (FFT bins), in dB.
                          Length = fft_size/2 + 1.
        spectrum_linear:   Same as *spectrum* but as linear 0.0–1.0.
        fft_size:          FFT window size used (e.g. 2048, 4096).
        waveform:          Recent raw sample block (mono-mixed), typically 1024 samples.
        correlation:       Stereo correlation coefficient (−1.0 … +1.0).
        stereo_angle:      Average stereo pan angle in degrees (−90 … +90).
        lufs_momentary:    EBU R128 momentary loudness (LUFS).
        lufs_short_term:   EBU R128 short-term loudness (LUFS).
        lufs_integrated:   EBU R128 integrated loudness (LUFS).
        loudness_range:    EBU R128 loudness range (LU).
        is_playing:        True if audio transport is currently playing.
        position_seconds:  Playback position in seconds.
        duration_seconds:  Total file duration in seconds.
        bpm:               Estimated BPM (0 if unavailable).
        beat_phase:        Beat phase 0.0–1.0 within current beat (for beat-sync visuals).
    """

    # Stream info
    sample_rate: float = 44100.0
    num_channels: int = 2
    is_playing: bool = False
    position_seconds: float = 0.0
    duration_seconds: float = 0.0

    # Per-channel levels
    channels: List[ChannelData] = field(default_factory=lambda: [ChannelData(), ChannelData()])

    # Spectrum
    spectrum: List[float] = field(default_factory=list)
    spectrum_linear: List[float] = field(default_factory=list)
    fft_size: int = 2048

    # Waveform
    waveform: List[float] = field(default_factory=list)

    # Stereo field
    correlation: float = 0.0
    stereo_angle: float = 0.0

    # Loudness (EBU R128)
    lufs_momentary: float = -100.0
    lufs_short_term: float = -100.0
    lufs_integrated: float = -100.0
    loudness_range: float = 0.0

    # Beat detection
    bpm: float = 0.0
    beat_phase: float = 0.0

    # ── Convenience helpers ─────────────────────────────────────────────────

    @property
    def left(self) -> ChannelData:
        """Shortcut for ``channels[0]``."""
        return self.channels[0] if self.channels else ChannelData()

    @property
    def right(self) -> ChannelData:
        """Shortcut for ``channels[1]``."""
        return self.channels[1] if len(self.channels) > 1 else self.left

    @property
    def mono_rms(self) -> float:
        """Average RMS across all channels (dB)."""
        if not self.channels:
            return -100.0
        return sum(ch.rms for ch in self.channels) / len(self.channels)

    @property
    def mono_peak(self) -> float:
        """Maximum peak across all channels (dB)."""
        if not self.channels:
            return -100.0
        return max(ch.peak for ch in self.channels)

    def freq_to_bin(self, freq_hz: float) -> int:
        """Convert a frequency (Hz) to the nearest FFT bin index."""
        if not self.spectrum or self.fft_size == 0:
            return 0
        bin_width = self.sample_rate / self.fft_size
        return max(0, min(len(self.spectrum) - 1, int(freq_hz / bin_width + 0.5)))

    def magnitude_at(self, freq_hz: float) -> float:
        """Get spectrum magnitude (dB) at closest bin to *freq_hz*."""
        idx = self.freq_to_bin(freq_hz)
        if 0 <= idx < len(self.spectrum):
            return self.spectrum[idx]
        return -100.0

    def band_magnitude(self, low_hz: float, high_hz: float) -> float:
        """Average spectrum magnitude (dB) over a frequency band."""
        lo = self.freq_to_bin(low_hz)
        hi = self.freq_to_bin(high_hz)
        if lo > hi:
            lo, hi = hi, lo
        if not self.spectrum:
            return -100.0
        band = self.spectrum[lo : hi + 1]
        return sum(band) / len(band) if band else -100.0
