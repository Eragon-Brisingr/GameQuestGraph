// Fill out your copyright notice in the Description page of Project Settings.


#include "GameQuestGraphBase.h"

#include "GameQuestComponent.h"
#include "GameQuestElementBase.h"
#include "GameQuestGraphBlueprint.h"
#include "GameQuestNodeBase.h"
#include "GameQuestSequenceBase.h"
#include "Net/UnrealNetwork.h"

TAutoConsoleVariable<bool> CVarGameQuestEnableCheat
{
	TEXT("GameQuest.EnableCheat"),
#if UE_BUILD_SHIPPING
	false,
#else
	true,
#endif
	TEXT("Enable cheat, e.g. console command finish quest element")
};

void UGameQuestGraphBase::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		if (UGameQuestGraphGeneratedClass* Class = Cast<UGameQuestGraphGeneratedClass>(GetClass()))
		{
			Class->PostQuestCDOInitProperties();
		}
	}
	UClass* Class = GetClass();
	for (TFieldIterator<FStructProperty> It{ Class }; It; ++It)
	{
		if (It->Struct->IsChildOf(FGameQuestNodeBase::StaticStruct()) == false)
		{
			continue;
		}
		FGameQuestNodeBase* QuestNode = It->ContainerPtrToValuePtr<FGameQuestNodeBase>(this);
		QuestNode->NodeProperty = *It;
		QuestNode->OwnerQuest = this;
		QuestNode->EvaluateParamsFunction = Class->FindFunctionByName(FGameQuestNodeBase::MakeEvaluateParamsFunctionName(It->GetFName()));
		QuestNode->WhenQuestInitProperties(*It);
	}
}

UWorld* UGameQuestGraphBase::GetWorld() const
{
#if WITH_EDITOR
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}
#endif
	return Super::GetWorld();
}

void UGameQuestGraphBase::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	if (const UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(GetClass()))
	{
		BPClass->GetLifetimeBlueprintReplicationList(OutLifetimeProps);
	}

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, bInterrupted, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, Owner, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, StartSequences, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, ActivatedSequences, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, ActivatedBranches, SharedParams);
}

int32 UGameQuestGraphBase::GetFunctionCallspace(UFunction* Function, FFrame* Stack)
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return FunctionCallspace::Local;
	}

	AActor* OwningActor = GetTypedOuter<AActor>();
	check(OwningActor);
	return OwningActor->GetFunctionCallspace(Function, Stack);
}

bool UGameQuestGraphBase::CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack)
{
	bool bProcessed = false;

	AActor* OwningActor = GetTypedOuter<AActor>();
	check(OwningActor);
	FWorldContext* const Context = GEngine->GetWorldContextFromWorld(GetWorld());
	if (Context != nullptr)
	{
		for (FNamedNetDriver& Driver : Context->ActiveNetDrivers)
		{
			if (Driver.NetDriver != nullptr && Driver.NetDriver->ShouldReplicateFunction(OwningActor, Function))
			{
				Driver.NetDriver->ProcessRemoteFunction(OwningActor, Function, Parameters, OutParms, Stack, this);
				bProcessed = true;
			}
		}
	}
	return bProcessed;
}

bool UGameQuestGraphBase::ReplicateSubobject(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool WroteSomething = false;
	for (FGameQuestNodeBase* ReplicateSubobjectNode : ReplicateSubobjectNodes)
	{
		WroteSomething |= ReplicateSubobjectNode->ReplicateSubobject(Channel, Bunch, RepFlags);
	}
	return WroteSomething;
}

void UGameQuestGraphBase::OnRep_ActivatedSequences()
{
#if WITH_EDITOR
	TGuardValue<int32> GPlayInEditorIDGuard(GPlayInEditorID, GetWorld()->GetOutermost()->GetPIEInstanceID());
#endif

	TSet<uint16> ActivatedSequencesSet{ ActivatedSequences };
	TSet<uint16> DeactivatedSequences{ PreActivatedSequences.Difference(ActivatedSequencesSet) };
	TSet<uint16> CurActivatedSequences{ ActivatedSequencesSet.Difference(PreActivatedSequences) };

	for (const uint16 SequenceId : DeactivatedSequences)
	{
		FGameQuestSequenceBase* Sequence = GetSequencePtr(SequenceId);
		Sequence->OnRepDeactivateSequence(SequenceId);
	}
	for (const uint16 SequenceId : CurActivatedSequences)
	{
		FGameQuestSequenceBase* Sequence = GetSequencePtr(SequenceId);
		Sequence->OnRepActivateSequence(SequenceId);
	}

	PreActivatedSequences = ::MoveTemp(ActivatedSequencesSet);
}

