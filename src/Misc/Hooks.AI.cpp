#include <New/Entity/AttachmentClass.h>

#include <Ext/Anim/Body.h>
#include <Ext/Scenario/Body.h>
#include <Ext/TerrainType/Body.h>
#include <Utilities/Macro.h>
#include <Interop/FogOwner.h>

#pragma region SmudgeUpdate

static bool __forceinline ShouldRemoveSmudgeCell(const int index, const int time, const int current)
{
	const auto cell = CellStruct{static_cast<short>(index & 511), static_cast<short>(index >> 9) };

	if (const auto pCell = MapClass::Instance.TryGetCellAt(cell))
	{
		if (pCell->SmudgeTypeIndex != -1)
		{
			// 迷雾中的弹坑跳过渐隐推进：冻结弹坑以固定混合标志绘制，本就不跟随渐隐状态，
			// 推进只会让迷雾格子反复执行昂贵的雾化重绘（冻结对象+雾层+黑幕+光效），毫无视觉
			// 收益；格子留在集合中，揭开迷雾后从当前状态继续正常渐隐
			if (ScenarioClass::Instance->SpecialFlags.FogOfWar && (pCell->Flags & CellFlags::Fogged))
				return false;

			const auto pCellExt = CellExt::Fetch(pCell);

			if ((pCellExt->SmudgeGenerate + time) > current)
				return false;

			const auto state = pCellExt->SmudgeState;

			if (state != BlitterFlags::TransLucent75)
			{
				pCellExt->SmudgeGenerate = current;
				pCellExt->SmudgeState = static_cast<BlitterFlags>(static_cast<size_t>(state) + 2u);
				pCell->MarkForRedraw();
				return false;
			}

			pCell->SmudgeTypeIndex = -1;
			pCell->MarkForRedraw();
		}
	}

	return true;
}

#pragma region FogSpread

namespace FogSpreadHelper
{
	std::vector<CellStruct> PendingCells;
	const ScenarioClass* LastScenario = nullptr;
	int LastSpreadFrame = 0;

	// 迷雾蔓延：原版的迷雾恢复是一次性的递归扩散（0x4ACC50 对自身置完全遮挡后，沿遮挡为
	// 完全遮挡的邻居立即递归波及整片区域）。FogSpreadRate>0 时蔓延前沿的格子进入待处理
	// 队列，每隔 FogSpreadRate 分钟才把队列整体推进一次（分钟换算为帧与原版 BuildupTime
	// 一致，1 分钟 = 900 帧），最终状态与原版完全一致，仅蔓延过程放缓。
	void ProcessQueue()
	{
		// 对局切换后丢弃上一局的残留队列并重新计时
		if (ScenarioClass::Instance != LastScenario)
		{
			LastScenario = ScenarioClass::Instance;
			LastSpreadFrame = Unsorted::CurrentFrame;
			PendingCells.clear();
			return;
		}

		const double minutes = RulesExt::Global()->FogSpreadRate;

		if (minutes <= 0.0 || PendingCells.empty())
			return;

		int interval = static_cast<int>(minutes * 900.0);

		if (interval < 1)
			interval = 1;

		const int current = Unsorted::CurrentFrame;

		if ((current - LastSpreadFrame) < interval)
			return;

		LastSpreadFrame = current;

		// 本次蔓延：队列中的格子交给原版扩散函数，它们递归出的新前沿格子会在钩子中重新
		// 入队，等待下一个间隔
		const auto cells = std::move(PendingCells);
		PendingCells.clear();

		for (const auto& coords : cells)
		{
			if (const auto pCell = MapClass::Instance.TryGetCellAt(coords))
			{
				// 已被前序蔓延处理的格子跳过（去重）
				if (pCell->Foggedness != -2)
					reinterpret_cast<void(__thiscall*)(MapClass*, const CellStruct*)>(0x4ACC50)(&MapClass::Instance, &coords);
			}
		}
	}
}

#pragma endregion

DEFINE_HOOK(0x55B6B3, LogicClass_AI_After, 0x5)
{
	for (auto const& attachment : AttachmentClass::Array)
		attachment->AI();

	FogSpreadHelper::ProcessQueue();

	const int time = RulesExt::Global()->SmudgeUpdateTime;

	if (time > 0)
	{
		auto& s = ScenarioExt::Global()->Smudges;

		if (!s.empty())
		{
			const int current = Unsorted::CurrentFrame;

			for (auto it = s.begin(); it != s.end(); )
			{
				if (ShouldRemoveSmudgeCell(*it, time, current))
					it = s.erase(it);
				else
					++it;
			}
		}
	}

	return 0;
}

DEFINE_HOOK(0x6B60DE, SmudgeTypeClass_Mark_SetContext, 0x6)
{
	GET(CellClass* const, pCell, EAX);

	ScenarioExt::Global()->Smudges.insert(MapClass::GetCellIndex(pCell->MapCoords));
	const auto pCellExt = CellExt::Fetch(pCell);
	pCellExt->SmudgeGenerate = Unsorted::CurrentFrame;
	pCellExt->SmudgeState = BlitterFlags::None;

	return 0;
}

DEFINE_HOOK(0x6B56AC, SmudgeTypeClass_DrawIt_DrawTrans, 0x5)
{
	GET(CellClass* const, pCell, ESI);
	REF_STACK(BlitterFlags, flags, STACK_OFFSET(0x3C, -0x3C));

	flags |= CellExt::Fetch(pCell)->SmudgeState;

	return 0;
}

#pragma endregion

#pragma region AirBarrier

void __fastcall FindMovingInfOrVeh(CellClass* const pCell, const AbstractType findType)
{
	const auto flag = pCell->OccupationFlags;
	pCell->OccupationFlags = 0;
	auto checkCell = pCell->MapCoords + CellStruct { 2, 2 };

	for (short checkX = checkCell.X - 4; checkX <= checkCell.X; ++checkX)
	{
		for (short checkY = checkCell.Y - 4; checkY <= checkCell.Y; ++checkY)
		{
			const auto pAdjCheckCell = MapClass::Instance.GetCellAt(CellStruct { checkX, checkY });

			for (auto pObject = pAdjCheckCell->FirstObject; pObject; pObject = pObject->NextObject)
			{
				if (pObject->WhatAmI() == findType && CellClass::Coord2Cell(static_cast<FootClass*>(pObject)->Locomotor->Head_To_Coord()) == pCell->MapCoords)
				{
					pCell->OccupationFlags = flag;
					return;
				}
			}
		}
	}
}

