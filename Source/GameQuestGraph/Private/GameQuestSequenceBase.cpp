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
	FAddNextSequenceIdFunc AddNextSequenceIdFunc;
	FName PreFinishedTagName;
}

void FGameQuestSequenceBase::TryActivateSequence()
{
	check(OwnerQuest);

	if (!ensure(OwnerQuest->bIsActivated))
	{
		return;
	}

	using namespace Context;
	const uint16 SequenceId = OwnerQuest->GetSequenceId(this);

	if (GetSequenceState() != EState::Deactivated)
	{
		AddNextSequenceIdFunc(SequenceId);
	}
	else if (CurrentFinishedSequenceId != GameQuest::IdNone)
	{
		PreSequence = CurrentFinishedSequenceId;
	}
	AddNextSequenceIdFunc(SequenceId);
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
	check(bInterrupted == false);
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

void FGameQuestSequenceBase::ExecuteFinishEvent(UFunction* FinishEvent, const TArray<uint16>& NextSequenceIds, uint16 BranchId) const
{
	using namespace Context;

	struct FLastFinished
	{
		UGameQuestGraphBase* Quest = nullptr;
		uint16 SequenceId = GameQuest::IdNone;
		uint16 BranchId = GameQuest::IdNone;
	};
	static TArray<FLastFinished, TInlineAllocator<2>> LastFinishedStack;

	const uint16 SequenceId = OwnerQuest->GetSequenceId(this);
	const bool bIsFirstEntryQuest = LastFinishedStack.Num() == 0 || LastFinishedStack.Last().Quest != OwnerQuest;
	if (bIsFirstEntryQuest)
	{
		LastFinishedStack.Push({ OwnerQuest, SequenceId, BranchId });
	}
	else
	{
		LastFinishedStack.Last().SequenceId = SequenceId;
		LastFinishedStack.Last().BranchId = BranchId;
	}

	bool bNextSequenceActivated = false;
	bool bNextHasInterrupted = false;
	if (FinishEvent)
	{
		TGuardValue FinishedSequenceGuard{ CurrentFinishedSequenceId, SequenceId };

		OwnerQuest->ProcessEvent(FinishEvent, nullptr);

		for (const uint16 NextSequenceId : NextSequenceIds)
		{
			FGameQuestSequenceBase* NextSequence = OwnerQuest->GetSequencePtr(NextSequenceId);
			if (NextSequence->bInterrupted)
			{
				bNextHasInterrupted |= true;
			}
			else if (NextSequence->PreSequence == SequenceId)
			{
				NextSequence->ActivateSequence(NextSequenceId);
				bNextSequenceActivated |= NextSequence->bIsActivated;
			}
		}
	}
	if (bIsFirstEntryQuest)
	{
		const FLastFinished LastFinished = LastFinishedStack.Pop();
		if (bNextSequenceActivated == false)
		{
			TGuardValue FinishedSequenceGuard{ CurrentFinishedSequenceId, LastFinished.SequenceId };
			TGuardValue FinishedBranchGuard{ CurrentFinishedBranchId, LastFinished.BranchId };
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
	TGuardValue<Context::FAddNextSequenceIdFunc> AddNextSequenceIdFuncGuard{ Context::AddNextSequenceIdFunc, [this](const uint16 SequenceId)
	{
		NextSequences.Add(SequenceId);
		MarkNodeNetDirty();
	}};
	ExecuteFinishEvent(OnElementFinishedEvent.Event, NextSequences, 0);
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
		TGuardValue<Context::FAddNextSequenceIdFunc> AddNextSequenceIdFuncGuard{ Context::AddNextSequenceIdFunc, [this](const uint16 SequenceId)
		{
			NextSequences.Add(SequenceId);
			MarkNodeNetDirty();
		}};
		ExecuteFinishEvent(OnSequenceFinished, NextSequences, 0);
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
		TGuardValue<Context::FAddNextSequenceIdFunc> AddNextSequenceIdFuncGuard{ Context::AddNextSequenceIdFunc, [this, Branch](const uint16 SequenceId)
		{
			Branch->NextSequences.Add(SequenceId);
			MarkNodeNetDirty();
		}};
		if (Branch->bAutoDeactivateOtherBranch)
		{
			DeactivateSequence(OwnerQuest->GetSequenceId(this));
			ExecuteFinishEvent(OnElementFinishedEvent.Event, Branch->NextSequences, FinishedElementId);
		}
		else
		{
			const bool IsAllBranchElementAllowFinish = !Branches.ContainsByPredicate([&](const FGameQuestSequenceBranchElement& E)->bool
			{
				const FGameQuestElementBase* Element = OwnerQuest->GetElementPtr(E.Element);
				if (Element == FinishedElement)
				{
					return false;
				}
				return Element->bIsActivated;
			});
			if (!IsAllBranchElementAllowFinish)
			{
#if WITH_EDITOR
				TGuardValue<int32> GPlayInEditorIDGuard(GPlayInEditorID, OwnerQuest->GetWorld()->GetOutermost()->GetPIEInstanceID());
#endif
				FGameQuestElementBase* Element = OwnerQuest->GetElementPtr(Branch->Element);
				Element->DeactivateElement(true);
				ExecuteFinishEvent(OnElementFinishedEvent.Event, Branch->NextSequences, FinishedElementId);
			}
			else
			{
				DeactivateSequence(OwnerQuest->GetSequenceId(this));
				ExecuteFinishEvent(OnElementFinishedEvent.Event, Branch->NextSequences, FinishedElementId);
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
			GetEvaluateGraphExposedInputs(bHasAuthority);
			TGuardValue FinishedSequenceGuard{ CurrentFinishedSequenceId, GameQuest::IdNone };
			if (CustomEntryName == NAME_None)
			{
				SubQuestInstance->DefaultEntry();
			}
			else
			{
				UFunction* CustomEntry = SubQuestInstance->FindFunction(CustomEntryName);
				if (ensure(CustomEntry))
				{
					SubQuestInstance->ProcessEvent(CustomEntry, nullptr);
				}
				else
				{
					UE_LOG(LogGameQuest, Error, TEXT("%s can not find custom quest entry %s"), *SubQuestInstance->GetClass()->GetName(), *CustomEntryName.ToString());
				}
			}
			MarkNodeNetDirty();
		};
		if (SubQuestClass.IsPending())
		{
			AsyncLoadHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(SubQuestClass.ToSoftObjectPath(), FStreamableDelegate::CreateWeakLambda(OwnerQuest.Get(), [this, WhenLoadClass]
			{
				AsyncLoadHandle.Reset();
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
	else if (AsyncLoadHandle)
	{
		AsyncLoadHandle->CancelHandle();
		AsyncLoadHandle.Reset();
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
	TGuardValue<Context::FAddNextSequenceIdFunc> AddNextSequenceIdFuncGuard{ Context::AddNextSequenceIdFunc, [this, &SubQuestFinishedTag](const uint16 SequenceId)
	{
		SubQuestFinishedTag.NextSequences.Add(SequenceId);
		MarkNodeNetDirty();
	}};
	ExecuteFinishEvent(FinishedTag.Event, SubQuestFinishedTag.NextSequences, 0);
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
