// Fill out your copyright notice in the Description page of Project Settings.


#include "GameQuestComponent.h"

#include "GameQuestGraphBase.h"
#include "GameQuestSequenceBase.h"
#include "Engine/ActorChannel.h"
#include "Net/UnrealNetwork.h"

UGameQuestComponent::UGameQuestComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UGameQuestComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	for (int32 Idx = ActivatedQuests.Num() - 1; Idx >= 0 && Idx < ActivatedQuests.Num(); --Idx)
	{
		UGameQuestGraphBase* Quest = ActivatedQuests[Idx];
		Quest->Tick(DeltaTime);
	}
}

void UGameQuestComponent::Activate(bool bReset)
{
	const bool bShouldActivate = ShouldActivate();
	Super::Activate(bReset);

	if (bShouldActivate)
	{
		for (UGameQuestGraphBase* Quest : ActivatedQuests)
		{
			Quest->ReactiveQuest();
		}
	}
}

void UGameQuestComponent::Deactivate()
{
	if (ShouldActivate() == false)
	{
		for (UGameQuestGraphBase* Quest : ActivatedQuests)
		{
			Quest->DeactivateQuest();
		}
	}
	Super::Deactivate();
}

void UGameQuestComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, ActivatedQuests, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, FinishedQuests, SharedParams);
}

bool UGameQuestComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool WroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	for (UGameQuestGraphBase* GameQuest : ActivatedQuests)
	{
		if (GameQuest)
		{
			WroteSomething |= Channel->ReplicateSubobject(GameQuest, *Bunch, *RepFlags);
			WroteSomething |= GameQuest->ReplicateSubobject(Channel, Bunch, RepFlags);
		}
	}

	for (UGameQuestGraphBase* GameQuest : FinishedQuests)
	{
		if (GameQuest)
		{
			WroteSomething |= Channel->ReplicateSubobject(GameQuest, *Bunch, *RepFlags);
			WroteSomething |= GameQuest->ReplicateSubobject(Channel, Bunch, RepFlags);
		}
	}

	return WroteSomething;
}

void UGameQuestComponent::OnRep_ActivatedQuests()
{
	TSet<TObjectPtr<UGameQuestGraphBase>> ActivatedQuestsSet{ ActivatedQuests };
	for (TObjectPtr<UGameQuestGraphBase>& AddGameQuest : ActivatedQuestsSet.Difference(PreActivatedQuests))
	{
		if (ensure(AddGameQuest))
		{
			AddGameQuest->bIsActivated = true;
			WhenQuestActivated(AddGameQuest);
		}
	}
	for (TObjectPtr<UGameQuestGraphBase> RemovedGameQuest : PreActivatedQuests.Difference(ActivatedQuestsSet))
	{
		if (ensure(RemovedGameQuest))
		{
			RemovedGameQuest->bIsActivated = false;
			WhenQuestDeactivated(RemovedGameQuest);
		}
	}
	PreActivatedQuests = ::MoveTemp(ActivatedQuestsSet);
}

void UGameQuestComponent::OnRep_FinishedQuests()
{
	TSet<TObjectPtr<UGameQuestGraphBase>> FinishedQuestsSet{ FinishedQuests };
	for (UGameQuestGraphBase* FinishGameQuest : FinishedQuestsSet.Difference(PreFinishedQuests))
	{
		if (ensure(FinishGameQuest))
		{
			WhenFinishedQuestAdded(FinishGameQuest);
		}
	}
	for (UGameQuestGraphBase* RemoveGameQuest : PreFinishedQuests.Difference(FinishedQuestsSet))
	{
		if (ensure(RemoveGameQuest))
		{
			WhenFinishedQuestRemoved(RemoveGameQuest);
		}
	}
	PreFinishedQuests = ::MoveTemp(FinishedQuestsSet);
}

void UGameQuestComponent::PostStartQuest(UGameQuestGraphBase* StartedQuest)
{
	WhenQuestStarted(StartedQuest);
	WhenQuestActivated(StartedQuest);
}

void UGameQuestComponent::PostFinishQuest(UGameQuestGraphBase* FinishedQuest)
{
	WhenQuestDeactivated(FinishedQuest);

	ActivatedQuests.RemoveSingle(FinishedQuest);
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ActivatedQuests, this);
	FinishedQuests.Add(FinishedQuest);
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, FinishedQuests, this);

	WhenQuestFinished(FinishedQuest);
}

void UGameQuestComponent::AddQuest(UGameQuestGraphBase* Quest, bool AutoActivate)
{
	if (!ensure(Quest && Quest->GetOuter() == this))
	{
		return;
	}
	if (!ensure(Quest->Owner == nullptr))
	{
		return;
	}
	Quest->Owner = this;
	switch (const UGameQuestGraphBase::EState State = Quest->GetQuestState())
	{
	case UGameQuestGraphBase::EState::Unactivated:
		ActivatedQuests.Add(Quest);
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ActivatedQuests, this);
		if (AutoActivate)
		{
			Quest->DefaultEntry();
		}
		break;
	case UGameQuestGraphBase::EState::Deactivated:
		ActivatedQuests.Add(Quest);
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ActivatedQuests, this);
		if (AutoActivate)
		{
			Quest->ReactiveQuest();
		}
		break;
	case UGameQuestGraphBase::EState::Finished:
		FinishedQuests.Add(Quest);
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, FinishedQuests, this);
		WhenFinishedQuestAdded(Quest);
		break;
	default: ;
	}
}

void UGameQuestComponent::RemoveQuest(UGameQuestGraphBase* Quest)
{
	if (!ensure(Quest->Owner == this))
	{
		return;
	}
	int32 Idx = ActivatedQuests.IndexOfByKey(Quest);
	if (Idx != INDEX_NONE)
	{
		ActivatedQuests.RemoveAt(Idx);
		Quest->DeactivateQuest();
	}
	else
	{
		Idx = FinishedQuests.IndexOfByKey(Quest);
		FinishedQuests.RemoveAt(Idx);
		WhenFinishedQuestRemoved(Quest);
	}
	Quest->Owner = nullptr;
}
