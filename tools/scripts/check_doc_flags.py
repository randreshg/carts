#!/usr/bin/env python3
"""Validate that documented carts-compile long flags map to real CLI options."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Set, Tuple


DEFAULT_DOCS: Sequence[str] = (
    "docs/compiler/pipeline.md",
    "docs/heuristics/distribution.md",
    "docs/heuristics/partitioning.md",
    "docs/profiling/diagnose-dataflow.md",
)

OPTION_DECL_RE = re.compile(
    r'cl::(?:opt|list)\s*<[^>]+>\s*[A-Za-z_]\w*\s*\(\s*"([^"]+)"',
    re.MULTILINE,
)
DOC_FLAG_RE = re.compile(r"--[A-Za-z0-9][A-Za-z0-9-]*")


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _resolve_paths(paths: Iterable[str], root: Path) -> List[Path]:
    resolved: List[Path] = []
    for raw in paths:
        path = Path(raw)
        if not path.is_absolute():
            path = root / path
        if path.is_file():
            resolved.append(path)
            continue
        if path.is_dir():
            resolved.extend(sorted(path.rglob("*.md")))
            continue
        raise FileNotFoundError(f"path not found: {raw}")
    return resolved


def _extract_compile_flags(source: Path) -> Set[str]:
    text = source.read_text(encoding="utf-8")
    flags: Set[str] = set()
    for match in OPTION_DECL_RE.finditer(text):
        option = match.group(1).strip()
        if not option or len(option) <= 1:
            continue
        flags.add(f"--{option}")
    return flags


def _extract_documented_flags(
    docs: Sequence[Path],
) -> Dict[str, List[Tuple[Path, int]]]:
    flags: Dict[str, List[Tuple[Path, int]]] = {}
    for doc in docs:
        for line_no, line in enumerate(
            doc.read_text(encoding="utf-8", errors="replace").splitlines(), 1
        ):
            for flag in DOC_FLAG_RE.findall(line):
                flags.setdefault(flag, []).append((doc, line_no))
    return flags


def _format_location(path: Path, line: int, root: Path) -> str:
    try:
        rel = path.relative_to(root)
    except ValueError:
        rel = path
    return f"{rel}:{line}"


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Check that documented carts-compile long flags exist in the CLI.",
    )
    parser.add_argument(
        "--source",
        default="tools/compile/Compile.cpp",
        help="Path to carts-compile source (default: %(default)s)",
    )
    parser.add_argument(
        "--docs",
        nargs="+",
        default=list(DEFAULT_DOCS),
        help="Docs files/directories to scan (default: compiler/docs set)",
    )
    args = parser.parse_args(argv)

    root = _repo_root()
    source = Path(args.source)
    if not source.is_absolute():
        source = root / source
    if not source.is_file() and args.source == "tools/compile/Compile.cpp":
        legacy_source = root / "tools/run/carts-compile.cpp"
        if legacy_source.is_file():
            source = legacy_source
    if not source.is_file():
        print(f"error: source not found: {source}", file=sys.stderr)
        return 2

    try:
        doc_paths = _resolve_paths(args.docs, root)
    except FileNotFoundError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    compile_flags = _extract_compile_flags(source)
    documented = _extract_documented_flags(doc_paths)

    unknown = sorted(flag for flag in documented if flag not in compile_flags)
    if not unknown:
        print("OK: documented long flags map to carts-compile options.")
        print(f"Checked {len(doc_paths)} docs, {len(documented)} distinct flags.")
        return 0

    print("Found undocumented or stale flags in docs:")
    for flag in unknown:
        locations = ", ".join(
            _format_location(path, line, root) for path, line in documented[flag][:3]
        )
        print(f"  {flag}: {locations}")

    print(
        "Hint: update docs or carts-compile options to keep them consistent.",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
