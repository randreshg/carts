# -*- Python -*-
"""
Lit configuration for CARTS tests.

This suite relies on the tools installed under `.install/` after running
`carts build`.  We keep the configuration lightweight so the existing
`carts lit` and `carts test` can simply invoke the bundled `llvm-lit`.
"""

import os

import lit.formats
import lit.util

import lit.llvm
lit.llvm.initialize(lit_config, config)
from lit.llvm import llvm_config


config.name = "carts"
use_lit_shell = True
lit_shell_env = os.environ.get("LIT_USE_INTERNAL_SHELL")
if lit_shell_env:
    use_lit_shell = not lit.util.pythonize_bool(lit_shell_env)
config.test_format = lit.formats.ShTest(execute_external=not use_lit_shell)
config.suffixes = [".mlir"]

config.test_source_root = os.path.dirname(__file__)
# Run tests in-place so that relative RUN lines keep working.
config.test_exec_root = config.test_source_root

# Keep llvm-lit resilient to interrupted writes or stray blank lines in the
# per-suite timing cache.
test_times_path = os.path.join(config.test_exec_root, ".lit_test_times.txt")
if os.path.exists(test_times_path):
    with open(test_times_path, "r") as time_file:
        cleaned_lines = [line for line in time_file if line.split()]
    with open(test_times_path, "w") as time_file:
        time_file.writelines(cleaned_lines)

# Determine locations of the installed toolchains.
project_root = os.path.dirname(config.test_source_root)
install_root = os.path.join(project_root, ".install")
carts_bin_dir = os.path.join(install_root, "carts", "bin")
llvm_bin_dir = os.path.join(install_root, "llvm", "bin")
llvm_lib_dir = os.path.join(install_root, "llvm", "lib")
build_bin_dir = os.path.join(project_root, "build", "bin")

carts_compile_tool = os.path.join(build_bin_dir, "carts-compile")
if not os.path.exists(carts_compile_tool):
    carts_compile_tool = os.path.join(install_root, "carts", "bin", "carts-compile")

required_tools = [
    ("%carts-compile", carts_compile_tool),
    ("%carts", os.path.join(install_root, "bin", "carts")),
    ("%FileCheck", os.path.join(llvm_bin_dir, "FileCheck")),
]

for subst, tool in required_tools:
    if not os.path.exists(tool):
        lit_config.fatal(
            f"Required tool '{tool}' was not found. "
            "Run `carts build` so the toolchain under .install/ is up to date."
        )
    config.substitutions.append((subst, tool))

# Export a few defaults so standard substitutions such as %S/%T are available.
config.llvm_tools_dir = llvm_bin_dir
config.llvm_lib_dir = llvm_lib_dir

# Add our installed bins to PATH for convenience.
config.environment["PATH"] = os.pathsep.join(
    [build_bin_dir, carts_bin_dir, llvm_bin_dir, config.environment.get("PATH", "")]
)

# These directories contain example inputs and should not be treated as lit tests.
config.excludes = ["examples", "carts"]

llvm_config.use_default_substitutions()