void UGameQuestGraphBase::OnRep_ActivatedBranches()
{
#if WITH_EDITOR
	TGuardValue<int32> GPlayInEditorIDGuard(GPlayInEditorID, GetWorld()->GetOutermost()->GetPIEInstanceID());
#endif

	TSet<uint16> ActivatedBranchesSet{ ActivatedBranches };
	TSet<uint16> DeactivatedBranches{ PreActivatedBranches.Difference(ActivatedBranchesSet) };
	TSet<uint16> CurActivatedBranches{ ActivatedBranchesSet.Difference(PreActivatedBranches) };

	for (const uint16 SequenceId : DeactivatedBranches)
	{
		FGameQuestSequenceBranch* SequenceBranch = GameQuestCastChecked<FGameQuestSequenceBranch>(GetSequencePtr(SequenceId));
		if (SequenceBranch->bIsBranchesActivated)
		{
			SequenceBranch->DeactivateBranches(false);
		}
	}
	for (const uint16 SequenceId : CurActivatedBranches)
	{
		FGameQuestSequenceBranch* SequenceBranch = GameQuestCastChecked<FGameQuestSequenceBranch>(GetSequencePtr(SequenceId));
		if (SequenceBranch->bIsBranchesActivated == false)
		{
			SequenceBranch->ActivateBranches(false);
		}
	}

	PreActivatedBranches = ::MoveTemp(ActivatedBranchesSet);
}

void UGameQuestGraphBase::Tick(float DeltaSeconds)
{
	for (int32 Idx = TickableSequences.Num() - 1; Idx >= 0 && Idx < TickableSequences.Num(); ++Idx)
	{
		FGameQuestSequenceBase* Sequence = TickableSequences[Idx];
		Sequence->Tick(DeltaSeconds);
	}
	for (int32 Idx = TickableElements.Num() - 1; Idx >= 0 && Idx < TickableElements.Num(); --Idx)
	{
		FGameQuestElementBase* Element = TickableElements[Idx];
		Element->Tick(DeltaSeconds);
	}
}

void UGameQuestGraphBase::SetElementFinishedToServer_Implementation(const uint16 ElementId, const FName& EventName)
{
	FGameQuestElementBase* Element = GetElementPtr(ElementId);
	if (!ensure(Element && GetSequencePtr(Element->Sequence)->bIsActivated))
	{
		return;
	}
	if (!Element->bIsActivated)
	{
		return;
	}
	if (Element->IsLocalJudgment() == false)
	{
		return;
	}
	UE_LOG(LogGameQuest, Verbose, TEXT("Server Receive Finish %s.%s.%s"), *GetName(), *Element->GetNodeName().ToString(), *EventName.ToString());
	Element->FinishElementByName(EventName);
}

void UGameQuestGraphBase::SetElementUnfinishedToServer_Implementation(const uint16 ElementId)
{
	FGameQuestElementBase* Element = GetElementPtr(ElementId);
	if (!ensure(Element && GetSequencePtr(Element->Sequence)->bIsActivated))
	{
		return;
	}
	if (!Element->bIsActivated)
	{
		return;
	}
	UE_LOG(LogGameQuest, Verbose, TEXT("Server Receive Cancel Finish %s.%s"), *GetName(), *Element->GetNodeName().ToString());
	Element->UnfinishedElement();
}

void UGameQuestGraphBase::ReactiveQuest()
{
#if WITH_EDITOR
	TGuardValue<int32> GPlayInEditorIDGuard(GPlayInEditorID, GetWorld()->GetOutermost()->GetPIEInstanceID());
#endif

	check(bIsActivated == false);
	bIsActivated = true;
	UE_LOG(LogGameQuest, Verbose, TEXT("ReactiveQuest %s"), *GetName());

	if (UGameQuestComponent* OwnerComp = Cast<UGameQuestComponent>(Owner))
	{
		OwnerComp->WhenQuestActivated(this);
	}
	const bool bHasAuthority = HasAuthority();
	for (int32 Idx = ActivatedSequences.Num() - 1; Idx >= 0 && Idx < ActivatedSequences.Num(); --Idx)
	{
		FGameQuestSequenceBase* Sequence = GetSequencePtr(ActivatedSequences[Idx]);
		if (ensure(Sequence->bIsActivated == false))
		{
			Sequence->bIsActivated = true;
			Sequence->WhenSequenceActivated(bHasAuthority);
		}
	}
}

void UGameQuestGraphBase::DeactivateQuest()
{
#if WITH_EDITOR
	TGuardValue<int32> GPlayInEditorIDGuard(GPlayInEditorID, GetWorld()->GetOutermost()->GetPIEInstanceID());
#endif

	check(bIsActivated);
	bIsActivated = false;
	UE_LOG(LogGameQuest, Verbose, TEXT("DeactivateQuest %s"), *GetName());

	const bool bHasAuthority = HasAuthority();
	for (int32 Idx = ActivatedSequences.Num() - 1; Idx >= 0 && Idx < ActivatedSequences.Num(); --Idx)
	{
		FGameQuestSequenceBase* Sequence = GetSequencePtr(ActivatedSequences[Idx]);
		if (ensure(Sequence->bIsActivated))
		{
			Sequence->bIsActivated = false;
			Sequence->WhenSequenceDeactivated(bHasAuthority);
		}
	}
	if (UGameQuestComponent* OwnerComp = Cast<UGameQuestComponent>(Owner))
	{
		OwnerComp->WhenQuestDeactivated(this);
	}
}

