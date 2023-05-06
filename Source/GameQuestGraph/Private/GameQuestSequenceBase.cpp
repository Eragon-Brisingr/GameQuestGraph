// Fill out your copyright notice in the Description page of Project Settings.


#include "GameQuestSequenceBase.h"

#include "GameQuestElementBase.h"
#include "GameQuestGraphBase.h"
#include "Engine/ActorChannel.h"
#include "Engine/AssetManager.h"

namespace Context
{
	// For finish quest element context
	uint16 CurrentFinishedSequenceId;
	uint16 CurrentFinishedBranchId;
	FSequenceIdList CurrentNextSequenceIds;
	FName PreFinishedTagName;
	bool bIsFirstStack = true;
}

void FGameQuestSequenceBase::TryActivateSequence()
{
	check(OwnerQuest);

	using namespace Context;
	const uint16 SequenceId = OwnerQuest->GetSequenceId(this);
	if (GetSequenceState() != EState::Deactivated)
	{
		CurrentNextSequenceIds.AddUnique({ this, SequenceId });
		return;
	}

	if (CurrentFinishedSequenceId != GameQuest::IdNone)
	{
		PreSequence = CurrentFinishedSequenceId;
		CurrentNextSequenceIds.AddUnique({ this, SequenceId });
	}
	else
	{
		check(OwnerQuest->StartSequences.Num() == 0);
		CurrentNextSequenceIds.AddUnique({ this, SequenceId });
	}
}

void FGameQuestSequenceBase::ActivateSequence(uint16 SequenceId)
{
	ActivateSequenceImpl<true>(SequenceId);
}

void FGameQuestSequenceBase::DeactivateSequence(uint16 SequenceId)
{
	DeactivateSequenceImpl<true>(SequenceId);
}

void FGameQuestSequenceBase::OnRepActivateSequence(uint16 SequenceId)
{
	ActivateSequenceImpl<false>(SequenceId);
}

void FGameQuestSequenceBase::OnRepDeactivateSequence(uint16 SequenceId)
{
	DeactivateSequenceImpl<false>(SequenceId);
}

template <bool bHasAuthority>
void FGameQuestSequenceBase::ActivateSequenceImpl(uint16 SequenceId)
{
#if WITH_EDITOR
	TGuardValue<int32> GPlayInEditorIDGuard(GPlayInEditorID, OwnerQuest->GetWorld()->GetOutermost()->GetPIEInstanceID());
#endif

	check(bIsActivated == false);
	bIsActivated = true;
	if constexpr (bHasAuthority)
	{
		UE_LOG(LogGameQuest, Verbose, TEXT("ActivateSequence %s.%s"), *OwnerQuest->GetName(), *GetNodeName().ToString());
		check(OwnerQuest->ActivatedSequences.Contains(SequenceId) == false);
		OwnerQuest->ActivatedSequences.Add(SequenceId);
		MARK_PROPERTY_DIRTY_FROM_NAME(UGameQuestGraphBase, ActivatedSequences, OwnerQuest);
		if (ShouldReplicatedSubobject())
		{
			OwnerQuest->ReplicateSubobjectNodes.Add(this);
		}
	}
	else
	{
		UE_LOG(LogGameQuest, Verbose, TEXT("OnRepActivateSequence %s.%s"), *OwnerQuest->GetName(), *GetNodeName().ToString());
	}
	if (IsTickable())
	{
		OwnerQuest->TickableSequences.Add(this);
	}
	GetEvaluateGraphExposedInputs(bHasAuthority);
	OwnerQuest->PreSequenceActivated(this, SequenceId);
	WhenSequenceActivated(bHasAuthority);
}

