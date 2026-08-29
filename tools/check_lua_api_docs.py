#!/usr/bin/env python3
"""Check docs/scripting.md against the Lua bindings in the source.

The scripting docs are hand-written, so they rot the moment someone adds a
binding and forgets the doc - the same failure the generated Component enum was
introduced to avoid. This scans the binding sources for every name registered
into the Lua state and reports the ones the docs never mention.

    python tools/check_lua_api_docs.py

Exits non-zero if anything is undocumented, so it can gate CI.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOC = os.path.join(ROOT, "docs", "scripting.md")
SKILL = os.path.join(ROOT, ".claude", "skills", "bubble-scripting", "SKILL.md")

SOURCES = [
    "projects/engine/src/scripting/bindings/scene_lua_bindings.cpp",
    "projects/engine/src/scripting/bindings/physics_lua_bindings.cpp",
    "projects/engine/src/scripting/bindings/window_input_bindings.cpp",
    "projects/engine/src/scripting/bindings/loader_lua_bindings.cpp",
    "projects/engine/src/scripting/bindings/free_function_lua_bindings.cpp",
    "projects/engine/src/engine.cpp",
    "deps/glm_lua_bindings/src/glm_lua_functions.cpp",
    "deps/glm_lua_bindings/src/glm_lua_vec2.cpp",
    "deps/glm_lua_bindings/src/glm_lua_vec3.cpp",
    "deps/glm_lua_bindings/src/glm_lua_vec4.cpp",
    "deps/glm_lua_bindings/src/glm_lua_mat.cpp",
]

# Component usertypes live one per file.
COMPONENT_DIR = "projects/engine/src/scene/components"

# Registration forms:
#   lua["Name"] = ...            lua.set( "Name", ... )
#   lua.set_function( "Name", )  lua.new_usertype<T>( "Name",
#   SetVar( "Name"sv, ... )      rigidBodyType["Name"] = ...
#   "Name", &Type::member        (usertype member lists)
PATTERNS = [
    re.compile(r'lua\s*\[\s*"([A-Za-z_][A-Za-z0-9_]*)"\s*\]\s*='),
    re.compile(r'lua\.set(?:_function)?\s*\(\s*"([A-Za-z_][A-Za-z0-9_]*)"'),
    re.compile(r'lua\.(?:new_usertype|new_enum)\s*<[^>]*>\s*\(\s*"([A-Za-z_][A-Za-z0-9_]*)"'),
    re.compile(r'SetVar\s*\(\s*"([A-Za-z_][A-Za-z0-9_]*)"'),
    re.compile(r'^\s*"([A-Za-z_][A-Za-z0-9_]*)"\s*,\s*(?:&|\[|sol::property)', re.M),
    re.compile(r'^\s*"([A-Za-z_][A-Za-z0-9_]*)"\s*,\s*$', re.M),
    re.compile(r'\w+Type\s*\[\s*"([A-Za-z_][A-Za-z0-9_]*)"\s*\]\s*='),
]

# Names that exist in the bindings but are deliberately not part of the
# scripting surface a doc reader needs. Keep this list short and justified.
IGNORE = {
    # sol2 plumbing / operator names, not callable identifiers.
    "call_constructor", "to_string", "index", "new_index",
    # Enum tables are emitted from Lua source strings, checked separately.
    "Component", "KeyboardKey", "MouseKey", "LightType",
    # Internal: the per-script OnUpdate slot the engine reads and clears.
    "OnUpdate",
}


def read(rel):
    path = os.path.join(ROOT, rel)
    if not os.path.exists(path):
        return None
    with open(path, "rb") as f:
        return f.read().decode("utf-8", "replace")


def registered_names():
    """Every identifier the engine puts into the Lua state, with its source."""
    found = {}
    files = list(SOURCES)

    comp_dir = os.path.join(ROOT, COMPONENT_DIR)
    if os.path.isdir(comp_dir):
        for name in sorted(os.listdir(comp_dir)):
            if name.endswith(".cpp"):
                files.append(os.path.join(COMPONENT_DIR, name).replace("\\", "/"))

    missing_sources = []
    for rel in files:
        text = read(rel)
        if text is None:
            missing_sources.append(rel)
            continue
        for pattern in PATTERNS:
            for match in pattern.finditer(text):
                found.setdefault(match.group(1), rel)

    return found, missing_sources


def lua_enum_keys():
    """Keys of the enum tables defined as Lua source strings in the bindings."""
    enums = {}
    for rel in SOURCES:
        text = read(rel)
        if text is None:
            continue
        for block in re.finditer(r'R"\((.*?)\)"', text, re.S):
            body = block.group(1)
            head = re.match(r'\s*([A-Za-z_][A-Za-z0-9_]*)\s*=', body)
            if head:
                keys = re.findall(r'^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=', body, re.M)
                enums.setdefault(head.group(1), set()).update(k for k in keys if k != head.group(1))
    return enums


def main():
    doc = read("docs/scripting.md")
    skill = read(".claude/skills/bubble-scripting/SKILL.md")
    if doc is None:
        sys.exit("missing docs/scripting.md")
    if skill is None:
        sys.exit("missing .claude/skills/bubble-scripting/SKILL.md")

    documented = doc + "\n" + skill

    names, missing_sources = registered_names()
    undocumented = sorted(
        (name, src) for name, src in names.items()
        if name not in IGNORE and not re.search(r'\b%s\b' % re.escape(name), documented)
    )

    problems = 0

    if missing_sources:
        print("Binding sources listed in this script no longer exist:")
        for rel in missing_sources:
            print("  %s" % rel)
        print("  -> update SOURCES in tools/check_lua_api_docs.py\n")
        problems += len(missing_sources)

    if undocumented:
        print("Registered in Lua but not mentioned in the docs:")
        width = max(len(n) for n, _ in undocumented)
        for name, src in undocumented:
            print("  %-*s  %s" % (width, name, src))
        print()
        problems += len(undocumented)

    # Enum tables: every key should appear somewhere in the docs, or the doc
    # should at least name the enum and say what it covers.
    for enum, keys in sorted(lua_enum_keys().items()):
        if not re.search(r'\b%s\b' % re.escape(enum), documented):
            print("Enum %s is not mentioned in the docs" % enum)
            problems += 1

    total = len([n for n in names if n not in IGNORE])
    if problems == 0:
        print("ok: %d registered names, all documented" % total)
        return 0

    print("%d problem(s) across %d registered names" % (problems, total))
    return 1


if __name__ == "__main__":
    sys.exit(main())