void UGameQuestGraphBase::PreSequenceActivated(FGameQuestSequenceBase* Sequence, uint16 SequenceId)
{
	OnPreSequenceActivatedNative.Broadcast(Sequence, SequenceId);
	WhenPreSequenceActivated(Sequence, SequenceId);
}

void UGameQuestGraphBase::PostSequenceDeactivated(FGameQuestSequenceBase* Sequence, uint16 SequenceId)
{
	OnPostSequenceDeactivatedNative.Broadcast(Sequence, SequenceId);
	WhenPostSequenceDeactivated(Sequence, SequenceId);
}

FGameQuestSequenceBase* UGameQuestGraphBase::GetSequencePtr(uint16 Id) const
{
	UGameQuestGraphGeneratedClass* Class = CastChecked<UGameQuestGraphGeneratedClass>(GetClass());
	const FStructProperty* SequenceProperty = Class->NodeIdPropertyMap[Id];
	check(SequenceProperty->Struct->IsChildOf(FGameQuestSequenceBase::StaticStruct()));
	const FGameQuestSequenceBase* Sequence = SequenceProperty->ContainerPtrToValuePtr<FGameQuestSequenceBase>(this);
	return const_cast<FGameQuestSequenceBase*>(Sequence);
}

FGameQuestElementBase* UGameQuestGraphBase::GetElementPtr(uint16 Id) const
{
	UGameQuestGraphGeneratedClass* Class = CastChecked<UGameQuestGraphGeneratedClass>(GetClass());
	const FStructProperty* ElementProperty = Class->NodeIdPropertyMap[Id];
	check(ElementProperty->Struct->IsChildOf(FGameQuestElementBase::StaticStruct()));
	const FGameQuestElementBase* Element = ElementProperty->ContainerPtrToValuePtr<FGameQuestElementBase>(this);
	return const_cast<FGameQuestElementBase*>(Element);
}

const GameQuest::FLogicList& UGameQuestGraphBase::GetLogicList(const FGameQuestNodeBase* Node) const
{
	UGameQuestGraphGeneratedClass* Class = CastChecked<UGameQuestGraphGeneratedClass>(GetClass());
	return Class->NodeIdLogicsMap[Class->NodeNameIdMap[Node->GetNodeName()]];
}

uint16 UGameQuestGraphBase::GetSequenceId(const FGameQuestSequenceBase* Sequence) const
{
	UGameQuestGraphGeneratedClass* Class = CastChecked<UGameQuestGraphGeneratedClass>(GetClass());
	return Class->NodeNameIdMap[Sequence->GetNodeName()];
}

uint16 UGameQuestGraphBase::GetElementId(const FGameQuestElementBase* Element) const
{
	UGameQuestGraphGeneratedClass* Class = CastChecked<UGameQuestGraphGeneratedClass>(GetClass());
	return Class->NodeNameIdMap[Element->GetNodeName()];
}

TArray<FName> UGameQuestGraphBase::GetRerouteTagNames() const
{
	const UGameQuestGraphGeneratedClass* Class = CastChecked<UGameQuestGraphGeneratedClass>(GetClass());
	TArray<FName> RerouteTags;
	for (const auto& [Name, _] : Class->RerouteTags)
	{
		RerouteTags.Add(Name);
	}
	return RerouteTags;
}

const FGameQuestRerouteTag* UGameQuestGraphBase::GetRerouteTag(const FName& Name) const
{
	const UGameQuestGraphGeneratedClass* Class = CastChecked<UGameQuestGraphGeneratedClass>(GetClass());
	const FStructProperty* RerouteTagProperty = Class->RerouteTags.FindRef(Name);
	if (RerouteTagProperty == nullptr)
	{
		return nullptr;
	}
	return RerouteTagProperty->ContainerPtrToValuePtr<FGameQuestRerouteTag>(this);
}

void UGameQuestGraphBase::BindingRerouteTags()
{
	const UGameQuestGraphGeneratedClass* Class = CastChecked<UGameQuestGraphGeneratedClass>(GetClass());
	for (const auto& [Name, StructProperty] : Class->RerouteTags)
	{
		UFunction* Event = Owner->GetClass()->FindFunctionByName(FGameQuestRerouteTag::MakeEventName(OwnerNode->GetNodeName(), Name));
		FGameQuestRerouteTag* RerouteTag = StructProperty->ContainerPtrToValuePtr<FGameQuestRerouteTag>(this);
		RerouteTag->Event = Event;
	}
}

