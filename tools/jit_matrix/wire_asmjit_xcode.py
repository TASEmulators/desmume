#!/usr/bin/env python3
"""Wire the vendored asmjit into the Cocoa Xcode project.

Xcode has no CMake, so the source list has to live in project.pbxproj. Doing
that by hand is 54 files x N targets of copy-paste, and a single typo costs an
afternoon of debugging. This script edits the project instead:

  * adds a PBXFileReference + PBXBuildFile for every compiled asmjit source
    (the list comes from gen_asmjit_sources.py, the same one the vcxproj and
    Makefile.am use);
  * compiles them with -std=c++20 through a per-file COMPILER_FLAGS, because
    the project standard is gnu++0x and asmjit needs C++17;
  * gives arm_jit.cpp / arm_jit_arm.cpp the same per-file flag, since they
    include asmjit headers;
  * adds "$(SRCROOT)/../../../3rdparty" to HEADER_SEARCH_PATHS (upstream
    includes <asmjit/core.h>) and ASMJIT_STATIC to the preprocessor defines;
  * turns GCC_PRECOMPILE_PREFIX_HEADER off for those targets. The prefix header
    defines HAVE_JIT and the platform macros, so it must still be included, but
    a PCH built with gnu++0x cannot be used by a TU compiled with -std=c++20.

Scope: targets that compile a JIT source, plus the Universal target. Universal
needs both backend TUs in its Sources phase; each C++ source uses an
architecture guard so only the matching slice emits any code.

The script is idempotent: running it twice changes nothing.

Usage:
    python tools/jit_matrix/wire_asmjit_xcode.py --check   # read-only report
    python tools/jit_matrix/wire_asmjit_xcode.py --apply
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_asmjit_sources as gen  # noqa: E402

PBXPROJ = (gen.PROJECT / "src" / "frontend" / "cocoa" /
           "DeSmuME (Latest).xcodeproj" / "project.pbxproj")

# Relative from the .xcodeproj directory (SRCROOT = .../src/frontend/cocoa)
# back to desmume/, which is the include root asmjit expects.
INCLUDE_ROOT = "$(SRCROOT)/../../../3rdparty"
SOURCE_ROOT_PREFIX = "../../../3rdparty/asmjit/src/asmjit"

CXX_FLAGS = "-std=c++20"
DEFINE = "ASMJIT_STATIC"

JIT_FILES = ("arm_jit.cpp", "arm_jit_arm.cpp")


def ref_path(project_rel: str) -> str:
    """PROJECT-relative source path -> path written into the pbxproj.

    References use sourceTree = SOURCE_ROOT, i.e. they are relative to the
    directory holding the .xcodeproj (src/frontend/cocoa).
    """
    target = (gen.PROJECT / project_rel).resolve()
    base = PBXPROJ.parent.parent.resolve()
    return Path(os.path.relpath(str(target), str(base))).as_posix()

RE_OBJECT = re.compile(r"^\t\t([0-9A-F]{24}) /\* (.+?) \*/ = \{", re.M)


def load_objects(text: str) -> dict[str, tuple[str, str, int, int]]:
    """id -> (display name, body, body_start, body_end).

    Body excludes the outer braces. Handles the minified objects this project
    contains (one whole object on a single line) as well as the usual layout.
    """
    out: dict[str, tuple[str, str, int, int]] = {}
    for m in RE_OBJECT.finditer(text):
        oid, name = m.group(1), m.group(2)
        start = m.end()
        depth, i = 1, start
        while i < len(text) and depth:
            c = text[i]
            if c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
            i += 1
        out[oid] = (name, text[start:i - 1], start, i - 1)
    return out


def list_span(body: str, key: str) -> tuple[int, int] | None:
    """Span of the values inside `KEY = ( ... );`.

    Parentheses inside the values ("$(inherited)", "$(SRCROOT)/...") are
    balanced, so the closing paren is found by depth, not by str.find.
    """
    m = re.search(re.escape(key) + r"\s*=\s*\(", body)
    if not m:
        return None
    start = m.end()
    depth = 1
    i = start
    while i < len(body) and depth:
        if body[i] == "(":
            depth += 1
        elif body[i] == ")":
            depth -= 1
        i += 1
    return start, i - 1


def list_entries(body: str, key: str) -> list[str] | None:
    span = list_span(body, key)
    if span is None:
        return None
    start, end = span
    return [v.strip() for v in body[start:end].split(",") if v.strip()]


def add_list_entry(body: str, key: str, value: str) -> tuple[str, bool]:
    """Ensure `value` is in the KEY = (...) list. Returns (body, changed).

    Values with `$` or `/` are quoted; the duplicate check compares unquoted,
    so re-running the script does not add a second copy.
    """
    literal = '"%s"' % value if ("$" in value or "/" in value) else value
    span = list_span(body, key)
    if span is not None:
        start, end = span
        if value in [v.strip().strip('"') for v in body[start:end].split(",")]:
            return body, False
        return body[:end] + "\t\t\t\t\t" + literal + ",\n\t\t\t\t" + body[end:], True

    # key absent (or a scalar): insert a fresh list right after buildSettings = {
    anchor = body.find("buildSettings = {")
    if anchor < 0:
        raise SystemExit("object has no buildSettings block:\n" + body[:200])
    at = anchor + len("buildSettings = {")
    ins = ("\n\t\t\t\t%s = (\n\t\t\t\t\t\"$(inherited)\",\n\t\t\t\t\t%s,\n\t\t\t\t);"
           % (key, literal))
    return body[:at] + ins + body[at:], True


def set_scalar(body: str, key: str, value: str) -> tuple[str, bool]:
    m = re.search(re.escape(key) + r"\s*=\s*([^;]+);", body)
    if m:
        if m.group(1).strip() == value:
            return body, False
        return body[:m.start(1)] + value + body[m.end(1):], True
    anchor = body.find("buildSettings = {")
    at = anchor + len("buildSettings = {")
    return body[:at] + "\n\t\t\t\t%s = %s;" % (key, value) + body[at:], True


class Editor:
    def __init__(self, text: str):
        self.text = text
        self.objects = load_objects(text)
        self.counter = 0
        self.used = set(self.objects)

    def new_id(self) -> str:
        while True:
            self.counter += 1
            oid = "A5F%021X" % self.counter
            if oid not in self.used:
                self.used.add(oid)
                return oid

    def replace_body(self, oid: str, new_body: str) -> None:
        name, body, start, end = self.objects[oid]
        self.text = self.text[:start] + new_body + self.text[end:]
        # offsets after this object shift; reload lazily by rebuilding
        self.objects = load_objects(self.text)

    def insert_before_section_end(self, section: str, block: str) -> None:
        marker = "/* End %s section */" % section
        at = self.text.find(marker)
        if at < 0:
            raise SystemExit("no section " + section)
        self.text = self.text[:at] + block + self.text[at:]
        self.objects = load_objects(self.text)


def sources_phase_files(editor: Editor) -> dict[str, list[str]]:
    """phase id -> file names it compiles."""
    refs = {}
    build_files = {}
    for oid, (name, body, _s, _e) in editor.objects.items():
        if "isa = PBXFileReference;" in body:
            m = re.search(r"name = \"?([^\";]+)\"?;", body) or re.search(r"path = \"?([^\";]+)\"?;", body)
            if m:
                refs[oid] = m.group(1)
        elif "isa = PBXBuildFile;" in body:
            m = re.search(r"fileRef = ([0-9A-F]{24})", body)
            if m:
                build_files[oid] = m.group(1)

    out = {}
    for oid, (name, body, _s, _e) in editor.objects.items():
        if "isa = PBXSourcesBuildPhase;" not in body:
            continue
        names = []
        for bid in re.findall(r"([0-9A-F]{24}) /\* .*? in Sources \*/", body):
            ref = build_files.get(bid)
            if ref:
                names.append(refs.get(ref, "?"))
        out[oid] = names
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true", help="write the project back")
    ap.add_argument("--check", action="store_true", help="report only (default)")
    args = ap.parse_args()

    compiled, _skipped = gen.classified()
    rel_names = list(compiled)          # PROJECT-relative, e.g. 3rdparty/asmjit/src/...
    stems = {r.rsplit("/", 1)[1] for r in rel_names}

    text = PBXPROJ.read_text(encoding="utf-8")
    editor = Editor(text)
    editor.objects = load_objects(text)

    phases = sources_phase_files(editor)

    # targets that compile a JIT source, plus Universal (both backends are in
    # its Sources phase but excluded from each other's architecture slice)
    jit_targets = []
    for oid, (name, body, _s, _e) in editor.objects.items():
        if "isa = PBXNativeTarget;" not in body:
            continue
        tname = re.search(r"name = \"([^\"]*)\";", body)
        tname = tname.group(1) if tname else oid
        phase_ids = re.findall(r"([0-9A-F]{24}) /\* Sources \*/", body)
        files = [f for p in phase_ids for f in phases.get(p, [])]
        # Universal and OpenEmu used to pull the JIT in through the
        # UniversalBuildInterface.cpp #include shim; that shim is retired, so
        # they need the same direct source registration as everyone else.
        uses_shim = "Universal" in tname or "OpenEmu Plug-in" in tname
        if any(f in JIT_FILES for f in files) or uses_shim:
            cfg_list = re.search(r"buildConfigurationList = ([0-9A-F]{24})", body)
            jit_targets.append((tname, phase_ids,
                                cfg_list.group(1) if cfg_list else None,
                                files, uses_shim))

    print("asmjit sources to wire : %d" % len(rel_names))
    print("targets wiring JIT     : %d" % len(jit_targets))
    for tname, phase_ids, _cl, files, shim in jit_targets:
        present = ", ".join(f for f in files if f in JIT_FILES) or "(none yet)"
        print("   %-62s %s%s" % (tname, present, "  [ex-shim]" if shim else ""))

    if args.check or not args.apply:
        # report what is missing without touching anything
        existing = set()
        for oid, (name, body, _s, _e) in editor.objects.items():
            if "isa = PBXFileReference;" in body:
                m = re.search(r"path = \"?([^\";]+)\"?;", body)
                if m:
                    existing.add(m.group(1))
        missing = [r for r in rel_names if ref_path(r) not in existing]
        print("\nmissing asmjit file refs: %d" % len(missing))
        if missing:
            print("   e.g. %s" % ", ".join(missing[:3]))
        return 0 if not missing else 1

    # ---------------------------------------------------------------- file refs
    ref_ids: dict[str, str] = {}
    existing_refs: dict[str, str] = {}
    for oid, (name, body, _s, _e) in editor.objects.items():
        if "isa = PBXFileReference;" in body:
            m = re.search(r"path = \"?([^\";]+)\"?;", body)
            if m:
                existing_refs[m.group(1)] = oid

    ref_lines = []
    for rel in rel_names:
        full = ref_path(rel)
        if full in existing_refs:
            ref_ids[rel] = existing_refs[full]
            continue
        rid = editor.new_id()
        ref_ids[rel] = rid
        base = rel.rsplit("/", 1)[1]
        ref_lines.append(
            '\t\t%s /* %s */ = {isa = PBXFileReference; fileEncoding = 4; '
            'lastKnownFileType = sourcecode.cpp.cpp; name = %s; path = %s; '
            'sourceTree = SOURCE_ROOT; };\n' % (rid, base, base, full)
        )

    if ref_lines:
        editor.insert_before_section_end("PBXFileReference", "".join(ref_lines))

    # --------------------------------------------------------------- build files
    # The two backend TUs already have file references from the 10 other JIT
    # targets; Universal only needs new PBXBuildFile entries pointing at them.
    backend_refs: dict[str, str] = {}
    for oid, (name, body, _s, _e) in editor.objects.items():
        if "isa = PBXFileReference;" in body:
            m = re.search(r"path = \"?([^\";]+)\"?;", body)
            if m:
                base = m.group(1).rsplit("/", 1)[-1]
                if base in JIT_FILES:
                    backend_refs[base] = oid

    build_lines = []
    build_ids: dict[tuple[str, str], str] = {}
    for tname, phase_ids, _cl, _files, uses_shim in jit_targets:
        for phase in phase_ids:
            body = editor.objects[phase][1]
            have = set()
            for bid in re.findall(r"([0-9A-F]{24}) /\* [^*]+ in Sources \*/", body):
                ref = re.search(r"fileRef = ([0-9A-F]{24})",
                                editor.objects.get(bid, ("", "", 0, 0))[1])
                if ref:
                    have.add(ref.group(1))
            new_entries = []
            for rel in rel_names:
                if ref_ids[rel] in have:
                    continue
                bid = editor.new_id()
                base = rel.rsplit("/", 1)[1]
                build_ids[(phase, rel)] = bid
                build_lines.append(
                    '\t\t%s /* %s in Sources */ = {isa = PBXBuildFile; fileRef = %s /* %s */; '
                    'settings = {COMPILER_FLAGS = "%s"; }; };\n'
                    % (bid, base, ref_ids[rel], base, CXX_FLAGS)
                )
                new_entries.append('\t\t\t\t%s /* %s in Sources */,\n' % (bid, base))

            # Universal compiles both backends; the per-arch exclusion added
            # below decides which one each slice actually builds. No explicit
            # COMPILER_FLAGS here -- the generic C++20 pass below adds it.
            if uses_shim:
                for base in JIT_FILES:
                    ref = backend_refs.get(base)
                    if not ref or ref in have:
                        continue
                    bid = editor.new_id()
                    build_lines.append(
                        '\t\t%s /* %s in Sources */ = {isa = PBXBuildFile; '
                        'fileRef = %s /* %s */; };\n'
                        % (bid, base, ref, base)
                    )
                    new_entries.append('\t\t\t\t%s /* %s in Sources */,\n' % (bid, base))

            if new_entries:
                at = body.rfind(");")
                editor.replace_body(phase, body[:at] + "".join(new_entries) + body[at:])

    if build_lines:
        editor.insert_before_section_end("PBXBuildFile", "".join(build_lines))

    # ------------------------------------- C++20 on the JIT translation units
    for oid, (name, body, _s, _e) in list(editor.objects.items()):
        if "isa = PBXBuildFile;" not in body:
            continue
        if "COMPILER_FLAGS" in body:
            continue
        ref = re.search(r"fileRef = ([0-9A-F]{24})", body)
        if not ref:
            continue
        refname = editor.objects.get(ref.group(1), ("", "", 0, 0))[0]
        if not any(f in refname for f in JIT_FILES):
            continue
        # bodies exclude the outer braces; append the per-file compiler flag
        new_body = body.rstrip() + ' settings = {COMPILER_FLAGS = "%s"; }; ' % CXX_FLAGS
        editor.replace_body(oid, new_body)

    # ------------------------------------------- drop the old AsmJit 1.0 copy
    # src/utils/AsmJit is kept in the tree for reference but must not be
    # compiled: its class names (asmjit::Assembler, ...) collide with the
    # vendored master, so a target that compiles both fails to link. The old
    # entries are recognisable as "same basename as a vendored source, but the
    # path is not under 3rdparty".
    dropped = []
    for tname, phase_ids, _cl, _files, _uni in jit_targets:
        for phase in phase_ids:
            body = editor.objects[phase][1]
            remove = []
            for bid, base in re.findall(r"\t\t\t\t([0-9A-F]{24}) /\* ([^*]+) in Sources \*/,\n", body):
                ref = re.search(r"fileRef = ([0-9A-F]{24})",
                                editor.objects.get(bid, ("", "", 0, 0))[1])
                path = ""
                if ref:
                    m = re.search(r"path = \"?([^\";]+)\"?;", editor.objects[ref.group(1)][1])
                    path = m.group(1) if m else ""
                if base.strip() in stems and "3rdparty/" not in path:
                    remove.append(bid)
            if not remove:
                continue
            for bid in remove:
                dropped.append(bid)
                body = re.sub(r"\t\t\t\t" + bid + r" /\* [^*]+ in Sources \*/,\n", "", body)
            editor.replace_body(phase, body)

    # ------------------------------------------------------- build settings
    for tname, _phase_ids, cfg_list, _files, _uses_shim in jit_targets:
        if not cfg_list:
            continue
        body = editor.objects[cfg_list][1]
        for cid in re.findall(r"([0-9A-F]{24}) /\* ([^*]+) \*/", body):
            conf_id = cid[0]
            cbody = editor.objects[conf_id][1]
            if "isa = XCBuildConfiguration;" not in cbody:
                continue
            new_body, _c1 = add_list_entry(cbody, "HEADER_SEARCH_PATHS", INCLUDE_ROOT)
            new_body, _c2 = add_list_entry(new_body, "GCC_PREPROCESSOR_DEFINITIONS", DEFINE)
            new_body, _c3 = set_scalar(new_body, "GCC_PRECOMPILE_PREFIX_HEADER", "NO")
            if new_body != cbody:
                editor.replace_body(conf_id, new_body)

    # ---------------------------------------------------------------- group
    have_group = any("isa = PBXGroup;" in b and re.search(r"\n\t\t\tname = asmjit;", b)
                     for _n, b, _s, _e in editor.objects.values())
    group_id = editor.new_id()
    children = "".join('%s /* %s */,\n' % (ref_ids[r], r.rsplit("/", 1)[1]) for r in rel_names)
    group = ('\t\t%s /* asmjit */ = {\n\t\t\tisa = PBXGroup;\n\t\t\tchildren = (\n%s'
             '\t\t\t);\n\t\t\tname = asmjit;\n\t\t\tsourceTree = "<group>";\n\t\t};\n'
             % (group_id, children))
    if not have_group:
        editor.insert_before_section_end("PBXGroup", group)

        # attach it to the main group
        main_group = re.search(r"mainGroup = ([0-9A-F]{24})", editor.text).group(1)
        body = editor.objects[main_group][1]
        at = body.find("children = (") + len("children = (")
        editor.replace_body(main_group, body[:at] + group_id + " /* asmjit */," + body[at:])

    # newline="" keeps the project LF-only; Xcode writes LF and a CRLF file
    # would turn every line into a diff.
    with open(PBXPROJ, "w", encoding="utf-8", newline="") as fh:
        fh.write(editor.text)

    print("\nwrote %s" % PBXPROJ)
    print("  file refs added   : %d" % len(ref_lines))
    print("  build files added : %d" % len(build_lines))
    print("  old AsmJit 1.0 entries dropped: %d" % len(dropped))
    return 0


if __name__ == "__main__":
    sys.exit(main())