template <bool bHasAuthority>
void FGameQuestSequenceBase::DeactivateSequenceImpl(uint16 SequenceId)
{
#if WITH_EDITOR
	TGuardValue<int32> GPlayInEditorIDGuard(GPlayInEditorID, OwnerQuest->GetWorld()->GetOutermost()->GetPIEInstanceID());
#endif

	check(bIsActivated);
	bIsActivated = false;
	if constexpr (bHasAuthority)
	{
		check(OwnerQuest->ActivatedSequences.Contains(SequenceId));
		OwnerQuest->ActivatedSequences.Remove(SequenceId);
		MARK_PROPERTY_DIRTY_FROM_NAME(UGameQuestGraphBase, ActivatedSequences, OwnerQuest);
	}
	if (IsTickable())
	{
		OwnerQuest->TickableSequences.Remove(this);
	}
	WhenSequenceDeactivated(bHasAuthority);
	OwnerQuest->PostSequenceDeactivated(this, SequenceId);
	if constexpr (bHasAuthority)
	{
		UE_LOG(LogGameQuest, Verbose, TEXT("DeactivateSequence %s.%s"), *OwnerQuest->GetName(), *GetNodeName().ToString());
	}
	else
	{
		UE_LOG(LogGameQuest, Verbose, TEXT("OnRepDeactivateSequence %s.%s"), *OwnerQuest->GetName(), *GetNodeName().ToString());
	}
}

void FGameQuestSequenceBase::InterruptSequence()
{
	if (!ensure(bInterrupted == false))
	{
		return;
	}
	bInterrupted = true;
	MarkNodeNetDirty();
	if (bIsActivated)
	{
		DeactivateSequence(OwnerQuest->GetSequenceId(this));
		OwnerQuest->InvokeInterruptQuest();
	}
}

FGameQuestSequenceBase::EState FGameQuestSequenceBase::GetSequenceState() const
{
	if (bIsActivated)
	{
		return EState::Activated;
	}
	if (bInterrupted)
	{
		return EState::Interrupted;
	}
	if (PreSequence == GameQuest::IdNone)
	{
		const uint16 SequenceId = OwnerQuest->GetSequenceId(this);
		if (OwnerQuest->StartSequences.Contains(SequenceId) == false)
		{
			return EState::Deactivated;
		}
		if (OwnerQuest->ActivatedSequences.Contains(SequenceId))
		{
			return EState::Deactivated;
		}
	}
	return EState::Finished;
}

template<typename TFunc>
void FGameQuestSequenceBase::ExecuteFinishEvent(UFunction* FinishEvent, const TFunc& LinkNextSequenceFunc)
{
	using namespace Context;
	bool bNextSequenceActivated = false;
	bool bNextHasInterrupted = false;
	const uint16 SequenceId = OwnerQuest->GetSequenceId(this);
	TGuardValue FinishedSequenceGuard{ CurrentFinishedSequenceId, SequenceId };
	if (FinishEvent)
	{
		FSequenceIdList& NextSequenceList = CurrentNextSequenceIds;
		TGuardValue NextSequencesGuard{ NextSequenceList, FSequenceIdList{} };

		TGuardValue bIsFirstStackGuard{ bIsFirstStack, false };
		OwnerQuest->ProcessEvent(FinishEvent, nullptr);

		if (NextSequenceList.Num() > 0)
		{
			LinkNextSequenceFunc(NextSequenceList);

			for (const FSequenceId& NextSequence : NextSequenceList)
			{
				bNextHasInterrupted |= NextSequence.Sequence->bInterrupted;
				if (NextSequence.Sequence->PreSequence == SequenceId)
				{
					NextSequence.Sequence->ActivateSequence(NextSequence.Id);
					bNextSequenceActivated |= NextSequence.Sequence->bIsActivated;
				}
			}
		}
	}
	if (bIsFirstStack && bNextSequenceActivated == false)
	{
		if (bNextHasInterrupted)
		{
			OwnerQuest->InvokeInterruptQuest();
		}
		else
		{
			OwnerQuest->InvokeFinishQuest();
		}
	}
}

bool FGameQuestSequenceBase::CanFinishListElements(const TArray<uint16>& Elements, const GameQuest::FLogicList& ElementLogics) const
{
	if (ensure(Elements.Num() != 0) == false)
	{
		return true;
	}
	const FGameQuestElementBase* FirstElement = OwnerQuest->GetElementPtr(Elements[0]);
	bool CheckFlag = FirstElement->bIsOptional ? true : FirstElement->bIsFinished;
	if (Elements.Num() == 1)
	{
		return CheckFlag;
	}
	for (int32 Idx = 1; Idx < Elements.Num(); ++Idx)
	{
		const EGameQuestSequenceLogic Logic = ElementLogics[Idx - 1];
		if (Logic == EGameQuestSequenceLogic::Or)
		{
			if (CheckFlag)
			{
				return true;
			}
			else
			{
				CheckFlag = true;
			}
		}
		const FGameQuestElementBase* Element = OwnerQuest->GetElementPtr(Elements[Idx]);
		CheckFlag &= Element->bIsOptional ? true : Element->bIsFinished;
	}
	return CheckFlag;
}