AActor* UGameQuestGraphBase::GetOwnerActor() const
{
	return GetTypedOuter<AActor>();
}

void UGameQuestGraphBase::InterruptQuest()
{
	if (!ensure(bIsActivated))
	{
		return;
	}
	for (const uint16 SequenceId : ActivatedSequences)
	{
		FGameQuestSequenceBase* Sequence = GetSequencePtr(SequenceId);
		Sequence->DeactivateSequence(SequenceId);
	}
	InvokeInterruptQuest();
}

struct FInterruptNextActivateSequence
{
	UGameQuestGraphBase& Quest;
	void Do(FGameQuestSequenceBase& Sequence, uint16 SequenceId)
	{
		if (Sequence.bIsActivated)
		{
			Sequence.InterruptSequence();
		}
		const TArray<uint16> NextSequences = Sequence.GetNextSequences();
		if (NextSequences.Num() > 0)
		{
			for (const uint16 NextSequenceId : NextSequences)
			{
				FGameQuestSequenceBase* NextSequence = Quest.GetSequencePtr(NextSequenceId);
				if (NextSequence->PreSequence != SequenceId)
				{
					continue;
				}
				if (NextSequence->bInterrupted == false)
				{
					Do(*NextSequence, Quest.GetSequenceId(NextSequence));
				}
			}
		}
		else if (Sequence.bInterrupted == false)
		{
			Sequence.InterruptSequence();
		}
	}
};

void UGameQuestGraphBase::InterruptSequence(FGameQuestSequenceBase& Sequence)
{
	if (!ensure(bIsActivated))
	{
		return;
	}
	const FGameQuestSequenceBase::EState SequenceState = Sequence.GetSequenceState();
	if (SequenceState == FGameQuestSequenceBase::EState::Interrupted)
	{
		return;
	}
	UE_LOG(LogGameQuest, Verbose, TEXT("InterruptSequence %s.%s"), *GetName(), *Sequence.GetNodeName().ToString());
	const uint16 SequenceId = GetSequenceId(&Sequence);
	if (SequenceState == FGameQuestSequenceBase::EState::Finished)
	{
		FInterruptNextActivateSequence{ *this }.Do(Sequence, SequenceId);
	}
	else
	{
		Sequence.InterruptSequence();
	}
}

void UGameQuestGraphBase::InterruptBranch(FGameQuestElementBase& Element)
{
	if (!ensure(bIsActivated))
	{
		return;
	}
	FGameQuestSequenceBranch* SequenceBranch = GameQuestCast<FGameQuestSequenceBranch>(GetSequencePtr(Element.Sequence));
	if (!ensure(SequenceBranch))
	{
		return;
	}
	const FGameQuestSequenceBase::EState SequenceState = SequenceBranch->GetSequenceState();
	if (SequenceState == FGameQuestSequenceBase::EState::Interrupted)
	{
		return;
	}
	const uint16 ElementId = GetElementId(&Element);
	FGameQuestSequenceBranchElement* Branch = SequenceBranch->Branches.FindByPredicate([&](const FGameQuestSequenceBranchElement& E){ return E.Element == ElementId; });
	if (!ensure(Branch))
	{
		return;
	}
	UE_LOG(LogGameQuest, Verbose, TEXT("InterruptBranch %s.%s"), *GetName(), *Element.GetNodeName().ToString());
	if (SequenceState == FGameQuestSequenceBase::EState::Deactivated || SequenceState == FGameQuestSequenceBase::EState::Activated)
	{
		if (SequenceBranch->Branches.Num() == 1)
		{
			SequenceBranch->InterruptSequence();
		}
		else
		{
			Branch->bInterrupted = true;
			SequenceBranch->MarkNodeNetDirty();
			if (Element.bIsActivated)
			{
				Element.DeactivateElement(true);
			}
		}
	}
	else if (SequenceState == FGameQuestSequenceBase::EState::Finished)
	{
		for (const uint16 NextSequenceId : Branch->NextSequences)
		{
			FGameQuestSequenceBase* NextSequence = GetSequencePtr(NextSequenceId);
			FInterruptNextActivateSequence{ *this }.Do(*NextSequence, NextSequenceId);
		}
	}
}

