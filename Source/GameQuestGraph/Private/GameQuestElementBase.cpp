// Fill out your copyright notice in the Description page of Project Settings.


#include "GameQuestElementBase.h"

#include "GameQuestGraphBase.h"
#include "GameQuestSequenceBase.h"
#include "Engine/ActorChannel.h"
#include "Engine/Console.h"


bool FGameQuestElementBase::IsInterrupted() const
{
	FGameQuestSequenceBase* SequencePtr = OwnerQuest->GetSequencePtr(Sequence);
	if (SequencePtr->bInterrupted)
	{
		return true;
	}
	FGameQuestSequenceBranch* SequenceBranch = GameQuestCast<FGameQuestSequenceBranch>(SequencePtr);
	if (SequenceBranch == nullptr)
	{
		return false;
	}
	const uint16 ElementId = OwnerQuest->GetElementId(this);
	const FGameQuestSequenceBranchElement* Branch = SequenceBranch->Branches.FindByPredicate([&](const FGameQuestSequenceBranchElement& E){ return E.Element == ElementId; });
	if (Branch == nullptr)
	{
		return false;
	}
	return Branch->bInterrupted;
}

void FGameQuestElementBase::WhenQuestInitProperties(const FStructProperty* Property)
{
	const UClass* QuestClass = OwnerQuest->GetClass();
	for (TFieldIterator<FStructProperty> It{ Property->Struct }; It; ++It)
	{
		if (It->Struct->IsChildOf(FGameQuestFinishEvent::StaticStruct()))
		{
			It->ContainerPtrToValuePtr<FGameQuestFinishEvent>(this)->Event = QuestClass->FindFunctionByName(FGameQuestFinishEvent::MakeEventName(Property->GetFName(), It->GetFName()));
		}
	}
}

void FGameQuestElementBase::WhenOnRepValue(const FGameQuestNodeBase& PreValue)
{
	Super::WhenOnRepValue(PreValue);
	const FGameQuestElementBase& PreElement = static_cast<const FGameQuestElementBase&>(PreValue);
	if (PreElement.bIsFinished != bIsFinished)
	{
		if (bIsFinished)
		{
			WhenFinished();
		}
		else
		{
			WhenUnfinished();
		}
	}
}

#if WITH_EDITOR
TSubclassOf<UGameQuestGraphBase> FGameQuestElementBase::GetSupportQuestGraph() const
{
	return UGameQuestGraphBase::StaticClass();
}
#endif

bool FGameQuestElementBase::ShouldEnableJudgment(bool bHasAuthority) const
{
	if (IsJudgmentBothSide())
	{
		if (bHasAuthority)
		{
			return true;
		}
		else
		{
			return OwnerQuest->IsLocalController();
		}
	}
	if (bHasAuthority)
	{
		const bool bIsLocalJudgment = IsLocalJudgment();
		return bIsLocalJudgment == false || OwnerQuest->IsLocalController();
	}
	else
	{
		const bool bIsLocalJudgment = IsLocalJudgment();
		return bIsLocalJudgment && OwnerQuest->IsLocalController();
	}
}

void FGameQuestElementBase::ActivateElement(bool bIsServer)
{
	if (!ensure(bIsActivated == false))
	{
		return;
	}
	if (bIsServer && ShouldReplicatedSubobject())
	{
		OwnerQuest->ReplicateSubobjectNodes.AddUnique(this);
	}

	bIsActivated = true;
	if (ShouldEnableJudgment(bIsServer))
	{
		UE_LOG(LogGameQuest, Verbose, TEXT("ActivateElement %s.%s"), *OwnerQuest->GetName(), *GetNodeName().ToString());
		if (IsTickable())
		{
			OwnerQuest->TickableElements.Add(this);
		}
		WhenElementActivated();
	}
	if (bIsActivated)
	{
		PostElementActivated();
	}
}