void __fastcall FindMovingInfAndVeh(CellClass* const pCell)
{
	const auto flag = pCell->OccupationFlags;
	pCell->OccupationFlags = 0;
	bool inf = false;
	bool veh = false;
	auto checkCell = pCell->MapCoords + CellStruct { 2, 2 };

	for (short checkX = checkCell.X - 4; checkX <= checkCell.X; ++checkX)
	{
		for (short checkY = checkCell.Y - 4; checkY <= checkCell.Y; ++checkY)
		{
			const auto pAdjCheckCell = MapClass::Instance.GetCellAt(CellStruct { checkX, checkY });

			for (auto pObject = pAdjCheckCell->FirstObject; pObject; pObject = pObject->NextObject)
			{
				const auto absType = pObject->WhatAmI();

				if (absType == AbstractType::Infantry)
				{
					if (!inf && CellClass::Coord2Cell(static_cast<FootClass*>(pObject)->Locomotor->Head_To_Coord()) == pCell->MapCoords)
					{
						pCell->OccupationFlags |= (flag & 0x1F);

						if (veh)
							return;

						inf = true;
					}
				}
				else if (absType == AbstractType::Unit)
				{
					if (!veh && CellClass::Coord2Cell(static_cast<FootClass*>(pObject)->Locomotor->Head_To_Coord()) == pCell->MapCoords)
					{
						pCell->OccupationFlags |= (flag & 0x20);

						if (inf)
							return;

						veh = true;
					}
				}
			}
		}
	}
}

void __fastcall FindAltMovingInfOrVeh(CellClass* const pCell, const AbstractType findType)
{
	const auto flag = pCell->AltOccupationFlags;
	pCell->AltOccupationFlags = 0;
	auto checkCell = pCell->MapCoords + CellStruct { 2, 2 };

	for (short checkX = checkCell.X - 4; checkX <= checkCell.X; ++checkX)
	{
		for (short checkY = checkCell.Y - 4; checkY <= checkCell.Y; ++checkY)
		{
			const auto pAdjCheckCell = MapClass::Instance.GetCellAt(CellStruct { checkX, checkY });

			for (auto pObject = pAdjCheckCell->AltObject; pObject; pObject = pObject->NextObject)
			{
				if (pObject->WhatAmI() == findType && CellClass::Coord2Cell(static_cast<FootClass*>(pObject)->Locomotor->Head_To_Coord()) == pCell->MapCoords)
				{
					pCell->AltOccupationFlags = flag;
					return;
				}
			}
		}
	}
}

void __fastcall FindAltMovingInfAndVeh(CellClass* const pCell)
{
	const auto flag = pCell->AltOccupationFlags;
	pCell->AltOccupationFlags = 0;
	bool inf = false;
	bool veh = false;
	auto checkCell = pCell->MapCoords + CellStruct { 2, 2 };

	for (short checkX = checkCell.X - 4; checkX <= checkCell.X; ++checkX)
	{
		for (short checkY = checkCell.Y - 4; checkY <= checkCell.Y; ++checkY)
		{
			const auto pAdjCheckCell = MapClass::Instance.GetCellAt(CellStruct { checkX, checkY });

			for (auto pObject = pAdjCheckCell->AltObject; pObject; pObject = pObject->NextObject)
			{
				const auto absType = pObject->WhatAmI();

				if (absType == AbstractType::Infantry)
				{
					if (!inf && CellClass::Coord2Cell(static_cast<FootClass*>(pObject)->Locomotor->Head_To_Coord()) == pCell->MapCoords)
					{
						pCell->AltOccupationFlags |= (flag & 0x1F);

						if (veh)
							return;

						inf = true;
					}
				}
				else if (absType == AbstractType::Unit)
				{
					if (!veh && CellClass::Coord2Cell(static_cast<FootClass*>(pObject)->Locomotor->Head_To_Coord()) == pCell->MapCoords)
					{
						pCell->AltOccupationFlags |= (flag & 0x20);

						if (inf)
							return;

						veh = true;
					}
				}
			}
		}
	}
}

DEFINE_HOOK(0x55B4E1, LogicClass_Update_UnmarkCellOccupationFlags, 0x5)
{
	const auto delay = RulesExt::Global()->CleanUpAirBarrier.Get();

	if (delay > 0 && !(Unsorted::CurrentFrame % delay))
	{
		auto& pMap = MapClass::Instance;
		pMap.CellIteratorReset();

		for (auto pCell = pMap.CellIteratorNext(); pCell; pCell = pMap.CellIteratorNext())
		{
			if ((0xFF & pCell->OccupationFlags) && !pCell->FirstObject)
			{
				pCell->OccupationFlags &= 0x3F; // ~(Aircraft | Building)

				if (pCell->OccupationFlags & 0x1F)
				{
					if (pCell->OccupationFlags & 0x20)
						FindMovingInfAndVeh(pCell);
					else
						FindMovingInfOrVeh(pCell, AbstractType::Infantry);
				}
				else if (pCell->OccupationFlags & 0x20)
				{
					FindMovingInfOrVeh(pCell, AbstractType::Unit);
				}
			}

			if ((0xFF & pCell->AltOccupationFlags) && !pCell->AltObject)
			{
				pCell->AltOccupationFlags &= 0x3F; // ~(Aircraft | Building)

				if (pCell->AltOccupationFlags & 0x1F)
				{
					if (pCell->AltOccupationFlags & 0x20)
						FindAltMovingInfAndVeh(pCell);
					else
						FindAltMovingInfOrVeh(pCell, AbstractType::Infantry);
				}
				else if (pCell->AltOccupationFlags & 0x20)
				{
					FindAltMovingInfOrVeh(pCell, AbstractType::Unit);
				}
			}
		}
	}

	return 0;
}

#pragma endregion

#pragma region DetectionLogic

