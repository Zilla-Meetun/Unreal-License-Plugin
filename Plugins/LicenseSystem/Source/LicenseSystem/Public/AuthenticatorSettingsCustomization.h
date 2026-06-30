#pragma once

#if WITH_EDITOR
#include "IDetailCustomization.h"

class FAuthenticatorSettingsCustomization : public IDetailCustomization
{
public:
	
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
#endif