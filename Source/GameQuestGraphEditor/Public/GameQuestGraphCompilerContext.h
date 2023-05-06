// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "KismetCompiler.h"

class UGameQuestGraphBlueprint;

class GAMEQUESTGRAPHEDITOR_API FGameQuestGraphCompilerContext : public FKismetCompilerContext
{
	using Super = FKismetCompilerContext;
public:
	FGameQuestGraphCompilerContext(UGameQuestGraphBlueprint* SourceSketch, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions);

	using FKismetCompilerContext::CreateVariable;

	// FKismetCompilerContext
	void SpawnNewClass(const FString& NewClassName) override;
	void PreCompile() override;
	void CreateClassVariablesFromBlueprint() override;
	void CreateFunctionList() override;
	void CopyTermDefaultsToDefaultObject(UObject* DefaultObject) override;
	void ValidatePin(const UEdGraphPin* Pin) const override;
	// End FKismetCompilerContext

	TSharedRef<struct FQuestNodeCollector> QuestNodeCollector;

	struct FRepFunction
	{
		const FStructProperty* Property;
		const FName FunctionName;
	};
	TArray<FRepFunction> RepFunctions;
	void CreateRepFunction(FStructProperty* Property, const FName& RefVarName);
};