DEFINE_FUNCTION(UGameQuestGraphBase::execInterruptNodeByRef)
{
	Stack.StepCompiledIn<FProperty>(nullptr);
	void* SrcPropertyAddr = Stack.MostRecentPropertyAddress;
	FProperty* MostRecentProperty = Stack.MostRecentProperty;
	P_FINISH;

	P_NATIVE_BEGIN;
	const FStructProperty* StructProperty = CastField<FStructProperty>(MostRecentProperty);
	if (!ensure(StructProperty))
	{
		return;
	}
	if (StructProperty->Struct->IsChildOf(FGameQuestSequenceBase::StaticStruct()))
	{
		P_THIS->InterruptSequence(*static_cast<FGameQuestSequenceBase*>(SrcPropertyAddr));
	}
	else if (StructProperty->Struct->IsChildOf(FGameQuestElementBase::StaticStruct()))
	{
		P_THIS->InterruptBranch(*static_cast<FGameQuestElementBase*>(SrcPropertyAddr));
	}
	else
	{
		ensure(false);
	}
	P_NATIVE_END;
}

void UGameQuestGraphBase::GetEvaluateGraphExposedInputs()
{
	if (OwnerNode)
	{
		OwnerNode->GetEvaluateGraphExposedInputs();
	}
}

int32 UGameQuestGraphBase::GetGraphDepth() const
{
	int32 Depth = 0;
	for (const UGameQuestGraphBase* OwnerGraph = Cast<UGameQuestGraphBase>(Owner); OwnerGraph; OwnerGraph = Cast<UGameQuestGraphBase>(OwnerGraph->Owner))
	{
		Depth += 1;
	}
	return Depth;
}

UGameQuestGraphBase::EState UGameQuestGraphBase::GetQuestState() const
{
	if (bIsActivated)
	{
		return EState::Activated;
	}
	if (bInterrupted)
	{
		return EState::Interrupted;
	}
	if (ActivatedSequences.Num() > 0)
	{
		return EState::Deactivated;
	}
	if (StartSequences.Num() == 0)
	{
		return EState::Unactivated;
	}
	return EState::Finished;
}

void UGameQuestGraphBase::ForceActivateBranchToServer_Implementation(const uint16 ElementBranchId)
{
	if (CVarGameQuestEnableCheat.GetValueOnGameThread() == false)
	{
		return;
	}
#if !UE_BUILD_SHIPPING || ALLOW_CONSOLE_IN_SHIPPING
	if (!ensure(bIsActivated))
	{
		return;
	}
	if (!ensure(ElementBranchId != GameQuest::IdNone))
	{
		return;
	}
	const FGameQuestElementBase* TargetElement = GetElementPtr(ElementBranchId);
	if (!ensure(TargetElement))
	{
		return;
	}
	FGameQuestSequenceBranch* SequenceBranch = GameQuestCast<FGameQuestSequenceBranch>(GetSequencePtr(TargetElement->Sequence));
	if (!ensure(SequenceBranch))
	{
		return;
	}
	const FGameQuestSequenceBranchElement* Branch = SequenceBranch->Branches.FindByPredicate([&](const FGameQuestSequenceBranchElement& E){ return E.Element == ElementBranchId; });
	if (!ensure(Branch))
	{
		return;
	}
	if (!ensure(SequenceBranch->GetSequenceState() != FGameQuestSequenceBase::EState::Finished))
	{
		return;
	}
	UE_LOG(LogGameQuest, Verbose, TEXT("ForceActivateBranch %s.%s"), *GetName(), *TargetElement->GetNodeName().ToString());
	if (SequenceBranch->bIsActivated == false)
	{
		ForceActivateSequenceToServer_Implementation(TargetElement->Sequence);
	}
	if (!ensure(SequenceBranch->GetSequenceState() != FGameQuestSequenceBase::EState::Deactivated))
	{
		return;
	}
	if (SequenceBranch->bIsBranchesActivated == false)
	{
		for (const uint16 ElementId : SequenceBranch->Elements)
		{
			FGameQuestElementBase* Element = GetElementPtr(ElementId);
			if (Element->bIsActivated)
			{
				Element->ForceFinishElement(NAME_None);
			}
		}
	}
	if (!ensure(SequenceBranch->bIsBranchesActivated || SequenceBranch->GetSequenceState() == FGameQuestSequenceBase::EState::Finished))
	{
		return;
	}
	ensure(TargetElement->bIsActivated || TargetElement->bIsFinished);
#endif
}

