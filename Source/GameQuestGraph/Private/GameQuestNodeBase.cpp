// Fill out your copyright notice in the Description page of Project Settings.


#include "GameQuestNodeBase.h"

#include "GameQuestGraphBase.h"
#include "Net/NetPushModelHelpers.h"

void FGameQuestNodeBase::GetEvaluateGraphExposedInputs() const
{
	GetEvaluateGraphExposedInputs(OwnerQuest->HasAuthority());
}

void FGameQuestNodeBase::GetEvaluateGraphExposedInputs(bool bHasAuthority) const
{
	if (EvaluateParamsFunction)
	{
		OwnerQuest->ProcessEvent(EvaluateParamsFunction, &bHasAuthority);
		MarkNodeNetDirty();
	}
}

void FGameQuestNodeBase::MarkNodeNetDirty() const
{
	if (NodeProperty == nullptr)
	{
		return;
	}
	UNetPushModelHelpers::MarkPropertyDirtyFromRepIndex(OwnerQuest, NodeProperty->RepIndex, NodeProperty->GetFName());
}
