"""Platform configuration and global state for CARTS CLI."""

from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import List, Optional
import platform as sys_platform
import os


# ============================================================================
# Constants and Enums
# ============================================================================


class Platform(Enum):
    MACOS = "macos"
    LINUX = "linux"
    WINDOWS = "windows"


# ============================================================================
# Platform Configuration
# ============================================================================


@dataclass
class PlatformConfig:
    """Platform-specific configuration for CARTS compilation.

    Auto-detects platform (macOS/Linux/Windows) and configures:
    - Include paths for system headers and LLVM/ARTS libraries
    - Sysroot flags for cross-compilation support
    - Linker selection (ld64.lld on macOS, ld.lld on Linux)
    - Library paths and runtime dependencies

    Usage: config = PlatformConfig.detect()
    """

    # Platform identification
    platform: Platform
    arch: str  # e.g., "arm64", "x86_64"

    # Installation directories
    carts_dir: Path              # Root CARTS source directory
    install_dir: Path            # .install directory
    llvm_install_dir: Path       # LLVM/Clang installation
    polygeist_install_dir: Path  # Polygeist (C-to-MLIR) installation
    arts_install_dir: Path       # ARTS runtime installation
    carts_install_dir: Path      # CARTS tools installation

    # Include/library paths (auto-detected)
    llvm_include_path: Path = field(default_factory=Path)
    llvm_lib_path: Path = field(default_factory=Path)
    llvm_cxx_include_path: Path = field(default_factory=Path)
    system_include_path: Optional[Path] = None
    system_cxx_include_path: Optional[Path] = None
    macos_sdk_path: Optional[Path] = None

    # Compiler flags (populated by _setup_*_flags methods)
    include_flags: List[str] = field(default_factory=list)        # -I flags
    cgeist_sysroot_flags: List[str] = field(
        default_factory=list)  # --sysroot for cgeist
    clang_sysroot_flags: List[str] = field(
        default_factory=list)  # -isysroot for clang
    clang_library_flags: List[str] = field(
        default_factory=list)  # -L flags for clang
    compile_library_flags: List[str] = field(
        default_factory=list)  # -L flags for final link
    clang_libraries: List[str] = field(
        default_factory=list)      # -l flags for clang
    compile_libraries: List[str] = field(
        default_factory=list)    # -l flags for final link
    compile_flags: List[str] = field(
        default_factory=list)        # General compile flags
    runtime_flags: List[str] = field(
        default_factory=list)        # LLVM runtime flags (-stdlib, -rtlib, etc.)
    linker_flags: List[str] = field(
        default_factory=list)         # Linker-specific flags
    linker_path: Optional[Path] = None  # Path to LLD linker
    linker_type: Optional[str] = None   # "lld" or "lld-link"

    @classmethod
    def detect(cls, script_dir: Optional[Path] = None) -> "PlatformConfig":
        """Auto-detect platform and configure paths."""
        if script_dir is None:
            script_dir = Path(__file__).parent.parent.resolve()

        # Determine CARTS_DIR
        if ".install/carts/bin" in str(script_dir):
            # Running from installed location
            carts_dir = script_dir.parent.parent.parent
        else:
            # Running from source location (tools/)
            carts_dir = script_dir.parent

        install_dir = carts_dir / ".install"

        config = cls(
            platform=cls._detect_platform(),
            arch=sys_platform.machine() or "unknown",
            carts_dir=carts_dir,
            install_dir=install_dir,
            llvm_install_dir=install_dir / "llvm",
            polygeist_install_dir=install_dir / "polygeist",
            arts_install_dir=install_dir / "arts",
            carts_install_dir=install_dir / "carts",
        )

        config._setup_paths()
        config._setup_platform_flags()
        config._setup_common_flags()
        config._detect_linker()

        return config

    @staticmethod
    def _detect_platform() -> Platform:
        """Detect the current platform."""
        system = sys_platform.system().lower()
        if system == "darwin":
            return Platform.MACOS
        elif system == "linux":
            return Platform.LINUX
        elif system == "windows" or "msys" in os.environ.get("OSTYPE", "").lower():
            return Platform.WINDOWS
        else:
            raise RuntimeError(f"Unsupported platform: {system}")

    def _setup_paths(self) -> None:
        """Setup include and library paths."""
        self.llvm_include_path = self.llvm_install_dir / \
            "lib" / "clang" / "18" / "include"
        self.llvm_cxx_include_path = self.llvm_install_dir / \
            "include" / "c++" / "v1"

        # Detect LLVM library path
        aarch64_path = self.llvm_install_dir / "lib" / "aarch64-unknown-linux-gnu"
        x86_64_path = self.llvm_install_dir / "lib" / "x86_64-unknown-linux-gnu"

        if aarch64_path.is_dir():
            self.llvm_lib_path = aarch64_path
        elif x86_64_path.is_dir():
            self.llvm_lib_path = x86_64_path
        else:
            self.llvm_lib_path = self.llvm_install_dir / "lib"

        # macOS SDK path
        self.macos_sdk_path = Path(
            "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk")

    def _setup_platform_flags(self) -> None:
        """Setup platform-specific flags."""
        if self.platform == Platform.MACOS:
            self._setup_macos_flags()
        elif self.platform == Platform.LINUX:
            self._setup_linux_flags()
        elif self.platform == Platform.WINDOWS:
            self._setup_windows_flags()

    def _setup_macos_flags(self) -> None:
        """Setup macOS-specific flags."""
        if self.macos_sdk_path and self.macos_sdk_path.exists():
            self.system_include_path = self.macos_sdk_path / "usr" / "include"
            self.system_cxx_include_path = self.macos_sdk_path / \
                "usr" / "include" / "c++" / "v1"

            self.include_flags.extend([
                f"-I{self.system_include_path}",
                f"-I{self.system_cxx_include_path}",
            ])

            self.cgeist_sysroot_flags = ["--sysroot", str(self.macos_sdk_path)]
            self.clang_sysroot_flags = ["-isysroot", str(self.macos_sdk_path)]

        # No-PIE flags for x86_64
        if self.arch == "x86_64":
            self.compile_flags.extend(["-fno-pie", "-Wl,-no_pie"])

    def _setup_linux_flags(self) -> None:
        """Setup Linux-specific flags.

        Uses LLVM's libc++/compiler-rt/libunwind instead of system libstdc++/libgcc.
        System C headers (/usr/include) are still used from glibc.
        """
        self.system_include_path = Path("/usr/include")

        self.include_flags.append(f"-I{self.system_include_path}")
        self.compile_flags.append(f"-I{self.llvm_cxx_include_path}")

        self.clang_library_flags.extend(["-L/usr/lib64", "-L/usr/lib"])
        self.compile_library_flags.extend(["-L/usr/lib64", "-L/usr/lib"])
        self.clang_libraries.extend(["-lpthread", "-lrt"])
        self.compile_libraries.extend(["-lpthread", "-lrt"])
        self.compile_flags.extend(["-fno-pie", "-no-pie"])
        self.runtime_flags.extend([
            "-stdlib=libc++", "-rtlib=compiler-rt", "--unwindlib=libunwind",
        ])

    def _setup_windows_flags(self) -> None:
        """Setup Windows-specific flags."""
        sdk_dir = os.environ.get(
            "WindowsSdkDir", os.environ.get("WINDOWSSDKDIR", ""))
        sdk_version = os.environ.get(
            "WindowsSDKVersion", os.environ.get("WINDOWSSDKVERSION", ""))
        vc_tools_dir = os.environ.get("VCToolsInstallDir", "")
        arch = os.environ.get("CARTS_WINDOWS_ARCH", "x64")

        if not sdk_dir or not sdk_version:
            raise RuntimeError(
                "Windows SDK environment not detected. Run from VS developer prompt.")
        if not vc_tools_dir:
            raise RuntimeError(
                "VCToolsInstallDir not set. Run from VS developer prompt.")

        sdk_dir = sdk_dir.rstrip("/\\")
        sdk_version = sdk_version.rstrip("/\\")
        vc_tools_dir = vc_tools_dir.rstrip("/\\")

        self.include_flags.extend([
            f"-I{sdk_dir}/Include/{sdk_version}/ucrt",
            f"-I{sdk_dir}/Include/{sdk_version}/shared",
            f"-I{sdk_dir}/Include/{sdk_version}/um",
            f"-I{vc_tools_dir}/include",
        ])

        self.clang_library_flags.extend([
            f"-L{sdk_dir}/Lib/{sdk_version}/ucrt/{arch}",
            f"-L{sdk_dir}/Lib/{sdk_version}/um/{arch}",
            f"-L{vc_tools_dir}/lib/{arch}",
        ])
        self.compile_library_flags.extend(self.clang_library_flags)

    def _setup_common_flags(self) -> None:
        """Setup common flags for all platforms."""
        # Include flags
        self.include_flags.insert(0, f"-I{self.llvm_include_path}")
        self.include_flags.extend([
            f"-I{self.carts_install_dir}/include",
            f"-I{self.arts_install_dir}/include",
            f"-I{self.llvm_install_dir}/include",
        ])

        # Library flags
        base_lib_flags = [
            f"-L{self.carts_install_dir}/lib",
            f"-L{self.carts_install_dir}/lib64",
            f"-L{self.arts_install_dir}/lib",
            f"-L{self.arts_install_dir}/lib64",
            f"-L{self.llvm_lib_path}",
            f"-L{self.llvm_install_dir}/lib",
            f"-L{self.llvm_install_dir}/lib64",
        ]
        self.clang_library_flags = base_lib_flags + self.clang_library_flags
        self.compile_library_flags = base_lib_flags + self.compile_library_flags

        # Libraries
        base_libs = ["-lc++", "-lc++abi", "-lunwind"]
        self.clang_libraries = base_libs + self.clang_libraries
        self.compile_libraries = [
            "-lcartstest", "-lcartsbenchmarks", "-larts", "-lomp",
        ] + base_libs + [lib for lib in self.compile_libraries if lib not in base_libs]

        # Compile flags — include LLVM runtime flags (-stdlib=libc++ etc.)
        self.compile_flags.insert(0, "-O3")
        self.compile_flags.extend(self.runtime_flags)

    def _detect_linker(self) -> None:
        """Detect and configure the appropriate linker."""
        linker_candidates = []

        if self.platform == Platform.MACOS:
            linker_candidates = [
                self.llvm_install_dir / "bin" / "ld64.lld",
                self.llvm_install_dir / "bin" / "lld",
            ]
        elif self.platform == Platform.LINUX:
            linker_candidates = [
                self.llvm_install_dir / "bin" / "ld.lld",
                self.llvm_install_dir / "bin" / "lld",
            ]
        elif self.platform == Platform.WINDOWS:
            linker_candidates = [
                self.llvm_install_dir / "bin" / "lld-link.exe",
                self.llvm_install_dir / "bin" / "lld-link",
            ]

        for candidate in linker_candidates:
            if candidate.is_file():
                self.linker_path = candidate
                self.linker_type = "lld-link" if "lld-link" in candidate.name else "lld"
                break

        if self.linker_path:
            if self.platform == Platform.WINDOWS:
                self.linker_flags.extend(
                    ["-fuse-ld=lld", f"--ld-path={self.linker_path}"])
            else:
                self.linker_flags.append(f"--ld-path={self.linker_path}")

        # Add rpaths for ELF/Mach-O targets
        if self.platform in (Platform.LINUX, Platform.MACOS):
            if self.platform == Platform.LINUX:
                self.linker_flags.append("-Wl,--disable-new-dtags")

            rpaths = [
                self.carts_install_dir / "lib",
                self.arts_install_dir / "lib",
                self.llvm_install_dir / "lib",
                self.carts_install_dir / "lib64",
                self.arts_install_dir / "lib64",
                self.llvm_install_dir / "lib64",
                self.llvm_lib_path,
            ]

            existing = set()
            for rp in rpaths:
                rpath_flag = f"-Wl,-rpath,{rp}"
                if rpath_flag not in existing:
                    self.linker_flags.append(rpath_flag)
                    existing.add(rpath_flag)


# ============================================================================
# Global State
# ============================================================================

_config: Optional[PlatformConfig] = None
_verbose: bool = False


def get_config() -> PlatformConfig:
    """Get or create the platform configuration."""
    global _config
    if _config is None:
        _config = PlatformConfig.detect()
    return _config


def set_verbose(verbose: bool) -> None:
    """Set verbose mode."""
    global _verbose
    _verbose = verbose


def is_verbose() -> bool:
    """Check if verbose mode is enabled."""
    return _verbose
