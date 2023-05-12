// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameQuestType.h"
#include "Engine/Blueprint.h"
#include "GameQuestGraphBlueprint.generated.h"

/**
 *
 */
UCLASS()
class GAMEQUESTGRAPH_API UGameQuestGraphBlueprint : public UBlueprint
{
	GENERATED_BODY()
public:
#if WITH_EDITOR
	UClass* GetBlueprintClass() const override;
	void GetReparentingRules(TSet<const UClass*>& AllowedChildrenOfClasses, TSet<const UClass*>& DisallowedChildrenOfClasses) const override;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UEdGraph> GameQuestGraph;

	UPROPERTY()
	uint16 NodeIdCounter = 0;
#endif
};

UCLASS()
class GAMEQUESTGRAPH_API UGameQuestGraphGeneratedClass : public UBlueprintGeneratedClass
{
	GENERATED_BODY()
public:
	void Serialize(FArchive& Ar) override;

	void PostQuestCDOInitProperties();

	UPROPERTY()
	TMap<FName, uint16> NodeNameIdMap;
	TMap<uint16, GameQuest::FLogicList> NodeIdLogicsMap;
	TMap<uint16, FStructProperty*> NodeIdPropertyMap;
	TMap<uint16, TArray<uint16, TInlineAllocator<1>>> NodeToSuccessorMap;
	TMap<uint16, TArray<uint16, TInlineAllocator<1>>> NodeToPredecessorMap;
	struct FEventNameNodeId
	{
		FName EventName;
		uint16 NodeId;
		friend FArchive& operator<<(FArchive& Ar, FEventNameNodeId& V)
		{
			Ar << V.EventName;
			Ar << V.NodeId;
			return Ar;
		}
	};
	TMap<uint16, TArray<FEventNameNodeId, TInlineAllocator<1>>> NodeIdEventNameMap;
	TMap<FName, FStructProperty*> FinishedTags;
	TMap<FName, TArray<FEventNameNodeId, TInlineAllocator<1>>> FinishedTagPreNodesMap;
};