void FGameQuestElementBase::DeactivateElement(bool bHasAuthority)
{
	if (!ensure(bIsActivated))
	{
		return;
	}
	PreElementDeactivated();
	bIsActivated = false;
	if (ShouldEnableJudgment(bHasAuthority))
	{
		UE_LOG(LogGameQuest, Verbose, TEXT("DeactivateElement %s.%s"), *OwnerQuest->GetName(), *GetNodeName().ToString());
		if (IsTickable())
		{
			OwnerQuest->TickableElements.Remove(this);
		}
		WhenElementDeactivated();
	}
}

void FGameQuestElementBase::PostElementActivated()
{
	WhenPostElementActivated();
#if !UE_BUILD_SHIPPING || ALLOW_CONSOLE_IN_SHIPPING
	if (OwnerQuest->IsLocalController())
	{
		RegisterFinishCommand();
		RefreshConsoleCommand();
	}
#endif
}

void FGameQuestElementBase::PreElementDeactivated()
{
#if !UE_BUILD_SHIPPING || ALLOW_CONSOLE_IN_SHIPPING
	if (OwnerQuest->IsLocalController())
	{
		UnregisterFinishCommand();
		RefreshConsoleCommand();
	}
#endif
	WhenPreElementDeactivated();
}

void FGameQuestElementBase::FinishElement(const FGameQuestFinishEvent& OnElementFinishedEvent, const FName& EventName)
{
	if (!ensure(bIsFinished == false))
	{
		return;
	}
	FGameQuestSequenceBase* OwnerSequence = OwnerQuest->GetSequencePtr(Sequence);
	if (!ensure(OwnerSequence->bIsActivated))
	{
		return;
	}
	bIsFinished = true;
	MarkNodeNetDirty();
	WhenFinished();

	if (OwnerQuest->HasAuthority())
	{
		UE_LOG(LogGameQuest, Verbose, TEXT("Finish %s.%s.%s"), *OwnerQuest->GetName(), *GetNodeName().ToString(), *EventName.ToString());
		if (bIsOptional == false)
		{
			OwnerSequence->WhenElementFinished(this, OnElementFinishedEvent);
		}
	}
	else
	{
		ensure(GetNodeStruct()->FindPropertyByName(EventName));

		UE_LOG(LogGameQuest, Verbose, TEXT("Client Send Finish %s.%s.%s"), *OwnerQuest->GetName(), *GetNodeName().ToString(), *EventName.ToString());
		OwnerQuest->SetElementFinishedToServer(OwnerQuest->GetElementId(this), EventName);
	}
}

void FGameQuestElementBase::UnfinishedElement()
{
	if (!ensure(bIsFinished))
	{
		return;
	}
	FGameQuestSequenceBase* OwnerSequence = OwnerQuest->GetSequencePtr(Sequence);
	if (!ensure(OwnerSequence->bIsActivated))
	{
		return;
	}
	bIsFinished = false;
	MarkNodeNetDirty();
	WhenUnfinished();

	if (OwnerQuest->HasAuthority())
	{
		UE_LOG(LogGameQuest, Verbose, TEXT("Cancel Finish %s.%s"), *OwnerQuest->GetName(), *GetNodeName().ToString());
		if (bIsOptional == false)
		{
			OwnerSequence->WhenElementUnfinished(this);
		}
	}
	else
	{
		bIsFinished = false;
		UE_LOG(LogGameQuest, Verbose, TEXT("Client Send Cancel Finished %s.%s"), *OwnerQuest->GetName(), *GetNodeName().ToString());
		OwnerQuest->SetElementUnfinishedToServer(OwnerQuest->GetElementId(this));
	}
}

