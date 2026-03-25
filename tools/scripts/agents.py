"""Agent resource commands for CARTS.

Single source of truth: `.carts/` directory (agent-neutral).
  - `.carts/project.md`  — project instructions
  - `.carts/skills/`     — agent skills
  - `.carts/rules/`      — path-scoped rules

Generated outputs (all gitignored):
  - Claude Code    -> CLAUDE.md + .claude/skills/ + .claude/rules/
  - OpenAI Codex   -> CODEX.md + ~/.codex/skills/
  - GitHub Copilot -> .github/copilot-instructions.md + .github/instructions/
  - Cursor         -> .cursorrules
  - Machine        -> .agents.json

Workflow:
  1. Edit `.carts/project.md` for project-level instructions
  2. Edit skills in `.carts/skills/<name>/SKILL.md`
  3. Edit rules in `.carts/rules/<name>.md`
  4. Run `carts agents generate` to produce all agent configs
  5. Run `carts agents install` to push skills to ~/.codex/skills/
"""

from __future__ import annotations

from dataclasses import dataclass
import os
from pathlib import Path
import re
import shutil
from typing import Dict, Iterable, List, Optional, Tuple

from dekk import Colors, Exit, Option, Typer, console, print_error, print_info, print_success, print_warning

from scripts.platform import get_config


agents_app = Typer(
    help="Manage CARTS agent resources for Claude, Codex, Copilot, and Cursor.",
    no_args_is_help=True,
)


_FRONTMATTER_RE = re.compile(r"^---\n(.*?)\n---\n?", re.DOTALL)
_SKILL_FILENAME = "SKILL.md"


@dataclass(frozen=True)
class RuleDefinition:
    source_file: Path
    name: str
    paths: List[str]
    body: str


def _repo_rules_source_dir() -> Path:
    """Agent-neutral source of truth for path-scoped rules."""
    return get_config().carts_dir / ".carts" / "rules"


def _discover_rules() -> List[RuleDefinition]:
    """Discover rules from .carts/rules/*.md."""
    root = _repo_rules_source_dir()
    if not root.is_dir():
        return []

    rules: List[RuleDefinition] = []
    for rule_file in sorted(root.glob("*.md")):
        text = rule_file.read_text(encoding="utf-8")
        match = _FRONTMATTER_RE.match(text)
        if not match:
            continue

        paths: List[str] = []
        in_paths = False
        for line in match.group(1).splitlines():
            stripped = line.strip()
            if stripped.startswith("paths:"):
                in_paths = True
                continue
            if in_paths and stripped.startswith("- "):
                path_val = stripped[2:].strip().strip('"').strip("'")
                paths.append(path_val)
            elif in_paths and not stripped.startswith("-"):
                in_paths = False

        if not paths:
            continue

        rules.append(RuleDefinition(
            source_file=rule_file,
            name=rule_file.stem,
            paths=paths,
            body=text[match.end():],
        ))
    return rules


@dataclass(frozen=True)
class SkillDefinition:
    source_dir: Path
    source_file: Path
    metadata: Dict[str, str]
    body: str

    @property
    def name(self) -> str:
        return self.metadata["name"]

    @property
    def description(self) -> str:
        return self.metadata["description"]


def _repo_skill_source_dir() -> Path:
    """Agent-neutral source of truth for skills."""
    return get_config().carts_dir / ".carts" / "skills"


def _claude_skills_dir() -> Path:
    """Claude Code auto-discovery directory."""
    return get_config().carts_dir / ".claude" / "skills"


def _codex_home() -> Path:
    override = os.environ.get("CODEX_HOME")
    if override:
        return Path(override).expanduser()
    return Path.home() / ".codex"


def _codex_skills_dir() -> Path:
    return _codex_home() / "skills"


def _parse_skill(skill_file: Path) -> SkillDefinition:
    text = skill_file.read_text(encoding="utf-8")
    match = _FRONTMATTER_RE.match(text)
    if not match:
        raise Exit(f"Skill file missing YAML frontmatter: {skill_file}")

    metadata: Dict[str, str] = {}
    for line in match.group(1).splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        if ":" not in stripped:
            continue
        key, value = stripped.split(":", 1)
        metadata[key.strip()] = value.strip()

    for key in ("name", "description"):
        if key not in metadata or not metadata[key]:
            raise Exit(f"Skill file missing required '{key}': {skill_file}")

    return SkillDefinition(
        source_dir=skill_file.parent,
        source_file=skill_file,
        metadata=metadata,
        body=text[match.end():],
    )


