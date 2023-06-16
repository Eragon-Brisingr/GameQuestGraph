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
	static FGameQuestElementPtr QuestElementToPtr(const FGameQuestElementBase& Node);
	DECLARE_FUNCTION(execQuestElementToPtr);

	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils", meta=(DisplayName = "Equal (QuestSequence)", CompactNodeTitle = "==", Keywords = "== equal"))
	static bool EqualEqual_QuestSequenceQuestSequence(const FGameQuestSequencePtr& LHS, const FGameQuestSequencePtr& RHS) { return LHS == RHS; }
	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static bool IsQuestSequenceValid(const FGameQuestSequencePtr& Sequence) { return Sequence; }
	UFUNCTION(BlueprintCallable, Category = "GameQuest|Utils")
	static void EvaluateSequenceExposedInputs(const FGameQuestSequencePtr& Sequence) { 	if (Sequence) { Sequence->GetEvaluateGraphExposedInputs(); } }
	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static FName GetQuestSequenceName(const FGameQuestSequencePtr& Sequence) { return Sequence ? Sequence->GetNodeName() : NAME_None; }
	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static int32 GetQuestSequenceId(const FGameQuestSequencePtr& Sequence) { return Sequence ? Sequence->OwnerQuest->GetSequenceId(*Sequence) : GameQuest::IdNone; }
	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static bool IsQuestSequenceActivate(const FGameQuestSequencePtr& Sequence) { return Sequence ? Sequence->bIsActivated : false; }
	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static bool IsQuestSequenceInterrupted(const FGameQuestSequencePtr& Sequence) { return Sequence ? Sequence->bInterrupted : false; }
	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static bool IsQuestSequenceBranchesActivate(const FGameQuestSequencePtr& Sequence);
	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static bool IsQuestSequenceBranchInterrupted(const FGameQuestSequencePtr& Sequence, int32 BranchIndex);
	UFUNCTION(BlueprintCallable, Category = "GameQuest|Utils")
	static TArray<FGameQuestElementPtr> GetQuestSequenceListElements(const FGameQuestSequencePtr& Sequence, TArray<EGameQuestSequenceLogic>& Logics);
	UFUNCTION(BlueprintCallable, Category = "GameQuest|Utils")
	static TArray<FGameQuestElementPtr> GetQuestSequenceBranchElements(const FGameQuestSequencePtr& Sequence);
	UFUNCTION(BlueprintCallable, Category = "GameQuest|Utils")
	static TArray<FGameQuestSequencePtr> GetQuestSequenceNextSequence(const FGameQuestSequencePtr& Sequence);
	UFUNCTION(BlueprintCallable, Category = "GameQuest|Utils")
	static TArray<FGameQuestSequencePtr> GetQuestSequenceBranchNextSequence(const FGameQuestSequencePtr& Sequence, int32 BranchIndex);
	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static bool IsQuestSequenceSingle(const FGameQuestSequencePtr& Sequence) { return Sequence ? GameQuestCast<FGameQuestSequenceSingle>(Sequence.SequencePtr) != nullptr : false; }
	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static bool IsQuestSequenceList(const FGameQuestSequencePtr& Sequence) { return Sequence ? GameQuestCast<FGameQuestSequenceList>(Sequence.SequencePtr) != nullptr : false; }
	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static bool IsQuestSequenceBranch(const FGameQuestSequencePtr& Sequence) { return Sequence ? GameQuestCast<FGameQuestSequenceBranch>(Sequence.SequencePtr) != nullptr : false; }
	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static bool IsQuestSequenceSubQuest(const FGameQuestSequencePtr& Sequence) { return Sequence ? GameQuestCast<FGameQuestSequenceSubQuest>(Sequence.SequencePtr) != nullptr : false; }

	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils", meta=(DisplayName = "Equal (QuestElement)", CompactNodeTitle = "==", Keywords = "== equal"))
	static bool EqualEqual_QuestElementQuestElement(const FGameQuestElementPtr& LHS, const FGameQuestElementPtr& RHS) { return LHS == RHS; }
	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static bool IsQuestElementValid(const FGameQuestElementPtr& Element) { return Element; }
	UFUNCTION(BlueprintCallable, Category = "GameQuest|Utils")
	static void EvaluateElementExposedInputs(const FGameQuestElementPtr& Element) { if (Element) { Element->GetEvaluateGraphExposedInputs(); } }
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
	static bool IsQuestElementBranchList(const FGameQuestElementPtr& Element) { return Element ? GameQuestCast<FGameQuestElementBranchList>(Element.ElementPtr) != nullptr : false; }
	UFUNCTION(BlueprintCallable, Category = "GameQuest|Utils")
	static TArray<FGameQuestElementPtr> GetQuestElementBranchListElements(const FGameQuestElementPtr& Element, TArray<EGameQuestSequenceLogic>& Logics);
	UFUNCTION(BlueprintPure, Category = "GameQuest|Utils")
	static UGameQuestElementScriptable* GetQuestElementScriptInstance(const FGameQuestElementPtr& Element);
};