void UGameQuestGraphBase::ForceActivateSequenceToServer_Implementation(const uint16 SequenceId)
{
	if (CVarGameQuestEnableCheat.GetValueOnGameThread() == false)
	{
		return;
	}
#if !UE_BUILD_SHIPPING || ALLOW_CONSOLE_IN_SHIPPING
	if (!ensure(bIsActivated))
	{
		return;
	}
	if (!ensure(SequenceId != GameQuest::IdNone))
	{
		return;
	}
	const FGameQuestSequenceBase* TargetSequence = GetSequencePtr(SequenceId);
	if (!ensure(TargetSequence))
	{
		return;
	}
	if (!ensure(TargetSequence->GetSequenceState() == FGameQuestSequenceBase::EState::Deactivated))
	{
		return;
	}
	const UGameQuestGraphGeneratedClass* Class = CastChecked<UGameQuestGraphGeneratedClass>(GetClass());
	struct FSequenceSearcher
	{
		UGameQuestGraphBase& Quest;
		const UGameQuestGraphGeneratedClass& Class;
		TArray<uint16> PendingActivatedIds;

		bool Search(const uint16 NodeId, TSet<uint16>& Visited)
		{
			if (Visited.Contains(NodeId))
			{
				return false;
			}
			Visited.Add(NodeId);
			if (Quest.ActivatedSequences.Contains(NodeId))
			{
				PendingActivatedIds.Add(NodeId);
				return true;
			}
			const auto* FromNodes = Class.NodeToPredecessorMap.Find(NodeId);
			if (FromNodes == nullptr)
			{
				return false;
			}
			for (const uint16 FromNodeId : *FromNodes)
			{
				if (Search(FromNodeId, Visited))
				{
					PendingActivatedIds.Add(NodeId);
					return true;
				}
			}
			return false;
		}
	};
	FSequenceSearcher Searcher{ *this, *Class };
	{
		TSet<uint16> Visited;
		Searcher.Search(SequenceId, Visited);
	}
	const TArray<uint16>& PendingActivatedIds = Searcher.PendingActivatedIds;
	if (!ensure(PendingActivatedIds.Num() > 0))
	{
		return;
	}
	UE_LOG(LogGameQuest, Verbose, TEXT("ForceActivateSequence %s.%s"), *GetName(), *TargetSequence->GetNodeName().ToString());
	struct FLocal
	{
		UGameQuestGraphBase& Quest;
		void FinishSequence(const FGameQuestSequenceSingle* SequenceSingle, const FName& EventName) const
		{
			FGameQuestElementBase* Element = Quest.GetElementPtr(SequenceSingle->Element);
			Element->ForceFinishElement(EventName);
		};
		void FinishSequence(const FGameQuestSequenceList* SequenceList) const
		{
			for (const uint16 ElementId : SequenceList->Elements)
			{
				FGameQuestElementBase* Element = Quest.GetElementPtr(ElementId);
				if (Element->bIsActivated)
				{
					Element->FinishElement({}, NAME_None);
				}
			}
		}
		void FinishSequence(const FGameQuestSequenceSubQuest* SequenceSubQuest, const FName& RerouteTag) const
		{
			UGameQuestGraphBase* SubQuestInstance = SequenceSubQuest->SubQuestInstance;
			if (SubQuestInstance == nullptr)
			{
				ensure(SequenceSubQuest->SubQuestClass.LoadSynchronous());
			}
			if (!ensure(SubQuestInstance))
			{
				return;
			}
			const UGameQuestGraphGeneratedClass* SubClass = CastChecked<UGameQuestGraphGeneratedClass>(SubQuestInstance->GetClass());
			const auto [EventName, ToActivateId] = SubClass->RerouteTagPreNodesMap[RerouteTag][0];
			if (SubClass->NodeIdPropertyMap[ToActivateId]->Struct->IsChildOf(FGameQuestSequenceBase::StaticStruct()))
			{
				SubQuestInstance->ForceActivateSequenceToServer(ToActivateId);
				const FGameQuestSequenceBase* SubSequence = SubQuestInstance->GetSequencePtr(ToActivateId);
				if (SubSequence->bIsActivated)
				{
					if (const FGameQuestSequenceSingle* SubSequenceSingle = GameQuestCast<FGameQuestSequenceSingle>(SubSequence))
					{
						FinishSequence(SubSequenceSingle, EventName);
					}
					else if (const FGameQuestSequenceList* SubSequenceList = GameQuestCast<FGameQuestSequenceList>(SubSequence))
					{
						FinishSequence(SubSequenceList);
					}
					else if (const FGameQuestSequenceSubQuest* SubSequenceSubQuest = GameQuestCast<FGameQuestSequenceSubQuest>(SubSequence))
					{
						FinishSequence(SubSequenceSubQuest, EventName);
					}
					else
					{
						ensure(false);
					}
				}
			}
			else
			{
				SubQuestInstance->ForceActivateBranchToServer(ToActivateId);
				const FGameQuestElementBase* ElementBranch = SubQuestInstance->GetElementPtr(ToActivateId);
				if (ElementBranch->bIsActivated)
				{
					SubQuestInstance->ForceFinishElementToServer(ToActivateId, EventName);
				}
			}
		}
	};

	for (int32 Idx = 0; Idx < PendingActivatedIds.Num() - 1; ++Idx)
	{
		FGameQuestSequenceBase* Sequence = GetSequencePtr(PendingActivatedIds[Idx]);
		if (Sequence->GetSequenceState() == FGameQuestSequenceBase::EState::Finished)
		{
			continue;
		}
		if (!ensure(Sequence->bIsActivated))
		{
			return;
		}

		if (const FGameQuestSequenceSingle* SequenceSingle = GameQuestCast<FGameQuestSequenceSingle>(Sequence))
		{
			using FEventNameToNextNode = UGameQuestGraphGeneratedClass::FEventNameNodeId;
			const FEventNameToNextNode* EventName = Class->NodeIdEventNameMap[SequenceSingle->Element].FindByPredicate([&](const FEventNameToNextNode& E){ return E.NodeId == PendingActivatedIds[Idx + 1]; });
			FLocal{ *this }.FinishSequence(SequenceSingle, EventName->EventName);
		}
		else if (const FGameQuestSequenceList* SequenceList = GameQuestCast<FGameQuestSequenceList>(Sequence))
		{
			FLocal{ *this }.FinishSequence(SequenceList);
		}
		else if (FGameQuestSequenceBranch* SequenceBranch = GameQuestCast<FGameQuestSequenceBranch>(Sequence))
		{
			for (const uint16 ElementId : SequenceBranch->Elements)
			{
				FGameQuestElementBase* Element = GetElementPtr(ElementId);
				if (Element->bIsActivated)
				{
					Element->FinishElement({}, NAME_None);
				}
			}
			ensure(SequenceBranch->bIsBranchesActivated);

			Idx += 1;
			const uint16 ElementId = PendingActivatedIds[Idx];
			FGameQuestElementBase* BranchElement = GetElementPtr(ElementId);
			using FEventNameToNextNode = UGameQuestGraphGeneratedClass::FEventNameNodeId;
			const FEventNameToNextNode* EventName = Class->NodeIdEventNameMap[ElementId].FindByPredicate([&](const FEventNameToNextNode& E){ return E.NodeId == PendingActivatedIds[Idx + 1]; });
			BranchElement->ForceFinishElement(EventName->EventName);
		}
		else if (const FGameQuestSequenceSubQuest* SequenceSubQuest = GameQuestCast<FGameQuestSequenceSubQuest>(Sequence))
		{
			using FEventNameToNextNode = UGameQuestGraphGeneratedClass::FEventNameNodeId;
			const FEventNameToNextNode* RerouteTagName = Class->NodeIdEventNameMap[PendingActivatedIds[Idx]].FindByPredicate([&](const FEventNameToNextNode& E){ return E.NodeId == PendingActivatedIds[Idx + 1]; });
			FLocal{ *this }.FinishSequence(SequenceSubQuest, RerouteTagName->EventName);
		}
		else
		{
			ensure(false);
		}
	}
	{
		ensure(GetSequencePtr(PendingActivatedIds[PendingActivatedIds.Num() - 1])->GetSequenceState() != FGameQuestSequenceBase::EState::Deactivated);
	}
#endif
}

