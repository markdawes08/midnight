#!/usr/bin/env python3
from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import os
import platform
import subprocess
from pathlib import Path


TEXT_EXTENSIONS = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".inl",
    ".ipp",
    ".ixx",
    ".cmake",
    ".txt",
    ".md",
    ".json",
    ".jsonc",
    ".tmj",
    ".tsj",
    ".tsx",
    ".tmx",
    ".xml",
    ".vert",
    ".frag",
    ".glsl",
    ".comp",
    ".geom",
    ".tesc",
    ".tese",
    ".lua",
    ".py",
    ".sh",
    ".bash",
    ".ini",
    ".toml",
    ".yaml",
    ".yml",
    ".cfg",
    ".conf",
}

TEXT_FILENAMES = {
    "CMakeLists.txt",
    ".gitignore",
    "README",
    "README.md",
    "LICENSE",
}

EXCLUDED_DIR_NAMES = {
    ".git",
    "build",
    "_context",
    ".cache",
    ".cmake",
    ".idea",
    ".vscode",
    "__pycache__",
}

EXCLUDED_FILE_NAMES = {
    "compile_commands.json",
}

EXCLUDED_EXTENSIONS = {
    ".o",
    ".obj",
    ".a",
    ".so",
    ".dll",
    ".dylib",
    ".exe",
    ".bin",
    ".spv",
    ".pyc",
}

LANG_BY_EXTENSION = {
    ".c": "c",
    ".cc": "cpp",
    ".cpp": "cpp",
    ".cxx": "cpp",
    ".h": "cpp",
    ".hh": "cpp",
    ".hpp": "cpp",
    ".hxx": "cpp",
    ".inl": "cpp",
    ".ipp": "cpp",
    ".ixx": "cpp",
    ".cmake": "cmake",
    ".json": "json",
    ".jsonc": "jsonc",
    ".tmj": "json",
    ".tsj": "json",
    ".tsx": "xml",
    ".tmx": "xml",
    ".xml": "xml",
    ".vert": "glsl",
    ".frag": "glsl",
    ".glsl": "glsl",
    ".comp": "glsl",
    ".geom": "glsl",
    ".tesc": "glsl",
    ".tese": "glsl",
    ".lua": "lua",
    ".py": "python",
    ".sh": "bash",
    ".bash": "bash",
    ".ini": "ini",
    ".toml": "toml",
    ".yaml": "yaml",
    ".yml": "yaml",
    ".md": "markdown",
    ".txt": "text",
    ".cfg": "ini",
    ".conf": "ini",
}

LANG_BY_FILENAME = {
    "CMakeLists.txt": "cmake",
    ".gitignore": "gitignore",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Export Midnight project context into a single Markdown file."
    )
    parser.add_argument(
        "-o",
        "--output",
        default="_context/midnight_context.md",
        help="Output file path relative to the project root.",
    )
    parser.add_argument(
        "--max-text-bytes",
        type=int,
        default=512 * 1024,
        help="Maximum size for a text file to include inline.",
    )
    return parser.parse_args()


def find_project_root() -> Path:
    script_path = Path(__file__).resolve()

    if script_path.parent.name == "tools":
        candidate = script_path.parent.parent
        if (candidate / "CMakeLists.txt").exists():
            return candidate

    current = Path.cwd().resolve()

    for candidate in [current, *current.parents]:
        if (candidate / "CMakeLists.txt").exists() and (candidate / "src").exists():
            return candidate

    raise SystemExit(
        "Could not find project root. Run this from inside the midnight project."
    )


def display_path(path: Path, root: Path) -> str:
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return path.as_posix()


def should_exclude(path: Path, root: Path) -> bool:
    rel = path.relative_to(root)

    for part in rel.parts:
        if part in EXCLUDED_DIR_NAMES:
            return True

    if path.name in EXCLUDED_FILE_NAMES:
        return True

    if path.suffix.lower() in EXCLUDED_EXTENSIONS:
        return True

    return False


def collect_files(root: Path) -> list[Path]:
    files: list[Path] = []

    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(
            name for name in dirnames if name not in EXCLUDED_DIR_NAMES
        )

        for filename in sorted(filenames):
            path = Path(dirpath) / filename

            if should_exclude(path, root):
                continue

            files.append(path)

    return sorted(files, key=lambda item: item.relative_to(root).as_posix())


def should_include_as_text(path: Path) -> bool:
    return path.name in TEXT_FILENAMES or path.suffix.lower() in TEXT_EXTENSIONS


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()

    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)

    return digest.hexdigest()


def format_bytes(value: int) -> str:
    units = ["B", "KiB", "MiB", "GiB"]
    size = float(value)

    for unit in units:
        if size < 1024.0 or unit == units[-1]:
            if unit == "B":
                return f"{int(size)} {unit}"
            return f"{size:.1f} {unit}"
        size /= 1024.0

    return f"{value} B"


def command_first_line(command: list[str], cwd: Path) -> str:
    try:
        completed = subprocess.run(
            command,
            cwd=cwd,
            check=False,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=5,
        )
    except Exception as error:
        return f"unavailable: {error}"

    lines = completed.stdout.strip().splitlines()

    if not lines:
        return "unavailable"

    return lines[0]


def png_dimensions(path: Path) -> tuple[int, int] | None:
    try:
        with path.open("rb") as handle:
            header = handle.read(24)
    except OSError:
        return None

    png_signature = b"\x89PNG\r\n\x1a\n"

    if len(header) < 24:
        return None

    if header[:8] != png_signature:
        return None

    if header[12:16] != b"IHDR":
        return None

    width = int.from_bytes(header[16:20], "big")
    height = int.from_bytes(header[20:24], "big")

    return width, height


