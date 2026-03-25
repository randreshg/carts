---
paths:
  - "tools/scripts/**/*"
  - "tools/carts_cli.py"
---

- ALWAYS use the `carts` CLI — never `make`/`ninja` or raw binaries
- New subcommands go in `tools/scripts/<name>.py`
- Python style: 4-space indent, snake_case functions, type hints
- The CLI is built on dekk — configuration in `.dekk.toml`