def _discover_skills() -> List[SkillDefinition]:
    root = _repo_skill_source_dir()
    if not root.is_dir():
        raise Exit(f"Skill source directory not found: {root}")

    skills: List[SkillDefinition] = []
    for skill_file in sorted(root.glob(f"*/{_SKILL_FILENAME}")):
        skills.append(_parse_skill(skill_file))
    return skills


def _render_codex_skill(skill: SkillDefinition) -> str:
    """Render a skill in Codex format (simpler frontmatter)."""
    return (
        "---\n"
        f"name: {skill.name}\n"
        f"description: {skill.description}\n"
        "---\n\n"
        f"{skill.body.lstrip()}"
    )


def _iter_skill_files(skill: SkillDefinition) -> Iterable[Tuple[Path, Path]]:
    for path in sorted(skill.source_dir.rglob("*")):
        if path.is_dir():
            continue
        relative = path.relative_to(skill.source_dir)
        yield path, relative


def _install_skills_to_dir(
    skills: List[SkillDefinition],
    target_dir: Path,
    renderer=None,
    force: bool = True,
    label: str = "target",
) -> List[str]:
    """Install skills into a directory, optionally transforming SKILL.md."""
    target_dir.mkdir(parents=True, exist_ok=True)
    installed: List[str] = []

    for skill in skills:
        dest = target_dir / skill.source_dir.name
        if dest.exists() and not force:
            print_warning(f"Skipping existing skill: {dest}")
            continue
        dest.mkdir(parents=True, exist_ok=True)

        for source_path, relative in _iter_skill_files(skill):
            target_path = dest / relative
            target_path.parent.mkdir(parents=True, exist_ok=True)
            if relative.as_posix() == _SKILL_FILENAME and renderer:
                target_path.write_text(renderer(skill), encoding="utf-8")
            else:
                shutil.copy2(source_path, target_path)

        installed.append(skill.name)
        print_success(f"  {label}: {skill.name}")

    return installed


def _check_state(skill: SkillDefinition, target_dir: Path, renderer=None) -> str:
    """Check if a skill is installed and up to date in target_dir."""
    dest = target_dir / skill.source_dir.name / _SKILL_FILENAME
    if not dest.is_file():
        return "missing"
    try:
        expected = renderer(skill) if renderer else skill.source_file.read_text(encoding="utf-8")
        if dest.read_text(encoding="utf-8") != expected:
            return "stale"
    except OSError:
        return "stale"
    return "ok"


# ============================================================================
# Commands
# ============================================================================

@agents_app.command("list")
def list_skills():
    """List repo-managed agent skills from .carts/skills/."""
    skills = _discover_skills()
    if not skills:
        print_warning("No repo-managed skills found")
        return

    source = _repo_skill_source_dir()
    console.print(f"[{Colors.INFO}]Source:[/{Colors.INFO}] {source}")
    console.print()
    for skill in skills:
        console.print(f"[{Colors.INFO}]{skill.name}[/{Colors.INFO}]")
        console.print(f"  {skill.description}")