namespace FoggedObjectHelper
{
	bool NoReveal = false;
	struct FoggedObjectClassFake
	{
		char _[0x40];
		RectangleStruct RenderDimension;
		char __[0x24];
		bool Visible;
	};
	static_assert(sizeof(FoggedObjectClassFake) == 0x78u);
	bool SetVectorCapacity(DynamicVectorClass<FoggedObjectClass*>* pVector, int capacity)
	{
		return reinterpret_cast<bool(__thiscall*)(DynamicVectorClass<FoggedObjectClass*>*, int, FoggedObjectClass **)>(0x45A680)(pVector, capacity, nullptr);
	}
	bool ObjectUnlimbo(ObjectClass* pObject, const CoordStruct& position, DirType face)
	{
		return reinterpret_cast<bool(__thiscall*)(ObjectClass*, const CoordStruct&, DirType)>(0x5F4EC0)(pObject, position, face);
	}
	void AddItemToVector(DynamicVectorClass<FoggedObjectClass*>* pVector, FoggedObjectClass* pItem)
	{
		if(pVector->Count < pVector->Capacity
			|| (pVector->IsAllocated || !pVector->Capacity)
				&& pVector->CapacityIncrement > 0
				&& FoggedObjectHelper::SetVectorCapacity(pVector, pVector->Capacity + pVector->CapacityIncrement))
		{
			pVector->Items[pVector->Count++] = std::move(pItem);
		}
	}
	DynamicVectorClass<FoggedObjectClass*>* CreateGameDynamicVector()
	{
		void* raw = YRMemory::Allocate(sizeof(DynamicVectorClass<FoggedObjectClass*>));
		*reinterpret_cast<void**>(raw) = reinterpret_cast<void*>(0x7E44F4u);
		auto pVector = static_cast<DynamicVectorClass<FoggedObjectClass*>*>(raw);
		pVector->Items = nullptr;
		pVector->Capacity = 0;
		pVector->IsInitialized = true;
		pVector->IsAllocated = false;
		pVector->Count = 0;
		pVector->CapacityIncrement = 1;
		FoggedObjectHelper::SetVectorCapacity(pVector, 1);
		return pVector;
	}
	void ClearAllFog()
	{
		MapClass::Instance.CellIteratorReset();
		for (auto pCell = MapClass::Instance.CellIteratorNext(); pCell; pCell = MapClass::Instance.CellIteratorNext())
		{
			pCell->ShroudCounter = 0;
			pCell->GapsCoveringThisCell = 0;
			pCell->AltFlags |= (AltCellFlags::Mapped | AltCellFlags::NoFog);
			pCell->Flags |= (CellFlags::CenterRevealed | CellFlags::EdgeRevealed);
			pCell->Flags &= ~CellFlags::Fogged;
			pCell->ClearFoggedObjects();
		}
		MapClass::Instance.sub_657CE0();
		MapClass::Instance.MarkNeedsRedraw(2);
	}
	static FoggedObjectClass* __fastcall CreateFoggedOverlay(void* pThis, void* _, const CoordStruct& coords, int OverlayTypeIndex, int OverlayData) JMP_THIS(0x4D0980);
	static FoggedObjectClass* __fastcall CreateFoggedSmudge(void* pThis, void* _, const CoordStruct& coords, int SmudgeTypeIndex, int SmudgeData) JMP_THIS(0x4D0C40);
	static FoggedObjectClass* __fastcall CreateFoggedTerrain(void* pThis, void* _, TerrainClass* pTerrain) JMP_THIS(0x4D1370);
	static void __fastcall TechnoClass_RevealLastSight(TechnoClass* pThis, void* _, bool OnlyOutline, bool RevealByHeight, bool specifiedHouse, HouseClass *pHouse)
	{
		if (!pThis->IsInPlayfield || !pThis->unknown_bool_250)
			return;

		const auto pOwner = pThis->Owner;
		if (pOwner->Type->MultiplayPassive)
			return;

		pThis->unknown_bool_250 = false;

		const int lastSightRange = pThis->LastSightRange;
		if (!lastSightRange)
			return;

		// 只更新计数，不更新视野
		FoggedObjectHelper::NoReveal = true;
		MapClass::Instance.RevealArea2(&pThis->LastSightCoords, lastSightRange, pOwner, false, false, false, true, true);
		FoggedObjectHelper::NoReveal = false;
	}
	class CellClassFake final : public CellClass
	{
	public:
		FoggedObjectClass* FreezeOverlay(DynamicVectorClass<FoggedObjectClass*>* pVector, bool visible)
		{
			const auto pObject = FoggedObjectHelper::CreateFoggedOverlay(
				YRMemory::Allocate(sizeof(FoggedObjectClassFake)),
				0,
				this->GetCoords(),
				this->OverlayTypeIndex,
				static_cast<unsigned int>(this->OverlayData)
			);
			reinterpret_cast<FoggedObjectClassFake*>(pObject)->Visible = visible;
			auto rect = reinterpret_cast<FoggedObjectClassFake*>(pObject)->RenderDimension;
			rect.X -= TacticalClass::Instance->TacticalPos.X;
			rect.Y -= TacticalClass::Instance->TacticalPos.Y;
			TacticalClass::Instance->RegisterDirtyArea(rect, 0);
			FoggedObjectHelper::AddItemToVector(pVector, pObject);
			return pObject;
		}
		FoggedObjectClass* FreezeSmudge(DynamicVectorClass<FoggedObjectClass*>* pVector, bool visible)
		{
			const auto pObject = FoggedObjectHelper::CreateFoggedSmudge(
				YRMemory::Allocate(sizeof(FoggedObjectClassFake)),
				0,
				this->GetCoords(),
				this->SmudgeTypeIndex,
				static_cast<unsigned int>(this->SmudgeData)
			);
			reinterpret_cast<FoggedObjectClassFake*>(pObject)->Visible = visible;
			auto rect = reinterpret_cast<FoggedObjectClassFake*>(pObject)->RenderDimension;
			rect.X -= TacticalClass::Instance->TacticalPos.X;
			rect.Y -= TacticalClass::Instance->TacticalPos.Y;
			TacticalClass::Instance->RegisterDirtyArea(rect, 0);
			FoggedObjectHelper::AddItemToVector(pVector, pObject);
			return pObject;
		}
	};
	class TerrainClassFake final : public TerrainClass
	{
	public:
		FoggedObjectClass* FreezeTerrain(DynamicVectorClass<FoggedObjectClass*>* pVector, bool visible)
		{
			const auto pObject = FoggedObjectHelper::CreateFoggedTerrain(
				YRMemory::Allocate(sizeof(FoggedObjectClassFake)),
				0,
				this
			);
			reinterpret_cast<FoggedObjectClassFake*>(pObject)->Visible = visible;
			auto rect = reinterpret_cast<FoggedObjectClassFake*>(pObject)->RenderDimension;
			rect.X -= TacticalClass::Instance->TacticalPos.X;
			rect.Y -= TacticalClass::Instance->TacticalPos.Y;
			TacticalClass::Instance->RegisterDirtyArea(rect, 0);
			FoggedObjectHelper::AddItemToVector(pVector, pObject);
			return pObject;
		}
		bool TerrainClass_Unlimbo_CheckFog(const CoordStruct& coords, DirType dir)
		{
			const bool result = FoggedObjectHelper::ObjectUnlimbo(this, coords, dir);
			if (result && ScenarioClass::Instance->SpecialFlags.FogOfWar)
			{
				const auto pCell = MapClass::Instance.GetCellAt(coords);
				if (this->IsAlive && (pCell->Flags & CellFlags::Fogged))
				{
					if (!pCell->FoggedObjects)
						pCell->FoggedObjects = FoggedObjectHelper::CreateGameDynamicVector();

					this->FreezeTerrain(pCell->FoggedObjects, false);
				}
			}
			return result;
		};
	};
	class OverlayClassFake final : public OverlayClass
	{
	public:
		bool OverlayClass_Unlimbo_CheckFog(const CoordStruct& coords, DirType dir)
		{
			const bool result = FoggedObjectHelper::ObjectUnlimbo(this, coords, dir);
			if (result && ScenarioClass::Instance->SpecialFlags.FogOfWar)
			{
				const auto pCell = MapClass::Instance.GetCellAt(coords);
				if (pCell->OverlayTypeIndex != -1 && (pCell->Flags & CellFlags::Fogged))
				{
					if (!pCell->FoggedObjects)
						pCell->FoggedObjects = FoggedObjectHelper::CreateGameDynamicVector();

					static_cast<FoggedObjectHelper::CellClassFake*>(pCell)->FreezeOverlay(pCell->FoggedObjects, false);
				}
			}
			return result;
		}
	};
	class SmudgeClassFake final : public SmudgeClass
	{
	public:
		bool SmudgeClass_Unlimbo_CheckFog(const CoordStruct& coords, DirType dir)
		{
			const bool result = FoggedObjectHelper::ObjectUnlimbo(this, coords, dir);
			if (result && ScenarioClass::Instance->SpecialFlags.FogOfWar)
			{
				const auto pCell = MapClass::Instance.GetCellAt(coords);
				if (pCell->SmudgeTypeIndex != -1 && (pCell->Flags & CellFlags::Fogged))
				{
					if (!pCell->FoggedObjects)
						pCell->FoggedObjects = FoggedObjectHelper::CreateGameDynamicVector();

					static_cast<FoggedObjectHelper::CellClassFake*>(pCell)->FreezeSmudge(pCell->FoggedObjects, false);
				}
			}
			return result;
		}
	};

