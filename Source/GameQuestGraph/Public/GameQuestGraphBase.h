// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameQuestType.h"
#include "UObject/Object.h"
#include "GameQuestGraphBase.generated.h"

class UGameQuestComponent;
struct FGameQuestNodeBase;
struct FGameQuestSequenceBase;
struct FGameQuestSequenceBranch;
struct FGameQuestSequenceSubQuest;
struct FGameQuestElementBase;

UCLASS(Abstract, BlueprintType)
class GAMEQUESTGRAPH_API UGameQuestGraphBase : public UObject
{
	GENERATED_BODY()

	friend UGameQuestComponent;
	friend FGameQuestSequenceBase;
	friend FGameQuestSequenceBranch;
	friend FGameQuestSequenceSubQuest;
	friend FGameQuestElementBase;
public:
	void PostInitProperties() override;
	UWorld* GetWorld() const override;
	bool IsSupportedForNetworking() const override { return true; }
	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	int32 GetFunctionCallspace(UFunction* Function, FFrame* Stack) override;
	bool CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack) override;
	virtual bool ReplicateSubobject(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags);
private:
	uint8 bIsActivated : 1;
	UPROPERTY(Replicated, SaveGame)
	uint8 bInterrupted : 1;

	UPROPERTY(Replicated, SaveGame)
	TArray<uint16> StartSequences;

	TSet<uint16> PreActivatedSequences;
	UPROPERTY(ReplicatedUsing = OnRep_ActivatedSequences, SaveGame)
	TArray<uint16> ActivatedSequences;
	UFUNCTION()
	void OnRep_ActivatedSequences();

	TSet<uint16> PreActivatedBranches;
	UPROPERTY(ReplicatedUsing = OnRep_ActivatedBranches)
	TArray<uint16> ActivatedBranches;
	UFUNCTION()
	void OnRep_ActivatedBranches();

	void Tick(float DeltaSeconds);
	TArray<FGameQuestElementBase*, TInlineAllocator<4>> TickableElements;
	TArray<FGameQuestSequenceBase*, TInlineAllocator<4>> TickableSequences;

	UFUNCTION(Server, Reliable)
	void SetElementFinishedToServer(const uint16 ElementId, const FName& EventName);
	UFUNCTION(Server, Reliable)
	void SetElementUnfinishedToServer(const uint16 ElementId);

	void ReactiveQuest();
	void DeactivateQuest();

	void PreSequenceActivated(FGameQuestSequenceBase* Sequence, uint16 SequenceId);
	void PostSequenceDeactivated(FGameQuestSequenceBase* Sequence, uint16 SequenceId);
protected:
	UPROPERTY(Replicated)
	TObjectPtr<UObject> Owner = nullptr;
	FGameQuestSequenceSubQuest* OwnerNode = nullptr;

	virtual void WhenPreSequenceActivated(FGameQuestSequenceBase* Sequence, uint16 SequenceId) {}
	virtual void WhenPostSequenceDeactivated(FGameQuestSequenceBase* Sequence, uint16 SequenceId) {}
