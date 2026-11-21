#!/usr/bin/env python3
"""Keep documentation/lua-api/desmume.lua in sync with desmume/src/lua-engine.cpp.

Two things are compared, both derived from the source rather than from a hand
written list:

  * every name of every ``static const struct luaL_Reg <table>[]`` that is passed
    to ``luaL_register(L, "<name>", <table>)``,
  * every global registered with ``lua_register(L, "<name>", ...)``.

The names declared in ``desmume.lua`` are collected from ``function t.f(...)``
lines (plus plain ``function f(...)`` for the globals), so the two sides can be
diffed table by table.

Run it after adding, removing or renaming a function:

    python3 documentation/lua-api/check_api.py

It exits non-zero and prints the difference when either side has a name the
other one does not.  ``--list`` prints the source-side API only, which is handy
when writing the .lua file from scratch.
"""

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
ENGINE = REPO_ROOT / "desmume" / "src" / "lua-engine.cpp"
STUB = Path(__file__).resolve().parent / "desmume.lua"

# The engine registers this table only when the host Lua has no bit library of
# its own (stock Lua 5.1).  LuaJIT ships ``bit``, and lua-language-server
# already declares it for the LuaJIT runtime, so it is deliberately not
# redeclared in desmume.lua.
SKIP_TABLES = {"bit_funcs"}

# print() and tostring() replace Lua built-ins and are already declared by the
# editor's Lua 5.1 stdlib; only their behaviour is documented, in comments.
SKIP_GLOBALS = {"print", "tostring"}

REG_TABLE_RE = re.compile(r"static\s+const\s+struct\s+luaL_Reg\s+(\w+)\s*\[\s*\]\s*=\s*\{(.*?)\n\};", re.S)
ENTRY_RE = re.compile(r'\{\s*"([^"]+)"\s*,\s*\w+\s*\}')
COMMENT_RE = re.compile(r"^\s*//")
LUA_REGISTER_RE = re.compile(r'luaL_register\s*\(\s*L\s*,\s*"([^"]+)"\s*,\s*(\w+)\s*\)')
LUA_REGISTER_GLOBAL_RE = re.compile(r'lua_register\s*\(\s*L\s*,\s*"([^"]+)"')


def source_api():
    """Return {library_name: {entry, ...}} plus the set of global names."""
    text = ENGINE.read_text(encoding="utf-8", errors="replace")

    tables = {}
    for match in REG_TABLE_RE.finditer(text):
        var, body = match.group(1), match.group(2)
        names = set()
        for line in body.splitlines():
            if COMMENT_RE.match(line):
                continue
            for entry in ENTRY_RE.finditer(line):
                names.add(entry.group(1))
        tables[var] = names

    # A table variable may be registered under several library names (agg is
    # registered three times) and a library name may be fed by several tables.
    libraries = {}
    for name, var in LUA_REGISTER_RE.findall(text):
        if var in SKIP_TABLES:
            continue
        libraries.setdefault(name, set()).update(tables.get(var, set()))

    globals_ = {n for n in LUA_REGISTER_GLOBAL_RE.findall(text) if n not in SKIP_GLOBALS}
    return libraries, globals_


def stub_api():
    """Return {library_name: {entry, ...}} plus the set of declared globals."""
    libraries, globals_ = {}, set()
    field_re = re.compile(r"^function\s+(\w+)\.(\w+)\s*\(")
    global_re = re.compile(r"^function\s+(\w+)\s*\(")

    for line in STUB.read_text(encoding="utf-8").splitlines():
        field = field_re.match(line)
        if field:
            libraries.setdefault(field.group(1), set()).add(field.group(2))
            continue
        global_ = global_re.match(line)
        if global_:
            globals_.add(global_.group(1))
    return libraries, globals_


def report():
    src_libs, src_globals = source_api()
    stub_libs, stub_globals = stub_api()

    failures = []
    for name in sorted(set(src_libs) | set(stub_libs)):
        src, stub = src_libs.get(name, set()), stub_libs.get(name, set())
        missing = sorted(src - stub)
        extra = sorted(stub - src)
        if missing:
            failures.append("%s: missing in desmume.lua: %s" % (name, ", ".join(missing)))
        if extra:
            failures.append("%s: declared but not registered: %s" % (name, ", ".join(extra)))

    for name in sorted(src_globals - stub_globals):
        failures.append("global %s: missing in desmume.lua" % name)
    for name in sorted(stub_globals - src_globals):
        failures.append("global %s: declared but not registered" % name)

    failures.extend(annotation_failures())
    return failures


LUA_BUILTIN_TYPES = {
    "any", "boolean", "function", "integer", "lightuserdata", "nil", "number",
    "string", "table", "thread", "userdata", "unknown", "true", "false", "self",
    "fun",
}