void UGameQuestGraphBase::ForceFinishElementToServer_Implementation(const uint16 ElementId, const FName& EventName)
{
	if (CVarGameQuestEnableCheat.GetValueOnGameThread() == false)
	{
		return;
	}
#if !UE_BUILD_SHIPPING || ALLOW_CONSOLE_IN_SHIPPING
	FGameQuestElementBase* Element = GetElementPtr(ElementId);
	if (!ensure(Element && GetSequencePtr(Element->Sequence)->bIsActivated))
	{
		return;
	}
	if (!Element->bIsActivated)
	{
		return;
	}
	UE_LOG(LogGameQuest, Verbose, TEXT("ForceFinishElement %s.%s.%s"), *GetName(), *Element->GetNodeName().ToString(), *EventName.ToString());
	Element->ForceFinishElement(EventName);
#endif
}

DEFINE_FUNCTION(UGameQuestGraphBase::execTryActivateSequence)
{
	Stack.StepCompiledIn<FProperty>(nullptr);
	void* SrcPropertyAddr = Stack.MostRecentPropertyAddress;
	FProperty* MostRecentProperty = Stack.MostRecentProperty;

	P_FINISH;

	P_NATIVE_BEGIN;
	check(CastFieldChecked<FStructProperty>(MostRecentProperty)->Struct->IsChildOf(FGameQuestSequenceBase::StaticStruct()));
	static_cast<FGameQuestSequenceBase*>(SrcPropertyAddr)->TryActivateSequence();
	P_NATIVE_END;
}