void FGameQuestSequenceSingle::WhenSequenceActivated(bool bHasAuthority)
{
	FGameQuestElementBase* ElementPtr = OwnerQuest->GetElementPtr(Element);
	ElementPtr->GetEvaluateGraphExposedInputs(bHasAuthority);
	ElementPtr->ActivateElement(bHasAuthority);
}

void FGameQuestSequenceSingle::WhenSequenceDeactivated(bool bHasAuthority)
{
	FGameQuestElementBase* ElementPtr = OwnerQuest->GetElementPtr(Element);
	if (ElementPtr->bIsActivated)
	{
		ElementPtr->DeactivateElement(bHasAuthority);
	}
}

void FGameQuestSequenceSingle::WhenElementFinished(FGameQuestElementBase* FinishedElement, const FGameQuestFinishEvent& OnElementFinishedEvent)
{
	DeactivateSequence(OwnerQuest->GetSequenceId(this));
	ExecuteFinishEvent(OnElementFinishedEvent.Event, [this](const Context::FSequenceIdList& InNextSequences)
	{
		if (InNextSequences.Num() == 0)
		{
			return;
		}
		for (const Context::FSequenceId& SequenceId : InNextSequences)
		{
			NextSequences.Add(SequenceId.Id);
		}
		MarkNodeNetDirty();
	});
}

void FGameQuestSequenceList::WhenQuestInitProperties(const FStructProperty* Property)
{
	const UClass* QuestClass = OwnerQuest->GetClass();
	OnSequenceFinished = QuestClass->FindFunctionByName(FGameQuestFinishEvent::MakeEventName(Property->GetFName(), GET_MEMBER_NAME_CHECKED(FGameQuestSequenceList, OnSequenceFinished)));
}

void FGameQuestSequenceList::WhenSequenceActivated(bool bHasAuthority)
{
	for (const uint16 ElementId : Elements)
	{
		FGameQuestElementBase* ElementPtr = OwnerQuest->GetElementPtr(ElementId);
		ElementPtr->GetEvaluateGraphExposedInputs(bHasAuthority);
		if (bIsActivated)
		{
			ElementPtr->ActivateElement(bHasAuthority);
		}
	}
	check(bIsActivated || Elements.ContainsByPredicate([this](uint16 Id) { return OwnerQuest->GetElementPtr(Id)->bIsActivated; }) == false);
}

void FGameQuestSequenceList::WhenSequenceDeactivated(bool bHasAuthority)
{
	for (const uint16 ElementId : Elements)
	{
		FGameQuestElementBase* Element = OwnerQuest->GetElementPtr(ElementId);
		if (Element->bIsActivated)
		{
			Element->DeactivateElement(bHasAuthority);
		}
	}
}

void FGameQuestSequenceList::WhenElementFinished(FGameQuestElementBase* FinishedElement, const FGameQuestFinishEvent& OnElementFinishedEvent)
{
	if (CanFinishListElements(Elements, *OwnerQuest->GetLogicList(this)))
	{
		DeactivateSequence(OwnerQuest->GetSequenceId(this));
		ExecuteFinishEvent(OnSequenceFinished, [this](const Context::FSequenceIdList& InNextSequences)
		{
			if (InNextSequences.Num() == 0)
			{
				return;
			}
			for (const Context::FSequenceId& SequenceId : InNextSequences)
			{
				NextSequences.Add(SequenceId.Id);
			}
			MarkNodeNetDirty();
		});
	}
}

TArray<uint16> FGameQuestSequenceBranch::GetNextSequences() const
{
	TArray<uint16> NextSequences;
	for (const FGameQuestSequenceBranchElement& Branch : Branches)
	{
		NextSequences.Append(Branch.NextSequences);
	}
	return NextSequences;
}

