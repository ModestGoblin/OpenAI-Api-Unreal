// Copyright Kellan Mythen 2021. All rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OpenAIDefinitions.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

/**
 * 
 */
class OPENAIAPI_API OpenAIParser
{
public:
	OpenAIParser();
	~OpenAIParser();
	TArray<FCompletion> ParseCompletions(FJsonObject);
	FCompletion ParseCompletion(const FJsonObject&);
};