	// FoggedObjectClass 关键字段布局（依据 0x4D0980/0x4D0EF0 的构造与 0x486C50 的清理逻辑）
	struct FoggedObjectFields
	{
		char _[0x24];
		int OverlayTypeIndex;           // +0x24 覆盖物：类型索引
		ObjectTypeClass* BuildingType;  // +0x28 建筑：冻结时的 BuildingClass*（按 ObjectTypeClass* 读取）
		int OverlayData;                // +0x2C 覆盖物：数量
		int FoggedType;                 // +0x30 类别：6=建筑 0x14=覆盖物 0x1D=弹坑
		CoordStruct Position;           // +0x34 冻结时的位置
		RectangleStruct RenderDimension;// +0x40 渲染矩形
	};
	static_assert(sizeof(FoggedObjectFields) == 0x50u);

	// 游戏内部的坐标→格子换算（与 0x486CA7 处的实现一致）
	CellStruct CoordToCell(const CoordStruct& coords)
	{
		const int x = (coords.X + (coords.X < 0 ? 255 : 0)) >> 8;
		const int y = (coords.Y + (coords.Y < 0 ? 255 : 0)) >> 8;
		return CellStruct { static_cast<short>(x), static_cast<short>(y) };
	}

	// 从格子的迷雾对象向量中移除指定项（保持其余项顺序）
	void RemoveFromVector(DynamicVectorClass<FoggedObjectClass*>* pVector, FoggedObjectClass* pItem)
	{
		for (int i = 0; i < pVector->Count; ++i)
		{
			if (pVector->Items[i] == pItem)
			{
				for (int j = i + 1; j < pVector->Count; ++j)
					pVector->Items[j - 1] = pVector->Items[j];

				--pVector->Count;
				return;
			}
		}
	}

	// 移除一个迷雾冻结对象：按其覆盖范围同步清理所有引用它的格子，标记脏区域后销毁
	void RemoveFoggedObject(CellClass* pOwnerCell, FoggedObjectClass* pItem)
	{
		const auto pFields = reinterpret_cast<FoggedObjectFields*>(pItem);
		const auto baseCell = CoordToCell(pFields->Position);

		if (pFields->FoggedType == 6)
		{
			// 建筑冻结对象同时被其覆盖的每个格子引用（0x4D2790 返回其覆盖范围）
			const auto pOffsets = reinterpret_cast<CellStruct* (__thiscall*)(FoggedObjectClass*)>(0x4D2790)(pItem);

			if (pOffsets)
			{
				for (auto pOffset = pOffsets; ; ++pOffset)
				{
					if (*pOffset == CellStruct { 0x7FFF, 0x7FFF })

						break;

					if (const auto pCell = MapClass::Instance.TryGetCellAt(baseCell + *pOffset))
					{
						if (pCell->FoggedObjects)
							RemoveFromVector(pCell->FoggedObjects, pItem);
}

				}
			}
		}
		else if (pOwnerCell && pOwnerCell->FoggedObjects)
		{
			// 非建筑冻结对象只被单个格子引用
			RemoveFromVector(pOwnerCell->FoggedObjects, pItem);
		}

		auto rect = pFields->RenderDimension;
		rect.X -= TacticalClass::Instance->TacticalPos.X;
		rect.Y -= TacticalClass::Instance->TacticalPos.Y;
		TacticalClass::Instance->RegisterDirtyArea(rect, 0);

		// 虚表 +0x20 处为标量删除析构（与 0x486D5D 处一致）
		using DeletingDtor_t = void(__thiscall*)(FoggedObjectClass*, bool);
		const auto pDtor = reinterpret_cast<DeletingDtor_t*>(*reinterpret_cast<void**>(pItem))[8];
		pDtor(pItem, true);
	}
}

// 重新启用功能
using FoggedObjectFields = FoggedObjectHelper::FoggedObjectFields;
using FoggedObjectHelper::RemoveFoggedObject;

DEFINE_HOOK(0x687C56, INIClass_ReadScenario_EnableFog, 0x5)
{
	const bool fog = RulesClass::Instance->FogOfWar;
	GameModeOptionsClass::Instance.FogOfWar = fog;
	ScenarioClass::Instance->SpecialFlags.FogOfWar = fog;
	return 0;
}

// 重新实现核心判定：位置处在迷雾中
DEFINE_HOOK(0x5865E2, MapClass_IsLocationFogged_Reimplement, 0x5)
{
	GET_STACK(const CoordStruct*, pCoords, STACK_OFFSET(0x0, 0x4));

	const int level = pCoords->Z / Unsorted::LevelHeight;
	const int extra = (level & 1) ? ((level >> 1) + 1) : (level >> 1);
	const CellStruct cell { static_cast<short>((pCoords->X >> 8) - extra), static_cast<short>((pCoords->Y >> 8) - extra) };

	R->EAX(!!(MapClass::Instance.GetCellAt(cell)->Flags & CellFlags::Fogged));
	return 0;
}