FUNC_LINE_RE = re.compile(r"^function\s+(?:(\w+)\.)?(\w+)\s*\((.*?)\)\s*(?:end)?\s*$")


def declared_types():
    """Names introduced with ---@class or ---@alias in the stub."""
    names = set()
    class_re = re.compile(r"^---@(?:class|alias)\s+([\w.]+)")
    for line in STUB.read_text(encoding="utf-8").splitlines():
        match = class_re.match(line)
        if match:
            names.add(match.group(1))
    return names


def first_type(text):
    """The leading type expression of an annotation, without its description.

    Types may contain spaces inside ``<...>`` and ``(...)`` - as in
    ``table<string, number|boolean>`` or ``fun(a:integer):nil`` - so a plain
    split() would eat the description that follows.
    """
    depth = 0
    out = []
    for ch in text:
        if ch in "<({[":
            depth += 1
        elif ch in ">)}]":
            depth -= 1
        elif ch.isspace() and depth <= 0:
            break
        out.append(ch)
    return "".join(out).strip()


def type_names(text, known):
    """Every identifier in a type expression that should be a known type.

    Identifiers immediately followed by ``:`` are the parameter names of a
    ``fun(...)`` signature, not types, so they are skipped.
    """
    names = set()
    for match in re.finditer(r"[A-Za-z_][\w.]*", text):
        ident = match.group(0)
        rest = text[match.end():].lstrip()
        if rest.startswith(":") or rest.startswith("?:"):
            continue
        if ident in LUA_BUILTIN_TYPES or ident in known:
            continue
        names.add(ident)
    return names


def annotation_failures():
    """Cross-check annotations against the signatures they decorate.

    The editor only warns about a few of these; a typo in a ---@param name or in
    a type name would otherwise be silently ignored, which is exactly the class
    of mistake this file exists to catch.
    """
    failures = []
    known = declared_types()
    pending_params = []
    pending_types = set()

    for number, line in enumerate(STUB.read_text(encoding="utf-8").splitlines(), start=1):
        if line.startswith("---@param"):
            rest = line[len("---@param"):].strip()
            parts = rest.split(None, 1)
            if not parts:
                failures.append("line %d: empty @param" % number)
                continue
            pending_params.append(parts[0].rstrip("?"))
            if len(parts) > 1:
                pending_types.add(first_type(parts[1]))
            continue

        if line.startswith("---@return"):
            pending_types.add(first_type(line[len("---@return"):]))
            continue

        if line.startswith("---@type"):
            pending_types.add(first_type(line[len("---@type"):]))
            continue

        if line.startswith("---@field"):
            rest = line[len("---@field"):].strip()
            parts = rest.split(None, 1)
            if len(parts) > 1:
                pending_types.add(first_type(parts[1]))
            continue

        if line.startswith("---@alias"):
            rest = line[len("---@alias"):].strip().split(None, 1)
            if len(rest) > 1:
                pending_types.add(first_type(rest[1]))
            continue

        if line.startswith("---@"):
            continue

        if line.startswith("---"):
            continue

        match = FUNC_LINE_RE.match(line)
        if match:
            raw = match.group(3).strip()
            actual = [p.strip().split("=")[0].strip() for p in raw.split(",") if p.strip()]
            for p in pending_params:
                if p != "..." and p not in actual:
                    failures.append("line %d: @param %s is not a parameter of %s" % (number, p, line))
            for p in actual:
                if p != "..." and p not in pending_params:
                    failures.append("line %d: parameter %s of %s has no @param" % (number, p, line))

        for name in sorted(t for t in pending_types if t):
            for u in sorted(type_names(name, known)):
                failures.append("line %d: unknown type %r" % (number, u))

        pending_params = []
        pending_types = set()

    return failures


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list", action="store_true", help="print the API found in lua-engine.cpp")
    args = parser.parse_args()

    if args.list:
        libs, globals_ = source_api()
        for name in sorted(libs):
            print("%s (%d)" % (name, len(libs[name])))
            for entry in sorted(libs[name]):
                print("    %s" % entry)
        print("globals (%d)" % len(globals_))
        for name in sorted(globals_):
            print("    %s" % name)
        return 0

    failures = report()
    if failures:
        print("desmume.lua is out of sync with lua-engine.cpp:")
        for line in failures:
            print("  " + line)
        return 1

    libs, globals_ = source_api()
    total = sum(len(v) for v in libs.values()) + len(globals_)
    print("OK: %d libraries, %d globals, %d names in sync" % (len(libs), len(globals_), total))
    return 0


if __name__ == "__main__":
    sys.exit(main())
