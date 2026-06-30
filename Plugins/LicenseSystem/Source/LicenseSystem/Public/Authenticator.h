#pragma once

#include "CoreMinimal.h"
#include "WebAuthModule.h"
#include "Authenticator.generated.h"


DECLARE_MULTICAST_DELEGATE_OneParam(FOnAuthenticationRequestResponse, const FString&);


UENUM()
enum class EAuthType : uint8
{
	ENone = 0 UMETA(displayName = "No Auth"),
	EToken = 1 UMETA(displayName = "Token"),
	EBearerToken = 2 UMETA(displayName = "Bearer Token"),
	EBasicAuth = 3 UMETA(displayName = "Basic Auth"),
};

class LICENSESYSTEM_API Authenticator : public TSharedFromThis<Authenticator>
{
	
public:
	Authenticator(): CustomBody(TMap<FString, FString>()), AuthorizationType(EAuthType::ENone),
	                          bIsAuthenticated(false)
	{
	};
	Authenticator(const FString& User, const FString& Pass = TEXT(""), EAuthType AuthType = EAuthType::EToken):
							  bIsAuthenticated(false)
	{
		if (AuthType == EAuthType::ENone)
		{
			UE_LOG(LogTemp, Warning, TEXT("Consider setting an AuthType."));
		}
		SetAuthType(AuthType);
		SetCredentials(User, Pass);
	}
	Authenticator(const TMap<FString, FString>& CustomCredentials, EAuthType AuthType = EAuthType::EToken): CustomBody(CustomCredentials),
							  bIsAuthenticated(false)
	{
		if (AuthType == EAuthType::ENone)
		{
			UE_LOG(LogTemp, Warning, TEXT("Consider setting an AuthType."));
		}
		SetAuthType(AuthType);
	}
	
	void SetAuthType(EAuthType AuthType);
	void SetCredentials(const FString& User, const FString& Pass);
	void AddCustomCredentials(const FString& HeaderName, const FString& HeaderValue);
	void SetRequestAuthentication(const FString& RequestAuthentication);
	
	void SendAuthenticationRequest(const FString& URL, const FString& AcceptResponseHeader = TEXT("Authorization"), FString Body = TEXT(""));
	
	FString GetAuthorizationCode();
	
	FOnAuthenticationRequestResponse OnAuthenticationRequestComplete;
	FOnAuthenticationRequestResponse OnAuthenticationRequestFailed;

	

private:
	static bool FindAuthorizationKey(const TSharedPtr<FJsonObject>& JsonObject,const FString& AuthorizationName, FString& OutValue);
	FString SetAuthorizationByType(const FString& AuthorizationValue) const;
	
	FString Username;
	FString Password;
	TMap<FString, FString> CustomBody;
	FString Authorization;
	EAuthType AuthorizationType;

	FString RequestAuthorizationHeader;

	bool bIsAuthenticated;
	
};
