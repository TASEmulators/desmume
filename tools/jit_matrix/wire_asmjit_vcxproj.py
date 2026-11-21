#!/usr/bin/env python3
"""Wire the vendored asmjit into the Windows project (DeSmuME.vcxproj).

Same job as wire_asmjit_xcode.py, for MSBuild:

  * the 69 src/utils/AsmJit entries (AsmJit 1.0) are gone; the 54 vendored
    sources take their place, each with <LanguageStandard>stdcpp20</LanguageStandard>
    because the project standard is stdcpp17/Default and asmjit needs C++17
    (the whole point is that arm_jit.cpp uses C++20 features through it);
  * arm_jit.cpp gets the same per-file standard;
  * desmume.props gains the include root and ASMJIT_STATIC.

The source list comes from gen_asmjit_sources.py, so everything stays in sync.

    python tools/jit_matrix/wire_asmjit_vcxproj.py --check
    python tools/jit_matrix/wire_asmjit_vcxproj.py --apply
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_asmjit_sources as gen  # noqa: E402

WINDOWS = gen.PROJECT / "src" / "frontend" / "windows"
VCXPROJ = WINDOWS / "DeSmuME.vcxproj"
PROPS = WINDOWS / "desmume.props"

# desmume/src/frontend/windows -> desmume/
VENDOR_REL = r"..\..\..\3rdparty"
OLD_PREFIX = r"..\..\utils\AsmJit"


def new_entries() -> list[str]:
    compiled, _ = gen.classified()
    out = []
    for rel in compiled:
        win = (VENDOR_REL + "\\" + rel.split("3rdparty/", 1)[1]).replace("/", "\\")
        out.append(
            '    <ClCompile Include="%s">\n'
            "      <LanguageStandard>stdcpp20</LanguageStandard>\n"
            "    </ClCompile>\n" % win
        )
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--check", action="store_true", help="report only (default)")
    args = ap.parse_args()

    text = VCXPROJ.read_text(encoding="utf-8-sig")
    entries = new_entries()

    old = re.findall(r"    <ClCompile Include=\"" + re.escape(OLD_PREFIX) + r"[^\"]*\" />\n", text)
    have_new = len(re.findall(r"3rdparty\\asmjit\\", text))

    print("vendored sources            : %d" % len(entries))
    print("AsmJit 1.0 ClCompile entries: %d" % len(old))
    print("vendored entries present    : %d" % have_new)
    print("arm_jit.cpp C++20           : %s"
          % ("yes" if re.search(r'Include="\.\.\\\.\.\\arm_jit\.cpp">\s*\n\s*<LanguageStandard>stdcpp20',
                                text) else "no"))

    if not args.apply:
        return 0 if (have_new == len(entries) and not old) else 1

    # 1. drop the old library's sources
    text, n_old = re.subn(r"    <ClCompile Include=\"" + re.escape(OLD_PREFIX) + r"[^\"]*\" />\n",
                          "", text)
    # 2. put the vendored ones where they used to be (just after advanscene.cpp)
    anchor = '    <ClCompile Include="..\\..\\utils\\advanscene.cpp" />\n'
    if anchor not in text:
        raise SystemExit("anchor not found: " + anchor)
    text = text.replace(anchor, anchor + "".join(entries), 1)

    # 3. arm_jit.cpp keeps the same per-file standard
    text = text.replace(
        '    <ClCompile Include="..\\..\\arm_jit.cpp" />\n',
        '    <ClCompile Include="..\\..\\arm_jit.cpp">\n'
        "      <LanguageStandard>stdcpp20</LanguageStandard>\n"
        "    </ClCompile>\n", 1)

    VCXPROJ.write_text(text, encoding="utf-8")

    # 4. include root + ASMJIT_STATIC, shared by every Windows project
    props = PROPS.read_text(encoding="utf-8-sig")
    changed = 0
    if r"..\..\..\3rdparty;" not in props:
        props = props.replace(
            r"..\..\libretro-common\include;",
            r"..\..\libretro-common\include;..\..\..\3rdparty;", 1)
        changed += 1
    if "ASMJIT_STATIC" not in props:
        props = props.replace("HAVE_LIBAGG=1;HAVE_JIT=1;",
                              "HAVE_LIBAGG=1;HAVE_JIT=1;ASMJIT_STATIC=1;", 1)
        changed += 1
    PROPS.write_text(props, encoding="utf-8")

    print("\nwrote %s (removed %d, added %d)" % (VCXPROJ.name, n_old, len(entries)))
    print("wrote %s (%d setting(s))" % (PROPS.name, changed))
    return 0


if __name__ == "__main__":
    sys.exit(main())
