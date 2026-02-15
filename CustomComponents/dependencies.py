"""
Dependency manager — automatic installation of Python packages for plugins.

When a plugin lists required packages (e.g., numpy, scipy) in its manifest,
this module checks if they're available and offers to install them.

This solves the "dependency hell" problem where a plugin works on the
developer's machine but fails on others because numpy is not installed.

Usage::

    from CustomComponents.dependencies import ensure_packages

    # Called automatically by the bridge before first render
    ensure_packages(["numpy", "scipy"])
"""

from __future__ import annotations

import importlib
import logging
import subprocess
import sys
from typing import Dict, List, Optional, Set

logger = logging.getLogger("maximeter.dependencies")

# Packages we bundle / guarantee are available
BUILTIN_PACKAGES: Set[str] = {
    "json", "math", "struct", "mmap", "time", "abc",
    "dataclasses", "enum", "typing", "pathlib", "logging",
    "os", "sys", "traceback", "inspect", "importlib",
    "collections", "functools", "itertools", "operator",
}

# Cache of checked packages (avoid re-checking every frame)
_checked: Dict[str, bool] = {}


def is_available(package_name: str) -> bool:
    """Check if a Python package is importable (without importing it fully)."""
    if package_name in _checked:
        return _checked[package_name]

    if package_name in BUILTIN_PACKAGES:
        _checked[package_name] = True
        return True

    try:
        importlib.import_module(package_name)
        _checked[package_name] = True
        return True
    except ImportError:
        _checked[package_name] = False
        return False


def install_package(package_name: str) -> bool:
    """Install a package via pip. Returns True on success."""
    logger.info("Installing package: %s", package_name)
    try:
        result = subprocess.run(
            [sys.executable, "-m", "pip", "install", "--quiet", package_name],
            capture_output=True,
            text=True,
            timeout=120,
        )
        if result.returncode == 0:
            logger.info("Successfully installed: %s", package_name)
            # Invalidate cache
            _checked.pop(package_name, None)
            return True
        else:
            logger.error("Failed to install %s: %s", package_name, result.stderr)
            return False
    except Exception as e:
        logger.error("Error installing %s: %s", package_name, e)
        return False


def ensure_packages(
    packages: List[str],
    auto_install: bool = True,
) -> Dict[str, bool]:
    """Check and optionally install a list of required packages.

    Args:
        packages:      List of package names (e.g., ["numpy", "scipy"]).
        auto_install:  If True, automatically install missing packages.

    Returns:
        Dict mapping package name to availability (True = available).
    """
    results: Dict[str, bool] = {}

    for pkg in packages:
        pkg = pkg.strip()
        if not pkg:
            continue

        if is_available(pkg):
            results[pkg] = True
        elif auto_install:
            success = install_package(pkg)
            results[pkg] = success
        else:
            logger.warning("Missing package: %s (auto-install disabled)", pkg)
            results[pkg] = False

    return results


def check_plugin_dependencies(manifest) -> List[str]:
    """Check which packages a plugin needs that aren't installed.

    Reads the ``requires`` field from the manifest if present.
    Returns list of missing package names.
    """
    requires = getattr(manifest, "requires", None)
    if not requires:
        return []

    missing = []
    for pkg in requires:
        if not is_available(pkg):
            missing.append(pkg)

    return missing


def get_numpy() -> Optional[object]:
    """Convenience: import and return numpy, or None if not available.

    Usage in plugins::

        np = get_numpy()
        if np is not None:
            arr = np.array(audio.spectrum_linear)
            smoothed = np.convolve(arr, np.ones(5)/5, mode='same')
    """
    try:
        import numpy
        return numpy
    except ImportError:
        return None
