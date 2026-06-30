#pragma once

#include "CoreMinimal.h"
#include "HttpFwd.h"
#include "Engine/DeveloperSettings.h"
#include "AuthenticatorSettings.generated.h"

UCLASS(config=Game, defaultconfig)
class LICENSESYSTEM_API UAuthenticatorSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:

#if WITH_EDITOR
	virtual FName GetContainerName() const override { return "Project"; }
	virtual FName GetCategoryName()  const override { return "Plugins"; }
	virtual FName GetSectionName()   const override { return "Authenticator"; }
#endif
	
	UPROPERTY(EditAnywhere, config, Category = "License", meta = (ToolTip = "License UUID to validate."))
	FString License;

	UPROPERTY(EditAnywhere, config, Category = "License", meta = (DisplayName = "License Server URL", ToolTip = "Base endpoint used for license validation. The plugin appends the license key unless the URL contains {License}."))
	FString LicenseServerUrl = TEXT("http://127.0.0.1:8765/licenses");

	UPROPERTY(EditAnywhere, config, Category = "License", meta = (DisplayName = "License Server Authorization Header", ToolTip = "Optional full Authorization header value sent to the license server."))
	FString LicenseServerAuthorizationHeader;

	FString ValidationResultMessage;
	bool bLicenseIsValid = false;
	
	TFunction<void()> OnValidationUIRefresh;
	
	static FString BuildLicenseValidationUrl(const FString& ServerUrl, const FString& License);

	bool IsLicenseValidUUID() const;
	void ValidateLicense();
	

private:
	void RefreshValidationUI() const;
	void OnHttpResponse(TSharedPtr<IHttpRequest> Request, TSharedPtr<IHttpResponse> Response, bool bWasSuccessful);

};
