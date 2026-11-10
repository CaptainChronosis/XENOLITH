"""Focused host verification only; builds no firmware and touches no device."""
from pathlib import Path
import hashlib
import json
import subprocess
import tempfile

source = Path(__file__).resolve().parents[1]
test = Path(__file__).with_name("test_core_performance.c")
results = {}
with tempfile.TemporaryDirectory(prefix="xenolith_performance_") as directory:
    for label, flags in (
        ("optimized", []),
        ("sine_cache", ["-DXENOLITH_TEST_SINE_CACHE"]),
        ("ubsan", ["-fsanitize=undefined,float-cast-overflow", "-fno-sanitize-recover=all"]),
    ):
        executable = Path(directory) / (label + ".exe")
        command = ["gcc", "-std=c11", "-O2", "-ffp-contract=off", "-Wall", "-Wextra", "-Werror",
                   *flags, str(test), "-o", str(executable)]
        subprocess.run(command, check=True, capture_output=True, text=True, timeout=60)
        result = subprocess.run([str(executable)], check=True, capture_output=True, text=True, timeout=60)
        results[label] = json.loads(result.stdout)
print(json.dumps({
    "passed": True,
    "scope": "Host PCM identity and spread/glide state/rate invariants; no firmware, listening or hardware timing claim.",
    "results": results,
    "sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
               for p in (source / "xenolith_core.c", source / "xenolith_core.h", test)},
}, indent=2))
