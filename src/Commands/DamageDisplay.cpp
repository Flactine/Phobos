#include "DamageDisplay.h"

#include <Utilities/GeneralUtils.h>

const char* DamageDisplayCommandClass::GetName() const
{
	return "Display Damage Numbers";
}

const wchar_t* DamageDisplayCommandClass::GetUIName() const
{
	return GeneralUtils::LoadStringUnlessMissing("TXT_DISPLAY_DAMAGE", L"Display Damage Dealt");
}

const wchar_t* DamageDisplayCommandClass::GetUICategory() const
{
	return CATEGORY_DEVELOPMENT;
}

const wchar_t* DamageDisplayCommandClass::GetUIDescription() const
{
	return GeneralUtils::LoadStringUnlessMissing("TXT_DISPLAY_DAMAGE_DESC", L"Display exact number of damage dealt to units & buildings on them.");
}

void DamageDisplayCommandClass::Execute(WWKey eInput) const
{
	Phobos::Config::DisplayDamageNumbers = !Phobos::Config::DisplayDamageNumbers;

	if (Phobos::Config::DisplayDamageNumbers)
		MessageListClass::Instance.PrintMessage(GeneralUtils::LoadStringUnlessMissing("MSG:DisplayDamageOn", L"Damage numbers display enabled."), RulesClass::Instance->MessageDelay, HouseClass::CurrentPlayer->ColorSchemeIndex, true);
	else
		MessageListClass::Instance.PrintMessage(GeneralUtils::LoadStringUnlessMissing("MSG:DisplayDamageOff", L"Damage numbers display disabled."), RulesClass::Instance->MessageDelay, HouseClass::CurrentPlayer->ColorSchemeIndex, true);

	VocClass::PlayGlobal(RulesClass::Instance->IncomingMessage, 0x2000, 1.0f);
}
