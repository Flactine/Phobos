#include "Body.h"
#include <ThemeClass.h>
#include <New/Entity/BannerClass.h>

// Ingame music and banner when the current player wins or loses
DEFINE_HOOK_AGAIN(0x4FCB7D, HouseClass_WinLoseTheme, 0x5)  // HouseClass::Flag_To_Win
DEFINE_HOOK(0x4FCD66, HouseClass_WinLoseTheme, 0x5)        // HouseClass::Flag_To_Lose
{
	const auto pThis = HouseClass::CurrentPlayer;
	const auto pSide = SideClass::Array.GetItemOrDefault(pThis->SideIndex);
	const auto pSideExt = SideExt::TryFetch(pSide);

	if (pSideExt)
	{
		const int themeIndex = (pThis->IsWinner) ? pSideExt->IngameScore_WinTheme : pSideExt->IngameScore_LoseTheme;
		if (themeIndex >= 0)
			ThemeClass::Instance.Play(themeIndex);

		const auto pBannerType = pThis->IsWinner
			? pSideExt->IngameScore_WinBanner.Get()
			: pSideExt->IngameScore_LoseBanner.Get();

		if (pBannerType)
		{
			const auto position = pThis->IsWinner
				? pSideExt->IngameScore_WinBanner_Position.Get()
				: pSideExt->IngameScore_LoseBanner_Position.Get();

			BannerClass::IngameScore = std::make_unique<BannerClass>(
				pBannerType, 0, position, 0, false, false);
		}
		else
		{
			BannerClass::IngameScore.reset();
		}
	}

	return 0;
}
