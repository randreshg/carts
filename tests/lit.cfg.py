# -*- Python -*-

import os
import platform
import re
import subprocess
import tempfile

import lit.formats
import lit.util

from lit.llvm import llvm_config
from lit.llvm.subst import ToolSubst
from lit.llvm.subst import FindTool

# Configuration file for the 'lit' test runner for CARTS.

# name: The name of this test suite.
config.name = 'CARTS'

# We prefer the lit internal shell which provides a better user experience on failures
use_lit_shell = True
lit_shell_env = os.environ.get("LIT_USE_INTERNAL_SHELL")
if lit_shell_env:
  use_lit_shell = not lit.util.pythonize_bool(lit_shell_env)

config.test_format = lit.formats.ShTest(execute_external=not use_lit_shell)

# suffixes: A list of file extensions to treat as test files.
config.suffixes = ['.mlir', '.ll', '.c', '.cpp']

# test_source_root: The root path where tests are located.
config.test_source_root = os.path.dirname(__file__)

# test_exec_root: The root path where tests should be run.
config.test_exec_root = os.path.join(os.path.dirname(__file__), 'temp')

# CARTS project root directory (parent of tests/)
carts_source_root = os.path.dirname(__file__)
install_dir = os.path.join(carts_source_root, '.install')
llvm_bin_dir = os.path.join(install_dir, 'llvm', 'bin')
carts_tools_dir = os.path.join(carts_source_root, 'tools')


# Ensure the install directory exists
if not os.path.exists(install_dir):
    raise Exception(f"CARTS install directory not found: {install_dir}. Please run 'carts build' first.")

if not os.path.exists(llvm_bin_dir):
    raise Exception(f"LLVM tools directory not found: {llvm_bin_dir}. Please run 'carts build' first.")

llvm_config.with_system_environment(['HOME', 'INCLUDE', 'LIB', 'TMP', 'TEMP'])

# Add the LLVM tools directory to PATH so FileCheck and llvm-lit are found
llvm_config.with_environment('PATH', llvm_bin_dir, append_path=True)

# Define tool substitutions for CARTS testing
config.substitutions.append(('%PATH%', config.environment['PATH']))

# Key CARTS tool substitutions
tools = [
    # Main CARTS wrapper - use absolute path to carts script in project root
    ToolSubst('%carts', os.path.join(carts_tools_dir, 'carts')),
    
    # LLVM tools from install directory
    ToolSubst('%mlir_translate', os.path.join(llvm_bin_dir, 'mlir-translate')),
    ToolSubst('%filecheck', os.path.join(llvm_bin_dir, 'FileCheck')),
    ToolSubst('%mlir_opt', os.path.join(llvm_bin_dir, 'mlir-opt')),
    ToolSubst('%llvm_lit', os.path.join(llvm_bin_dir, 'llvm-lit')),
]

# Add tool substitutions
for tool in tools:
    config.substitutions.append((tool.command, tool.path))

# excludes: A list of directories to exclude from the testsuite
config.excludes = [
    'Inputs', 
    'CMakeLists.txt', 
    'README.txt', 
    'LICENSE.txt',
    'lit.cfg.py',
    'lit.site.cfg.py',
    'temp'
]

# FileCheck options for consistent behavior
config.environment["FILECHECK_OPTS"] = "-enable-var-scope --allow-unused-prefixes=false"

# Create temp directory for test execution if it doesn't exist
if not os.path.exists(config.test_exec_root):
    os.makedirs(config.test_exec_root)