@agents_app.command("status")
def status(
    codex_dir: Optional[Path] = Option(
        None,
        "--codex-dir",
        help="Codex skills directory (default: $CODEX_HOME/skills or ~/.codex/skills)",
    ),
):
    """Show agent config and skill installation status."""
    carts_dir = get_config().carts_dir
    source_dir = _repo_skill_source_dir()
    claude_dir = _claude_skills_dir()
    effective_codex_dir = codex_dir.expanduser() if codex_dir else _codex_skills_dir()
    skills = _discover_skills()

    project_md = carts_dir / ".carts" / "project.md"
    console.print(f"[{Colors.INFO}]Source of truth:[/{Colors.INFO}] .carts/")
    console.print(f"  project.md: [{Colors.SUCCESS if project_md.is_file() else Colors.WARNING}]{'present' if project_md.is_file() else 'missing'}[/{Colors.SUCCESS if project_md.is_file() else Colors.WARNING}]")
    console.print(f"  skills/: [{Colors.SUCCESS}]{len(skills)} skill(s)[/{Colors.SUCCESS}]")
    console.print()

    # Generated agent config files
    _agent_configs = [
        ("CLAUDE.md", carts_dir / "CLAUDE.md", "generated -> Claude Code"),
        ("AGENTS.md", carts_dir / "AGENTS.md", "generated -> Codex/all agents"),
        ("CODEX.md", carts_dir / "CODEX.md", "generated -> Codex"),
        (".cursorrules", carts_dir / ".cursorrules", "generated -> Cursor"),
        (".editorconfig", carts_dir / ".editorconfig", "editor config"),
        (".github/copilot-instructions.md", carts_dir / ".github" / "copilot-instructions.md", "generated -> Copilot"),
        (".agents.json", carts_dir / ".agents.json", "generated -> machine-readable"),
    ]
    console.print(f"[{Colors.INFO}]Agent config files:[/{Colors.INFO}]")
    for label, path, role in _agent_configs:
        exists = path.is_file()
        color = Colors.SUCCESS if exists else Colors.WARNING
        state = "present" if exists else "missing"
        console.print(f"  {label}: [{color}]{state}[/{color}]  ({role})")

    # Copilot per-directory instructions
    instructions_dir = carts_dir / ".github" / "instructions"
    if instructions_dir.is_dir():
        instr_files = sorted(instructions_dir.glob("*.instructions.md"))
        console.print(f"  .github/instructions/: [{Colors.SUCCESS}]{len(instr_files)} file(s)[/{Colors.SUCCESS}]")
    else:
        console.print(f"  .github/instructions/: [{Colors.WARNING}]missing[/{Colors.WARNING}]")

    console.print()
    console.print(f"[{Colors.INFO}]Skills:[/{Colors.INFO}]")
    for skill in skills:
        claude_state = _check_state(skill, claude_dir)
        codex_state = _check_state(skill, effective_codex_dir, renderer=_render_codex_skill)

        def _color(s: str) -> str:
            return Colors.SUCCESS if s == "ok" else Colors.WARNING
        console.print(
            f"  [{Colors.INFO}]{skill.name}[/{Colors.INFO}]  "
            f"claude=[{_color(claude_state)}]{claude_state}[/{_color(claude_state)}]  "
            f"codex=[{_color(codex_state)}]{codex_state}[/{_color(codex_state)}]"
        )


@agents_app.command("generate")
def generate(
    target: Optional[str] = Option(
        None,
        "--target",
        help="Target: claude, codex, copilot, cursor, all (default: all)",
    ),
):
    """Generate all agent configs from .carts/ source of truth.

    Reads .carts/project.md, .carts/skills/, and .carts/rules/ to generate:
    - CLAUDE.md + .claude/skills/ + .claude/rules/ (for Claude Code)
    - CODEX.md (for Codex)
    - .cursorrules (for Cursor)
    - .github/copilot-instructions.md + .github/instructions/ (for Copilot)
    - AGENTS.md (detailed reference, from .carts/agents-reference.md)
    - .agents.json (machine-readable manifest)
    """
    carts_dir = get_config().carts_dir
    effective_target = (target or "all").lower()
    generated: List[str] = []
    skills = _discover_skills()
    project_content = _read_project_md()

    if not project_content:
        print_error(".carts/project.md not found — this is the source of truth for agent configs")
        raise Exit(code=1)

    rules = _discover_rules()

    # Claude Code: CLAUDE.md + .claude/skills/ + .claude/rules/
    if effective_target in ("all", "claude"):
        # CLAUDE.md = project.md content (direct copy)
        claude_md_path = carts_dir / "CLAUDE.md"
        claude_md_path.write_text(project_content, encoding="utf-8")
        generated.append("CLAUDE.md")

        # Sync skills to .claude/skills/
        claude_dir = _claude_skills_dir()
        _install_skills_to_dir(skills, claude_dir, label="claude")
        generated.append(f".claude/skills/ ({len(skills)} skills)")

        # Sync rules to .claude/rules/
        _generate_claude_rules(carts_dir, rules)
        generated.append(f".claude/rules/ ({len(rules)} rules)")
        print_success(f"Generated CLAUDE.md + .claude/skills/ ({len(skills)} skills) + .claude/rules/ ({len(rules)} rules)")

    # Codex: CODEX.md
    if effective_target in ("all", "codex"):
        codex_path = carts_dir / "CODEX.md"
        codex_content = _generate_codex_md(project_content)
        codex_path.write_text(codex_content, encoding="utf-8")
        generated.append("CODEX.md")
        print_success("Generated CODEX.md")

    # Cursor: .cursorrules
    if effective_target in ("all", "cursor"):
        cursor_path = carts_dir / ".cursorrules"
        cursor_content = _generate_cursorrules(project_content)
        cursor_path.write_text(cursor_content, encoding="utf-8")
        generated.append(".cursorrules")
        print_success("Generated .cursorrules")

    # Copilot: .github/copilot-instructions.md + .github/instructions/
    if effective_target in ("all", "copilot"):
        copilot_dir = carts_dir / ".github"
        copilot_dir.mkdir(parents=True, exist_ok=True)
        copilot_path = copilot_dir / "copilot-instructions.md"
        copilot_content = _generate_copilot_instructions(project_content)
        copilot_path.write_text(copilot_content, encoding="utf-8")
        generated.append(".github/copilot-instructions.md")

        # Per-directory instructions from rules
        _generate_copilot_per_directory(carts_dir, rules)
        generated.append(f".github/instructions/ ({len(rules)} rules)")
        print_success(f"Generated .github/copilot-instructions.md + .github/instructions/ ({len(rules)} rules)")

    # AGENTS.md (detailed reference, direct copy from .carts/agents-reference.md)
    if effective_target == "all":
        agents_ref = carts_dir / ".carts" / "agents-reference.md"
        if agents_ref.is_file():
            agents_md = carts_dir / "AGENTS.md"
            agents_md.write_text(agents_ref.read_text(encoding="utf-8"), encoding="utf-8")
            generated.append("AGENTS.md")
            print_success("Generated AGENTS.md")

    # Machine-readable manifest
    if effective_target == "all":
        _generate_agents_json(carts_dir, skills)
        generated.append(".agents.json")
        print_success("Generated .agents.json")

    if not generated:
        print_warning(f"Unknown target: {target}. Use: claude, codex, copilot, cursor, all")
    else:
        print_info(f"Generated {len(generated)} target(s) from .carts/")