void FGameQuestElementBase::FinishElementByName(const FName& EventName)
{
	if (EventName == NAME_None)
	{
		FinishElement({}, NAME_None);
	}
	else
	{
		const FStructProperty* EventProperty = FindFProperty<FStructProperty>(GetNodeStruct(), EventName);
		if (!ensure(EventProperty))
		{
			return;
		}
		const FGameQuestFinishEvent* GameQuestFinishEvent = EventProperty->ContainerPtrToValuePtr<FGameQuestFinishEvent>(this);
		FinishElement(*GameQuestFinishEvent, EventName);
	}
}

void FGameQuestElementBase::ForceFinishElement(const FName& EventName)
{
	WhenForceFinishElement(EventName);
	if (ensure(bIsFinished))
	{
		return;
	}
	FinishElementByName(EventName);
}

void FGameQuestElementBase::WhenForceFinishElement(const FName& EventName)
{
	FinishElementByName(EventName);
}

#if !UE_BUILD_SHIPPING || ALLOW_CONSOLE_IN_SHIPPING
void FGameQuestElementBase::RegisterFinishCommand()
{
	FString GameQuestName = OwnerQuest->GetClass()->GetName();
	GameQuestName.RemoveFromEnd(TEXT("_C"));
#if WITH_EDITOR
	const UWorld* World = OwnerQuest->GetWorld();
	FString Scope;
	switch (World->GetNetMode())
	{
	case NM_Client:
		Scope = FString::Printf(TEXT("Client%d"), GPlayInEditorID - 1);
		break;
	case NM_DedicatedServer:
	case NM_ListenServer:
		Scope = FString::Printf(TEXT("Server"));
		break;
	default:
		break;
	}
	const FString CommandPrefix = FString::Printf(TEXT("GameQuest.FinishElement.%s.%s.%s"), *Scope, *GameQuestName, *GetNodeName().ToString());
#else
	const FString CommandPrefix = FString::Printf(TEXT("GameQuest.FinishElement.%s.%s"), *GameQuestName, *GetNodeName().ToString());
#endif

	const uint16 ElementId = OwnerQuest->GetElementId(this);
	const TArray<FName, TInlineAllocator<1>> EventNames = GetActivatedFinishEventNames();
	if (EventNames.Num() > 0)
	{
		if (EventNames.Num() == 1)
		{
			const FString Tooltip = FString::Printf(TEXT("Force Finish Quest %s.%s"), *OwnerQuest->GetName(), *GetNodeName().ToString());
			IConsoleCommand* ConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(*CommandPrefix, *Tooltip,
				FConsoleCommandDelegate::CreateWeakLambda(OwnerQuest.Get(), [this, EventName = EventNames[0], ElementId]
				{
					OwnerQuest->ForceFinishElementToServer(ElementId, EventName);
				}));
			FinishCommands.Add(ConsoleCommand);
		}
		else
		{
			for (const FName& EventName : EventNames)
			{
				const FString Tooltip = FString::Printf(TEXT("Force Finish Quest %s.%s By Branch %s"), *OwnerQuest->GetName(), *GetNodeName().ToString(), *EventName.ToString());
				IConsoleCommand* ConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(*FString::Printf(TEXT("%s.%s"), *CommandPrefix, *EventName.ToString()), *Tooltip,
					FConsoleCommandDelegate::CreateWeakLambda(OwnerQuest.Get(), [this, EventName, ElementId]
					{
						OwnerQuest->ForceFinishElementToServer(ElementId, EventName);
					}));
				FinishCommands.Add(ConsoleCommand);
			}
		}
	}
	else
	{
		const FString Tooltip = FString::Printf(TEXT("Force Finish Quest %s.%s"), *OwnerQuest->GetName(), *GetNodeName().ToString());
		IConsoleCommand* ConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(*CommandPrefix, *Tooltip,
			FConsoleCommandDelegate::CreateWeakLambda(OwnerQuest.Get(), [this, ElementId]
			{
				OwnerQuest->ForceFinishElementToServer(ElementId, NAME_None);
			}));
		FinishCommands.Add(ConsoleCommand);
	}
}

