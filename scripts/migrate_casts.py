#!/usr/bin/env python3
"""Migrate .cast<T>() / .dyn_cast<T>() / .dyn_cast_or_null<T>() / .isa<T>()
to free functions cast<T>(), dyn_cast<T>(), dyn_cast_or_null<T>(), isa<T>().

Handles:
  expr.cast<Type>()           -> cast<Type>(expr)
  expr.dyn_cast<Type>()       -> dyn_cast<Type>(expr)
  expr.dyn_cast_or_null<Type>() -> dyn_cast_or_null<Type>(expr)
  expr.isa<Type>()            -> isa<Type>(expr)

Where expr can be:
  - simple identifier: val.cast<T>()
  - function call: foo().cast<T>()
  - member call: obj.bar().cast<T>()
  - chained: a.b().cast<T>()
  - subscript: arr[i].cast<T>()
  - complex nested: getOp()->getResult(0).cast<T>()
"""
import re
import sys
import os

CAST_FUNCS = r'(?:cast|dyn_cast|dyn_cast_or_null|isa)'

def find_expr_start(line, dot_pos):
    """Find the start of the expression before .cast<T>().
    Walk backwards from dot_pos handling parens, brackets, arrows, identifiers."""
    i = dot_pos - 1
    while i >= 0 and line[i] in ' \t':
        i -= 1
    if i < 0:
        return -1

    # Walk backwards through the expression
    while i >= 0:
        ch = line[i]
        if ch == ')':
            # Match balanced parens
            depth = 1
            i -= 1
            while i >= 0 and depth > 0:
                if line[i] == ')':
                    depth += 1
                elif line[i] == '(':
                    depth -= 1
                i -= 1
            # Now i points to char before '(', continue to get the function name
            continue
        elif ch == ']':
            # Match balanced brackets
            depth = 1
            i -= 1
            while i >= 0 and depth > 0:
                if line[i] == ']':
                    depth += 1
                elif line[i] == '[':
                    depth -= 1
                i -= 1
            continue
        elif ch == '>':
            # Could be template or arrow. Check for ->
            if i > 0 and line[i-1] == '-':
                i -= 2
                continue
            # Template argument — match balanced <>
            depth = 1
            i -= 1
            while i >= 0 and depth > 0:
                if line[i] == '>':
                    depth += 1
                elif line[i] == '<':
                    depth -= 1
                i -= 1
            continue
        elif ch.isalnum() or ch == '_':
            # Identifier or number
            while i >= 0 and (line[i].isalnum() or line[i] == '_'):
                i -= 1
            # Check for :: prefix (namespace)
            if i >= 1 and line[i-1:i+1] == '::':
                i -= 2
                continue
            # Check for -> prefix
            if i >= 1 and line[i-1:i+1] == '->':
                i -= 2
                continue
            # Check for . prefix
            if i >= 0 and line[i] == '.':
                i -= 1
                continue
            break
        elif ch == '.':
            i -= 1
            continue
        elif ch == ':' and i > 0 and line[i-1] == ':':
            i -= 2
            continue
        else:
            break

    return i + 1


def transform_line(line):
    """Transform all .cast<T>() patterns in a single line."""
    # Pattern: .cast_func<TemplateArg>()
    # We find each occurrence and transform right-to-left to avoid index shifts
    pattern = re.compile(r'\.(cast|dyn_cast|dyn_cast_or_null|isa)<([^>]+)>\(\)')

    # Find all matches
    matches = list(pattern.finditer(line))
    if not matches:
        return line

    # Process right-to-left
    result = line
    for m in reversed(matches):
        dot_pos = m.start()  # position of the '.'
        func_name = m.group(1)
        template_arg = m.group(2)
        end_pos = m.end()

        # Find start of expression before the dot
        expr_start = find_expr_start(result, dot_pos)
        if expr_start < 0 or expr_start >= dot_pos:
            # Can't determine expression, skip
            continue

        expr = result[expr_start:dot_pos]

        # Build replacement
        replacement = f'{func_name}<{template_arg}>({expr})'
        result = result[:expr_start] + replacement + result[end_pos:]

    return result


def process_file(filepath):
    """Process a single file, return True if modified."""
    try:
        with open(filepath, 'r') as f:
            lines = f.readlines()
    except (IOError, UnicodeDecodeError):
        return False

    modified = False
    new_lines = []
    for line in lines:
        new_line = transform_line(line)
        if new_line != line:
            modified = True
        new_lines.append(new_line)

    if modified:
        with open(filepath, 'w') as f:
            f.writelines(new_lines)
        print(f'Updated: {filepath}')

    return modified


def find_cpp_files(directories):
    """Find all .cpp, .cc, .h, .hpp files in the given directories."""
    files = []
    for d in directories:
        for root, dirs, filenames in os.walk(d):
            # Skip llvm-project directory
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