// 像黑幕那样自动刷新周围格子的绘制状态，防止边缘出现锯齿
DEFINE_HOOK(0x4A9CA0, DisplayClass_RevealFogShroud_Reimplement, 0x8)
{
	enum { SkipGameCode = 0x4A9DC6 };

	GET(DisplayClass*, pThis, ECX);
	GET_STACK(CellStruct*, pCellStruct, STACK_OFFSET(0x0, 0x4));
	GET_STACK(HouseClass*, pHouse, STACK_OFFSET(0x0, 0x8));
	GET_STACK(bool, increase, STACK_OFFSET(0x0, 0xC));
	// Kaleidoscope fog owner: only the owning house updates the fog
	if (PhobosFogOwner && pHouse != PhobosFogOwner)
		return SkipGameCode;

	const auto pCell = pThis->GetCellAt(*pCellStruct);
	const auto flags = pCell->Flags;

		// Kaleidoscope fog-owner diag (first 8 owner-matching calls)
	static int s_kdiag = 0;
	if (PhobosFogOwner && pHouse == PhobosFogOwner && s_kdiag < 8)
	{
		++s_kdiag;
		FILE* f = fopen("fog-diag.log", "a");
		if (f)
		{
			fprintf(f, "reveal house=%p cell=(%d,%d) inc=%d PRE flags=%08X V=%d F=%d\n", (void*)pHouse, pCellStruct->X, pCellStruct->Y, increase ? 1 : 0, (unsigned)pCell->Flags, pCell->Visibility, pCell->Foggedness);
		}
	}

	const bool edgeNotRevealed = !(flags & CellFlags::EdgeRevealed);
	const bool shouldRadarRedraw = edgeNotRevealed || !(pCell->AltFlags & AltCellFlags::Mapped);
	bool shouldRedraw = shouldRadarRedraw;
	pCell->Flags = FoggedObjectHelper::NoReveal ? (flags & ~CellFlags::IsPlot) : (flags & ~CellFlags::IsPlot | CellFlags::EdgeRevealed);

	if (increase)
		pCell->IncreaseShroudCounter();
	else
		pCell->ReduceShroudCounter();

	const char newVisibility = TacticalClass::Instance->GetOcclusion(*pCellStruct, false);
	if (newVisibility != pCell->Visibility)
	{
		shouldRedraw = true;
		pCell->Visibility = newVisibility;
	}

	if (pCell->Visibility == -1)
		pCell->AltFlags |= AltCellFlags::NoFog;

	const char newFoggedness = TacticalClass::Instance->GetOcclusion(*pCellStruct, true);
	if (newFoggedness != pCell->Foggedness)
	{
		shouldRedraw = true;
		pCell->Foggedness = newFoggedness;
	}

	if (pCell->Foggedness == -1)
	{
		pCell->Flags |= CellFlags::CenterRevealed;

		// 修复：完全可见的格子不应保留 Fogged 标记。间谍卫星揭幕后格子带有
		// EdgeRevealed，而 CleanFog 只在格子“新变为 EdgeRevealed”时触发；此后
		// FogCell（迷雾扩散/下坡遮挡）会重新给这些格子打上 Fogged 却不更新
		// Foggedness，形成“逻辑迷雾/视觉无雾”的错位——单位走入后被当作处于
		// 迷雾中（不绘制且被取消选中）。凡视野更新到完全可见的格子即清理。
		if (pCell->Flags & CellFlags::Fogged)
			pCell->CleanFog();
	}

	if (shouldRedraw)
		TacticalClass::Instance->RegisterCellAsVisible(pCell);

	if ((pCell->AltFlags & AltCellFlags::Mapped) && ScenarioClass::Instance->SpecialFlags.FogOfWar)
	{
		for (size_t i = 0; i < 8; ++i)
		{
			auto adjCell = Unsorted::AdjacentCell[i & 7] + *pCellStruct;
			const auto pAdjCell = pThis->GetCellAt(adjCell);

			if (adjCell != *pCellStruct && !(pAdjCell->Flags & CellFlags::CenterRevealed))
			{
				const char adjNewFoggedness = TacticalClass::Instance->GetOcclusion(adjCell, true);
				if (adjNewFoggedness == -1)
				{
					if (pAdjCell->Flags & CellFlags::EdgeRevealed)
					{
						pAdjCell->Flags |= CellFlags::CenterRevealed;
						TacticalClass::Instance->RegisterCellAsVisible(pAdjCell);

						for (size_t j = 0; j < 8; ++j)
						{
							const auto nextAdjCell = Unsorted::AdjacentCell[j & 7] + adjCell;
							const auto pNextAdjCell = pThis->GetCellAt(nextAdjCell);

							const char nextAdjNewFoggedness = TacticalClass::Instance->GetOcclusion(nextAdjCell, true);
							if (nextAdjNewFoggedness != pNextAdjCell->Foggedness)
							{
								pNextAdjCell->Foggedness = nextAdjNewFoggedness;
								TacticalClass::Instance->RegisterCellAsVisible(pNextAdjCell);
							}
						}

						continue;
					}
				}
				else
				{
					if (adjNewFoggedness == -2 || (pAdjCell->Flags & CellFlags::EdgeRevealed))
					{
						if (adjNewFoggedness >= 0 && adjNewFoggedness != pAdjCell->Foggedness)
						{
							pAdjCell->Foggedness = adjNewFoggedness;
							TacticalClass::Instance->RegisterCellAsVisible(pAdjCell);
						}

						continue;
					}
				}

				pThis->MapCellFoggedness(&adjCell, pHouse);
				continue;
			}
		}
	}

	if (shouldRedraw)
		MapClass::Instance.RevealCheck(pCell, pHouse, shouldRadarRedraw);

	if ((pCell->Flags & CellFlags::EdgeRevealed) && edgeNotRevealed && ScenarioClass::Instance->SpecialFlags.FogOfWar)
		pCell->CleanFog();

	return SkipGameCode;
}

// 让部分无效更新不更新实际视野
DEFINE_HOOK(0x4A9DF1, DisplayClass_MapCellFoggedness_NoUpdate, 0x6)
{
	enum { SkipGameCode = 0x4A9DFE };

	GET(CellFlags, flags, EAX);

	R->ECX(!(flags & CellFlags::EdgeRevealed));

	flags &= ~CellFlags::IsPlot;
	if (!FoggedObjectHelper::NoReveal)
		flags |= CellFlags::EdgeRevealed;

	R->EAX(flags);

	return SkipGameCode;
}

// 像黑幕刷新那样考虑飞行单位，确保未移动的飞行单位的视野不会被刷新掉，并且友军视野不只看建筑，避免频繁刷新
DEFINE_HOOK(0x4ADFF0, MapClass_AllToSee_Reimplement, 0x5)
{
	enum { SkipGameCode = 0x4AE0A5 };

	GET_STACK(bool, notForBuilding, STACK_OFFSET(0x0, 0x4));
	GET_STACK(bool, revealFlag, STACK_OFFSET(0x0, 0x8));

	const auto& vec = TechnoClass::Array;
	for (int i = 0; i < vec.Count; ++i)
	{
		const auto pTechno = vec.Items[i];
		if (pTechno && !pTechno->InLimbo && pTechno->IsOnMap)
		{
			if (pTechno->WhatAmI() != AbstractType::Building || !notForBuilding)
			{
				const auto pOwner = pTechno->Owner;
				if (pOwner->IsControlledByCurrentPlayer())
				{
					if (pTechno->DiscoveredByCurrentPlayer)
						pTechno->See(false, revealFlag);
				}
				else if (RulesClass::Instance->AllyReveal)
				{
					if (pOwner->IsAlliedWith(HouseClass::CurrentPlayer))
						pTechno->See(revealFlag, false);
				}
			}
		}
	}

	return SkipGameCode;
}

// 增加一种立即刷新迷雾的机制，甚至！？性能提升？！
DEFINE_HOOK(0x4ACBC4, MapClass_FogSpread_SkipWithSpySat, 0x5)
{
	enum { SkipGameCode = 0x4ACC4B, SpreadFogOfWar = 0x4ACBF8 };

	GET(MapClass*, pThis, ECX);

	const auto pPlayer = HouseClass::CurrentPlayer;
	if (pPlayer->Defeated)
		return SkipGameCode;

	pThis->CellIteratorReset();
	if (RulesClass::Instance->ShadowGrow)
	{
		for (auto pCell = pThis->CellIteratorNext(); pCell; pCell = pThis->CellIteratorNext())
			pCell->Flags |= CellFlags::IsPlot;
	}
	else
	{
		for (auto pCell = pThis->CellIteratorNext(); pCell; pCell = pThis->CellIteratorNext())
		{
			const auto flags = pCell->Flags;
			if (!(flags & CellFlags::CenterRevealed) && (flags & CellFlags::EdgeRevealed))
				pCell->Flags = flags | CellFlags::IsPlot;
		}
	}

	return SpreadFogOfWar;
}

