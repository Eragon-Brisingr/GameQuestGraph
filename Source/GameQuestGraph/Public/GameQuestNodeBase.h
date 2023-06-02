// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameQuestType.h"
#include "GameQuestNodeBase.generated.h"

class UGameQuestGraphBase;

USTRUCT(BlueprintType, BlueprintInternalUseOnly, meta = (Hidden))
struct GAMEQUESTGRAPH_API FGameQuestNodeBase
{
	GENERATED_BODY()

	friend class UGameQuestGraphBase;
public:
	virtual ~FGameQuestNodeBase() = default;

	virtual UScriptStruct* GetNodeStruct() const { return NodeProperty->Struct; }
	virtual FName GetNodeName() const { return NodeProperty->GetFName(); }

	TObjectPtr<UGameQuestGraphBase> OwnerQuest = nullptr;
	TObjectPtr<UFunction> EvaluateParamsFunction = nullptr;
	void GetEvaluateGraphExposedInputs() const;
	void GetEvaluateGraphExposedInputs(bool bHasAuthority) const;

	static FName MakeEvaluateParamsFunctionName(const FName& NodeName) { return *FString::Printf(TEXT("%s_EvaluateActionParams"), *NodeName.ToString()); }

	void MarkNodeNetDirty() const;
protected:
	virtual void WhenQuestInitProperties(const FStructProperty* Property) {}
	virtual bool ShouldReplicatedSubobject() const { return false; }
	virtual bool ReplicateSubobject(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) { return false; }
	virtual void WhenOnRepValue(const FGameQuestNodeBase& PreValue) {}

#if WITH_EDITOR
	friend class UGameQuestGraphBlueprint;
#endif
private:
	FStructProperty* NodeProperty;
};

template<typename T>
T* GameQuestCast(FGameQuestNodeBase* Node)
{
	if (Node->GetNodeStruct()->IsChildOf(T::StaticStruct()))
	{
		return (T*)Node;
	}
	return nullptr;
}

template<typename T>
const T* GameQuestCast(const FGameQuestNodeBase* Node)
{
	if (Node->GetNodeStruct()->IsChildOf(T::StaticStruct()))
	{
		return (T*)Node;
	}
	return nullptr;
}

template<typename T>
T* GameQuestCastChecked(FGameQuestNodeBase* Node)
{
	check(Node->GetNodeStruct()->IsChildOf(T::StaticStruct()));
	return (T*)Node;
}

template<typename T>
const T* GameQuestCastChecked(const FGameQuestNodeBase* Node)
{
	check(Node->GetNodeStruct()->IsChildOf(T::StaticStruct()));
	return (T*)Node;
}
