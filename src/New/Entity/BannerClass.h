#pragma once

#include <New/Type/BannerTypeClass.h>

class BannerTypeClass;

class BannerClass
{
public:
	static std::vector<std::unique_ptr<BannerClass>> Array;
	// Separate from trigger banners: no trigger ID can replace or delete it.
	static std::unique_ptr<BannerClass> IngameScore;

	BannerTypeClass* Type {};
	int ID {};
	Point2D Position {};
	int Variable {};
	int ShapeFrameIndex {};
	bool IsGlobalVariable {};
	bool UseVariable { true };
	int Duration { -1 };
	int Delay { -1 };

	BannerClass() = default;

	BannerClass
	(
		BannerTypeClass* pBannerType,
		int id,
		Point2D position,
		int variable,
		bool isGlobalVariable,
		bool useVariable = true
	);

	void Render();

	static void Clear();
	bool Load(PhobosStreamReader& Stm, bool RegisterForChange);
	bool Save(PhobosStreamWriter& Stm) const;

	static bool LoadGlobals(PhobosStreamReader& Stm);
	static bool SaveGlobals(PhobosStreamWriter& Stm);

private:
	template <typename T>
	bool Serialize(T& Stm);

	void RenderPCX(Point2D position);
	void RenderSHP(Point2D position);
	void RenderCSF(Point2D position);
};
