#include <Ext/Bullet/Body.h>
#include <Ext/Techno/Body.h>
#include <Utilities/Macro.h>

// FlaresBridge: 让攻击者以拦截器方式交火抛射体（诱饵）。
//
// 原版把 Bullet 视为非法目标，所以 Kaleidoscope 的 Flares（把敌人目标改成
// 诱饵抛射体）没有任何效果。本 fork 的 Interceptor 路径已经解决了这个问题
// 的两半：
//   * 0x6FC7EB TechnoClass_CanFire_InterceptBullet 让 Bullet 目标通过 CanFire；
//   * 0x6F3339 TechnoClass_WhatWeaponShouldIUse_Interceptor 在目标是 Bullet 时
//     返回拦截武器。
// 唯一缺的是驱动：外部模块 SetTarget 不会触发 TechnoExt::ApplyInterceptor
// （那只为拥有 InterceptorType 的单位扫描 BulletClass::Array）。
// 这个导出按需为任意攻击者补上等价步骤。
DEFINE_EXPORT(HRESULT, Phobos_Flares_Engage, TechnoClass* pTechno, BulletClass* pBullet)
{
	if (!pTechno || !pBullet)
		return E_POINTER;

	__try
	{
		pTechno->SetTarget(pBullet);

		// 立即尝试开火一次：SelectWeapon 会走拦截武器的选择逻辑，
		// CanFire 的白名单让 Bullet 目标通过。ROF/弹药/射程不满足也
		// 没关系——目标已锁定，正常攻击任务会在后续帧继续尝试。
		const int index = pTechno->SelectWeapon(pBullet);

		if (index >= 0)
			pTechno->Fire(pBullet, index);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return E_FAIL;
	}

	return S_OK;
}
