from pathlib import Path
import argparse, shutil, sys
sys.excepthook=lambda t,v,b:(print("Selection fixture preparation failed: "+t.__name__),sys.exit(1))
p=argparse.ArgumentParser();p.add_argument("--root",type=Path,required=True);p.add_argument("--out",type=Path,required=True);a=p.parse_args()
a.out.mkdir(parents=True,exist_ok=True)
s=(a.root/"src/Ext/Techno/Body.Visuals.cpp").read_text(encoding="utf-8-sig")
start=s.index("void TechnoExt::DrawSelectBox(");start=s.index("auto [point, visible]",start);end=s.index("if (visible && pGroundShape)",start)
(a.out/"select-anchor.h").write_text("static Point2D Anchor(TechnoClass* pThis){\n"+s[start:end]+"return point;\n}\n")
s=(a.root/"src/Interop/Underwater.h").read_text(encoding="utf-8-sig")
(a.out/"depth-consumer.h").write_text(s.replace("#include <TechnoClass.h>",""))
shutil.copyfile(Path(__file__).with_name("runtime-test.cpp"),a.out/"runtime-test.cpp")
print("Prepared actual selection anchor and optional depth API consumer.")
