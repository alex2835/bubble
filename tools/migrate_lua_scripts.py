#!/usr/bin/env python3
"""Rewrite .lua game scripts for the snake_case Lua API.

The engine's Lua API was PascalCase (CreateEntity, entity:GetTransform(),
Component.Tag, KeyboardKey.SPACE) and is now snake_case. Type names are
unchanged (Entity, Transform, RayHitResult, vec3, ...).

    python tools/migrate_lua_scripts.py path/to/scripts          # preview
    python tools/migrate_lua_scripts.py path/to/scripts --write  # apply

Renames are applied by category, never as a blind find-replace:

  global        a bare identifier, e.g. CreateEntity()  -> create_entity()
  member        only after '.' or ':',  e.g. :GetTag()  -> :get_tag()
  enum:<Table>  only after '<Table>.',  KeyboardKey.A   -> KeyboardKey.a

That distinction matters: `A` is a KeyboardKey member, so rewriting it as a
bare word would corrupt every `A` in the file.

Strings and comments are left alone.
"""

import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MAP_FILE = os.path.join(HERE, "lua_api_rename_map.txt")


def load_map():
    globals_, members, enums = {}, {}, {}
    if not os.path.exists(MAP_FILE):
        sys.exit("missing %s" % MAP_FILE)
    with open(MAP_FILE, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) != 3:
                continue
            cat, old, new = parts
            if cat == "global":
                globals_[old] = new
            elif cat == "member":
                members[old] = new
            elif cat.startswith("enum:"):
                enums.setdefault(cat[5:], {})[old] = new
    return globals_, members, enums


# Lua strings and comments, so their contents can be skipped.
SKIP = re.compile(
    r'--\[(=*)\[.*?\]\1\]'      # long comment
    r'|--[^\n]*'                # line comment
    r'|\[(=*)\[.*?\]\2\]'       # long string
    r'|"(?:\\.|[^"\\])*"'       # double-quoted
    r"|'(?:\\.|[^'\\])*'",      # single-quoted
    re.S)


def protected_spans(text):
    return [m.span() for m in SKIP.finditer(text)]


def migrate(text, globals_, members, enums):
    spans = protected_spans(text)

    def guarded(pos):
        return any(a <= pos < b for a, b in spans)

    edits = []

    # enum members: only when qualified by their own table.
    for table, mapping in enums.items():
        pattern = re.compile(r'\b(%s\s*\.\s*)([A-Za-z_]\w*)\b' % re.escape(table))
        for m in pattern.finditer(text):
            if guarded(m.start(2)):
                continue
            new = mapping.get(m.group(2))
            if new and new != m.group(2):
                edits.append((m.start(2), m.end(2), m.group(2), new))

    # members: only after '.' or ':'.
    member_pat = re.compile(r'(?<=[.:])\s*([A-Za-z_]\w*)\b')
    for m in member_pat.finditer(text):
        if guarded(m.start(1)):
            continue
        # An enum table access is handled above; do not double-edit.
        new = members.get(m.group(1))
        if new and new != m.group(1):
            edits.append((m.start(1), m.end(1), m.group(1), new))

    # globals: bare identifiers not preceded by '.' or ':'.
    for old, new in globals_.items():
        pattern = re.compile(r'(?<![.:\w])\b%s\b' % re.escape(old))
        for m in pattern.finditer(text):
            if guarded(m.start()):
                continue
            edits.append((m.start(), m.end(), old, new))

    if not edits:
        return text, []

    edits.sort()
    out, last, applied = [], 0, []
    for start, end, old, new in edits:
        if start < last:
            continue
        out.append(text[last:start])
        out.append(new)
        applied.append((old, new))
        last = end
    out.append(text[last:])
    return "".join(out), applied


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("path", help="a .lua file, or a directory to walk")
    ap.add_argument("--write", action="store_true", help="apply changes (default: preview)")
    args = ap.parse_args()

    globals_, members, enums = load_map()

    files = []
    if os.path.isfile(args.path):
        files = [args.path]
    else:
        for root, _, names in os.walk(args.path):
            files += [os.path.join(root, n) for n in sorted(names) if n.endswith(".lua")]

    if not files:
        print("no .lua files under %s" % args.path)
        return 0

    total = 0
    for path in files:
        with open(path, "rb") as f:
            raw = f.read().decode("utf-8", "replace")
        nl = "\r\n" if "\r\n" in raw else "\n"
        text = raw.replace("\r\n", "\n")

        new_text, applied = migrate(text, globals_, members, enums)
        if not applied:
            continue

        total += len(applied)
        counts = {}
        for old, new in applied:
            counts[(old, new)] = counts.get((old, new), 0) + 1
        print("%s  (%d)" % (path, len(applied)))
        for (old, new), n in sorted(counts.items()):
            print("    %-34s -> %-34s x%d" % (old, new, n))

        if args.write:
            out = new_text.replace("\n", nl) if nl == "\r\n" else new_text
            with open(path, "wb") as f:
                f.write(out.encode("utf-8"))

    if total == 0:
        print("nothing to change in %d file(s)" % len(files))
    elif args.write:
        print("\napplied %d rename(s)" % total)
    else:
        print("\n%d rename(s) would be applied - re-run with --write" % total)
    return 0


if __name__ == "__main__":
    sys.exit(main())
