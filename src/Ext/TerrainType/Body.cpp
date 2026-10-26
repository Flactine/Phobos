#include "Body.h"

#include <Ext/Anim/Body.h>
#include <ConvertClass.h>
#include <FileSystem.h>
#include <Memory.h>
#include <Surface.h>

TerrainTypeExt::ExtContainer TerrainTypeExt::ExtMap;

int TerrainTypeExt::GetTiberiumGrowthStage()
{
	return GeneralUtils::GetRangedRandomOrSingleValue(this->SpawnsTiberium_GrowthStage.Get());
}

int TerrainTypeExt::GetCellsPerAnim()
{
	return GeneralUtils::GetRangedRandomOrSingleValue(this->SpawnsTiberium_CellsPerAnim.Get());
}

void TerrainTypeExt::PlayDestroyEffects(const CoordStruct& coords)
{
	VocClass::PlayIndexAtPos(this->DestroySound, coords);
	AnimExt::CreateRandomAnim(this->DestroyAnim, coords);
}

void TerrainTypeExt::Remove(TerrainClass* pTerrain)
{
	if (!pTerrain)
		return;

	RectangleStruct rect = RectangleStruct {};
	rect = *pTerrain->GetRenderDimensions(&rect);
	TacticalClass::Instance->RegisterDirtyArea(rect, false);
	pTerrain->Disappear(true);
	pTerrain->UnInit();
}

// =============================
// load / save

template <typename T>
void TerrainTypeExt::Serialize(T& Stm)
{
	Stm
		.Process(this->SpawnsTiberium_Type)
		.Process(this->SpawnsTiberium_Range)
		.Process(this->SpawnsTiberium_GrowthStage)
		.Process(this->SpawnsTiberium_CellsPerAnim)
		.Process(this->SpawnsTiberium_Particle)
		.Process(this->DestroyAnim)
		.Process(this->DestroySound)
		.Process(this->MinimapColor)
		.Process(this->IsPassable)
		.Process(this->CanBeBuiltOn)
		.Process(this->HasDamagedFrames)
		.Process(this->HasCrumblingFrames)
		.Process(this->CrumblingSound)
		.Process(this->AnimationLength)
		.Process(this->PaletteFile)
		.Process(this->PaletteRemap)
		.Process(this->IgnoredByMouse)
		;
}

void TerrainTypeExt::LoadFromINIFile(CCINIClass* const pINI)
{
	auto pThis = this->OwnerObject();
	const char* pSection = pThis->ID;
	INI_EX exINI(pINI);

	this->SpawnsTiberium_Type.Read(exINI, pSection, "SpawnsTiberium.Type");
	this->SpawnsTiberium_Range.Read(exINI, pSection, "SpawnsTiberium.Range");
	this->SpawnsTiberium_GrowthStage.Read(exINI, pSection, "SpawnsTiberium.GrowthStage");
	this->SpawnsTiberium_CellsPerAnim.Read(exINI, pSection, "SpawnsTiberium.CellsPerAnim");
	this->SpawnsTiberium_Particle.Read(exINI, pSection, "SpawnsTiberium.Particle");

	this->DestroyAnim.Read(exINI, pSection, "DestroyAnim");
	this->DestroySound.Read(exINI, pSection, "DestroySound");

	this->MinimapColor.Read(exINI, pSection, "MinimapColor");

	this->IsPassable.Read(exINI, pSection, "IsPassable");
	this->CanBeBuiltOn.Read(exINI, pSection, "CanBeBuiltOn");

	this->HasDamagedFrames.Read(exINI, pSection, "HasDamagedFrames");
	this->HasCrumblingFrames.Read(exINI, pSection, "HasCrumblingFrames");
	this->CrumblingSound.Read(exINI, pSection, "CrumblingSound");
	this->AnimationLength.Read(exINI, pSection, "AnimationLength");

	//Strength is already part of ObjecTypeClass::ReadIni Duh!
	//this->TerrainStrength.Read(exINI, pSection, "Strength");

	this->IgnoredByMouse.Read(exINI, pSection, "IgnoredByMouse");

	INI_EX exArtINI(&CCINIClass::INI_Art);

	this->PaletteFile.Read(&CCINIClass::INI_Art, pThis->ImageFile, "Palette");
	this->PaletteRemap.Read(exArtINI, pThis->ImageFile, "PaletteRemap");
	this->Palette = GeneralUtils::BuildPalette(this->PaletteFile);

	// PlainPalette is created lazily in GetPlainPalette on first use.
	// The generated color schemes replace palette indices 16-31 with shades of the owning house
	// color; the lazy converter is built from the unmodified palette for PaletteRemap=no.
	if (GeneralUtils::IsValidString(this->PaletteFile) && !this->Palette)
		Debug::Log("[Developer warning] [%s] has Palette=%s set but no palette file was loaded (missing file or wrong filename). Missing palettes cause issues with lighting recalculations.\n", pThis->ImageFile, this->PaletteFile.data());
}

