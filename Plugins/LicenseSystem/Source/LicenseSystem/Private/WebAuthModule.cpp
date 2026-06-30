// Copyright Epic Games, Inc. All Rights Reserved.

#include "WebAuthModule.h"
#include "AuthenticatorSettings.h"
#include "Misc/MessageDialog.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "Containers/Ticker.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogLicenseSystem);
#if WITH_EDITOR
#include "Editor.h"                
#include "Misc/MessageDialog.h"
#include "ISettingsModule.h"
#include "AuthenticatorSettingsCustomization.h"
#endif
#include "Misc/CoreDelegates.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"


#define LOCTEXT_NAMESPACE "FLicenseSystemModule"
FString FLicenseSystemModule::LoginUserId;
FString FLicenseSystemModule::LoginToken;
FLicenseSystemModule::FLicenseSystemModule()
{}

void FLicenseSystemModule::StartupModule()
{

	OnLicenseRequestResponseDelegate.AddRaw(this, &FLicenseSystemModule::OnLicenseRequestResponse);
	FCoreDelegates::OnFEngineLoopInitComplete.AddRaw(this, &FLicenseSystemModule::OnEngineInitComplete);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddRaw(this, &FLicenseSystemModule::OnPostLoadMap);
	
#if WITH_EDITOR
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(
		"AuthenticatorSettings",
		FOnGetDetailCustomizationInstance::CreateStatic(&FAuthenticatorSettingsCustomization::MakeInstance)
	);

	PropertyModule.NotifyCustomizationModuleChanged();
	
	FEditorDelegates::BeginPIE.AddRaw(this, &FLicenseSystemModule::OnBeginPIE);
	FEditorDelegates::BeginStandaloneLocalPlay.AddRaw(this, &FLicenseSystemModule::OnBeginStandaloneLocalPlay);
	FEditorDelegates::PostPIEStarted.AddRaw(this, &FLicenseSystemModule::OnPostPIEStarted);

#endif
}

void FLicenseSystemModule::ShutdownModule()
{
	OnLicenseRequestResponseDelegate.RemoveAll(this);
	FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	
#if WITH_EDITOR
	if (FModuleManager::Get().IsModuleLoaded("Settings"))
	{
		ISettingsModule* Settings = FModuleManager::LoadModulePtr<ISettingsModule>("Settings");
		Settings->UnregisterSettings("Project", "Plugins", "Authenticator");

		FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor")
		.UnregisterCustomClassLayout("AuthenticatorSettings");
	}
	
	FEditorDelegates::BeginPIE.RemoveAll(this);
	FEditorDelegates::BeginStandaloneLocalPlay.RemoveAll(this);
	FEditorDelegates::PostPIEStarted.RemoveAll(this);

#endif

	
}

FLicenseSystemModule& FLicenseSystemModule::Get()
{
	return FModuleManager::LoadModuleChecked<FLicenseSystemModule>("LicenseSystem");
}

void FLicenseSystemModule::SetUserId(const FString& UserId)
{
	LoginUserId = UserId;
}

FString FLicenseSystemModule::GetLoginToken()
{
	return LoginToken;
}

void FLicenseSystemModule::SetLoginToken(const FString& Token)
{
	LoginToken = Token;
}

FString FLicenseSystemModule::GetUserId()
{
	return LoginUserId;
}

