#include "Authenticator.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

void Authenticator::SetAuthType(EAuthType AuthType)
{
	AuthorizationType = AuthType;
}

void Authenticator::SetCredentials(const FString& User, const FString& Pass = TEXT(""))
{
	Username = User;
	Password = Pass;
}

void Authenticator::AddCustomCredentials(const FString& HeaderName, const FString& HeaderValue)
{
	CustomBody.Add(HeaderName, HeaderValue);
}


void Authenticator::SetRequestAuthentication(const FString& RequestAuthentication)
{
	RequestAuthorizationHeader = RequestAuthentication;
}

void Authenticator::SendAuthenticationRequest(const FString& URL, const FString& AcceptResponseHeader, FString Body)
{
	if (AuthorizationType == EAuthType::ENone)
	{
		UE_LOG(LogTemp, Warning, TEXT("No Authentication type is set.\nPlease choose a type if Authentication is needed"))
		OnAuthenticationRequestFailed.Broadcast(TEXT("No Authentication type is set.\nPlease choose a type if Authentication is needed"));
		return;
	}
	if ((Username.IsEmpty() && Password.IsEmpty()) && Body.IsEmpty() && CustomBody.IsEmpty())
	{
		UE_LOG(LogTemp, Verbose, TEXT("Missing Username/Password Credentials, Custom Credentials and Request body. At least one is needed for Authentication.\nPlease include if Authentication is needed"))
		OnAuthenticationRequestFailed.Broadcast(TEXT("Missing Username/Password Credentials, Custom Credentials and Request body. At least one is needed for Authentication.\nPlease include if Authentication is needed"));
		return;
	}
	if (AuthorizationType == EAuthType::EBasicAuth)
	{
		if (Username.IsEmpty() && Password.IsEmpty())
		{
			UE_LOG(LogTemp, Verbose, TEXT("Basic Auth Requires Either Username or Password to be set"))
			OnAuthenticationRequestFailed.Broadcast(TEXT("Basic Auth Requires Either Username or Password to be set"));
			return;
		}
		Body = FString::Printf(TEXT("Basic %s"), *FBase64::Encode(Username+ ":" +Password)); //Need to decide if the request should be made
		OnAuthenticationRequestComplete.Broadcast("Basic "+FBase64::Encode(Username+ ":" +Password)); // or if the username and password is enough 
		return; // Maybe 
	}
	
	if (Body.IsEmpty())
	{
		TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();

		if (!Username.IsEmpty())
		{
			JsonObject->SetStringField(TEXT("Username"), Username);
		}
		if (!Password.IsEmpty())
		{
			JsonObject->SetStringField(TEXT("Password"), Password);
		}
		for (TPair<FString, FString> Entry : CustomBody)
		{
			JsonObject->SetStringField(Entry.Key, Entry.Value);
		}
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
		FJsonSerializer::Serialize(JsonObject, Writer);
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->OnProcessRequestComplete().BindLambda([this, AcceptResponseHeader](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
	{
		
		if (!this)
		{
			return;
		}
		if (!bWasSuccessful  || !Response.IsValid())
		{
			this->OnAuthenticationRequestFailed.Broadcast(TEXT("Request was unsuccessful. Response is not valid"));
			return;
		}
		FString ResponseString = Response->GetContentAsString();
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);
		if (!FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
		{
			this->OnAuthenticationRequestFailed.Broadcast(FString::Printf(TEXT("Request response is not a valid json.\nResponse received: %s"), *ResponseString));
			return;
		}
		FString AuthorizationValue;
		if (!FindAuthorizationKey(JsonObject, AcceptResponseHeader, AuthorizationValue))
		{
			this->OnAuthenticationRequestFailed.Broadcast(FString::Printf(TEXT("Could not find authorization key: %s in request to %s\nResponse: %s"), *AcceptResponseHeader, *Request->GetURL(), *ResponseString));
			return;
		}
		this->bIsAuthenticated = true;
		this->Authorization = SetAuthorizationByType(AuthorizationValue);
		this->OnAuthenticationRequestComplete.Broadcast(this->Authorization);
	   
	});
	Request->SetURL(URL);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), RequestAuthorizationHeader);
	Request->SetContentAsString(Body);
	Request->ProcessRequest();
}

FString Authenticator::GetAuthorizationCode()
{
	if (!bIsAuthenticated)
	{
		UE_LOG(LogTemp, Warning, TEXT("Authorization value may be incorrect. Has not been verified"))
	}
	return Authorization;
}

bool Authenticator::FindAuthorizationKey(const TSharedPtr<FJsonObject>& JsonObject,
	const FString& AuthorizationName, FString& OutValue)
{
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : JsonObject->Values)
	{
		if (Pair.Key.Equals(AuthorizationName, ESearchCase::IgnoreCase))
		{
			if (Pair.Value.IsValid() && Pair.Value->Type == EJson::String)
			{
				OutValue = Pair.Value->AsString();
				return true;
			}
		}
		if (Pair.Value->Type == EJson::Object)
		{
			TSharedPtr<FJsonObject> NestedObject = Pair.Value->AsObject();
			if (FindAuthorizationKey(NestedObject, AuthorizationName, OutValue))
				return true;
		}
		
		if (Pair.Value->Type == EJson::Array)
		{
			for (const auto& Element : Pair.Value->AsArray())
			{
				if (Element->Type == EJson::Object)
				{
					if (FindAuthorizationKey(Element->AsObject(), AuthorizationName,OutValue))
						return true;
				}
			}
		}
	}
	return false;
}

FString Authenticator::SetAuthorizationByType(const FString& AuthorizationValue) const
{
	switch (AuthorizationType)
	{
	case EAuthType::ENone:
		{
			return AuthorizationValue;
		}
	case EAuthType::EToken:
		{
			return AuthorizationValue;
		}
	case EAuthType::EBearerToken:
		{
			return FString::Printf(TEXT("Bearer %s"), *AuthorizationValue);
		}
	case EAuthType::EBasicAuth:
		{
			return FString::Printf(TEXT("Basic %s"), *FBase64::Encode(Username+ ":" +AuthorizationValue));
		}
	default:
		{
			return AuthorizationValue;
		}
	}
}


