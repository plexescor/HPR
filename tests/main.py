"""
HPR Extension Test Runner
=========================
1. Lists available test suites and lets you pick one or run all.
2. Discovers HPR binary/binaries inside build/ and lets you pick one (Windows
   may have Debug, Release, MinSizeRel, etc.).
3. Copies the chosen suite folder(s) into the HPR extensions directory.
4. Launches HPR.
5. On exit (normal or Ctrl-C), removes only the folders that were copied.

Add new test suite folder names to TEST_SUITES below.
"""

import atexit
import os
import platform
import shutil
import signal
import subprocess
import sys
from pathlib import Path


# CONFIGURATION — add new test-suite folder names and expected keys here

TEST_SUITES: list[str] = [
    "lifecyclehooks",
    "otherextensions",
    "csvio",
    "windowbackends"
    # "test_my_new_suite",
]

# Suite-specific expected CSV key lists
LIFECYCLEHOOKS_KEYS: list[str] = ["Init", "Tick", "Exit"]
OTHEREXTENSIONS_KEYS: list[str] = ["GetLoadedExtensions", "UnloadExtension"]
CSVIO_KEYS: list[str] = ["WriteCSV", "ReadCSV"]
WINDOWBACKENDS_KEYS: list[str] = ["GetCurrentWindow", "GetCurrentTitle", "RegisterBackend"]

# Master dictionary mapping suite names to their sublists of expected keys
EXPECTED_CSV_KEYS: dict[str, list[str]] = {
    "lifecyclehooks": LIFECYCLEHOOKS_KEYS,
    "otherextensions": OTHEREXTENSIONS_KEYS,
    "csvio": CSVIO_KEYS,
    "windowbackends": WINDOWBACKENDS_KEYS,
}


# Paths

SCRIPT_DIR = Path(__file__).resolve().parent   # tests/
REPO_ROOT  = SCRIPT_DIR.parent                 # HPR/
BUILD_DIR  = REPO_ROOT / "build"


def get_extensions_dir() -> Path:
    """Return the platform-specific HPR extensions directory."""
    system = platform.system()
    if system == "Windows":
        appdata = os.environ.get("APPDATA", "")
        if not appdata:
            sys.exit("[ERROR] %APPDATA% is not set.")
        return Path(appdata) / "HPR" / "HPR_Config" / "extensions"
    else:
        # Linux / macOS
        xdg = os.environ.get("XDG_CONFIG_HOME", "")
        base = Path(xdg) if xdg else Path.home() / ".config"
        return base / "HPR" / "extensions"


def find_hpr_binaries() -> list[Path]:
    """
    Discover HPR executables inside build/.

    Windows — looks for HPR.exe in known config subdirs (Debug, Release,
              RelWithDebInfo, MinSizeRel) and in build/ itself.
    Linux   — looks for an 'HPR' binary directly in build/.
    """
    candidates: list[Path] = []
    system = platform.system()

    if system == "Windows":
        config_dirs = ["Debug", "Release", "RelWithDebInfo", "MinSizeRel"]
        for cfg in config_dirs:
            p = BUILD_DIR / cfg / "HPR.exe"
            if p.is_file():
                candidates.append(p)
        # fallback: root of build/
        root_bin = BUILD_DIR / "HPR.exe"
        if root_bin.is_file():
            candidates.append(root_bin)
    else:
        p = BUILD_DIR / "HPR"
        if p.is_file():
            candidates.append(p)

    return candidates



# Prompt helpers


def prompt_int(prompt: str, lo: int, hi: int) -> int:
    """Ask for an integer in [lo, hi] and keep asking until we get one."""
    while True:
        raw = input(prompt).strip()
        if raw.isdigit():
            val = int(raw)
            if lo <= val <= hi:
                return val
        print(f"  Please enter a number between {lo} and {hi}.")


def choose_suites() -> list[str]:
    """Let the user choose all suites or a specific one."""
    print()
    print("Available Test Suites:")
    print("  0) Run ALL suites")
    for i, name in enumerate(TEST_SUITES, start=1):
        print(f"  {i}) {name}")
    print()

    choice = prompt_int(f"Select suite [0-{len(TEST_SUITES)}]: ", 0, len(TEST_SUITES))
    if choice == 0:
        return list(TEST_SUITES)
    return [TEST_SUITES[choice - 1]]


def choose_binary(binaries: list[Path]) -> Path:
    """If there is more than one HPR binary, let the user pick."""
    if len(binaries) == 1:
        print(f"[INFO] Using HPR binary: {binaries[0]}")
        return binaries[0]

    print()
    print("Available HPR Builds:")
    for i, p in enumerate(binaries, start=1):
        try:
            rel = p.relative_to(BUILD_DIR)
        except ValueError:
            rel = p
        print(f"  {i}) {rel}")
    print()

    choice = prompt_int(f"Select build [1-{len(binaries)}]: ", 1, len(binaries))
    return binaries[choice - 1]



# Copy / cleanup


# Tracks what we copied so cleanup is surgical
_copied_dirs: list[Path] = []


def copy_suites(suites: list[str], ext_dir: Path) -> None:
    """Copy each suite folder into the HPR extensions directory."""
    ext_dir.mkdir(parents=True, exist_ok=True)

    # First, wipe all known test suite folders from the HPR extensions dir
    # so old files don't interfere when testing only 1 suite
    for suite_name in TEST_SUITES:
        old_dir = ext_dir / suite_name
        if old_dir.exists():
            shutil.rmtree(old_dir)

    # Copy selected suite(s) exactly the same way
    for suite in suites:
        src = SCRIPT_DIR / suite
        if not src.is_dir():
            print(f"[WARN] Suite folder not found, skipping: {src}")
            continue

        dst = ext_dir / suite
        shutil.copytree(src, dst)
        _copied_dirs.append(dst)
        print(f"[INFO] Copied  {src.name}  ->  {dst}")