// 迷雾扩散的递归调用点：原版在此立即递归处理完全遮挡的邻居。FogSpreadRate>0 时
// 改为入队等待按间隔处理（见 FogSpreadHelper::ProcessQueue），蔓延间隔由此可控。
// 注意该处 ESI 为邻居格子（0x4ACCFB 处取得），且仅在"邻居遮挡为完全遮挡且尚未
// 处理"时到达（0x4ACD10/0x4ACD15 的两道检查已通过）。
DEFINE_HOOK(0x4ACD1D, DisplayClass_SpreadFog_RateLimit, 0xC)
{
	enum { SkipRecursion = 0x4ACD29 };

	GET(CellClass* const, pNeighbor, ESI);

	if (RulesExt::Global()->FogSpreadRate > 0.0 && FogSpreadHelper::PendingCells.size() <= 200000u)
	{
		FogSpreadHelper::PendingCells.push_back(pNeighbor->MapCoords);
		return SkipRecursion;
	}

	return 0; // 原版立即递归
}

// Frozen animation segments bypass AnimClass::DrawIt. At 4D20FA ESI is
// still the validated AnimTypeClass (WhatAmI == 5 at 4D205C). X is the
// local at ESP+38, and Y is still in EDX, committed to ESP+3C at 4D2102.
// Do not use 4D21E4: both animation branches clear ESI before that shared
// DrawSHP site; the building overlay SHP also reaches it with another type.
DEFINE_HOOK(0x4D20FA, FoggedObjectClass_DrawFoggedObjects_AnimDrawOffset, 0x6)
{
	GET(AnimTypeClass* const, pType, ESI);
	R->ref_Stack<int>(0x38) += AnimTypeExt::Fetch(pType)->XDrawOffset;
	R->EDX(static_cast<int>(R->EDX()) + pType->YDrawOffset);
	return 0; // replay MOV CL,[ESI+35D] once; native code stores the adjusted Y
}

// 让单位在更新视野的时候不会多更新一次之前的视野
DEFINE_FUNCTION_JUMP(CALL6, 0x415672, FoggedObjectHelper::TechnoClass_RevealLastSight);
DEFINE_FUNCTION_JUMP(CALL6, 0x4157F7, FoggedObjectHelper::TechnoClass_RevealLastSight);
DEFINE_FUNCTION_JUMP(CALL6, 0x416C6D, FoggedObjectHelper::TechnoClass_RevealLastSight);
DEFINE_FUNCTION_JUMP(CALL6, 0x4CD43F, FoggedObjectHelper::TechnoClass_RevealLastSight);
DEFINE_FUNCTION_JUMP(CALL6, 0x4DA6F7, FoggedObjectHelper::TechnoClass_RevealLastSight);
DEFINE_FUNCTION_JUMP(CALL6, 0x51A8D7, FoggedObjectHelper::TechnoClass_RevealLastSight);
DEFINE_FUNCTION_JUMP(CALL6, 0x54C93C, FoggedObjectHelper::TechnoClass_RevealLastSight);
DEFINE_FUNCTION_JUMP(CALL6, 0x73AC5F, FoggedObjectHelper::TechnoClass_RevealLastSight);

// 迷雾遮蔽时增加地形对象、覆盖物、弹坑
DEFINE_HOOK(0x486B21, CellClass_FogCell_FreezeObjects, 0x6)
{
	enum { SkipGameCode = 0x486BAB };

	GET(FoggedObjectHelper::CellClassFake*, pCell, EBP);
	GET(DynamicVectorClass<FoggedObjectClass*>*, pVector, EDI);

    pCell->Flags |= CellFlags::Fogged;
	for (auto pObject = pCell->FirstObject; pObject; pObject = pObject->NextObject)
	{
		const auto absType = pObject->WhatAmI();
		if (absType == AbstractType::Building)
		{
			const auto& pBuilding = static_cast<BuildingClass*>(pObject);
			if (pBuilding->IsAllFogged())
				pBuilding->FreezeInFog(pVector, pCell, (!pBuilding->IsStrange() && pBuilding->Translucency != 15));
		}
		else if (absType == AbstractType::Unit
			|| absType == AbstractType::Aircraft
			|| absType == AbstractType::Infantry)
		{
			pObject->Deselect();
		}
		else if (absType == AbstractType::Terrain)
		{
			static_cast<FoggedObjectHelper::TerrainClassFake*>(pObject)->FreezeTerrain(pVector, true);
		}
	}

	if (pCell->OverlayTypeIndex != -1)
		pCell->FreezeOverlay(pVector, true);

	if (pCell->SmudgeTypeIndex != -1)
		pCell->FreezeSmudge(pVector, true);

	return SkipGameCode;
}

// 地形对象被迷雾遮蔽时跳过绘制
DEFINE_HOOK(0x6D9313, TacticalClass_DrawObjectsInLayers_SkipTerrainInFog, 0x6)
{
	enum { SkipDraw = 0x6D940C };

	GET(const CoordStruct*, pCoord, EAX);

	return (ScenarioClass::Instance->SpecialFlags.FogOfWar && MapClass::Instance.IsLocationFogged(*pCoord)) ? SkipDraw : 0;
}

// 覆盖物被迷雾遮蔽时跳过绘制
DEFINE_HOOK(0x6D70BC, TacticalClass_DrawCellOverlay_SkipOverlayInFog, 0xA)
{
	enum { Draw = 0x6D70C6, SkipDraw = 0x6D71A4 };

	GET(const CellClass*, pCell, EBX);

	return (pCell->OverlayTypeIndex != -1 && (!ScenarioClass::Instance->SpecialFlags.FogOfWar || !(pCell->Flags & CellFlags::Fogged))) ? Draw : SkipDraw;
}

// 弹坑被迷雾遮蔽时跳过绘制
DEFINE_HOOK(0x48049E, CellClass_DrawCellSmudge_SkipSmudgeInFog, 0x6)
{
	enum { Draw = 0x4804A4, SkipDraw = 0x4804FB };

	GET(const CellClass*, pThis, ESI);

	return (pThis->SmudgeTypeIndex != -1 && (!ScenarioClass::Instance->SpecialFlags.FogOfWar || !(pThis->Flags & CellFlags::Fogged))) ? Draw : SkipDraw;
}

// 被击败或是观察者时揭开所有迷雾
DEFINE_HOOK(0x4FC200, HouseClass_AcceptDefeat_RevealFog, 0x5)
{
	if (ScenarioClass::Instance->SpecialFlags.FogOfWar)
		FoggedObjectHelper::ClearAllFog();

	return 0;
}

