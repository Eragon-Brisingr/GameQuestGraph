// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameQuestGraphEditorSettings.generated.h"

UCLASS(Config=Editor, defaultconfig)
class GAMEQUESTGRAPHEDITOR_API UGameQuestGraphEditorSettings : public UObject
{
	GENERATED_BODY()
public:
	UGameQuestGraphEditorSettings();

	UPROPERTY(Config)
	TSoftObjectPtr<UScriptStruct> SequenceSingleType;
	UPROPERTY(Config)
	TSoftObjectPtr<UScriptStruct> SequenceListType;
	UPROPERTY(Config)
	TSoftObjectPtr<UScriptStruct> SequenceBranchType;
	UPROPERTY(Config)
	TSoftObjectPtr<UScriptStruct> SequenceSubQuestType;
	UPROPERTY(Config)
	TSoftObjectPtr<UScriptStruct> ElementBranchListType;
	UPROPERTY(Config)
	TSoftObjectPtr<UScriptStruct> ElementScriptType;
};
