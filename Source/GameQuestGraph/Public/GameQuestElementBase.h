// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameQuestNodeBase.h"
#include "UObject/Object.h"
#include "GameQuestElementBase.generated.h"

struct FGameQuestSequenceBase;
struct FGameQuestSequenceBranch;
struct FGameQuestElementBranchList;

USTRUCT(meta = (Hidden))
struct GAMEQUESTGRAPH_API FGameQuestElementBase : public FGameQuestNodeBase
{
	GENERATED_BODY()

	friend UGameQuestGraphBase;
	friend FGameQuestSequenceBranch;
	friend FGameQuestElementBranchList;
public:
	FGameQuestElementBase()
		: bIsOptional(false)
		, bIsActivated(false)
		, bIsFinished(false)
	{}

	UPROPERTY(NotReplicated)
	uint16 Sequence = 0;

	UPROPERTY(BlueprintReadOnly, NotReplicated)
	uint8 bIsOptional : 1;
	uint8 bIsActivated : 1;
	UPROPERTY(BlueprintReadOnly, SaveGame)
	uint8 bIsFinished : 1;
	bool IsInterrupted() const;

	void WhenQuestInitProperties(const FStructProperty* Property) override;
	void WhenOnRepValue(const FGameQuestNodeBase& PreValue) override;

	virtual bool IsJudgmentBothSide() const { return false; }
	virtual bool IsLocalJudgment() const { return false; }
	virtual bool IsTickable() const { return false; }
#if WITH_EDITOR
	virtual TSubclassOf<UGameQuestGraphBase> GetSupportQuestGraph() const;
#endif

	bool ShouldEnableJudgment(bool bHasAuthority) const;
	void ActivateElement(bool bIsServer);
	void DeactivateElement(bool bHasAuthority);
	void PostElementActivated();
	void PreElementDeactivated();

	void Tick(float DeltaSeconds) { WhenTick(DeltaSeconds); }

	virtual void WhenElementActivated() {}
	virtual void WhenElementDeactivated() {}
	virtual void WhenPostElementActivated() {}
	virtual void WhenPreElementDeactivated() {}
	virtual void WhenTick(float DeltaSeconds) {}

	void FinishElement(const FGameQuestFinishEvent& OnElementFinishedEvent, const FName& EventName);
	void UnfinishedElement();
	virtual void FinishElementByName(const FName& EventName);
protected:
	void ForceFinishElement(const FName& EventName);
	virtual void WhenForceFinishElement(const FName& EventName);

	virtual void WhenFinished() {}
	virtual void WhenUnfinished() {}

#if WITH_EDITOR
	friend class UBPNode_GameQuestElementBase;
#endif
#if !UE_BUILD_SHIPPING || ALLOW_CONSOLE_IN_SHIPPING
	void RegisterFinishCommand();
	void UnregisterFinishCommand();
	bool HasFinishEventBranch() const;
	virtual TArray<FName, TInlineAllocator<1>> GetActivatedFinishEventNames() const;
	void RefreshConsoleCommand();
	TArray<IConsoleCommand*> FinishCommands;
#endif
};

USTRUCT(meta = (DisplayName = "Branch List", BranchElement))
struct GAMEQUESTGRAPH_API FGameQuestElementBranchList : public FGameQuestElementBase
{
	GENERATED_BODY()
public:
	UPROPERTY()
	TArray<uint16> Elements;

	UPROPERTY()
	FGameQuestFinishEvent OnListFinished;

	bool IsJudgmentBothSide() const override { return true; }
	void WhenElementActivated() override;
	void WhenElementDeactivated() override;
	void WhenForceFinishElement(const FName& EventName) override;
};

UCLASS(Abstract, Blueprintable, DefaultToInstanced)
class GAMEQUESTGRAPH_API UGameQuestElementScriptable : public UObject
{
	GENERATED_BODY()

	friend struct FGameQuestElementScript;
public:
	UGameQuestElementScriptable(const FObjectInitializer& ObjectInitializer = FObjectInitializer());

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditDefaultsOnly, Transient, Category = "Settings")
	TSoftClassPtr<UGameQuestGraphBase> SupportType;
