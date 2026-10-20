// The generated headers contain the unmodified production function bodies.
#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <iostream>
#include <iterator>
#include <map>
#include <utility>
#include <vector>
struct Point2D {int X=0,Y=0;Point2D operator+(Point2D p)const{return {X+p.X,Y+p.Y};}Point2D operator-(Point2D p)const{return {X-p.X,Y-p.Y};}Point2D& operator+=(Point2D p){X+=p.X;Y+=p.Y;return *this;}bool operator==(const Point2D&)const=default;};
struct CellStruct {int X=0,Y=0;bool operator==(const CellStruct&)const=default;};
struct CoordStruct {int X=0,Y=0,Z=0;};
struct RectangleStruct {int X=0,Y=0,Width=0,Height=0;bool operator==(const RectangleStruct&)const=default;};
struct Image {int Width=60,Height=48,Frames=1;};
struct ObjectType {Image* AlphaImage=nullptr;};
enum class AbstractType {TerrainType,UnitType};
enum class Mission {Construction,Guard};
enum class CloakState {Cloaked,Uncloaked};
int CoordsQueries=0,PowerQueries=0,FogQueries=0,Creates=0,Deletes=0;
struct ObjectClass {
 virtual ~ObjectClass()=default;bool IsAlive=true,InLimbo=false;ObjectType* Type=nullptr;CoordStruct Location{};
 ObjectType* GetType(){return Type;}CoordStruct GetCoords(){++CoordsQueries;return Location;}
};
struct AnimClass:ObjectClass {ObjectClass* OwnerObject=nullptr;};
struct Disguise {AbstractType Kind=AbstractType::TerrainType;AbstractType WhatAmI(){return Kind;}};
struct TechnoClass:ObjectClass {
 bool Deactivated=false,Disguised=false;CloakState CloakState=::CloakState::Uncloaked;int Height=0,RandomFactor=0;Disguise* DisguisedAs=nullptr;
 int GetHeight(){return Height;}bool IsDisguised(){return Disguised;}Disguise* GetDisguise(bool){return DisguisedAs;}
};
struct FootClass:TechnoClass {CellStruct LastMapCoords{},CurrentMapCoords{};};
struct BuildingClass:TechnoClass {
 AnimClass* Anims[21]{};bool AllFogged=false,Online=true;Mission CurrentMission=Mission::Guard;int LimboID=-1;
 bool IsAllFogged(){++FogQueries;return AllFogged;}bool IsPowerOnline(){++PowerQueries;return Online;}Mission GetCurrentMission(){return CurrentMission;}
};
template<class T,bool=true>T abstract_cast(ObjectClass* p){return dynamic_cast<T>(p);}
struct TacticalClass {
 inline static TacticalClass* Instance=nullptr;Point2D TacticalPos{100,200};std::vector<RectangleStruct> Dirty;
 std::pair<Point2D,bool> CoordsToClient(CoordStruct c){return {{c.X-c.Y,(c.X+c.Y)/2-c.Z},true};}
 void RegisterDirtyArea(RectangleStruct r,bool){Dirty.push_back(r);}
};
struct CellClass {static CoordStruct Cell2Coord(CellStruct p){return {p.X*256,p.Y*256,0};}};
struct ScenarioClass {inline static ScenarioClass* Instance=nullptr;struct {bool FogOfWar=false;}SpecialFlags;};
struct MapClass {inline static MapClass* Ptr=nullptr;struct Ref {bool IsLocationFogged(CoordStruct){return Ptr->LocationFogged;}};inline static Ref Instance;bool LocationFogged=false;};
struct Unsorted {inline static int CurrentFrame=1,ScenarioInit=0;};
struct TechnoExt {static TechnoClass* Fetch(TechnoClass* p){return p;}};
struct BuildingExt {static BuildingClass* Fetch(BuildingClass* p){return p;}};
struct OptionalBool {bool Value=false;bool Get(bool){return Value;}};
struct BuildingTypeExt {struct Value {OptionalBool NoAlphaImageOnBuildup;};inline static Value Data;static Value* Fetch(ObjectType*){return &Data;}};
struct RulesExt {struct Value {bool NoAlphaImageOnBuildup=false;};inline static Value Data;static Value* Global(){return &Data;}};
struct AlphaShapeClass {ObjectClass* Source;RectangleStruct Rect;Image* AlphaImage;};
struct AlphaMap {std::map<ObjectClass*,AlphaShapeClass*> Values;AlphaShapeClass* get_or_default(ObjectClass* p){auto i=Values.find(p);return i==Values.end()?nullptr:i->second;}};
struct AresFunctions {inline static AlphaMap* AlphaExtMap=nullptr;};
template<class T>void GameDelete(T* p){++Deletes;AresFunctions::AlphaExtMap->Values.erase(p->Source);delete p;}
template<class T>T* GameCreate(ObjectClass* p,int x,int y){
 ++Creates;assert(Unsorted::ScenarioInit==1);
 if(auto* old=AresFunctions::AlphaExtMap->get_or_default(p))GameDelete(old);
 auto* image=p->Type->AlphaImage;auto* a=new T{p,{x,y,image->Width,image->Height},image};
 AresFunctions::AlphaExtMap->Values[p]=a;return a;
}
#include "alpha-body.h"
struct AnimTypeClass {int YDrawOffset=0;int XOffset=0;};
struct AnimTypeExt {int XDrawOffset;static AnimTypeExt* Fetch(AnimTypeClass* p){static AnimTypeExt data;data.XDrawOffset=p->XOffset;return &data;}};
struct Registers {
 uintptr_t Source=0;unsigned Y=0;alignas(4) unsigned char Stack[256]{};
 unsigned EDX()const{return Y;}void EDX(int v){Y=static_cast<unsigned>(v);}
 template<class T>T& ref_Stack(size_t at){return *reinterpret_cast<T*>(Stack+at);}
};
#define DEFINE_HOOK(address,name,size) unsigned name(Registers* R)
#define GET(type,name,reg) type name=reinterpret_cast<type>(R->Source)
#include "offset-body.h"
void DropAll(){while(!AresFunctions::AlphaExtMap->Values.empty())GameDelete(AresFunctions::AlphaExtMap->Values.begin()->second);}
int main(){
 TacticalClass tactical;ScenarioClass scenario;MapClass map;AlphaMap alphas;
 TacticalClass::Instance=&tactical;ScenarioClass::Instance=&scenario;MapClass::Ptr=&map;AresFunctions::AlphaExtMap=&alphas;
 Image image;ObjectType type{&image};BuildingClass building;building.Type=&type;building.Location={1000,300,0};
 UpdateAlphaShape(&building);assert(Creates==1 && Deletes==0 && alphas.get_or_default(&building));
 const auto saved=alphas.get_or_default(&building)->Rect;
 scenario.SpecialFlags.FogOfWar=true;map.LocationFogged=true;building.AllFogged=true;
 building.Online=false;building.CloakState=CloakState::Cloaked;building.Deactivated=true;
 const auto beforePower=PowerQueries,beforeCoords=CoordsQueries;const auto beforeDirty=tactical.Dirty.size();
 for(int i=0;i<100;++i){Unsorted::CurrentFrame=i;UpdateAlphaShape(&building);}
 assert(Creates==1&&Deletes==0&&alphas.get_or_default(&building)->Rect==saved);
 assert(PowerQueries==beforePower&&CoordsQueries==beforeCoords&&tactical.Dirty.size()==beforeDirty);
 DropAll();const int missing=Creates;UpdateAlphaShape(&building);assert(Creates==missing&&alphas.Values.empty());
 building.AllFogged=false;UpdateAlphaShape(&building);assert(alphas.Values.empty()); // hidden state becomes observable
 building.Online=true;building.Deactivated=false;building.CloakState=CloakState::Uncloaked;Unsorted::CurrentFrame=101;
 // A fogged center does not hide a partially visible building's light.
 UpdateAlphaShape(&building);assert(alphas.get_or_default(&building));
 const auto partial=Creates;image.Frames=2;UpdateAlphaShape(&building);assert(Creates==partial+1);image.Frames=1;
 scenario.SpecialFlags.FogOfWar=false;building.AllFogged=true;building.Online=false;UpdateAlphaShape(&building);assert(!alphas.get_or_default(&building));
 building.Online=true;building.AllFogged=false;
 AnimClass attached;attached.Type=&type;attached.OwnerObject=&building;attached.Location=building.Location;building.Anims[7]=&attached;
 UpdateAlphaShape(&attached);assert(alphas.get_or_default(&attached));const auto animRect=alphas.get_or_default(&attached)->Rect;
 scenario.SpecialFlags.FogOfWar=true;building.AllFogged=true;const auto frozenCreates=Creates,frozenDeletes=Deletes;
 for(int i=0;i<100;++i)UpdateAlphaShape(&attached);
 assert(Creates==frozenCreates&&Deletes==frozenDeletes&&alphas.get_or_default(&attached)->Rect==animRect);
 // An arbitrary attached effect is not one of the native frozen anim segments.
 building.Anims[7]=nullptr;UpdateAlphaShape(&attached);assert(!alphas.get_or_default(&attached));
 FootClass unit;unit.Type=&type;ObjectClass loose;loose.Type=&type;
 scenario.SpecialFlags.FogOfWar=false;UpdateAlphaShape(&unit);UpdateAlphaShape(&loose);assert(alphas.get_or_default(&unit)&&alphas.get_or_default(&loose));
 scenario.SpecialFlags.FogOfWar=true;UpdateAlphaShape(&unit);UpdateAlphaShape(&loose);assert(!alphas.get_or_default(&unit)&&!alphas.get_or_default(&loose));
 scenario.SpecialFlags.FogOfWar=false;unit.Height=-128;UpdateAlphaShape(&unit);assert(!alphas.get_or_default(&unit));
 unit.Height=0;unit.InLimbo=true;UpdateAlphaShape(&unit);assert(!alphas.get_or_default(&unit));
 unit.InLimbo=false;unit.Deactivated=true;UpdateAlphaShape(&unit);assert(!alphas.get_or_default(&unit));
 unit.Deactivated=false;unit.CloakState=CloakState::Cloaked;UpdateAlphaShape(&unit);assert(!alphas.get_or_default(&unit));
 unit.CloakState=CloakState::Uncloaked;Disguise disguise;unit.Disguised=true;unit.DisguisedAs=&disguise;UpdateAlphaShape(&unit);assert(!alphas.get_or_default(&unit));
 unit.Disguised=false;Unsorted::CurrentFrame=102;UpdateAlphaShape(&unit);assert(!alphas.get_or_default(&unit)); // old 2-frame schedule
 Unsorted::CurrentFrame=103;unit.LastMapCoords={1,1};unit.CurrentMapCoords={2,1};UpdateAlphaShape(&unit);assert(alphas.get_or_default(&unit));
 DropAll();assert(!Unsorted::ScenarioInit);
 for(auto offset:std::array<Point2D,5>{{{0,0},{17,-31},{-53,11},{1,1},{-1,-1}}}){
  AnimTypeClass anim;anim.XOffset=offset.X;anim.YDrawOffset=offset.Y;Registers r;
  r.Source=reinterpret_cast<uintptr_t>(&anim);r.Y=700;r.ref_Stack<int>(0x38)=1200;r.ref_Stack<int>(0x3C)=0x1234;
  assert(FoggedObjectClass_DrawFoggedObjects_AnimDrawOffset(&r)==0);
  assert(r.ref_Stack<int>(0x38)==1200+offset.X&&static_cast<int>(r.Y)==700+offset.Y);
  assert(r.Source==reinterpret_cast<uintptr_t>(&anim)&&r.ref_Stack<int>(0x3C)==0x1234);
  r.ref_Stack<int>(0x3C)=static_cast<int>(r.Y); // native 4D2102 commits adjusted Y
  r.Source=0; // native animation branch clears ESI before the former hook
  assert(r.ref_Stack<int>(0x38)==1200+offset.X&&r.ref_Stack<int>(0x3C)==700+offset.Y);
 }
 std::cout<<"PASS: production alpha updater, frozen/native-source ownership, no hidden-state query or recreation, partial visibility, reveal, no speculative lights, normal inactive/mobile/standalone policy, old frame schedule.\n";
 std::cout<<"PASS: production offset callback, signed X/Y, nonzero origin, native Y commit, ESI lifetime and untouched stack neighbours.\n";
}
