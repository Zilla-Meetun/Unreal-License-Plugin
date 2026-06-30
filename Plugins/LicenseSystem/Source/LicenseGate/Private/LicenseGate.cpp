#include "LicenseGate.h"

#include "AuthenticatorSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"

DEFINE_LOG_CATEGORY(LogLicenseGate);
#define LOCTEXT_NAMESPACE "FLicenseGateModule"

class FHttpModule;

inline bool ShouldRunEditorLicenseCheck()
{
#if WITH_EDITOR
	return (GIsEditor && !IsRunningCommandlet());
#else
	return false;
#endif
}

void FLicenseGateModule::StartupModule()
{
	if (ShouldRunEditorLicenseCheck())
	{
		StartLicenseGateAsync(8.0f);
	}
}

void FLicenseGateModule::ShutdownModule()
{
	if (PendingRequest.IsValid())
	{
		const auto Status = PendingRequest->GetStatus();
		if (Status == EHttpRequestStatus::Processing)
			PendingRequest->CancelRequest();
		PendingRequest.Reset();
	}
}

void FLicenseGateModule::StartLicenseGateAsync(float TimeoutSeconds)
{
	bLicenseGateFinished.store(false, std::memory_order_release);

	FString License, Reason;
	if (!ReadLicenseFromSettings(License, Reason))
	{
		if (!Reason.IsEmpty())
		{
			KillEditorNow(Reason);
		}
		return;
	}

	const UAuthenticatorSettings* Settings = GetDefault<UAuthenticatorSettings>();
	const FString Url = UAuthenticatorSettings::BuildLicenseValidationUrl(Settings->LicenseServerUrl, License);

	FHttpModule& Http = FHttpModule::Get();
	PendingRequest = Http.CreateRequest();
	PendingRequest->SetURL(Url);
	PendingRequest->SetVerb(TEXT("GET"));
	PendingRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));
	if (!Settings->LicenseServerAuthorizationHeader.IsEmpty())
	{
		PendingRequest->SetHeader(TEXT("Authorization"), Settings->LicenseServerAuthorizationHeader);
	}
	PendingRequest->SetTimeout(FMath::Max(1.0f, TimeoutSeconds));
	PendingRequest->OnProcessRequestComplete().BindRaw(this, &FLicenseGateModule::OnLicenseHttpComplete);
	PendingRequest->ProcessRequest();
}

bool FLicenseGateModule::ReadLicenseFromSettings(FString& OutLicense, FString& OutReason) const
{
	const UAuthenticatorSettings* Settings = GetDefault<UAuthenticatorSettings>();
	OutLicense = Settings ? Settings->License : FString();
	OutLicense.TrimStartAndEndInline();

	if (OutLicense.IsEmpty())
	{
		OutReason.Empty();
		return false;
	}

	FGuid ParsedGuid;
	if (!FGuid::Parse(OutLicense, ParsedGuid))
	{
		OutReason = TEXT("No valid license found. Enter a valid license key in Project Settings > Plugins > Authenticator.");
		return false;
	}

	return true;
}

void FLicenseGateModule::OnLicenseHttpComplete(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSucceeded)
{
	if (bLicenseGateFinished.exchange(true)) return;

	auto Fail = [this](const FString& Msg)
	{
		if (!IsInGameThread())
		{
			AsyncTask(ENamedThreads::GameThread, [this, Msg]() { KillEditorNow(Msg); });
		}
		else
		{
			KillEditorNow(Msg);
		}
	};

	if (!bSucceeded || !Response.IsValid())
		return Fail(TEXT("Unable to reach licensing server."));

	const int32 Code = Response->GetResponseCode();
	if (Code == 404) return Fail(TEXT("Unable to reach licensing server."));
	if (Code < 200 || Code >= 300)
		return Fail(FString::Printf(TEXT("License server returned HTTP %d."), Code));

	const FString Body = Response->GetContentAsString();
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Root) || !Root.IsValid())
		return Fail(TEXT("Malformed licensing response (Invalid Json Response)."));

	const TSharedPtr<FJsonObject>* LicenseObj = nullptr;
	if (!Root->TryGetObjectField(TEXT("license"), LicenseObj) || !LicenseObj || !LicenseObj->IsValid())
		return Fail(TEXT("Malformed licensing response (Missing License)."));

	bool bIsValid = false;
	if (!(*LicenseObj)->TryGetBoolField(TEXT("is_valid"), bIsValid) || !bIsValid)
		return Fail(TEXT("License has been checked and is not valid."));

}


void FLicenseGateModule::KillEditorNow(const FString& Reason) const
{
	UE_LOG(LogLicenseGate, Error, TEXT("Unreal Editor will now close.\n\nReason:\n%s"), *Reason);
	const FString Title = TEXT("License Check Failed");
	const FString Body  = FString::Printf(TEXT("Reason:\n%s\nLicense invalidated."), *Reason);
	FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *Body, *Title);
	FPlatformMisc::RequestExitWithStatus(true, 1);


	FPlatformProcess::SleepNoStats(0.1f);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLicenseGateModule, LicenseGate)