DEFINE_FUNCTION(UGameQuestGraphBase::execCallNodeRepFunction)
{
	Stack.StepCompiledIn<FProperty>(nullptr);
	void* SrcPropertyAddr = Stack.MostRecentPropertyAddress;
	FProperty* MostRecentProperty = Stack.MostRecentProperty;
	Stack.StepCompiledIn<FProperty>(nullptr);
	void* SrcPropertyAddr_PreValue = Stack.MostRecentPropertyAddress;
	FProperty* MostRecent_PreValue = Stack.MostRecentProperty;
	P_FINISH;

	P_NATIVE_BEGIN;
	check(CastFieldChecked<FStructProperty>(MostRecentProperty)->Struct->IsChildOf(FGameQuestNodeBase::StaticStruct()));
	check(CastFieldChecked<FStructProperty>(MostRecent_PreValue)->Struct->IsChildOf(FGameQuestNodeBase::StaticStruct()));
	static_cast<FGameQuestNodeBase*>(SrcPropertyAddr)->WhenOnRepValue(*static_cast<FGameQuestNodeBase*>(SrcPropertyAddr_PreValue));
	P_NATIVE_END;
}

TArray<Context::FAddNextSequenceIdFunc> StartCachedAddNextSequenceIdFuncList;

bool UGameQuestGraphBase::TryStartGameQuest()
{
	if (GetQuestState() != EState::Unactivated)
	{
		UE_LOG(LogGameQuest, Warning, TEXT("%s is started, can't restart"), *GetName());
		return false;
	}

	UE_LOG(LogGameQuest, Verbose, TEXT("StartGameQuest %s"), *GetName());
	bIsActivated = true;

	StartCachedAddNextSequenceIdFuncList.Push(MoveTemp(Context::AddNextSequenceIdFunc));
	Context::AddNextSequenceIdFunc = [this](const uint16 SequenceId)
	{
		StartSequences.Add(SequenceId);
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, StartSequences, this);
	};

	if (UGameQuestComponent* OwnerComp = Cast<UGameQuestComponent>(Owner))
	{
		OwnerComp->PostStartQuest(this);
	}
	return true;
}

void UGameQuestGraphBase::PostExecuteEntryEvent()
{
	ensure(StartSequences.Num() > 0);
	for (const uint16 SequenceId : StartSequences)
	{
		FGameQuestSequenceBase* Sequence = GetSequencePtr(SequenceId);
		if (Sequence->bInterrupted)
		{
			continue;
		}
		Sequence->ActivateSequence(SequenceId);
	}
	Context::AddNextSequenceIdFunc = StartCachedAddNextSequenceIdFuncList.Pop();
}

void UGameQuestGraphBase::ProcessRerouteTag(FName RerouteTagName, FGameQuestRerouteTag& RerouteTag)
{
	const uint16 FinishedSequenceId = Context::CurrentFinishedSequenceId;
	if (!ensure(FinishedSequenceId != GameQuest::IdNone))
	{
		return;
	}
	if (RerouteTag.PreSequenceId != GameQuest::IdNone)
	{
		return;
	}
	RerouteTag.PreSequenceId = FinishedSequenceId;
	RerouteTag.PreBranchId = Context::CurrentFinishedBranchId;
	if (OwnerNode == nullptr)
	{
		return;
	}
	OwnerNode->ProcessRerouteTag(RerouteTagName, RerouteTag);
}

void UGameQuestGraphBase::InvokeFinishQuest()
{
	check(GetQuestState() == EState::Activated);
	if (ActivatedSequences.Num() > 0)
	{
		return;
	}

	bIsActivated = false;

	UE_LOG(LogGameQuest, Verbose, TEXT("FinishQuest %s"), *GetName());
	if (UGameQuestComponent* OwnerComp = Cast<UGameQuestComponent>(Owner))
	{
		OwnerComp->PostFinishQuest(this);
	}
	else if (OwnerNode)
	{
		OwnerNode->WhenSubQuestFinished();
	}
}

void UGameQuestGraphBase::InvokeInterruptQuest()
{
	check(GetQuestState() == EState::Activated);
	if (ActivatedSequences.Num() > 0)
	{
		return;
	}

	bIsActivated = false;
	bInterrupted = true;
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, bInterrupted, this);

	UE_LOG(LogGameQuest, Verbose, TEXT("InterruptQuest %s"), *GetName());
	if (UGameQuestComponent* OwnerComp = Cast<UGameQuestComponent>(Owner))
	{
		OwnerComp->PostFinishQuest(this);
	}
	else if (OwnerNode)
	{
		OwnerNode->WhenSubQuestInterrupted();
	}
}

bool UGameQuestGraphBase::HasAuthority() const
{
	if (const AActor* OwnerActor = Owner ? Owner->GetTypedOuter<AActor>() : nullptr)
	{
		return OwnerActor->HasAuthority();
	}
	return false;
}

bool UGameQuestGraphBase::IsLocalController() const
{
	if (const AActor* OwnerActor = Owner ? Owner->GetTypedOuter<AActor>() : nullptr)
	{
		for (const AActor* TestActor = OwnerActor; TestActor; TestActor = TestActor->GetOwner())
		{
			if (const AController* Controller = Cast<AController>(TestActor))
			{
				return Controller->IsLocalController();
			}
		}
	}
	return false;
}