def install_repo_skills_to_codex(codex_dir: Optional[Path] = None,
                                 force: bool = True) -> List[str]:
    """Public API: install skills to Codex directory (used by install.py)."""
    skills = _discover_skills()
    effective_codex_dir = codex_dir.expanduser() if codex_dir else _codex_skills_dir()
    return _install_skills_to_dir(
        skills, effective_codex_dir,
        renderer=_render_codex_skill,
        force=force,
        label="codex",
    )


@agents_app.command("install")
def install(
    codex_dir: Optional[Path] = Option(
        None,
        "--codex-dir",
        help="Install destination for Codex skills (default: $CODEX_HOME/skills or ~/.codex/skills)",
    ),
    force: bool = Option(
        True,
        "--force/--no-force",
        help="Replace existing installed CARTS skills in the destination",
    ),
):
    """Install skills into ~/.codex/skills/ for Codex agent."""
    skills = _discover_skills()
    effective_codex_dir = codex_dir.expanduser() if codex_dir else _codex_skills_dir()
    print_info(f"Installing CARTS skills into {effective_codex_dir}")
    installed = _install_skills_to_dir(
        skills, effective_codex_dir,
        renderer=_render_codex_skill,
        force=force,
        label="codex",
    )
    print_info(f"Installed {len(installed)} Codex skill(s)")


# ============================================================================
# Config generators (CLAUDE.md -> other formats)
# ============================================================================

def _read_project_md() -> Optional[str]:
    """Read .carts/project.md as the source of truth."""
    project_md = get_config().carts_dir / ".carts" / "project.md"
    if not project_md.is_file():
        return None
    return project_md.read_text(encoding="utf-8")


def _generate_claude_rules(carts_dir: Path, rules: List[RuleDefinition]) -> None:
    """Generate .claude/rules/ from .carts/rules/ (Claude Code paths: frontmatter)."""
    rules_dir = carts_dir / ".claude" / "rules"
    rules_dir.mkdir(parents=True, exist_ok=True)
    for rule in rules:
        paths_yaml = "\n".join(f'  - "{p}"' for p in rule.paths)
        content = f"---\npaths:\n{paths_yaml}\n---\n{rule.body}"
        (rules_dir / f"{rule.name}.md").write_text(content, encoding="utf-8")


def _generate_copilot_per_directory(carts_dir: Path, rules: List[RuleDefinition]) -> None:
    """Generate .github/instructions/ from .carts/rules/ (Copilot applyTo: frontmatter)."""
    instr_dir = carts_dir / ".github" / "instructions"
    instr_dir.mkdir(parents=True, exist_ok=True)
    for rule in rules:
        apply_to = ",".join(rule.paths)
        content = f"---\napplyTo: {apply_to}\n---\n{rule.body}"
        (instr_dir / f"{rule.name}.instructions.md").write_text(content, encoding="utf-8")


