// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameQuestGraphEditorSettings.generated.h"

class UGameQuestGraphBase;

UCLASS(Config=Editor, defaultconfig, CollapseCategories)
class GAMEQUESTGRAPHEDITOR_API UGameQuestGraphEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UGameQuestGraphEditorSettings();

	FName GetContainerName() const override { return TEXT("Project"); }
	FName GetCategoryName() const override { return TEXT("Plugins"); }
	FName GetSectionName() const override { return TEXT("GameQuestGraphEditorSettings"); }

	UPROPERTY(EditAnywhere, Config, Category = "GameQuest")
	TSoftObjectPtr<UScriptStruct> SequenceSingleType;
	UPROPERTY(EditAnywhere, Config, Category = "GameQuest")
	TSoftObjectPtr<UScriptStruct> SequenceListType;
	UPROPERTY(EditAnywhere, Config, Category = "GameQuest")
	TSoftObjectPtr<UScriptStruct> SequenceBranchType;
	UPROPERTY(EditAnywhere, Config, Category = "GameQuest")
	TSoftObjectPtr<UScriptStruct> SequenceSubQuestType;
	UPROPERTY(EditAnywhere, Config, Category = "GameQuest")
	TSoftObjectPtr<UScriptStruct> ElementBranchListType;
	UPROPERTY(EditAnywhere, Config, Category = "GameQuest")
	TSoftObjectPtr<UScriptStruct> ElementScriptType;

	UPROPERTY(EditAnywhere, Config, Category = "GameQuest")
	TArray<TSoftObjectPtr<UScriptStruct>> HiddenNativeTypes;
	UPROPERTY(EditAnywhere, Config, Category = "GameQuest")
	TArray<TSoftClassPtr<UObject>> HiddenScriptTypes;
	UPROPERTY(EditAnywhere, Config, Category = "GameQuest", meta = (AllowAbstract))
	TArray<TSoftClassPtr<UGameQuestGraphBase>> HiddenQuestTypes;
};
