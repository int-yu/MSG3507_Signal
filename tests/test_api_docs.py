"""Documentation contract for the public fixed-point API.

The test deliberately consumes the public headers so a newly exported symbol
cannot silently be omitted from the reference manual or symbol manifest.
"""
from __future__ import annotations

import re
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
INCLUDE = ROOT / "include"
DOCS = ROOT / "docs"
API = DOCS / "API.md"
USAGE = DOCS / "API_USAGE.md"
MANIFEST = DOCS / "api-symbols.txt"
EXAMPLES = (
    "preprocess_filter.c",
    "measure_spectrum.c",
    "tracking_adaptive.c",
    "platform_backends.c",
)
FORBIDDEN = re.compile(r"\b(?:float|double|malloc|calloc|realloc|free)\b|#\s*include\s*<math\.h>")


def exported_symbols() -> set[str]:
    names: set[str] = set()
    for header in (INCLUDE / "sigq15.h", INCLUDE / "sigq15_backends.h"):
        names.update(re.findall(r"\b(sigq(?:15|31)_[a-z0-9_]+)\s*\(", header.read_text(encoding="utf-8")))
    return names


def test_manifest_matches_public_headers() -> None:
    listed = {
        line.strip() for line in MANIFEST.read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.startswith("#")
    }
    assert listed == exported_symbols()


def test_api_reference_covers_every_symbol_with_fixed_point_rules() -> None:
    text = API.read_text(encoding="utf-8")
    missing = sorted(name for name in exported_symbols() if name not in text)
    assert not missing, "missing API entries: " + ", ".join(missing)
    for required in ("Q1.15", "Q2.14", "Q8.8", "Q16.16", "Q12.20", "Q2.30", "饱和", "诊断", "工作区"):
        assert required in text, f"missing fixed-point guidance: {required}"


def test_usage_links_and_realtime_examples_are_complete() -> None:
    text = USAGE.read_text(encoding="utf-8")
    assert "AGC" in text and "THD" in text and "不得" in text
    for example in EXAMPLES:
        path = ROOT / "examples" / "api_usage" / example
        assert path.is_file(), f"missing example: {path}"
        assert example in text, f"tutorial does not link {example}"
        assert not FORBIDDEN.search(path.read_text(encoding="utf-8")), f"forbidden realtime token in {example}"


def test_examples_compile_with_gcc() -> None:
    gcc = shutil.which("gcc")
    if gcc is None:
        raise AssertionError("GCC is required for the documentation contract")
    for example in EXAMPLES:
        output = ROOT / "tests" / (Path(example).stem + ".doc-test.exe")
        command = [
            gcc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-Iinclude",
            "src/sigq15.c", "backends/sigq15_backends.c",
            str(Path("examples") / "api_usage" / example), "-o", str(output),
        ]
        completed = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
        assert completed.returncode == 0, completed.stdout + completed.stderr
        output.unlink(missing_ok=True)


def test_api_reference_explains_public_state_and_result_fields() -> None:
    text = API.read_text(encoding="utf-8")
    required_fields = (
        "saturation_count", "overflow_count", "invalid_count", "alpha_q15",
        "filled", "post_shift", "rms_q15", "frequency_millihz",
        "delay_q16_samples", "power_q30", "gain_q15", "amplitude_q15",
        "normalized", "integrator_q30", "coherence_q15", "thdn_q15",
    )
    missing = [field for field in required_fields if field not in text]
    assert not missing, "missing public field explanations: " + ", ".join(missing)