void FGameQuestSequenceBranch::WhenSequenceActivated(bool bHasAuthority)
{
	for (const uint16 ElementId : Elements)
	{
		FGameQuestElementBase* ElementPtr = OwnerQuest->GetElementPtr(ElementId);
		ElementPtr->GetEvaluateGraphExposedInputs(bHasAuthority);
		if (bIsActivated)
		{
			ElementPtr->ActivateElement(bHasAuthority);
		}
	}
	check(bIsActivated || Elements.ContainsByPredicate([this](uint16 Id) { return OwnerQuest->GetElementPtr(Id)->bIsActivated; }) == false);
	if (bIsActivated && bIsBranchesActivated == false && CanActivateBranchElement())
	{
		ActivateBranches(bHasAuthority);
	}
}

void FGameQuestSequenceBranch::WhenSequenceDeactivated(bool bHasAuthority)
{
	for (const uint16 ElementId : Elements)
	{
		FGameQuestElementBase* Element = OwnerQuest->GetElementPtr(ElementId);
		if (Element->bIsActivated)
		{
			Element->DeactivateElement(bHasAuthority);
		}
	}
	if (bIsBranchesActivated)
	{
		DeactivateBranches(bHasAuthority);
	}
}

void FGameQuestSequenceBranch::WhenElementFinished(FGameQuestElementBase* FinishedElement, const FGameQuestFinishEvent& OnElementFinishedEvent)
{
	const uint16 FinishedElementId = OwnerQuest->GetElementId(FinishedElement);
	if (Elements.Contains(FinishedElementId))
	{
		if (bIsBranchesActivated == false && CanActivateBranchElement())
		{
			ActivateBranches(true);
		}
	}
	else if (FGameQuestSequenceBranchElement* Branch = Branches.FindByPredicate([&](const FGameQuestSequenceBranchElement& E) { return E.Element == FinishedElementId; }))
	{
		auto LinkNextSequenceFunc = [&](const Context::FSequenceIdList& InNextSequences)
		{
			if (InNextSequences.Num() == 0)
			{
				return;
			}
			for (const Context::FSequenceId& SequenceId : InNextSequences)
			{
				Branch->NextSequences.Add(SequenceId.Id);
			}
			MarkNodeNetDirty();
		};
		TGuardValue CurrentFinishedBranchIdGuard{ Context::CurrentFinishedBranchId, FinishedElementId };
		if (Branch->bAutoDeactivateOtherBranch)
		{
			DeactivateSequence(OwnerQuest->GetSequenceId(this));
			ExecuteFinishEvent(OnElementFinishedEvent.Event, LinkNextSequenceFunc);
		}
		else
		{
			const bool IsAllBranchElementAllowFinish = Branches.ContainsByPredicate([this](const FGameQuestSequenceBranchElement& E) { return OwnerQuest->GetElementPtr(E.Element)->bIsActivated; }) == false;
			if (!IsAllBranchElementAllowFinish)
			{
#if WITH_EDITOR
				TGuardValue<int32> GPlayInEditorIDGuard(GPlayInEditorID, OwnerQuest->GetWorld()->GetOutermost()->GetPIEInstanceID());
#endif
				FGameQuestElementBase* Element = OwnerQuest->GetElementPtr(Branch->Element);
				Element->DeactivateElement(true);
				ExecuteFinishEvent(OnElementFinishedEvent.Event, LinkNextSequenceFunc);
			}
			else
			{
				DeactivateSequence(OwnerQuest->GetSequenceId(this));
				ExecuteFinishEvent(OnElementFinishedEvent.Event, LinkNextSequenceFunc);
			}
		}
	}
	else if (const FGameQuestSequenceBranchElement* ListBranch = Branches.FindByPredicate([&](const FGameQuestSequenceBranchElement& E)
		{
			const FGameQuestElementBase* Element = OwnerQuest->GetElementPtr(E.Element);
			if (const FGameQuestElementBranchList* BranchList = GameQuestCast<FGameQuestElementBranchList>(Element))
			{
				return BranchList->Elements.Contains(FinishedElementId);
			}
			return false;
		}))
	{
		FGameQuestElementBranchList* BranchList = GameQuestCastChecked<FGameQuestElementBranchList>(OwnerQuest->GetElementPtr(ListBranch->Element));
		if (CanFinishListElements(BranchList->Elements, *OwnerQuest->GetLogicList(BranchList)))
		{
			BranchList->FinishElement(BranchList->OnListFinished, GET_MEMBER_NAME_CHECKED(FGameQuestElementBranchList, OnListFinished));
		}
	}
	else
	{
		checkNoEntry();
	}
}

