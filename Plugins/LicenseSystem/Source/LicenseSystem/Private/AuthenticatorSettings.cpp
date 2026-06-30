#include "AuthenticatorSettings.h"
#include "HttpModule.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Interfaces/IHttpResponse.h"
#include "Async/Async.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"


bool UAuthenticatorSettings::IsLicenseValidUUID() const
{
	FGuid ParsedGuid;
	return FGuid::Parse(License, ParsedGuid);
}

FString UAuthenticatorSettings::BuildLicenseValidationUrl(const FString& ServerUrl, const FString& License)
{
	FString TrimmedServerUrl = ServerUrl;
	TrimmedServerUrl.TrimStartAndEndInline();
	TrimmedServerUrl.RemoveFromEnd(TEXT("/"));

	if (TrimmedServerUrl.Contains(TEXT("{License}")))
	{
		return TrimmedServerUrl.Replace(TEXT("{License}"), *FGenericPlatformHttp::UrlEncode(License));
	}

	return FString::Printf(TEXT("%s/%s"), *TrimmedServerUrl, *FGenericPlatformHttp::UrlEncode(License));
}

void UAuthenticatorSettings::RefreshValidationUI() const
{
	if (OnValidationUIRefresh)
	{
		OnValidationUIRefresh();
	}
}

void UAuthenticatorSettings::ValidateLicense()
{
	if (!IsLicenseValidUUID())
	{
		ValidationResultMessage = TEXT("Error: License is not a valid UUID.");
		RefreshValidationUI();
		return;
	}
	
	ValidationResultMessage = TEXT("Requesting Validation...");
	RefreshValidationUI();
	const FString Endpoint = BuildLicenseValidationUrl(LicenseServerUrl, License);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Endpoint);
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	if (!LicenseServerAuthorizationHeader.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), LicenseServerAuthorizationHeader);
	}
	// Bind callback
	Request->OnProcessRequestComplete().BindUObject(this, &UAuthenticatorSettings::OnHttpResponse);
	Request->ProcessRequest();
}

void UAuthenticatorSettings::OnHttpResponse(TSharedPtr<IHttpRequest> Request,
	TSharedPtr<IHttpResponse> Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		ValidationResultMessage = TEXT("Error: Network failure while validating license.");
		if (OnValidationUIRefresh)
		{
			AsyncTask(ENamedThreads::GameThread, [this]() {
			   RefreshValidationUI();
		   });
		}
		return;
	}

	const FString Content = Response->GetContentAsString();
	//UE_LOG(LogTemp, Display, TEXT("Response: %s"), *Content);
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		ValidationResultMessage = TEXT("Error: Invalid response from license server.");
		if (OnValidationUIRefresh)
		{
			AsyncTask(ENamedThreads::GameThread, [this]() {
			   RefreshValidationUI();
		   });
		}
		return;
	}
	
	bool bValid = false;
	
	const TSharedPtr<FJsonObject>* LicenseObj = nullptr;
	if (JsonObject->TryGetObjectField(TEXT("license"), LicenseObj) && LicenseObj && LicenseObj->IsValid())
	{
		(*LicenseObj)->TryGetBoolField(TEXT("is_valid"), bValid);
	}

	if (bValid)
	{
		ValidationResultMessage = TEXT("License is valid.");
		if (OnValidationUIRefresh)
		{
			AsyncTask(ENamedThreads::GameThread, [this]() {
			   RefreshValidationUI();
		   });
		}
		return;
	}
	
	ValidationResultMessage = TEXT("Error: License is invalid or not recognized.");
	if (OnValidationUIRefresh)
	{
		AsyncTask(ENamedThreads::GameThread, [this]() {
		   RefreshValidationUI();
	   });
	}
}
