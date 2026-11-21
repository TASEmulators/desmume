#!/usr/bin/env python3
"""Verify that the Cocoa project is wired for asmjit.

Run this before building on macOS: it catches the mistakes that would otherwise
show up 40 minutes into an Xcode build as "Undefined symbols: asmjit::...".

    python3 tools/jit_matrix/check_xcode_asmjit.py

Exit code 0 = the project looks buildable.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_asmjit_sources as gen                                    # noqa: E402
from wire_asmjit_xcode import (PBXPROJ, JIT_FILES, CXX_FLAGS,       # noqa: E402
                               load_objects, ref_path)

OK = "  ok    "
BAD = "  FAIL  "
problems: list[str] = []


def check(cond: bool, msg: str) -> bool:
    print((OK if cond else BAD) + msg)
    if not cond:
        problems.append(msg)
    return cond


def main() -> int:
    text = PBXPROJ.read_text(encoding="utf-8")
    objects = load_objects(text)

    print("project: %s\n" % PBXPROJ)
    check(text.count("{") == text.count("}"),
          "braces balanced (%d/%d)" % (text.count("{"), text.count("}")))
    check(text.count("(") == text.count(")"),
          "parens balanced (%d/%d)" % (text.count("("), text.count(")")))

    compiled, _ = gen.classified()
    stems = {c.rsplit("/", 1)[1] for c in compiled}

    ref_by_path, refname = {}, {}
    for oid, (name, body, _s, _e) in objects.items():
        if "isa = PBXFileReference;" not in body:
            continue
        m = re.search(r"path = \"?([^\";]+)\"?;", body)
        if m:
            ref_by_path[m.group(1)] = oid
        n = re.search(r"name = \"?([^\";]+)\"?;", body) or m
        refname[oid] = n.group(1) if n else "?"

    missing = []
    for c in compiled:
        full = ref_path(c)
        if full not in ref_by_path:
            missing.append(c)
        elif not (gen.PROJECT / c).exists():
            missing.append(c + " (missing on disk)")
    check(not missing, "all %d asmjit sources referenced%s"
          % (len(compiled), "" if not missing else ": " + ", ".join(missing[:4])))

    build_by_id = {}
    for oid, (name, body, _s, _e) in objects.items():
        if "isa = PBXBuildFile;" in body:
            m = re.search(r"fileRef = ([0-9A-F]{24})", body)
            f = re.search(r"COMPILER_FLAGS = \"?([^\";]+)\"?;", body)
            if m:
                build_by_id[oid] = (m.group(1), f.group(1) if f else "")

    jit_targets = 0
    for oid, (name, body, _s, _e) in objects.items():
        if "isa = PBXNativeTarget;" not in body:
            continue
        tname = re.search(r"name = \"([^\"]*)\";", body)
        tname = tname.group(1) if tname else name
        phase_ids = re.findall(r"([0-9A-F]{24}) /\* Sources \*/", body)

        entries = []
        for p in phase_ids:
            for bid in re.findall(r"([0-9A-F]{24}) /\* .*? in Sources \*/", objects[p][1]):
                ref, flags = build_by_id.get(bid, (None, ""))
                entries.append((refname.get(ref, "?"), flags))

        jit = [(n, f) for n, f in entries if n in JIT_FILES]
        if not jit:
            continue

        jit_targets += 1
        asmjit = [(n, f) for n, f in entries if n in stems]
        cfg_list = re.search(r"buildConfigurationList = ([0-9A-F]{24})", body)
        cfg_ok, cfg_detail = check_settings(objects, cfg_list.group(1) if cfg_list else None)

        print("\n%s" % tname)
        check(len(asmjit) == len(stems), "    %d/%d asmjit sources compiled%s"
              % (len(asmjit), len(stems),
                 "" if len(asmjit) == len(stems) else
                 " (extra: %s)" % ", ".join(sorted({n for n, f in asmjit if CXX_FLAGS not in f})[:4])))
        check(all(CXX_FLAGS in f for _n, f in asmjit),
              "    asmjit sources use %s%s"
              % (CXX_FLAGS, "" if all(CXX_FLAGS in f for _n, f in asmjit) else
                 " (missing: %s)" % ", ".join(n for n, f in asmjit if CXX_FLAGS not in f)))
        check(all(CXX_FLAGS in f for _n, f in jit),
              "    %s use %s" % ("/".join(n for n, _f in jit), CXX_FLAGS))
        check(cfg_ok, "    settings [HeaderSearch|Static|PCH-off]: %s" % cfg_detail)

    check(jit_targets >= 1, "\n%d target(s) compile a JIT backend" % jit_targets)

    print()
    if problems:
        print("%d problem(s)" % len(problems))
        return 1
    print("wiring looks complete")
    return 0


def check_settings(objects, cfg_list: str | None):
    if not cfg_list:
        return False, "none"
    body = objects.get(cfg_list, ("", "", 0, 0))[1]
    ids = [c[0] for c in re.findall(r"([0-9A-F]{24}) /\* ([^*]+) \*/", body)]
    if not ids:
        return False, "none"

    ok = True
    bits = []
    for cid in ids:
        cbody = objects.get(cid, ("", "", 0, 0))[1]
        inc = re.search(r"HEADER_SEARCH_PATHS = \((.*?)\);", cbody, re.S)
        defs = re.search(r"GCC_PREPROCESSOR_DEFINITIONS = \((.*?)\);", cbody, re.S)
        pch = re.search(r"GCC_PRECOMPILE_PREFIX_HEADER = (\w+);", cbody)

        inc_ok = bool(inc) and '"$(SRCROOT)/../../../3rdparty"' in inc.group(1)
        def_ok = bool(defs) and "ASMJIT_STATIC" in defs.group(1)
        pch_ok = bool(pch) and pch.group(1) == "NO"

        name = re.search(r"\n\s*name = ([^;]+);", cbody)
        label = (name.group(1) if name else cid).strip()
        bits.append("%s=%s" % (label, "".join(
            [("H" if inc_ok else "H!"), ("D" if def_ok else "D!"), ("P" if pch_ok else "P!")])))
        ok = ok and inc_ok and def_ok and pch_ok
    return ok, ", ".join(bits)


if __name__ == "__main__":
    sys.exit(main())
