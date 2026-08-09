from pathlib import Path
import json
import re
import sys

plugin_root = Path(__file__).resolve().parents[1]
root = plugin_root / "Source" / "AkumasRPGFramework"
issues = []
warnings = []
source_files = [p for p in root.rglob("*") if p.suffix in {".h", ".cpp"}]

for p in source_files:
    s = p.read_text(errors="replace")
    t = re.sub(r"//.*", "", s)
    t = re.sub(r'"(?:\\.|[^"\\])*"', '""', t)
    if t.count("{") != t.count("}"):
        issues.append(f"{p.relative_to(root)} brace mismatch {t.count('{')} vs {t.count('}')}")
    if p.suffix == ".h" and '.generated.h"' in s:
        includes = [m.group(1) for m in re.finditer(r'^#include\s+"([^"]+)"', s, re.M)]
        generated = [x for x in includes if x.endswith(".generated.h")]
        if len(generated) != 1:
            issues.append(f"{p.relative_to(root)} generated include count={len(generated)}")
        elif includes[-1] != generated[0]:
            issues.append(f"{p.relative_to(root)} generated include is not the final include")

classes = {}
for p in root.rglob("*.h"):
    s = p.read_text(errors="replace")
    for m in re.finditer(r"\bclass\s+AKUMASRPGFRAMEWORK_API\s+(\w+)\s*[:{]", s):
        classes.setdefault(m.group(1), []).append(p.relative_to(root).as_posix())
for name, locations in classes.items():
    if len(locations) > 1:
        issues.append(f"duplicate exported class {name}: {locations}")

cpp_text = "\n".join(p.read_text(errors="replace") for p in root.rglob("*.cpp"))
for hp in root.rglob("*.h"):
    s = hp.read_text(errors="replace")
    for m in re.finditer(r"UFUNCTION\s*\(([^)]*\b(?:Server|Client|NetMulticast)\b[^)]*)\)\s*(?:virtual\s+)?[^;{]+?\b(\w+)\s*\([^;{]*\)\s*;", s, re.S):
        fn = m.group(2)
        if not re.search(r"::" + re.escape(fn) + r"_Implementation\s*\(", cpp_text):
            issues.append(f"{hp.relative_to(root)} RPC {fn} missing _Implementation")

for p in source_files:
    for line_no, line in enumerate(p.read_text(errors="replace").splitlines(), 1):
        m = re.match(r'\s*#include\s+"([^"]+)"', line)
        if not m:
            continue
        inc = m.group(1)
        if inc.endswith(".generated.h") or "ARPG" not in Path(inc).name:
            continue
        if not ((root / "Public" / inc).exists() or (root / "Private" / inc).exists() or (p.parent / inc).exists()):
            issues.append(f"{p.relative_to(root)}:{line_no} missing plugin include {inc}")

for p in root.rglob("*.cpp"):
    s = p.read_text(errors="replace")
    if re.search(r"SetTimer(?:ForNextTick)?\([^\n]*&\w+::(?:SaveNow|LoadNow|SavePersistentWorld|LoadPersistentWorld)", s):
        issues.append(f"{p.relative_to(root)} timer appears bound to a bool-returning method")

storage_header = root / "Public" / "Crafting" / "ARPGStorageActor.h"
if storage_header.exists() and "TObjectPtr<UARPGFactionOwnershipComponent> Ownership" in storage_header.read_text():
    issues.append("Storage actor duplicates base building Ownership component")

try:
    descriptor = json.loads((plugin_root / "AkumasRPGFramework.uplugin").read_text())
    plugin_refs = {entry.get("Name") for entry in descriptor.get("Plugins", []) if isinstance(entry, dict)}
    for module_only_name in ("GameplayTags", "GameplayTasks"):
        if module_only_name in plugin_refs:
            issues.append(f".uplugin incorrectly declares runtime module {module_only_name} as a plugin dependency")
except Exception as exc:
    issues.append(f"invalid .uplugin JSON: {exc}")

markers = []
for p in source_files:
    for line_no, line in enumerate(p.read_text(errors="replace").splitlines(), 1):
        if re.search(r"\b(TODO|FIXME|PLACEHOLDER|STUB)\b", line, re.I):
            markers.append(f"{p.relative_to(root)}:{line_no}")
if markers:
    warnings.append(f"development markers found: {markers[:20]}")

line_count = sum(len(p.read_text(errors="replace").splitlines()) for p in source_files)
result = {
    "issues": issues,
    "warnings": warnings,
    "exported_class_count": len(classes),
    "source_file_count": len(source_files),
    "source_line_count": line_count,
}
print(json.dumps(result, indent=2))
sys.exit(1 if issues else 0)