// 修复间谍卫星与迷雾的交互：原版的整图揭幕只清除黑幕（清 ShroudCounter/Gaps、
// 置 Mapped|NoFog），完全不处理迷雾层——被揭示的格子迷雾标记仍在，却又没有迷雾
// 快照可画，表现为“看起来全开了”，但真实单位走入这些格子时会因迷雾中不绘制
// 真实物体而消失。按选定行为，揭示时连同迷雾一并彻底清除。
// 注意除了下述标记外，还必须重置格子的 Visibility/Foggedness（绘制与选中管线
// 依据这两个字段判断格子是否可见），未探索过的格子中它们是“完全遮挡”值。
// 钩点改在 0x577EE9（下一格迭代调用处，含被原版跳过的 3 个特殊格子）：
// 这些格子若保留 !CenterRevealed&&EdgeRevealed 状态，会成为迷雾扩散的种子，
// 从它们出发逐圈重新迷雾整张地图并经 FogCell 打回 Fogged 标记。此处对全部
// 格子（含特殊格子）补齐 CenterRevealed|EdgeRevealed 并清理迷雾状态，黑幕
// 的跳过逻辑（ShroudCounter/Gaps 不动）保持原版行为不变。
DEFINE_HOOK(0x577EE9, DisplayClass_RevealWholeMap_ClearFog, 0x5)
{
	GET(CellClass* const, pCell, EAX);

	if (ScenarioClass::Instance->SpecialFlags.FogOfWar)
	{
		pCell->Flags = (pCell->Flags & ~CellFlags::Fogged) | CellFlags::CenterRevealed | CellFlags::EdgeRevealed;
		pCell->Visibility = -1;
		pCell->Foggedness = -1;
		pCell->ClearFoggedObjects();
	}

	return 0;
}

// 地形对象、覆盖物、弹坑出现时检查是否被迷雾遮蔽，虽然感觉没必要，但既然建筑也这么做了那就蛮做
DEFINE_FUNCTION_JUMP(CALL, 0x5FC4B1, FoggedObjectHelper::OverlayClassFake::OverlayClass_Unlimbo_CheckFog);
DEFINE_FUNCTION_JUMP(CALL, 0x5FD2D2, FoggedObjectHelper::OverlayClassFake::OverlayClass_Unlimbo_CheckFog);
DEFINE_FUNCTION_JUMP(CALL, 0x6B4B14, FoggedObjectHelper::SmudgeClassFake::SmudgeClass_Unlimbo_CheckFog);
DEFINE_FUNCTION_JUMP(CALL, 0x71D012, FoggedObjectHelper::TerrainClassFake::TerrainClass_Unlimbo_CheckFog);

DEFINE_HOOK(0x4D1B2E, FoggedObjectClass_DrawFoggedObjects_DrawTerrain, 0x6)
{
	enum { SkipGameCode = 0x4D2311 };

	GET(SHPStruct*, pImage, EAX);
	GET(const CoordStruct*, pCoord, ESI);
	GET(const RectangleStruct*, pBounds, EDI);
	GET(CellClass*, pCell, EBP);
	// 不能在这里读 ECX 当 Type：0x4D1B28 的 call [edx+9Ch]（Type 的虚函数，返回 SHP）已经执行过，
	// ECX 是易失寄存器、可能已被它破坏，之前那样取到的 Type 可能是垃圾；TerrainTypeExt::Fetch
	// 会顺着垃圾指针得到无效色盘并传给 DrawSHP，底层 blitter 会做出野跳转（EIP 落到非代码地址）
	// 而崩溃。改为从栈上取游戏在 call 之前保存的 Type 指针（即当时的 ECX，[esp+18h]）。
	GET_STACK(TerrainTypeClass*, pType, STACK_OFFSET(0x18, 0x0));
	GET_STACK(int, frame, STACK_OFFSET(0x154, -0x134));

	// 参数校验：任一指针不可信、或该对象并非地形时退回原版绘制。返回 0 会重新执行被覆盖的原
	// 指令（mov ebx,eax / mov eax,esi / mov ecx,[eax]），它们依赖进入钩子时的 EAX=SHP 与
	// ESI=坐标指针。Syringe 会保存并恢复整组寄存器，这里再把 EAX 显式写回 SHP 作为双保险。
	if (!pImage || !pCoord || !pBounds || !pCell || !pType || pType->WhatAmI() != AbstractType::TerrainType)
	{
		R->EAX(pImage);
		return 0;
	}

	if (pImage)
	{
		auto point = TacticalClass::Instance->CoordsToClient(*pCoord).first;
		point.X += (DSurface::ViewBounds.X - pBounds->X);
		point.Y += (DSurface::ViewBounds.Y - pBounds->Y);
		int zAdjust = -TacticalClass::Instance->AdjustForZ(pCoord->Z);

		if (!pCell->LightConvert)
			pCell->InitLightConvert();

		ConvertClass* pPalette = pCell->LightConvert;
		int intensity = static_cast<int>(pCell->Intensity_Terrain);
		auto const pTypeExt = TerrainTypeExt::Fetch(pType);

		if (const auto pPalettes = pTypeExt->Palette)
		{
			if (!pTypeExt->PaletteRemap && pTypeExt->PlainPalette)
			{
				pPalette = pTypeExt->PlainPalette;
			}
			else
			{
				const int wallOwnerIndex = pCell->WallOwnerIndex;
				int colorSchemeIndex = HouseClass::CurrentPlayer->ColorSchemeIndex;

				if (wallOwnerIndex >= 0)
					colorSchemeIndex = HouseClass::Array[wallOwnerIndex]->ColorSchemeIndex;

				pPalette = pPalettes->Items[colorSchemeIndex]->LightConvert;
			}

			intensity = static_cast<int>(pCell->Intensity_Normal);

			if (pType->SpawnsTiberium)
				point.Y -= 16;
		}
		else if (pType->SpawnsTiberium)
		{
			pPalette = FileSystem::GRFTXT_TIBERIUM_PAL;
			intensity = static_cast<int>(pCell->Intensity_Normal);
			point.Y -= 16;
		}

		DSurface::Temp->DrawSHP(pPalette, pImage, frame, &point, pBounds, BlitterFlags(0x4E00), 0, zAdjust - 4, ZGradient::Deg90, intensity, 0, 0, 0, 0, 0);
		if (Game::bDrawShadow)
			DSurface::Temp->DrawSHP(pPalette, pImage, frame + (pImage->Frames / 2), &point, pBounds, BlitterFlags(0x4601), 0, zAdjust - 2, ZGradient::Ground, 1000, 0, 0, 0, 0, 0);
	}

	return SkipGameCode;
}

#pragma region FoggedEffects

namespace FoggedEffectHelper
{
	// 特效可见性判定：两端点都在迷雾中时才视为不可见，任一端可见即绘制
	__forceinline bool IsCoordFogged(const CoordStruct& coords)
	{
		return MapClass::Instance.IsLocationFogged(coords);
	}

	__forceinline bool IsBeamFogged(const CoordStruct& source, const CoordStruct& target)
	{
		return ScenarioClass::Instance->SpecialFlags.FogOfWar
			&& IsCoordFogged(source) && IsCoordFogged(target);
	}
}

// 激光（含飞碟激光分段、激光型尾迹）两端都在迷雾中时跳过绘制
DEFINE_HOOK(0x550274, LaserDrawClass_Draw_SkipInFog, 0x5)
{
	GET(LaserDrawClass* const, pThis, EBX);

	if (FoggedEffectHelper::IsBeamFogged(pThis->Source, pThis->Target))
		return 0x5509CA;

	return 0;
}

// 电弧（含电弧型尾迹）两端都在迷雾中时跳过绘制，寿命照常折半递减
DEFINE_HOOK(0x4C27D5, EBolt_DrawAll_SkipInFog_1, 0x7)
{
	GET(EBolt* const, pThis, ESI);
	GET(int, visible, EAX);

	if ((visible & 0xFF) && FoggedEffectHelper::IsBeamFogged(pThis->Point1, pThis->Point2))
		return 0x4C281A;

	return 0;
}

