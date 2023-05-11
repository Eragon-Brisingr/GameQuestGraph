// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameQuestSequenceBase.h"
#include "GameQuestElementBase.h"
#include "GameQuestGraphBase.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameQuestFunctionLibrary.generated.h"

UCLASS()
class GAMEQUESTGRAPH_API UGameQuestFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, CustomThunk, Category = "GameQuest|Utils", meta = (CustomStructureParam = Node, CompactNodeTitle = "->"))
	static FGameQuestSequencePtr QuestSequenceToPtr(const FGameQuestSequenceBase& Node);
	DECLARE_FUNCTION(execQuestSequenceToPtr);

	UFUNCTION(BlueprintPure, CustomThunk, Category = "GameQuest|Utils", meta = (CustomStructureParam = Node, CompactNodeTitle = "->"))
	static FGameQuestSequencePtr QuestElementToPtr(const FGameQuestSequenceBase& Node);
	DECLARE_FUNCTION(execQuestElementToPtr);

	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static bool IsQuestSequenceValid(const FGameQuestSequencePtr& Sequence) { return Sequence; }
	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static FName GetQuestSequenceName(const FGameQuestSequencePtr& Sequence) { return Sequence ? Sequence->GetNodeName() : NAME_None; }
	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static int32 GetQuestSequenceId(const FGameQuestSequencePtr& Sequence) { return Sequence ? Sequence->OwnerQuest->GetSequenceId(*Sequence) : GameQuest::IdNone; }

	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static bool IsQuestElementValid(const FGameQuestElementPtr& Element) { return Element; }
	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static FName GetQuestElementName(const FGameQuestElementPtr& Element) { return Element ? Element->GetNodeName() : NAME_None; }
	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static int32 GetQuestElementId(const FGameQuestElementPtr& Element) { return Element ? Element->OwnerQuest->GetElementId(*Element) : GameQuest::IdNone; }
	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static bool IsQuestElementFinished(const FGameQuestElementPtr& Element) { return Element ? Element->bIsFinished : false; }
	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static bool IsQuestElementActivated(const FGameQuestElementPtr& Element) { return Element ? Element->bIsActivated : false; }
	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static bool IsQuestElementOptional(const FGameQuestElementPtr& Element) { return Element ? Element->bIsOptional : false; }
	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static UGameQuestElementScriptable* GetQuestElementScriptInstance(const FGameQuestElementPtr& Element);
};
