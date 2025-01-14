// Copyright Kellan Mythen 2021. All rights Reserved.


#include "OpenAICallGPT.h"
#include "OpenAIUtils.h"
#include "Http.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "OpenAIParser.h"


UOpenAICallGPT::UOpenAICallGPT()
{
}

UOpenAICallGPT::~UOpenAICallGPT()
{
}

UOpenAICallGPT* UOpenAICallGPT::OpenAICallGPT3(EOAEngineType engineInput, FString promptInput, FGPT3Settings settingsInput)
{
	UOpenAICallGPT* BPNode = NewObject<UOpenAICallGPT>();
	BPNode->engine = engineInput;
	BPNode->prompt = promptInput;
	BPNode->settings = settingsInput;
	return BPNode;
}

void UOpenAICallGPT::Activate()
{
	FString _apiKey;
	if (UOpenAIUtils::getUseApiKeyFromEnvironmentVars())
		_apiKey = UOpenAIUtils::GetEnvironmentVariable(TEXT("OPENAI_API_KEY"));
	else
		_apiKey = UOpenAIUtils::getApiKey();


	// checking parameters are valid
	if (_apiKey.IsEmpty())
	{
		Finished.Broadcast({}, TEXT("Api key is not set"), false);
	} else if (prompt.IsEmpty())
	{
		Finished.Broadcast({}, TEXT("Prompt is empty"), false);
	} else if (settings.bestOf < settings.numCompletions)
	{
		Finished.Broadcast({}, TEXT("BestOf must be greater than numCompletions"), false);
	} else if (settings.maxTokens <= 0 || settings.maxTokens >= 2048)
	{
		Finished.Broadcast({}, TEXT("maxTokens must be within 0 and 2048"), false);
	}



	auto HttpRequest = FHttpModule::Get().CreateRequest();
	
	
	FString apiMethod;
	switch (engine)
	{
	case EOAEngineType::DAVINCI:
			apiMethod = "davinci";
	break;
	case EOAEngineType::CURIE:
			apiMethod = "curie";
	break;
	case EOAEngineType::BABBAGE:
			apiMethod = "babbage";
	break;
	case EOAEngineType::ADA:
			apiMethod = "ada";
	break;
	}

	// converting parameters to strings
	FString tempPrompt = settings.startSequence + prompt;
	FString tempHeader = "Bearer ";
	tempHeader += _apiKey;

	// set headers
	FString url = FString::Printf(TEXT("https://api.openai.com/v1/engines/%s/completions"), *apiMethod);
	HttpRequest->SetURL(url);
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("Authorization"), tempHeader);

	//build payload
	TSharedPtr<FJsonObject> _payloadObject = MakeShareable(new FJsonObject());
	_payloadObject->SetStringField(TEXT("prompt"), tempPrompt);
	_payloadObject->SetNumberField(TEXT("max_tokens"), settings.maxTokens);
	_payloadObject->SetNumberField(TEXT("temperature"), FMath::Clamp(settings.temperature, 0.0f, 1.0f));
	_payloadObject->SetNumberField(TEXT("top_p"), FMath::Clamp(settings.topP, 0.0f, 1.0f));
	_payloadObject->SetNumberField(TEXT("n"), settings.numCompletions);
	_payloadObject->SetNumberField(TEXT("best_of"), settings.bestOf);
	if (!(settings.presencePenalty == 0))
		_payloadObject->SetNumberField(TEXT("presence_penalty"), FMath::Clamp(settings.presencePenalty, 0.0f, 1.0f));
	if (!(settings.frequencyPenalty == 0))
		_payloadObject->SetNumberField(TEXT("frequency_penalty"), FMath::Clamp(settings.frequencyPenalty, 0.0f, 1.0f));
	if(!settings.stopSequence.IsEmpty())
		_payloadObject->SetStringField(TEXT("stop"), settings.stopSequence);

	// convert payload to string
	FString _payload;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&_payload);
	FJsonSerializer::Serialize(_payloadObject.ToSharedRef(), Writer);

	// commit request
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetContentAsString(_payload);

	if (HttpRequest->ProcessRequest())
	{
		HttpRequest->OnProcessRequestComplete().BindUObject(this, &UOpenAICallGPT::OnResponse);
	}
	else
	{
		Finished.Broadcast({}, ("Error sending request"), false);
	}


	
}

void UOpenAICallGPT::OnResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool WasSuccessful)
{
	if (!WasSuccessful)
	{
		UE_LOG(LogTemp, Warning, TEXT("Error processing request. \n%s \n%s"), *Response->GetContentAsString(), *Response->GetURL());
		if (Finished.IsBound())
		{
			Finished.Broadcast({}, *Response->GetContentAsString(), false);
		}

		return;
	}

	TSharedPtr<FJsonObject> responseObject;
	TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (FJsonSerializer::Deserialize(reader, responseObject))
	{
		bool err = responseObject->HasField("error");

		if (err)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s"), *Response->GetContentAsString());
			Finished.Broadcast({}, TEXT("Api error"), false);
			return;
		}
		
		OpenAIParser parser;
		TArray<FCompletion> _out;

		auto CompletionsObject = responseObject->GetArrayField(TEXT("Choices"));
		for (auto& elem : CompletionsObject)
		{
			_out.Add(parser.ParseCompletion(*elem->AsObject()));
		}

		Finished.Broadcast(_out, "", true);
	}


}

