#if WITH_EDITOR

#include "AuthenticatorSettingsCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"
#include "AuthenticatorSettings.h"

#define LOCTEXT_NAMESPACE "AuthenticatorSettings"

TSharedRef<IDetailCustomization> FAuthenticatorSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FAuthenticatorSettingsCustomization());
}

void FAuthenticatorSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() == 0) return;

	UAuthenticatorSettings* Settings = Cast<UAuthenticatorSettings>(Objects[0].Get());
	if (!Settings) return;

	// Set the UI refresh callback
	Settings->OnValidationUIRefresh = [&DetailBuilder]()
	{
		DetailBuilder.ForceRefreshDetails();
	};

	IDetailCategoryBuilder& LicenseCategory = DetailBuilder.EditCategory("License");

	// Validate button
	LicenseCategory.AddCustomRow(FText::FromString("ValidateLicense"))
	.WholeRowContent()
	[
		SNew(SButton)
		.Text(FText::FromString("Validate License"))
		.OnClicked_Lambda([Settings]()
		{
			Settings->ValidateLicense();
			return FReply::Handled();
		})
	];

	// Validation result text
	if (!Settings->ValidationResultMessage.IsEmpty())
	{
		LicenseCategory.AddCustomRow(FText::FromString("ValidationMessage"))
		.WholeRowContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString(Settings->ValidationResultMessage))
			.ColorAndOpacity_Lambda([Settings]()
			{
				const FString& Msg = Settings->ValidationResultMessage;
				if (Msg.StartsWith(TEXT("Error")))
					return FSlateColor(FLinearColor::Red);
				else if (Msg.StartsWith(TEXT("License")))
					return FSlateColor(FLinearColor::Green);
				else if (Msg.StartsWith(TEXT("Requesting")))
					return FSlateColor(FLinearColor::White);

				return FSlateColor::UseForeground();
			})
			.Font(IDetailLayoutBuilder::GetDetailFontBold())
		];
	}
}


#undef LOCTEXT_NAMESPACE

#endif
