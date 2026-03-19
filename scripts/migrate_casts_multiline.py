#!/usr/bin/env python3
"""Fix remaining multi-line .cast<T>() / .dyn_cast<T>() / .isa<T>() patterns.

Handles chained method patterns like:
    expr
        .getType()
        .cast<MemRefType>()
        .getShape()
->
    cast<MemRefType>(expr
        .getType())
        .getShape()
"""
import re
import sys
import os


def find_expr_start_in_text(text, dot_pos):
    """Walk backwards from dot_pos to find the start of the expression.
    Handles multi-line chained calls."""
    i = dot_pos - 1
    # Skip whitespace (including newlines)
    while i >= 0 and text[i] in ' \t\n\r':
        i -= 1
    if i < 0:
        return -1

    while i >= 0:
        ch = text[i]
        if ch == ')':
            depth = 1
            i -= 1
            while i >= 0 and depth > 0:
                if text[i] == ')':
                    depth += 1
                elif text[i] == '(':
                    depth -= 1
                i -= 1
            continue
        elif ch == ']':
            depth = 1
            i -= 1
            while i >= 0 and depth > 0:
                if text[i] == ']':
                    depth += 1
                elif text[i] == '[':
                    depth -= 1
                i -= 1
            continue
        elif ch == '>':
            if i > 0 and text[i-1] == '-':
                i -= 2
                continue
            depth = 1
            i -= 1
            while i >= 0 and depth > 0:
                if text[i] == '>':
                    depth += 1
                elif text[i] == '<':
                    depth -= 1
                i -= 1
            continue
        elif ch.isalnum() or ch == '_':
            while i >= 0 and (text[i].isalnum() or text[i] == '_'):
                i -= 1
            if i >= 1 and text[i-1:i+1] == '::':
                i -= 2
                continue
            if i >= 1 and text[i-1:i+1] == '->':
                i -= 2
                continue
            if i >= 0 and text[i] == '.':
                i -= 1
                continue
            break
        elif ch == '.':
            i -= 1
            continue
        elif ch == ':' and i > 0 and text[i-1] == ':':
            i -= 2
            continue
        else:
            break

    return i + 1


def process_file(filepath):
    """Process a file for multi-line cast patterns."""
    try:
        with open(filepath, 'r') as f:
            text = f.read()
    except (IOError, UnicodeDecodeError):
        return False

    # Pattern: whitespace.cast_func<Type>()
    pattern = re.compile(r'\.(cast|dyn_cast|dyn_cast_or_null|isa)<([^>]+)>\(\)')

    modified = False
    offset = 0

    while True:
        m = pattern.search(text, offset)
        if not m:
            break

        dot_pos = m.start()  # position of '.'
        func = m.group(1)
        template_arg = m.group(2)
        end_pos = m.end()

        expr_start = find_expr_start_in_text(text, dot_pos)
        if expr_start < 0 or expr_start >= dot_pos:
            offset = end_pos
            continue

        expr = text[expr_start:dot_pos]
        # Strip trailing whitespace from expression (preserve leading indent)
        expr_stripped = expr.rstrip()

        replacement = f'{func}<{template_arg}>({expr_stripped})'
        text = text[:expr_start] + replacement + text[end_pos:]
        modified = True
        offset = expr_start + len(replacement)

    if modified:
        with open(filepath, 'w') as f:
            f.write(text)
        print(f'Updated: {filepath}')

    return modified


def find_cpp_files(directories):
    files = []
    for d in directories:
        for root, dirs, filenames in os.walk(d):
            if 'llvm-project' in root:
                continue
            for fn in filenames:
                if fn.endswith(('.cpp', '.cc', '.h', '.hpp')):
                    files.append(os.path.join(root, fn))
    return sorted(files)


if __name__ == '__main__':
    base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    dirs = [
        os.path.join(base, 'lib', 'arts'),
        os.path.join(base, 'include', 'arts'),
        os.path.join(base, 'external', 'Polygeist', 'lib', 'polygeist'),
        os.path.join(base, 'external', 'Polygeist', 'tools'),
    ]

    files = find_cpp_files(dirs)
    total = 0
    for f in files:
        if process_file(f):
            total += 1
    print(f'\nTotal files modified: {total}')
