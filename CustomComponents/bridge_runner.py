"""
Bridge Runner — entry point for the Python subprocess spawned by the C++ host.

Usage (from C++):
    ChildProcess proc;
    proc.start("python bridge_runner.py --plugins-dir ./plugins");

The runner initialises the ComponentRegistry, scans the plugins directory,
and enters the JSON-line IPC loop.
"""

import argparse
import logging
import os
import sys
from pathlib import Path

# Ensure the parent directory is in sys.path so `CustomComponents` can be imported
_this_dir = Path(__file__).resolve().parent
_project_root = _this_dir.parent
if str(_project_root) not in sys.path:
    sys.path.insert(0, str(_project_root))

from CustomComponents.registry import ComponentRegistry
from CustomComponents.bridge import BridgeProtocol


def main():
    parser = argparse.ArgumentParser(description="MaxiMeter Python Bridge")
    parser.add_argument(
        "--plugins-dir",
        type=str,
        default=str(_this_dir / "plugins"),
        help="Path to the plugins directory (default: ./plugins/)",
    )
    parser.add_argument(
        "--log-level",
        type=str,
        default="INFO",
        choices=["DEBUG", "INFO", "WARNING", "ERROR"],
        help="Logging level",
    )
    parser.add_argument(
        "--log-file",
        type=str,
        default="",
        help="Path to log file (default: stderr)",
    )
    args = parser.parse_args()

    # Configure logging (to stderr or file, never stdout — that's for IPC)
    log_handlers = []
    if args.log_file:
        log_handlers.append(logging.FileHandler(args.log_file))
    else:
        log_handlers.append(logging.StreamHandler(sys.stderr))

    logging.basicConfig(
        level=getattr(logging, args.log_level),
        format="[%(levelname)s] %(name)s: %(message)s",
        handlers=log_handlers,
    )

    logger = logging.getLogger("maximeter.bridge_runner")
    logger.info("Starting MaxiMeter Python Bridge")
    logger.info("Plugins dir: %s", args.plugins_dir)
    logger.info("Python %s", sys.version)

    # Initialize registry and scan
    registry = ComponentRegistry(args.plugins_dir)
    registry.scan()

    # Enter IPC loop
    bridge = BridgeProtocol(registry)
    bridge.run()

    logger.info("Bridge exited")


if __name__ == "__main__":
    main()