void FGameQuestSequenceBranch::WhenElementUnfinished(FGameQuestElementBase* FinishedElement)
{
	if (bIsBranchesActivated && CanActivateBranchElement() == false)
	{
		DeactivateBranches(true);
	}
}

bool FGameQuestSequenceBranch::CanActivateBranchElement() const
{
	if (Elements.Num() == 0)
	{
		return true;
	}
	return CanFinishListElements(Elements, *OwnerQuest->GetLogicList(this));
}

void FGameQuestSequenceBranch::ActivateBranches(bool bHasAuthority)
{
	check(bIsBranchesActivated == false);
	check(CanActivateBranchElement());

	bIsBranchesActivated = true;
	OwnerQuest->ActivatedBranches.Add(OwnerQuest->GetSequenceId(this));
	MARK_PROPERTY_DIRTY_FROM_NAME(UGameQuestGraphBase, ActivatedBranches, OwnerQuest);
	UE_LOG(LogGameQuest, Verbose, TEXT("ActivateBranches %s.%s"), *OwnerQuest->GetName(), *GetNodeName().ToString());
	for (const FGameQuestSequenceBranchElement& Branch : Branches)
	{
		if (Branch.bInterrupted)
		{
			continue;
		}
		FGameQuestElementBase* Element = OwnerQuest->GetElementPtr(Branch.Element);
		Element->GetEvaluateGraphExposedInputs(bHasAuthority);
		if (bIsBranchesActivated && Element->bIsFinished == false)
		{
			Element->ActivateElement(bHasAuthority);
		}
	}
	check(bIsBranchesActivated || Branches.ContainsByPredicate([this](const FGameQuestSequenceBranchElement& E) { return OwnerQuest->GetElementPtr(E.Element)->bIsActivated; }) == false);
}

void FGameQuestSequenceBranch::DeactivateBranches(bool bHasAuthority)
{
	check(bIsBranchesActivated);
	bIsBranchesActivated = false;
	OwnerQuest->ActivatedBranches.Remove(OwnerQuest->GetSequenceId(this));
	MARK_PROPERTY_DIRTY_FROM_NAME(UGameQuestGraphBase, ActivatedBranches, OwnerQuest);
	for (const FGameQuestSequenceBranchElement& Branch : Branches)
	{
		if (Branch.bInterrupted)
		{
			continue;
		}
		FGameQuestElementBase* Element = OwnerQuest->GetElementPtr(Branch.Element);
		if (Element->bIsActivated)
		{
			Element->DeactivateElement(bHasAuthority);
		}
	}
	UE_LOG(LogGameQuest, Verbose, TEXT("DeactivateBranches %s.%s"), *OwnerQuest->GetName(), *GetNodeName().ToString());
}

bool FGameQuestSequenceSubQuest::ReplicateSubobject(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool WroteSomething = false;
	if (SubQuestInstance)
	{
		WroteSomething |= Channel->ReplicateSubobject(SubQuestInstance, *Bunch, *RepFlags);
		WroteSomething |= SubQuestInstance->ReplicateSubobject(Channel, Bunch, RepFlags);
	}
	return WroteSomething;
}

void FGameQuestSequenceSubQuest::WhenSequenceActivated(bool bHasAuthority)
{
	if (bHasAuthority == false)
	{
		return;
	}

	if (!ensure(SubQuestClass.IsNull() == false))
	{
		return;
	}

	if (SubQuestInstance)
	{
		SubQuestInstance->ReactiveQuest();
	}
	else
	{
		auto WhenLoadClass = [this, bHasAuthority]
		{
			const TSubclassOf<UGameQuestGraphBase> QuestClass = SubQuestClass.Get();
			if (!ensure(QuestClass))
			{
				return;
			}
			SubQuestInstance = NewObject<UGameQuestGraphBase>(OwnerQuest, QuestClass);
			SubQuestInstance->Owner = OwnerQuest;
			SubQuestInstance->OwnerNode = this;
			SubQuestInstance->BindingFinishedTags();
			using namespace Context;
			TGuardValue FinishedSequenceGuard{ CurrentFinishedSequenceId, GameQuest::IdNone };
			TGuardValue CurrentFinishedBranchIdGuard{ Context::CurrentFinishedBranchId, GameQuest::IdNone };
			TGuardValue NextSequencesGuard{ CurrentNextSequenceIds, FSequenceIdList{} };
			GetEvaluateGraphExposedInputs(bHasAuthority);
			SubQuestInstance->DefaultEntry();
			MarkNodeNetDirty();
		};
		if (SubQuestClass.IsPending())
		{
			UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(SubQuestClass.ToSoftObjectPath(), FStreamableDelegate::CreateWeakLambda(OwnerQuest.Get(), [WhenLoadClass]
			{
				WhenLoadClass();
			}));
		}
		else
		{
			WhenLoadClass();
		}
	}
}

