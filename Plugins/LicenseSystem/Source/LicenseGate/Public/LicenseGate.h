#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogLicenseGate, Log, All);

class FLicenseGateModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void StartLicenseGateAsync(float TimeoutSeconds);
	bool ReadLicenseFromSettings(FString& OutLicense, FString& OutReason) const;
	void OnLicenseHttpComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded);
	void KillEditorNow(const FString& Reason) const;

	// Keep request alive & track completion
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> PendingRequest;
	std::atomic<bool> bLicenseGateFinished{false};
};
