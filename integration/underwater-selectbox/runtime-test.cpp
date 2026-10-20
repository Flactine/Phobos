#include <cassert>
#include <cstdio>
#include <cstring>
#include <utility>
struct Point2D {int X=0,Y=0;Point2D& operator+=(Point2D p){X+=p.X;Y+=p.Y;return *this;}};
enum class AbstractType {Unit,Aircraft};
struct TechnoClass {int depth=0,height=-128;AbstractType type=AbstractType::Unit;Point2D render{123,240};AbstractType WhatAmI(){return type;}Point2D GetRenderCoords(){return render;}int GetHeight(){return height;}};
struct Loco {int calls=0;Point2D Shadow_Point(){++calls;return {3,-40};}};
struct FootClass:TechnoClass {Loco native;Loco* Locomotor=&native;};
static bool modulePresent=true,exportPresent=true;static int resolves=0,modules=0,queries=0;
static int __cdecl Query(TechnoClass* t){++queries;return t?t->depth:0;}
static void* Module(const char* name){++modules;assert(!strcmp(name,"Kaleidoscope.dll"));return modulePresent?reinterpret_cast<void*>(1):nullptr;}
static void* Proc(void*,const char* name){++resolves;assert(!strcmp(name,"Kaleidoscope_GetUnderwaterDepth"));return exportPresent?reinterpret_cast<void*>(&Query):nullptr;}
#define GetModuleHandleA Module
#define GetProcAddress Proc
#include "depth-consumer.h"
struct TacticalClass {
 static TacticalClass* Instance;static int AdjustForZ(int z){return z/4;}
 std::pair<Point2D,bool> CoordsToClient(Point2D p){return {p,true};}
};static TacticalClass tactical;TacticalClass* TacticalClass::Instance=&tactical;
#include "select-anchor.h"
int main(int argc,char** argv) {
 if(argc>1 && !strcmp(argv[1],"no-module"))modulePresent=false;
 if(argc>1 && !strcmp(argv[1],"no-export"))exportPresent=false;
 const bool available=modulePresent && exportPresent;
 FootClass unit;
 for(int depth:{-1,0,1,8,128,256,128,1,0}) {
  unit.depth=depth;const auto count=unit.native.calls;const auto p=Anchor(&unit);
  const bool under=available && depth>0;
  assert(p.X==(under?123:126) && p.Y==(under?240:200));
  assert(unit.native.calls==count+(under?0:1));
 }
 unit.type=AbstractType::Aircraft;unit.depth=128;unit.height=160;const auto p=Anchor(&unit);
 assert(p.X==123 && p.Y==280); // original aircraft surface projection
 assert(modules==1 && resolves==(modulePresent?1:0));
 assert(queries==(available?9:0)); // aircraft never enters optional depth query
 puts("PASS: production Phobos SelectBox anchor and cached public depth consumer; descent/ascent depth1/0, ordinary and aircraft original projection, missing module/export passthrough, no canvas/coordinates mutation.");
}