void FGameQuestElementBase::UnregisterFinishCommand()
{
	for (IConsoleCommand* ConsoleCommand : FinishCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(ConsoleCommand);
	}
	FinishCommands.Empty();
}

bool FGameQuestElementBase::HasFinishEventBranch() const
{
	if (GameQuestCast<FGameQuestSequenceSingle>(OwnerQuest->GetSequencePtr(Sequence)) != nullptr)
	{
		return true;
	}
	if (const FGameQuestSequenceBranch* SequenceBranch = GameQuestCast<FGameQuestSequenceBranch>(OwnerQuest->GetSequencePtr(Sequence)))
	{
		const uint16 ElementId = OwnerQuest->GetElementId(this);
		return SequenceBranch->Elements.Contains(ElementId) == false;
	}
	return false;
}

TArray<FName, TInlineAllocator<1>> FGameQuestElementBase::GetActivatedFinishEventNames() const
{
	TArray<FName, TInlineAllocator<1>> FinishEventNames;
	if (HasFinishEventBranch())
	{
		for (TFieldIterator<FStructProperty> It(GetNodeStruct()); It; ++It)
		{
			if (It->Struct != FGameQuestFinishEvent::StaticStruct())
			{
				continue;
			}
			FinishEventNames.Add(It->GetFName());
		}
	}
	return FinishEventNames;
}

void FGameQuestElementBase::RefreshConsoleCommand()
{
	const UGameViewportClient* GameViewport = OwnerQuest->GetWorld()->GetGameViewport();
	if (GameViewport && GameViewport->ViewportConsole)
	{
		GameViewport->ViewportConsole->BuildRuntimeAutoCompleteList(false);
	}
}
#endif

void FGameQuestElementBranchList::WhenElementActivated()
{
	for (const uint16 ElementId : Elements)
	{
		if (bIsActivated)
		{
			FGameQuestElementBase* Element = OwnerQuest->GetElementPtr(ElementId);
			Element->ActivateElement(OwnerQuest->HasAuthority());
		}
	}
	check(bIsActivated || Elements.ContainsByPredicate([this](uint16 Id) { return OwnerQuest->GetElementPtr(Id)->bIsActivated; }) == false);
}

void FGameQuestElementBranchList::WhenElementDeactivated()
{
	for (const uint16 ElementId : Elements)
	{
		FGameQuestElementBase* Element = OwnerQuest->GetElementPtr(ElementId);
		if (Element->bIsActivated)
		{
			Element->DeactivateElement(OwnerQuest->HasAuthority());
		}
	}
}

void FGameQuestElementBranchList::WhenForceFinishElement(const FName& EventName)
{
	for (const uint16 ElementId : Elements)
	{
		FGameQuestElementBase* Element = OwnerQuest->GetElementPtr(ElementId);
		if (Element->bIsActivated)
		{
			Element->ForceFinishElement(NAME_None);
		}
	}
}

UGameQuestElementScriptable::UGameQuestElementScriptable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if WITH_EDITORONLY_DATA
	, SupportType(UGameQuestGraphBase::StaticClass())
#endif
	, bTickable(false)
	, bLocalJudgment(false)
{

}

UWorld* UGameQuestElementScriptable::GetWorld() const
{
	if (Owner == nullptr)
	{
		return nullptr;
	}
	return Owner->OwnerQuest->GetWorld();
}

void UGameQuestElementScriptable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	if (UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(GetClass()))
	{
		BPClass->GetLifetimeBlueprintReplicationList(OutLifetimeProps);
	}
}

int32 UGameQuestElementScriptable::GetFunctionCallspace(UFunction* Function, FFrame* Stack)
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return FunctionCallspace::Local;
	}

	if (AActor* OwningActor = GetTypedOuter<AActor>())
	{
		return OwningActor->GetFunctionCallspace(Function, Stack);
	}
	return Super::GetFunctionCallspace(Function, Stack);
}