#endif
protected:
	FGameQuestElementBase* Owner = nullptr;

	UWorld* GetWorld() const override;
	bool IsSupportedForNetworking() const override { return true; }
	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	int32 GetFunctionCallspace(UFunction* Function, FFrame* Stack) override;
	bool CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack) override;
	void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

	UPROPERTY(EditDefaultsOnly, Transient, Category = "Settings")
	uint8 bTickable : 1;
	// false mean server check
	// true mean local player check
	UPROPERTY(EditDefaultsOnly, Transient, Category = "Settings")
	uint8 bLocalJudgment : 1;

	virtual bool ReplicateSubobject(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) { return false; }

	UFUNCTION(BlueprintNativeEvent, Category = "GameQuest")
	void WhenElementActivated();
	UFUNCTION(BlueprintNativeEvent, Category = "GameQuest")
	void WhenElementDeactivated();
	UFUNCTION(BlueprintNativeEvent, Category = "GameQuest")
	void WhenPostElementActivated();
	UFUNCTION(BlueprintNativeEvent, Category = "GameQuest")
	void WhenPreElementDeactivated();
	UFUNCTION(BlueprintNativeEvent, Category = "GameQuest")
	void WhenTick(float DeltaSeconds);
	// when cheat finished element, can set to finished state
	UFUNCTION(BlueprintNativeEvent, Category = "GameQuest")
	void WhenForceFinishElement(const FName& EventName);

	UFUNCTION(BlueprintCallable, Category = "GameQuest")
	void GetEvaluateGraphExposedInputs() const { Owner->GetEvaluateGraphExposedInputs(); }
	UFUNCTION(BlueprintCallable, Category = "GameQuest")
	UGameQuestGraphBase* GetOwnerQuest() const { return Owner->OwnerQuest; }
	UFUNCTION(BlueprintCallable, Category = "GameQuest")
	AActor* GetOwnerActor() const;
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "GameQuest")
	void FinishElement(const FGameQuestFinishEvent& Event);
	DECLARE_FUNCTION(execFinishElement);
	UFUNCTION(BlueprintCallable, Category = "GameQuest")
	void UnfinishedElement() { Owner->UnfinishedElement(); }
	UFUNCTION(BlueprintCallable, Category = "GameQuest")
	bool IsFinished() const { return Owner->bIsFinished; }
	UFUNCTION(BlueprintCallable, Category = "GameQuest")
	bool IsActivated() const { return Owner->bIsActivated; }
	UFUNCTION(BlueprintCallable, Category = "GameQuest")
	bool IsOptional() const { return Owner->bIsOptional; }
};

USTRUCT(meta = (Hidden))
struct GAMEQUESTGRAPH_API FGameQuestElementScript : public FGameQuestElementBase
{
	GENERATED_BODY()

	using Super = FGameQuestElementBase;
public:
	UPROPERTY(BlueprintReadWrite, Instanced, NotReplicated)
	TObjectPtr<UGameQuestElementScriptable> Instance;

	void WhenQuestInitProperties(const FStructProperty* Property) override;
	bool IsLocalJudgment() const override { return Instance ? Instance->bLocalJudgment : false; }
	bool IsTickable() const override { return Instance ? Instance->bTickable : false; }
	bool ShouldReplicatedSubobject() const override { return true; }
	bool ReplicateSubobject(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

	void WhenElementActivated() override { if (ensure(Instance)) Instance->WhenElementActivated(); }
	void WhenElementDeactivated() override { if (ensure(Instance)) Instance->WhenElementDeactivated(); }
	void WhenPostElementActivated() override { if (ensure(Instance)) Instance->WhenPostElementActivated(); }
	void WhenPreElementDeactivated() override { if (ensure(Instance)) Instance->WhenPreElementDeactivated(); }
	void WhenTick(float DeltaSeconds) override { if (ensure(Instance)) Instance->WhenTick(DeltaSeconds); }
	void WhenForceFinishElement(const FName& EventName) override { if (ensure(Instance)) Instance->WhenForceFinishElement(EventName); }
	void FinishElementByName(const FName& EventName) override;

#if !UE_BUILD_SHIPPING || ALLOW_CONSOLE_IN_SHIPPING
	TArray<FName, TInlineAllocator<1>> GetActivatedFinishEventNames() const override;
#endif

#if WITH_EDITOR
	TSubclassOf<UGameQuestGraphBase> GetSupportQuestGraph() const override;
#endif
};