void FGameQuestSequenceSubQuest::WhenSequenceDeactivated(bool bHasAuthority)
{
	if (bHasAuthority == false)
	{
		if (SubQuestInstance)
		{
			SubQuestInstance->bIsActivated = false;
		}
		return;
	}

	if (SubQuestInstance && SubQuestInstance->bIsActivated)
	{
		SubQuestInstance->DeactivateQuest();
	}
}

void FGameQuestSequenceSubQuest::WhenOnRepValue(const FGameQuestNodeBase& PreValue)
{
	const FGameQuestSequenceSubQuest& PreValueImpl = static_cast<const FGameQuestSequenceSubQuest&>(PreValue);
	if (SubQuestInstance != PreValueImpl.SubQuestInstance)
	{
		SubQuestInstance->bIsActivated = true;
		SubQuestInstance->OwnerNode = this;
	}
}

void FGameQuestSequenceSubQuest::WhenTick(float DeltaSeconds)
{
	if (SubQuestInstance && ensure(SubQuestInstance->bIsActivated))
	{
		SubQuestInstance->Tick(DeltaSeconds);
	}
}

TArray<uint16> FGameQuestSequenceSubQuest::GetNextSequences() const
{
	TArray<uint16> NextSequences;
	for (const FGameQuestSequenceSubQuestFinishedTag& FinishedTag : FinishedTags)
	{
		NextSequences.Append(FinishedTag.NextSequences);
	}
	return NextSequences;
}

void FGameQuestSequenceSubQuest::ProcessFinishedTag(const FName& FinishedTagName, const FGameQuestFinishedTag& FinishedTag)
{
	check(FinishedTags.ContainsByPredicate([&](const FGameQuestSequenceSubQuestFinishedTag& E){ return E.TagName == FinishedTagName; }) == false);
	UE_LOG(LogGameQuest, Verbose, TEXT("SubQuestProcessFinishedTag %s.%s.%s"), *GetNodeName().ToString(), *SubQuestInstance->GetName(), *FinishedTagName.ToString());
	FGameQuestSequenceSubQuestFinishedTag& SubQuestFinishedTag = FinishedTags.Add_GetRef({ FinishedTagName, FinishedTag.PreSequenceId, FinishedTag.PreBranchId, Context::PreFinishedTagName });
	TGuardValue PreFinishedTagNameGuard{ Context::PreFinishedTagName, FinishedTagName };
	ExecuteFinishEvent(FinishedTag.Event, [this, &SubQuestFinishedTag](const Context::FSequenceIdList& InNextSequences)
	{
		if (InNextSequences.Num() == 0)
		{
			return;
		}
		for (const Context::FSequenceId& SequenceId : InNextSequences)
		{
			SubQuestFinishedTag.NextSequences.Add(SequenceId.Id);
		}
	});
	MarkNodeNetDirty();
}

void FGameQuestSequenceSubQuest::WhenSubQuestFinished()
{
	DeactivateSequence(OwnerQuest->GetSequenceId(this));
	if (UFunction* FinishCompletedEvent = OwnerQuest->GetClass()->FindFunctionByName(FGameQuestFinishedTag::MakeEventName(GetNodeName(), FGameQuestFinishedTag::FinishCompletedTagName)))
	{
		ProcessFinishedTag(FGameQuestFinishedTag::FinishCompletedTagName, { FinishCompletedEvent, Context::CurrentFinishedSequenceId, Context::CurrentFinishedBranchId });
	}
	OwnerQuest->InvokeFinishQuest();
}

void FGameQuestSequenceSubQuest::WhenSubQuestInterrupted()
{
	bInterrupted = true;
	MarkNodeNetDirty();
	DeactivateSequence(OwnerQuest->GetSequenceId(this));
	OwnerQuest->InvokeInterruptQuest();
}
