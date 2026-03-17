"""Backward-compatibility shim for carts-benchmarks SLURM scripts.

Only `print_footer` is still imported from this module.  Once the
benchmarks submodule inlines or replaces it, delete this file.
"""

from sniff import Colors, console


def print_footer(title: str, style: str = Colors.SUCCESS) -> None:
    from rich import box
    from rich.panel import Panel
    from rich.text import Text
    console.print()
    console.print(Panel(Text.from_markup(title, style=style),
                        box=box.HEAVY, border_style=style, padding=(0, 1)))
    console.print()
