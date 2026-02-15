"""ARTS configuration file parser."""

from pathlib import Path
from typing import Dict, Optional


def parse_arts_cfg(path: Optional[Path]) -> Dict[str, str]:
    """Parse an ARTS config file (arts.cfg) into a key/value dict.

    This parser is intentionally simple: it ignores section headers and
    supports `key=value` lines, stripping whitespace and inline comments.
    """
    if not path or not path.exists():
        return {}

    values: Dict[str, str] = {}
    try:
        for raw in path.read_text().splitlines():
            line = raw.strip()
            if not line:
                continue
            if line.startswith("#") or line.startswith(";"):
                continue
            if line.startswith("[") and line.endswith("]"):
                continue
            if "=" not in line:
                continue
            key, value = line.split("=", 1)
            key = key.strip()
            # Strip common inline comments.
            value = value.split("#", 1)[0].split(";", 1)[0].strip()
            if key:
                values[key] = value
    except Exception:
        return {}
    return values
