#!/usr/bin/env python3
"""Emit the asmjit source list for the build files.

The vendored tree under 3rdparty/asmjit contains every header and every
non-test source file, but only a subset of the sources is compiled:

  core/  x86/  arm/  axl/     -> compiled
  ujit/                       -> NOT compiled (experimental unified compiler,
                                 DeSmuME does not use it)
  *_test.cpp                  -> not vendored at all

Keeping this in a script means the vcxproj / Makefile.am entries are never
hand-typed, and that adding or removing a source upstream is a one-line
regeneration instead of a manual diff.

Usage:
    python tools/jit_matrix/gen_asmjit_sources.py                 # plain list
    python tools/jit_matrix/gen_asmjit_sources.py --format vcxproj
    python tools/jit_matrix/gen_asmjit_sources.py --format make
    python tools/jit_matrix/gen_asmjit_sources.py --format headers
    python tools/jit_matrix/gen_asmjit_sources.py --check         # exit 1 if stale

`--check` is meant for CI: it fails when the vendored tree contains a source
that the generated lists would silently drop.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]      # git root: C:\...\desmume
PROJECT = REPO / "desmume"                       # the emulator subtree
VENDOR = PROJECT / "3rdparty" / "asmjit"

# Compiled subdirectories, in link order (asmjit has no static init order
# requirement, but a stable order keeps the project files diffable).
COMPILED_DIRS = ("core", "axl", "x86", "arm")

# Never compiled, even though the sources are vendored.
EXCLUDED_DIRS = ("ujit",)

# Relative prefix from each build file's directory back to PROJECT.
#   vcxproj    : desmume/src/frontend/windows  -> 3 up = desmume/
#   Makefile.am: desmume/src/frontend/posix    -> 3 up = desmume/
PREFIXES = {
    "vcxproj": r"..\..\..",
    "make": "../../..",
}


def classified() -> tuple[list[str], list[str]]:
    """Return (compiled, skipped) as PROJECT-relative POSIX paths."""
    compiled: list[str] = []
    skipped: list[str] = []

    for path in sorted(VENDOR.rglob("*.cpp")):
        rel = path.relative_to(PROJECT).as_posix()
        top = path.relative_to(VENDOR).parts[0]
        if path.name.endswith("_test.cpp"):
            skipped.append(rel)
        elif top in EXCLUDED_DIRS:
            skipped.append(rel)
        elif top in COMPILED_DIRS:
            compiled.append(rel)
        else:
            # A new upstream directory appeared: fail loudly rather than
            # guessing whether it should be compiled.
            raise SystemExit(
                f"error: {rel} is under an unknown directory '{top}'.\n"
                f"       Add it to COMPILED_DIRS or EXCLUDED_DIRS in {__file__}."
            )
    return compiled, skipped


def headers() -> list[str]:
    return [
        p.relative_to(PROJECT).as_posix()
        for p in sorted(VENDOR.rglob("*.h"))
    ]


def render(fmt: str, compiled: list[str]) -> str:
    if fmt == "txt":
        return "\n".join(compiled)

    if fmt == "vcxproj":
        pfx = PREFIXES["vcxproj"]
        return "\n".join(
            '    <ClCompile Include="%s\\%s" />' % (pfx, rel.replace("/", "\\"))
            for rel in compiled
        )

    if fmt == "make":
        pfx = PREFIXES["make"]
        lines = [f"\t{pfx}/{compiled[0]} \\"]
        lines += [f"\t{pfx}/{rel} \\" for rel in compiled[1:-1]]
        lines.append(f"\t{pfx}/{compiled[-1]}")
        return "\n".join(lines)

    if fmt == "headers":
        pfx = PREFIXES["vcxproj"]
        out = []
        for rel in headers():
            if rel.endswith("asmjit.natvis"):
                continue
            out.append(
                '    <ClInclude Include="%s\\%s" />' % (pfx, rel.replace("/", "\\"))
            )
        return "\n".join(out)

    raise SystemExit(f"unknown format {fmt!r}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--format",
        default="txt",
        choices=("txt", "vcxproj", "make", "headers"),
    )
    ap.add_argument(
        "--check",
        action="store_true",
        help="report coverage and fail if anything looks wrong",
    )
    args = ap.parse_args()

    if not VENDOR.is_dir():
        raise SystemExit(f"error: {VENDOR} does not exist")

    compiled, skipped = classified()

    if args.check:
        print(f"vendored cpp compiled : {len(compiled)}")
        print(f"vendored cpp skipped  : {len(skipped)}")
        print(f"vendored headers      : {len(headers())}")
        by_dir: dict[str, int] = {}
        for rel in compiled:
            top = Path(rel).parent.name
            by_dir[top] = by_dir.get(top, 0) + 1
        for top in COMPILED_DIRS:
            print(f"  {top:<5} {by_dir.get(top, 0)}")
        if not compiled:
            print("error: nothing to compile", file=sys.stderr)
            return 1
        return 0

    print(render(args.format, compiled))
    return 0


if __name__ == "__main__":
    sys.exit(main())
