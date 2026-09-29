#!/usr/bin/env python3
"""
Parse host clang -ast-dump output from SDL2 headers and emit a JSON list of
(name, return type, args, signature) pairs.

Usage:
  python3 tools/parse_sdl2_funcs.py /tmp/sdl2-ast.txt /tmp/sdl2-funcs.json
"""
import re
import sys
import json

TOP_RE = re.compile(
    r'^\|-FunctionDecl\s+0x[0-9a-f]+\s+<.*?>\s+'
    r'(?:line:\d+:\d+|col:\d+)\s+'
    r'(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s+'
    r"'(?P<sig>[^']+)'"
    r'\s*(?P<storage>extern|used)?\s*$'
)


def parse_ast_dump(path):
    funcs = []
    seen = set()
    with open(path, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            # Only top-level declarations matter. Lines not starting with '|-' are nested.
            if not line.startswith('|-'):
                continue
            # Drop static/inline redeclarations and compiler builtins.
            if 'static inline' in line or ' implicit ' in line or ' prev ' in line:
                continue

            m = TOP_RE.match(line)
            if not m:
                continue

            name = m.group('name')
            # We only care about SDL public API symbols; this also eliminates system functions.
            if not name.startswith('SDL_'):
                continue
            if name in seen:
                continue
            seen.add(name)

            full_type = m.group('sig').strip()
            if not full_type.endswith(')'):
                continue

            # Split return type from parameter list by finding the outermost '('.
            depth = 0
            paren_pos = -1
            for i in range(len(full_type) - 1, -1, -1):
                c = full_type[i]
                if c == ')':
                    depth += 1
                elif c == '(':
                    depth -= 1
                    if depth == 0:
                        paren_pos = i
                        break
            if paren_pos < 0:
                continue

            ret = full_type[:paren_pos].strip()
            args = full_type[paren_pos + 1:-1].strip()

            funcs.append({
                'name': name,
                'ret': ret,
                'args': args,
                'sig': full_type,
            })

    return funcs


def main():
    ast_path = sys.argv[1]
    out_path = sys.argv[2]
    funcs = parse_ast_dump(ast_path)
    with open(out_path, 'w', encoding='utf-8') as f:
        json.dump(funcs, f, indent=2)
    print(f"Parsed {len(funcs)} SDL2 functions to {out_path}")


if __name__ == '__main__':
    main()