def binary_description(path: Path) -> str:
    details: list[str] = []

    suffix = path.suffix.lower().lstrip(".")
    if suffix:
        details.append(suffix)

    dimensions = png_dimensions(path)
    if dimensions is not None:
        details.append(f"{dimensions[0]}x{dimensions[1]}")

    return ", ".join(details) if details else "binary"


def language_for(path: Path) -> str:
    if path.name in LANG_BY_FILENAME:
        return LANG_BY_FILENAME[path.name]

    return LANG_BY_EXTENSION.get(path.suffix.lower(), "text")


def fence_for(text: str) -> str:
    longest = 0
    current = 0

    for character in text:
        if character == "`":
            current += 1
            longest = max(longest, current)
        else:
            current = 0

    return "`" * max(3, longest + 1)


def read_text_file(path: Path, max_text_bytes: int) -> tuple[str | None, str | None]:
    size = path.stat().st_size

    if size > max_text_bytes:
        return None, f"too large for inline export: {format_bytes(size)}"

    data = path.read_bytes()

    if b"\x00" in data[:8192]:
        return None, "contains null bytes, treated as binary"

    text = data.decode("utf-8", errors="replace")
    return text, None


def build_markdown(root: Path, output_path: Path, max_text_bytes: int) -> str:
    files = collect_files(root)

    text_entries: list[tuple[Path, int, str, str]] = []
    binary_entries: list[tuple[Path, int, str, str]] = []
    skipped_text_entries: list[tuple[Path, int, str, str]] = []

    for path in files:
        size = path.stat().st_size
        digest = sha256_file(path)

        if should_include_as_text(path):
            text, reason = read_text_file(path, max_text_bytes)

            if text is None:
                skipped_text_entries.append((path, size, digest, reason or "skipped"))
            else:
                text_entries.append((path, size, digest, text))
        else:
            binary_entries.append((path, size, digest, binary_description(path)))

    now = dt.datetime.now(dt.timezone.utc).strftime("%Y-%m-%d %H:%M:%S UTC")

    out: list[str] = []

    out.append("# Midnight project context")
    out.append("")
    out.append(f"Generated: `{now}`")
    out.append(f"Project root: `{root}`")
    out.append(f"Output file: `{display_path(output_path, root)}`")
    out.append("")
    out.append("## Local tool snapshot")
    out.append("")
    out.append(f"- OS: `{platform.platform()}`")
    out.append(f"- Python: `{platform.python_version()}`")
    out.append(f"- CMake: `{command_first_line(['cmake', '--version'], root)}`")
    out.append(f"- Ninja: `{command_first_line(['ninja', '--version'], root)}`")
    out.append(f"- C++ compiler: `{command_first_line(['c++', '--version'], root)}`")
    out.append("")
    out.append("## Current build commands")
    out.append("")
    out.append("```bash")
    out.append("cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug")
    out.append("cmake --build build")
    out.append("./build/midnight")
    out.append("```")
    out.append("")
    out.append("## Project file list")
    out.append("")
    out.append("```text")

    for path in files:
        rel = display_path(path, root)
        size = format_bytes(path.stat().st_size)
        out.append(f"{rel} ({size})")

    out.append("```")
    out.append("")

    out.append("## Binary asset and non-text manifest")
    out.append("")

    if not binary_entries:
        out.append("No binary or non-text files found.")
        out.append("")
    else:
        out.append("| Path | Size | SHA-256 | Details |")
        out.append("|---|---:|---|---|")

        for path, size, digest, details in binary_entries:
            rel = display_path(path, root)
            out.append(f"| `{rel}` | {format_bytes(size)} | `{digest}` | {details} |")

        out.append("")

    out.append("## Text files skipped from inline export")
    out.append("")

    if not skipped_text_entries:
        out.append("No text files were skipped.")
        out.append("")
    else:
        out.append("| Path | Size | SHA-256 | Reason |")
        out.append("|---|---:|---|---|")

        for path, size, digest, reason in skipped_text_entries:
            rel = display_path(path, root)
            out.append(f"| `{rel}` | {format_bytes(size)} | `{digest}` | {reason} |")

        out.append("")

    out.append("## Inline text file contents")
    out.append("")

    for path, size, digest, text in text_entries:
        rel = display_path(path, root)
        lang = language_for(path)
        fence = fence_for(text)

        out.append(f"### `{rel}`")
        out.append("")
        out.append(f"- Size: `{format_bytes(size)}`")
        out.append(f"- SHA-256: `{digest}`")
        out.append("")
        out.append(f"{fence}{lang}")

        if text:
            out.append(text.rstrip("\n"))

        out.append(fence)
        out.append("")

    return "\n".join(out) + "\n"


def main() -> int:
    args = parse_args()
    root = find_project_root()

    output_path = Path(args.output)

    if not output_path.is_absolute():
        output_path = root / output_path

    output_path.parent.mkdir(parents=True, exist_ok=True)

    markdown = build_markdown(
        root=root,
        output_path=output_path,
        max_text_bytes=args.max_text_bytes,
    )

    output_path.write_text(markdown, encoding="utf-8")

    print(f"[Midnight] Wrote {display_path(output_path, root)}")
    print(f"[Midnight] Size: {format_bytes(output_path.stat().st_size)}")
    print("[Midnight] Upload that file here after each increment.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