void FLicenseSystemModule::CheckLicenseAsync() const
{
	const UAuthenticatorSettings* Settings = GetDefault<UAuthenticatorSettings>();
	const FString License = Settings ? Settings->License : FString();
	FGuid Dummy;
	if (!FGuid::Parse(License, Dummy))
	{
		OnLicenseRequestResponseDelegate.Broadcast(TEXT("Not a valid UUID"));
		return;
	}

	const FString URL = UAuthenticatorSettings::BuildLicenseValidationUrl(Settings->LicenseServerUrl, License);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(URL);
	Req->SetVerb(TEXT("GET"));
	Req->SetHeader(TEXT("Accept"), TEXT("application/json"));
	if (!Settings->LicenseServerAuthorizationHeader.IsEmpty())
	{
		Req->SetHeader(TEXT("Authorization"), Settings->LicenseServerAuthorizationHeader);
	}
	Req->SetTimeout(8.0f); 
	

	Req->OnProcessRequestComplete().BindLambda(
		[this](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOK)
		{
			if (this == nullptr)
			{
				return;
			}
			bool bIsValid = false;
			
			if (!bOK || !Resp.IsValid())
			{
				this->OnLicenseRequestResponseDelegate.Broadcast(TEXT("Network error contacting license server."));
				return;
			}

			const FString Body = Resp->GetContentAsString();
			TSharedPtr<FJsonObject> Root;
			auto Reader = TJsonReaderFactory<>::Create(Body);
			if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
			{
				this->OnLicenseRequestResponseDelegate.Broadcast(TEXT("Invalid JSON from license server."));
				return;
			}

			const TSharedPtr<FJsonObject>* LicenseObj;
			if (!Root->TryGetObjectField(TEXT("license"), LicenseObj) || !LicenseObj->IsValid())
			{
				this->OnLicenseRequestResponseDelegate.Broadcast(TEXT("Missing 'license' object in response."));
				return;
			}
			if (!(*LicenseObj)->TryGetBoolField(TEXT("is_valid"), bIsValid))
			{
				this->OnLicenseRequestResponseDelegate.Broadcast(TEXT("Missing 'is_valid' in response."));
				return;
			}
			if (!bIsValid)
			{
				this->OnLicenseRequestResponseDelegate.Broadcast(TEXT("License invalid."));
			}
          
		});

	Req->ProcessRequest();
	
}


void FLicenseSystemModule::OnLicenseRequestResponse(const FString& Response)
{
	UE_LOG(LogLicenseSystem, Error , TEXT("%s"), *Response);
#if WITH_EDITOR
	if (GIsEditor && GEditor && GEditor->IsPlayingSessionInEditor())
	{
		GEditor->RequestEndPlayMap();
	
		if (!KillTickerHandle.IsValid())
		{
			const FString LocalResponse = Response;
			KillTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateLambda([LocalResponse](float)
				{
					if (GEditor && !GEditor->IsPlayingSessionInEditor())
					{
						FMessageDialog::Open(
							EAppMsgType::Ok,
							FText::FromString(FString::Printf(TEXT("this application is using unlicensed code and has been closed, please contact the app developer for assistance")))
						);
						return false;
					}
					return true;
				}),
				0.0f
			);
		}
		return;
	}
#else

	{
		const FString Title = TEXT("License Check Failed");
		const FString Body  = FString::Printf(
			TEXT("This application is using unlicensed code and has been closed, please contact the app developer for assistance"),
			*Response
		);
		
		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *Body, *Title);
	}
	
	UWorld* World = (GEngine ? (GEngine->GetCurrentPlayWorld()
		? GEngine->GetCurrentPlayWorld()
		: (GEngine->GameViewport ? GEngine->GameViewport->GetWorld() : nullptr)) : nullptr);

	if (World)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			UKismetSystemLibrary::QuitGame(World, PC, EQuitPreference::Quit, false);
			return;
		}
	}

	// Fallback if no world/PC available.

#endif
	if(!GIsEditor)
		FGenericPlatformMisc::RequestExit(false);
}

#if WITH_EDITOR
void FLicenseSystemModule::OnBeginPIE(const bool bIsSimulating)
{
	CheckLicenseAsync();
}

void FLicenseSystemModule::OnBeginStandaloneLocalPlay(uint32 ProcessID)
{
	CheckLicenseAsync();
}

void FLicenseSystemModule::OnPostPIEStarted(bool bIsSimulating)
{
	CheckLicenseAsync();
}

#endif

void FLicenseSystemModule::OnPostWorldInit(UWorld* World,
	FWorldInitializationValues WorldInitializationValues)
{
	if (!World) return;

#if WITH_EDITOR
	if (World->WorldType == EWorldType::PIE) return;
#endif

	if (World->IsGameWorld())
	{
		FWorldDelegates::OnPostWorldInitialization.RemoveAll(this);
		
		CheckLicenseAsync();
	}
}

void FLicenseSystemModule::OnEngineInitComplete()
{
	FWorldDelegates::OnPostWorldInitialization.AddRaw(this,
		&FLicenseSystemModule::OnPostWorldInit);
}

void FLicenseSystemModule::OnPostLoadMap(UWorld* World)
{
	if (!World) return;

#if WITH_EDITOR
	if (World->WorldType == EWorldType::PIE) return; 
#endif

	if (!bDidStandaloneCheck && World->IsGameWorld())
	{
		bDidStandaloneCheck = true;    
		CheckLicenseAsync();
	}
}


#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLicenseSystemModule, LicenseSystem)
