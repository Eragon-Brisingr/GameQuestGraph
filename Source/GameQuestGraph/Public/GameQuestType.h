// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameQuestType.generated.h"

GAMEQUESTGRAPH_API DECLARE_LOG_CATEGORY_EXTERN(LogGameQuest, Log, All);

class UGameQuestGraphBase;

UENUM()
enum class EGameQuestSequenceLogic : uint8
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
struct GAMEQUESTGRAPH_API FGameQuestRerouteTag
{
	GENERATED_BODY()
public:
	TObjectPtr<UFunction> Event = nullptr;
	UPROPERTY(SaveGame)
	uint16 PreSequenceId = GameQuest::IdNone;
	UPROPERTY(SaveGame)
	uint16 PreBranchId = GameQuest::IdNone;

	static const FName FinishCompletedTagName;
	static FName MakeVariableName(const FName& RerouteTag)
	{
		return *FString::Printf(TEXT("__GQRT_%s"), *RerouteTag.ToString());
	}
	static FName MakeEventName(const FName& NodeName, const FName& RerouteTag)
	{
		return *FString::Printf(TEXT("__GQRT_%s_%s"), *NodeName.ToString(), *RerouteTag.ToString());
	}
};

struct FGameQuestSequenceBase;

USTRUCT(BlueprintType)
struct GAMEQUESTGRAPH_API FGameQuestSequencePtr
{
	GENERATED_BODY()
public:
	FGameQuestSequencePtr() = default;
	FGameQuestSequencePtr(FGameQuestSequenceBase& Sequence);

	TWeakObjectPtr<UGameQuestGraphBase> Owner;
	FGameQuestSequenceBase* SequencePtr = nullptr;

	operator bool() const { return Owner.IsValid(); }
	friend bool operator==(const FGameQuestSequencePtr& LHS, const FGameQuestSequencePtr& RHS) { return LHS.SequencePtr == RHS.SequencePtr; }
	friend uint32 GetTypeHash(const FGameQuestSequencePtr& Ptr) { return GetTypeHash(Ptr.SequencePtr); }
	FGameQuestSequenceBase* operator->() const { return SequencePtr; }
	FGameQuestSequenceBase* operator*() const { return SequencePtr; }
};

struct FGameQuestElementBase;

USTRUCT(BlueprintType)
struct GAMEQUESTGRAPH_API FGameQuestElementPtr
{
	GENERATED_BODY()
public:
	FGameQuestElementPtr() = default;
	FGameQuestElementPtr(FGameQuestElementBase& Element);

	TWeakObjectPtr<UGameQuestGraphBase> Owner;
	FGameQuestElementBase* ElementPtr = nullptr;

	operator bool() const { return Owner.IsValid(); }
	friend bool operator==(const FGameQuestElementPtr& LHS, const FGameQuestElementPtr& RHS) { return LHS.ElementPtr == RHS.ElementPtr; }
	friend uint32 GetTypeHash(const FGameQuestElementPtr& Ptr) { return GetTypeHash(Ptr.ElementPtr); }
	FGameQuestElementBase* operator->() const { return ElementPtr; }
	FGameQuestElementBase* operator*() const { return ElementPtr; }
};