public:
	virtual FGameQuestSequenceBase* GetSequencePtr(uint16 Id) const;
	virtual FGameQuestElementBase* GetElementPtr(uint16 Id) const;
	virtual const GameQuest::FLogicList& GetLogicList(const FGameQuestNodeBase* Node) const;
	virtual uint16 GetSequenceId(const FGameQuestSequenceBase* Sequence) const;
	virtual uint16 GetElementId(const FGameQuestElementBase* Element) const;
	virtual TArray<FName> GetRerouteTagNames() const;
	virtual const FGameQuestRerouteTag* GetRerouteTag(const FName& Name) const;
	virtual void BindingRerouteTags();

	UFUNCTION(BlueprintCallable, Category = "GameQuest")
	UObject* GetOwner() const { return Owner; }
	UFUNCTION(BlueprintCallable, Category = "GameQuest")
	UGameQuestComponent* GetComponent(UGameQuestGraphBase*& MainQuest) const;
	UFUNCTION(BlueprintCallable, Category = "GameQuest")
	AActor* GetOwnerActor() const;
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "GameQuest")
	TArray<FGameQuestSequencePtr> GetStartSequences() const;
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "GameQuest")
	TArray<FGameQuestSequencePtr> GetActivatedSequences() const;

	FGameQuestSequenceSubQuest* GetOwnerNode() const { return OwnerNode; }
	const TArray<uint16>& GetStartSequencesIds() const { return StartSequences; }
	const TArray<uint16>& GetActivatedSequenceIds() const { return ActivatedSequences; }

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "GameQuest")
	void DefaultEntry();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "GameQuest")
	void InterruptQuest();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "GameQuest")
	void InterruptSequence(const FGameQuestSequencePtr& Sequence);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "GameQuest")
	void InterruptBranch(const FGameQuestElementPtr& BranchElement);

	void InterruptSequence(FGameQuestSequenceBase& Sequence);
	void InterruptBranch(FGameQuestElementBase& Element);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, CustomThunk, Category = "GameQuest", meta = (CustomStructureParam = Node))
	void InterruptNodeByRef(UPARAM(Ref) FGameQuestNodeBase& Node);
	DECLARE_FUNCTION(execInterruptNodeByRef);

	UFUNCTION(BlueprintCallable, Category = "GameQuest")
	void GetEvaluateGraphExposedInputs();
	UFUNCTION(BlueprintCallable, Category = "GameQuest")
	int32 GetGraphDepth() const;

	enum class EState : uint8
	{
		Unactivated,
		Deactivated,
		Activated,
		Finished,
		Interrupted,
	};
	EState GetQuestState() const;

	UFUNCTION(Server, Reliable)
	void ForceActivateSequenceToServer(const uint16 SequenceId);
	UFUNCTION(Server, Reliable)
	void ForceActivateBranchToServer(const uint16 ElementBranchId);
	UFUNCTION(Server, Reliable)
	void ForceFinishElementToServer(const uint16 ElementId, const FName& EventName);

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSequenceActivatedNative, FGameQuestSequenceBase* Sequence, uint16 SequenceId);
	FOnSequenceActivatedNative OnPreSequenceActivatedNative;
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSequenceDeactivatedNative, FGameQuestSequenceBase* Sequence, uint16 SequenceId);
	FOnSequenceDeactivatedNative OnPostSequenceDeactivatedNative;
private:
#if WITH_EDITOR
	friend class FGameQuestGraphCompilerContext;
	friend class UBPNode_GameQuestSequenceBase;
	friend class UBPNode_GameQuestEntryEvent;
	friend class UBPNode_GameQuestRerouteTag;
	friend class UBPNode_GameQuestSequenceSubQuest;
#endif
	friend struct FGameQuestPrivateVisitor;

	UFUNCTION(BlueprintCallable, CustomThunk, meta = (CustomStructureParam = Sequence, BlueprintInternalUseOnly = true))
	void TryActivateSequence(UPARAM(Ref) FGameQuestSequenceBase& Sequence);
	DECLARE_FUNCTION(execTryActivateSequence);

	UFUNCTION(BlueprintCallable, CustomThunk, meta = (CustomStructureParam = "Node,PreValue", BlueprintInternalUseOnly = true))
	void CallNodeRepFunction(UPARAM(Ref) FGameQuestNodeBase& Node, const FGameQuestNodeBase& PreValue);
	DECLARE_FUNCTION(execCallNodeRepFunction);

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = true))
	bool TryStartGameQuest();
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = true))
	void PostExecuteEntryEvent();

	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = true))
	void ProcessRerouteTag(FName RerouteTagName, UPARAM(Ref) FGameQuestRerouteTag& RerouteTag);

	void InvokeFinishQuest();
	void InvokeInterruptQuest();

	TArray<FGameQuestNodeBase*> ReplicateSubobjectNodes;
public:
	UFUNCTION(BlueprintCallable, Category = "GameQuest")
	bool HasAuthority() const;
	UFUNCTION(BlueprintCallable, Category = "GameQuest")
	bool IsLocalController() const;
};
