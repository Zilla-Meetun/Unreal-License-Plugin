#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Engine/World.h"
#ifndef LICENSESYSTEM_API
#define LICENSESYSTEM_API
#endif
DECLARE_LOG_CATEGORY_EXTERN(LogLicenseSystem, Log, All);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnLicenseRequestResponse, const FString&);

class LICENSESYSTEM_API FLicenseSystemModule : public IModuleInterface
{
public:
	FLicenseSystemModule();
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	static FLicenseSystemModule& Get();
	static void SetUserId(const FString& UserId);
	static void SetLoginToken(const FString& Token);
	static FString GetUserId();
	static FString GetLoginToken();
	
private:
	void CheckLicenseAsync() const;
	
	FOnLicenseRequestResponse OnLicenseRequestResponseDelegate;
	void OnLicenseRequestResponse(const FString& Response);

#if WITH_EDITOR
	void OnBeginPIE(const bool bIsSimulating);
	void OnBeginStandaloneLocalPlay(uint32 ProcessID);
	void OnPostPIEStarted(bool bIsSimulating);
	
	FTSTicker::FDelegateHandle KillTickerHandle;
	bool bBlockNextPIE = false;
#endif
	
	
	void OnPostWorldInit(UWorld* World, FWorldInitializationValues WorldInitializationValues);
	void OnEngineInitComplete();
	void OnPostLoadMap(UWorld* InWorld);

	FDelegateHandle PostLoadMapHandle;
	bool bDidStandaloneCheck = false;

	static FString LoginUserId;
	static FString LoginToken;

};