DEFINE_HOOK(0x4C28F9, EBolt_DrawAll_SkipInFog_2, 0x7)
{
	GET(EBolt* const, pThis, ESI);
	GET(int, visible, EAX);

	if ((visible & 0xFF) && FoggedEffectHelper::IsBeamFogged(pThis->Point1, pThis->Point2))
		return 0x4C293D;

	return 0;
}

// 辐射波（含辐射型尾迹）两端都在迷雾中时跳过绘制，存活计时照常递增
DEFINE_HOOK(0x65978C, RadBeam_AI_SkipInFog_1, 0x5)
{
	GET(RadBeam* const, pThis, ESI);

	if (*(bool*)((char*)pThis + 9) && FoggedEffectHelper::IsBeamFogged(pThis->SourceLocation, pThis->TargetLocation))
		return 0x659968;

	return 0;
}

DEFINE_HOOK(0x659CB7, RadBeam_AI_SkipInFog_2, 0x5)
{
	GET(RadBeam* const, pThis, EDI);

	if (*(bool*)((char*)pThis + 9) && FoggedEffectHelper::IsBeamFogged(pThis->SourceLocation, pThis->TargetLocation))
		return 0x659E09;

	return 0;
}

// 声波/磁波两端都在迷雾中时跳过绘制，波动推进与伤害逻辑照常执行
DEFINE_HOOK(0x762C4E, WaveClass_UpdateWave_SkipInFog, 0x6)
{
	struct WaveEndpointsFake
	{
		char _[0xAC];
		AbstractClass* SourceObj;  // +0xAC
		char __[0x12C - 0xB0];
		bool DrawFlag;             // +0x12C
		char ___[0x1D4 - 0x12D];
		AbstractClass* TargetObj;  // +0x1D4
	};

	GET(WaveClass* const, pThis, ESI);
	const auto pFake = reinterpret_cast<WaveEndpointsFake*>(pThis);

	if (pFake->SourceObj && pFake->TargetObj)
	{
		CoordStruct source;
		CoordStruct target;
		pFake->SourceObj->GetCoords(&source);
		pFake->TargetObj->GetCoords(&target);

		if (FoggedEffectHelper::IsBeamFogged(source, target))
			return 0x762D57;
	}

	return 0;
}

#pragma endregion

// 修复迷雾中建筑绘制崩溃：原分支在绘制建筑遮挡时会使用 BUILDINGZ.SHA 的缓存指针
// （FileSystem::BUILDINGZ_SHA），但该缓冲在进入游戏时已被释放，读取已释放内存导致崩溃。
// 原版迷雾逻辑从不执行到此处，故原版不崩溃。将遮挡图参数置空以跳过遮挡绘制，
// 与同分支的无遮挡子路径传 0 的行为一致，迷雾中的建筑不需要遮挡效果。
DEFINE_HOOK(0x4D1E46, FoggedObjectClass_DrawFoggedObjects_NullBuildingZSHA, 0x7)
{
	R->EAX(0); // 原本存入 BUILDINGZ_SHA，改为空指针
	return 0;
}

// Fogged building AlphaImage uses the existing native AlphaShape pipeline.
// Hooks.AlphaImage.cpp preserves the last visible native source while all
// foundation cells are fogged; do not duplicate its light as a frozen SHP.

// 迷雾中的建筑被摧毁后，移除其冻结在迷雾中的贴图，防止残留
DEFINE_HOOK(0x441F60, BuildingClass_AfterDestruction_RemoveFoggedImage, 0x5)
{
	GET(BuildingClass* const, pThis, ECX);

	if (ScenarioClass::Instance->SpecialFlags.FogOfWar)
	{
		const auto& position = pThis->Location;
		const auto pFoundation = pThis->GetFoundationData(false);
		const CellStruct foundationEnd { 0x7FFF, 0x7FFF };

		// 建筑的迷雾冻结对象只有一个，被注册在其覆盖的每个格子中；
		// 以两种可能的基准格并集扫描，找到后由 RemoveFoggedObject 统一清理全部引用
		const CellStruct bases[2]
		{
			pThis->GetMapCoords(),
			FoggedObjectHelper::CoordToCell(position)
		};

		for (const auto& base : bases)
		{
			for (auto pOffset = pFoundation; *pOffset != foundationEnd; ++pOffset)
			{
				const auto pCell = MapClass::Instance.TryGetCellAt(base + *pOffset);

				if (!pCell || !pCell->FoggedObjects)
					continue;

				const auto& pVector = pCell->FoggedObjects;

				for (int i = pVector->Count - 1; i >= 0; --i)
				{
					const auto pItem = pVector->Items[i];
					const auto pFields = reinterpret_cast<FoggedObjectFields*>(pItem);

					if (pFields->FoggedType == 6 && pFields->BuildingType == reinterpret_cast<ObjectTypeClass*>(pThis) && pFields->Position == position)
					{
						RemoveFoggedObject(pCell, pItem);
						return 0;
					}
				}
			}
		}
	}

	return 0;
}

// 迷雾中矿石被采集后，同步冻结的覆盖物贴图：
// 数量变化就地更新，类型变化或耗尽则移除后重建（0x480C67 为两条扣减路径的汇合点）
DEFINE_HOOK(0x480C67, CellClass_ReduceTiberium_SyncFoggedOverlay, 0x6)
{
	GET(CellClass* const, pRawCell, ESI);

	// 耗尽路径中 ESI 已被调整为 cell+0x24（&MapCoords），此处还原
	auto pCell = pRawCell;
	if (MapClass::Instance.TryGetCellAt(pRawCell->MapCoords) != pRawCell)
		pCell = reinterpret_cast<CellClass*>(reinterpret_cast<char*>(pRawCell) - 0x24);

	if (!ScenarioClass::Instance->SpecialFlags.FogOfWar
		|| !pCell->FoggedObjects
		|| !(pCell->Flags & CellFlags::Fogged))
	{
		return 0;
	}

	const auto& pVector = pCell->FoggedObjects;

	for (int i = pVector->Count - 1; i >= 0; --i)
	{
		const auto pItem = pVector->Items[i];
		const auto pFields = reinterpret_cast<FoggedObjectFields*>(pItem);

		if (pFields->FoggedType != 0x14) // 只处理覆盖物冻结对象
			continue;

		if (pFields->OverlayTypeIndex != pCell->OverlayTypeIndex)
		{
			RemoveFoggedObject(pCell, pItem);

			// 覆盖物仍存在时重新冻结
			if (pCell->OverlayTypeIndex != -1)
			{
				if (!pCell->FoggedObjects)
					pCell->FoggedObjects = FoggedObjectHelper::CreateGameDynamicVector();

				static_cast<FoggedObjectHelper::CellClassFake*>(pCell)->FreezeOverlay(pCell->FoggedObjects, true);
			}
		}
		else
		{
			// 数量变化：就地更新（绘制侧直接读取这两个字段）
			pFields->OverlayData = static_cast<int>(pCell->OverlayData);
		}
	}

	return 0;
}

#pragma endregion

#pragma endregion
