// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameQuestType.generated.h"

GAMEQUESTGRAPH_API DECLARE_LOG_CATEGORY_EXTERN(LogGameQuest, Log, All);

UENUM()
enum class EGameQuestSequenceLogic
{
	And,
	Or,
};

namespace GameQuest
{
	constexpr uint16 IdNone = 0;
	using FLogicList = TArray<EGameQuestSequenceLogic, TInlineAllocator<4>>;
}

USTRUCT(BlueprintType)
struct GAMEQUESTGRAPH_API FGameQuestFinishEvent
{
	GENERATED_BODY()
public:
	TObjectPtr<UFunction> Event = nullptr;

	static FName MakeEventName(const FName& NodeName, const FName& EventName)
	{
		return *FString::Printf(TEXT("__GQF_%s_%s"), *NodeName.ToString(), *EventName.ToString());
	}
};

USTRUCT(BlueprintType, BlueprintInternalUseOnly)
struct GAMEQUESTGRAPH_API FGameQuestFinishedTag
{
	GENERATED_BODY()
public:
	TObjectPtr<UFunction> Event = nullptr;
	UPROPERTY(Transient)
	uint16 PreSequenceId = GameQuest::IdNone;
	UPROPERTY(Transient)
	uint16 PreBranchId = GameQuest::IdNone;

	static const FName FinishCompletedTagName;
	static FName MakeVariableName(const FName& FinishedTag)
	{
		return *FString::Printf(TEXT("__GQFT_%s"), *FinishedTag.ToString());
	}
	static FName MakeEventName(const FName& NodeName, const FName& FinishedTag)
	{
		return *FString::Printf(TEXT("__GQFT_%s_%s"), *NodeName.ToString(), *FinishedTag.ToString());
	}
};