def _generate_codex_md(claude_content: str) -> str:
    """CLAUDE.md -> CODEX.md (shorter, command-focused)."""
    lines = claude_content.splitlines()
    output: List[str] = []
    skip_section = False
    for line in lines:
        if line.startswith("## Pipeline Overview"):
            skip_section = True
            continue
        if skip_section and line.startswith("## "):
            skip_section = False
        if skip_section:
            continue
        if line.startswith("## Reference Documentation"):
            skip_section = True
            continue
        if line == "# CARTS - Compiler for Asynchronous Runtime Systems":
            output.append("# CARTS - Codex Agent Guide")
            continue
        output.append(line)

    output.extend(["", "## Skills", "", "Run `carts agents install` to install repo-managed skills into `~/.codex/skills/`.", ""])
    return "\n".join(output)


def _generate_cursorrules(claude_content: str) -> str:
    """CLAUDE.md -> .cursorrules (system-prompt style)."""
    lines = claude_content.splitlines()
    output: List[str] = [
        "You are working on CARTS, an MLIR-based compiler that transforms OpenMP-annotated",
        "C/C++ into task-based parallel executables targeting the ARTS asynchronous runtime.",
        "",
    ]

    in_section = False
    keep_sections = {"## Critical Rules", "## Essential Commands", "## Coding Conventions", "## Key Architectural Concepts"}
    rename_map = {"## Critical Rules": "## Rules", "## Essential Commands": "## Commands", "## Key Architectural Concepts": "## Key Concepts"}

    for line in lines:
        if line.startswith("## "):
            in_section = line in keep_sections
            if in_section and line in rename_map:
                output.append(rename_map[line])
                continue
        if in_section:
            output.append(line)

    output.extend([
        "", "## File Locations", "",
        "- Pass implementations: `lib/arts/passes/opt/` and `lib/arts/passes/transforms/`",
        "- Analysis: `lib/arts/analysis/`",
        "- Headers: `include/arts/`",
        "- Tests: `tests/contracts/` (MLIR lit) and `tests/examples/` (end-to-end)",
        "- Pipeline definition: `tools/compile/Compile.cpp`",
        "",
    ])
    return "\n".join(output)


def _generate_agents_json(carts_dir: Path, skills: List[SkillDefinition]) -> None:
    """Generate .agents.json machine-readable manifest."""
    import json
    manifest = {
        "project": "carts",
        "description": "MLIR-based compiler for asynchronous runtime systems",
        "cli": "carts",
        "language": "C++",
        "style": "LLVM",
        "source_of_truth": ".carts/",
        "agent_configs": {
            "claude": {"instructions": "CLAUDE.md", "skills": ".claude/skills/"},
            "codex": {"instructions": "CODEX.md", "skills": "~/.codex/skills/ (via carts agents install)"},
            "copilot": {"instructions": ".github/copilot-instructions.md", "per_directory": ".github/instructions/"},
            "cursor": {"instructions": ".cursorrules"},
        },
        "commands": {
            "build": "carts build",
            "test": "carts test",
            "test_single": "carts lit {file}",
            "format": "carts format",
            "doctor": "carts doctor",
            "compile": "carts compile {file} -O3",
            "pipeline": "carts pipeline --json",
        },
        "skills": [{"name": s.name, "description": s.description} for s in skills],
    }
    agents_json = carts_dir / ".agents.json"
    agents_json.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def _generate_copilot_instructions(claude_content: str) -> str:
    """CLAUDE.md -> .github/copilot-instructions.md."""
    lines = claude_content.splitlines()
    output: List[str] = ["# CARTS Copilot Instructions", ""]

    in_section = False
    keep_sections = {"## Essential Commands", "## Project Layout", "## Coding Conventions", "## Key Architectural Concepts"}
    rename_map = {"## Essential Commands": "## Build and Test", "## Key Architectural Concepts": "## Key Concepts", "## Project Layout": "## Architecture"}

    started = False
    for line in lines:
        if not started:
            if line.startswith("## "):
                started = True
            elif line and not line.startswith("#"):
                output.append(line)
                continue
            else:
                continue

        if line.startswith("## "):
            in_section = line in keep_sections
            if in_section and line in rename_map:
                output.append(rename_map[line])
                continue
        if in_section:
            output.append(line)

    output.append("")
    return "\n".join(output)