def print_results() -> None:
    """
    For each copied suite, scan every CSV file inside its output/ directory,
    validate every key against EXPECTED_CSV_KEYS, and report results.
    Prints a grand summary (total / passed / failed / rates) at the end.
    """
    print("\n" + "=" * 60)
    print("  Test Results")
    print("=" * 60)

    total_tests  = 0
    total_passed = 0
    total_failed = 0

    for suite in _copied_dirs:
        output_dir = suite / "output"
        print(f"\n  Suite: {suite.name}")
        print("  " + "-" * 40)

        expected_keys = EXPECTED_CSV_KEYS.get(suite.name, [])
        seen_keys: set[str] = set()

        if not output_dir.is_dir():
            print("    ✗ [FAILED] No output/ directory found — did HPR run long enough?")
            total_tests += len(expected_keys) if expected_keys else 1
            total_failed += len(expected_keys) if expected_keys else 1
            continue

        csv_files = sorted(output_dir.glob("*.csv"))
        if not csv_files:
            print("    ✗ [FAILED] No CSV files found in output/")
            total_tests += len(expected_keys) if expected_keys else 1
            total_failed += len(expected_keys) if expected_keys else 1
            continue

        for csv_file in csv_files:
            print(f"\n    File: {csv_file.name}")
            with open(csv_file, "r", newline="") as f:
                any_row = False
                for line in f:
                    line = line.strip()
                    # Skip blank lines and HPR-style # comments
                    if not line or line.startswith("#"):
                        continue
                    any_row = True
                    # HPR CSVs: key,value  (no quotes, split on first comma)
                    parts = line.split(",", 1)
                    if len(parts) == 2:
                        key   = parts[0].strip()
                        value = parts[1].strip()
                        seen_keys.add(key)
                        total_tests += 1

                        # Validate key against expected suite keys
                        if expected_keys and key not in expected_keys:
                            total_failed += 1
                            print(f"      ✗ {key:<29} -> {value} (UNEXPECTED KEY)")
                        elif value.upper() == "PASSED":
                            total_passed += 1
                            print(f"      ✓ {key:<29} -> {value}")
                        else:
                            total_failed += 1
                            display_val = value if value else "(empty)"
                            print(f"      ✗ {key:<29} -> {display_val}")
                    else:
                        # Single-column row — print as-is
                        print(f"        {line}")
            if not any_row:
                print("      (empty file)")

        # Check for any expected keys that were NOT found in the CSV output
        if expected_keys:
            missing_keys = [k for k in expected_keys if k not in seen_keys]
            for mk in missing_keys:
                total_tests += 1
                total_failed += 1
                print(f"      ✗ {mk:<29} -> MISSING (FAILED)")

    
    # Grand summary
    
    success_rate = (total_passed / total_tests * 100) if total_tests else 0.0
    failure_rate = (total_failed / total_tests * 100) if total_tests else 0.0

    print("\n" + "=" * 60)
    print("  Summary")
    print("=" * 60)
    print(f"  Total Tests  : {total_tests}")
    print(f"  Passed       : {total_passed}")
    print(f"  Failed       : {total_failed}")
    print(f"  Success Rate : {success_rate:.1f}%")
    print(f"  Failure Rate : {failure_rate:.1f}%")
    print("=" * 60)

def cleanup() -> None:
    """Delete only the folders we copied. Called on normal exit and signals."""
    if not _copied_dirs:
        return
    print()
    print("[INFO] Cleaning up copied test suite(s)...")
    for d in list(_copied_dirs):
        if d.exists():
            shutil.rmtree(d)
            print(f"[INFO] Removed {d}")
    _copied_dirs.clear()


def _signal_handler(sig, frame):
    print("\n[INFO] Interrupted — running cleanup.")
    cleanup()
    sys.exit(0)



# Main


def main() -> None:
    print("=" * 60)
    print("  HPR Extension Test Runner")
    print("=" * 60)

    # 1. Validate suite list
    if not TEST_SUITES:
        sys.exit("[ERROR] TEST_SUITES is empty. Add at least one suite folder name.")

    for suite in TEST_SUITES:
        if not (SCRIPT_DIR / suite).is_dir():
            print(f"[WARN] Suite listed in TEST_SUITES but folder not found: {suite}")

    # 2. Find HPR binaries
    binaries = find_hpr_binaries()
    if not binaries:
        sys.exit(
            f"[ERROR] No HPR binary found inside {BUILD_DIR}.\n"
            "        Build the project first."
        )

    # 3. User choices
    selected_suites = choose_suites()
    hpr_binary      = choose_binary(binaries)

    # 4. Resolve extensions dir
    ext_dir = get_extensions_dir()
    print(f"\n[INFO] HPR extensions directory: {ext_dir}")

    # 5. Register cleanup (atexit covers normal exit; signals cover Ctrl-C)
    atexit.register(cleanup)
    signal.signal(signal.SIGINT,  _signal_handler)
    signal.signal(signal.SIGTERM, _signal_handler)

    # 6. Copy suites
    print()
    copy_suites(selected_suites, ext_dir)

    # 7. Launch HPR (blocks until HPR closes)
    print(f"\n[INFO] Launching HPR: {hpr_binary}")
    print("-" * 60)
    try:
        proc = subprocess.run([str(hpr_binary)], check=False)
        print(f"\n[INFO] HPR exited with code {proc.returncode}.")
    except FileNotFoundError:
        print(f"[ERROR] Could not launch HPR binary: {hpr_binary}")

    # Explicit call in case atexit ordering is unpredictable
    print_results()
    cleanup()


if __name__ == "__main__":
    main()