ConvertClass* TerrainTypeExt::GetPlainPalette()
{
	// 不序列化、也不在读档反序列化时立即重建：加载窗口内 DSurface::Temp 可能尚未
	// 重建（悬垂指针），此时构造 ConvertClass 会在查询 surface 像素格式时崩溃
	// （2026.10.26 迷雾存档读档崩溃）。惰性到首次绘制使用时创建，彼时 Temp 必然有效。
	if (!this->PaletteRemap && !this->PlainPalette && this->Palette && this->Palette->Count > 0)
		this->PlainPalette = GameCreate<ConvertClass>(this->Palette->Items[0]->Colors, FileSystem::TEMPERAT_PAL, DSurface::Temp, 0x35, false);

	return this->PlainPalette;
}

void TerrainTypeExt::LoadFromStream(PhobosStreamReader& Stm)
{
	ObjectTypeExt::LoadFromStream(Stm);
	this->Serialize(Stm);
	this->Palette = GeneralUtils::BuildPalette(this->PaletteFile);
}

void TerrainTypeExt::SaveToStream(PhobosStreamWriter& Stm)
{
	ObjectTypeExt::SaveToStream(Stm);
	this->Serialize(Stm);
}

bool TerrainTypeExt::LoadGlobals(PhobosStreamReader& Stm)
{
	return Stm
		.Success();
}

bool TerrainTypeExt::SaveGlobals(PhobosStreamWriter& Stm)
{
	return Stm
		.Success();
}

// =============================
// container

TerrainTypeExt::ExtContainer::ExtContainer() : Container("TerrainTypeClass") { }
TerrainTypeExt::ExtContainer::~ExtContainer() = default;

// =============================
// container hooks

DEFINE_HOOK(0x71DBC0, TerrainTypeClass_CTOR, 0x7)
{
	GET(TerrainTypeClass*, pItem, ESI);

	TerrainTypeExt::ExtMap.TryAllocate(pItem);

	// Override the default value (true) from game constructor.
	pItem->RadarInvisible = false;

	return 0;
}

DEFINE_HOOK_AGAIN(0x71DC04, TerrainTypeClass_SDDTOR, 0x6)
DEFINE_HOOK(0x71E364, TerrainTypeClass_SDDTOR, 0x6)
{
	GET(TerrainTypeClass*, pItem, ECX);

	TerrainTypeExt::ExtMap.Remove(pItem);

	// FileSystem owns cached SHP references; raw theater images are freed below.
	if (pItem->Image && !pItem->ImageAllocated && pItem->Image->IsReference())
		pItem->Image = nullptr;

	return 0;
}

DEFINE_HOOK(0x71E0A6, TerrainTypeClass_LoadFromINI, 0x5)
{
	GET(TerrainTypeClass*, pItem, ESI);
	GET_STACK(CCINIClass*, pINI, STACK_OFFSET(0x210, 0x4));

	TerrainTypeExt::ExtMap.LoadFromINI(pItem, pINI);

	return 0;
}
