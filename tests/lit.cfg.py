# -*- Python -*-
"""
Lit configuration for CARTS tests.

This suite relies on the tools installed under `.install/` after running
`carts build`.  We keep the configuration lightweight so the existing
`tools/carts check` command can simply invoke `llvm-lit tests`.
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

# Determine locations of the installed toolchains.
project_root = os.path.dirname(config.test_source_root)
install_root = os.path.join(project_root, ".install")
carts_bin_dir = os.path.join(install_root, "carts", "bin")
llvm_bin_dir = os.path.join(install_root, "llvm", "bin")
llvm_lib_dir = os.path.join(install_root, "llvm", "lib")

required_tools = {
    "%carts-compile": os.path.join(carts_bin_dir, "carts-compile"),
    "%FileCheck": os.path.join(llvm_bin_dir, "FileCheck"),
}

for subst, tool in required_tools.items():
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
    [carts_bin_dir, llvm_bin_dir, config.environment.get("PATH", "")]
)

# These directories contain example inputs and should not be treated as lit tests.
config.excludes = ["examples", "carts"]

llvm_config.use_default_substitutions()
