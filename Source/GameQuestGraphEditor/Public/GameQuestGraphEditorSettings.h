// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameQuestGraphEditorSettings.generated.h"

UCLASS(Config=Editor, defaultconfig, CollapseCategories)
class GAMEQUESTGRAPHEDITOR_API UGameQuestGraphEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UGameQuestGraphEditorSettings();

	FName GetContainerName() const override { return TEXT("Project"); }
	FName GetCategoryName() const override { return TEXT("Plugins"); }
	FName GetSectionName() const override { return TEXT("GameQuestGraphEditorSettings"); }

	UPROPERTY(EditAnywhere, Config)
	TSoftObjectPtr<UScriptStruct> SequenceSingleType;
	UPROPERTY(EditAnywhere, Config)
	TSoftObjectPtr<UScriptStruct> SequenceListType;
	UPROPERTY(EditAnywhere, Config)
	TSoftObjectPtr<UScriptStruct> SequenceBranchType;
	UPROPERTY(EditAnywhere, Config)
	TSoftObjectPtr<UScriptStruct> SequenceSubQuestType;
	UPROPERTY(EditAnywhere, Config)
	TSoftObjectPtr<UScriptStruct> ElementBranchListType;
	UPROPERTY(EditAnywhere, Config)
	TSoftObjectPtr<UScriptStruct> ElementScriptType;

	UPROPERTY(EditAnywhere, Config)
	TArray<TSoftObjectPtr<UScriptStruct>> HiddenNativeTypes;
	UPROPERTY(EditAnywhere, Config)
	TArray<TSoftClassPtr<UObject>> HiddenScriptTypes;
	UPROPERTY(EditAnywhere, Config)
	TArray<TSoftClassPtr<UGameQuestGraphBase>> HiddenGameQuestGraphTypes;
};