bool UGameQuestElementScriptable::CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack)
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

void UGameQuestElementScriptable::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);

#if WITH_EDITOR
	if (IsRunningCookCommandlet() == false)
	{
		static const FName SupportTypeTagName = TEXT("SupportType");
		OutTags.Add(FAssetRegistryTag(SupportTypeTagName, SupportType.ToString(), FAssetRegistryTag::TT_Alphabetical));
	}
#endif
}

void UGameQuestElementScriptable::WhenElementActivated_Implementation() {}
void UGameQuestElementScriptable::WhenElementDeactivated_Implementation() {}
void UGameQuestElementScriptable::WhenPostElementActivated_Implementation() {}
void UGameQuestElementScriptable::WhenPreElementDeactivated_Implementation() {}
void UGameQuestElementScriptable::WhenTick_Implementation(float DeltaSeconds) {}

void UGameQuestElementScriptable::WhenForceFinishElement_Implementation(const FName& EventName)
{
	Owner->FinishElementByName(EventName);
}

void FGameQuestElementScript::WhenQuestInitProperties(const FStructProperty* Property)
{
	Super::WhenQuestInitProperties(Property);
	if (Instance)
	{
		Instance->Owner = this;

		const UClass* QuestClass = OwnerQuest->GetClass();
		for (TFieldIterator<FStructProperty> It{ Instance->GetClass() }; It; ++It)
		{
			if (It->Struct->IsChildOf(FGameQuestFinishEvent::StaticStruct()))
			{
				It->ContainerPtrToValuePtr<FGameQuestFinishEvent>(Instance)->Event = QuestClass->FindFunctionByName(FGameQuestFinishEvent::MakeEventName(Property->GetFName(), It->GetFName()));
			}
		}
	}
}

bool FGameQuestElementScript::ReplicateSubobject(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool WroteSomething = false;
	if (Instance)
	{
		WroteSomething |= Channel->ReplicateSubobject(Instance, *Bunch, *RepFlags);
		WroteSomething |= Instance->ReplicateSubobject(Channel, Bunch, RepFlags);
	}
	return WroteSomething;
}

AActor* UGameQuestElementScriptable::GetOwnerActor() const
{
	return GetTypedOuter<AActor>();
}

DEFINE_FUNCTION(UGameQuestElementScriptable::execFinishElement)
{
	Stack.StepCompiledIn<FProperty>(nullptr);
	void* SrcPropertyAddr = Stack.MostRecentPropertyAddress;
	FProperty* MostRecentProperty = Stack.MostRecentProperty;
	P_FINISH;

	P_NATIVE_BEGIN;
	check(CastFieldChecked<FStructProperty>(MostRecentProperty)->Struct->IsChildOf(FGameQuestFinishEvent::StaticStruct()));
	P_THIS->Owner->FinishElement(*static_cast<FGameQuestFinishEvent*>(SrcPropertyAddr), MostRecentProperty->GetFName());
	P_NATIVE_END;
}

#if !UE_BUILD_SHIPPING || ALLOW_CONSOLE_IN_SHIPPING
TArray<FName, TInlineAllocator<1>> FGameQuestElementScript::GetActivatedFinishEventNames() const
{
	TArray<FName, TInlineAllocator<1>> FinishEventNames;
	if (Instance && HasFinishEventBranch())
	{
		for (TFieldIterator<FStructProperty> It(Instance->GetClass()); It; ++It)
		{
			if (It->Struct != FGameQuestFinishEvent::StaticStruct())
			{
				continue;
			}
			FinishEventNames.Add(It->GetFName());
		}
	}
	return FinishEventNames;
}
#endif

#if WITH_EDITOR
TSubclassOf<UGameQuestGraphBase> FGameQuestElementScript::GetSupportQuestGraph() const
{
	return Instance ? Instance->SupportType.Get() : nullptr;
}
#endif
