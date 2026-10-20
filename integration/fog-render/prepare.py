"""Compile the actual production bodies against controlled native adapters."""
from pathlib import Path
import argparse
import shutil
import sys

sys.excepthook = lambda t, v, b: (print("Fixture preparation failed: " + t.__name__), sys.exit(1))
p = argparse.ArgumentParser()
p.add_argument("--root", type=Path, required=True)
p.add_argument("--out", type=Path, required=True)
a = p.parse_args()
a.out.mkdir(parents=True, exist_ok=True)
alpha = (a.root / "src/Misc/Hooks.AlphaImage.cpp").read_text(encoding="utf-8-sig")
start = alpha.index("static void __fastcall UpdateAlphaShape(")
end = alpha.index("\n}\n", start) + 3
(a.out / "alpha-body.h").write_text(alpha[start:end], encoding="utf-8")
source = (a.root / "src/Misc/Hooks.AI.cpp").read_text(encoding="utf-8-sig")
start = source.index("DEFINE_HOOK(0x4D20FA, FoggedObjectClass_DrawFoggedObjects_AnimDrawOffset, 0x6)")
end = source.index("\n}\n", start) + 3
(a.out / "offset-body.h").write_text(source[start:end], encoding="utf-8")
shutil.copyfile(Path(__file__).with_name("runtime-test.cpp"), a.out / "runtime-test.cpp")
print("Prepared unmodified production alpha updater and animation callback bodies.")
