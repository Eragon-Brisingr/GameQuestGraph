// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameQuestNodeBase.h"
#include "GameQuestSequenceBase.generated.h"

struct FGameQuestElementBase;
struct FGameQuestSequenceBase;

namespace Context
{
	extern uint16 CurrentFinishedSequenceId;
	extern uint16 CurrentFinishedBranchId;

	using FAddNextSequenceIdFunc = TFunction<void(uint16)>;
	extern FAddNextSequenceIdFunc AddNextSequenceIdFunc;
}

USTRUCT(meta = (Hidden))
struct GAMEQUESTGRAPH_API FGameQuestSequenceBase : public FGameQuestNodeBase
{
	GENERATED_BODY()

	friend FGameQuestElementBase;
	friend UGameQuestGraphBase;
public:
	FGameQuestSequenceBase()
		: bIsActivated(false)
		, bInterrupted(false)
	{}

protected:
	void TryActivateSequence();

	void ActivateSequence(uint16 SequenceId);
	void DeactivateSequence(uint16 SequenceId);

	void OnRepActivateSequence(uint16 SequenceId);
	void OnRepDeactivateSequence(uint16 SequenceId);
private:
	template<bool bHasAuthority>
	void ActivateSequenceImpl(uint16 SequenceId);
	template<bool bHasAuthority>
	void DeactivateSequenceImpl(uint16 SequenceId);

	void Tick(float DeltaSeconds) { WhenTick(DeltaSeconds); }
public:
	UPROPERTY()
	uint16 PreSequence = GameQuest::IdNone;
	uint8 bIsActivated : 1;
	UPROPERTY()
	uint8 bInterrupted : 1;
	void InterruptSequence();

	enum class EState : uint8
	{
		Deactivated,
		Activated,
		Finished,
		Interrupted,
	};
	EState GetSequenceState() const;

	virtual TArray<uint16> GetNextSequences() const { unimplemented(); return {}; }
protected:
	virtual bool IsTickable() const { return false; }
	virtual void WhenTick(float DeltaSeconds) {}

	virtual void WhenSequenceActivated(bool bHasAuthority) {}
	virtual void WhenSequenceDeactivated(bool bHasAuthority) {}

	virtual void WhenElementFinished(FGameQuestElementBase* FinishedElement, const FGameQuestFinishEvent& OnElementFinishedEvent) { unimplemented(); }
	virtual void WhenElementUnfinished(FGameQuestElementBase* FinishedElement) { unimplemented(); }

	void ExecuteFinishEvent(UFunction* FinishEvent, const TArray<uint16>& NextSequenceIds, uint16 BranchId) const;
	bool CanFinishListElements(const TArray<uint16>& Elements, const GameQuest::FLogicList& ElementLogics) const;
};

USTRUCT(meta = (DisplayName = "Sequence Single"))
struct GAMEQUESTGRAPH_API FGameQuestSequenceSingle : public FGameQuestSequenceBase
{
	GENERATED_BODY()
public:
	UPROPERTY(NotReplicated)
	uint16 Element;

	UPROPERTY()
	TArray<uint16> NextSequences;

	TArray<uint16> GetNextSequences() const override { return NextSequences; }

	void WhenSequenceActivated(bool bHasAuthority) override;
	void WhenSequenceDeactivated(bool bHasAuthority) override;

	void WhenElementFinished(FGameQuestElementBase* FinishedElement, const FGameQuestFinishEvent& OnElementFinishedEvent) override;
	void WhenElementUnfinished(FGameQuestElementBase* FinishedElement) override {}
};

USTRUCT(meta = (DisplayName = "Sequence List"))
struct GAMEQUESTGRAPH_API FGameQuestSequenceList : public FGameQuestSequenceBase
{
	GENERATED_BODY()
public:
	void WhenQuestInitProperties(const FStructProperty* Property) override;

	UPROPERTY(NotReplicated)
	TArray<uint16> Elements;

	UPROPERTY()
	TArray<uint16> NextSequences;

	TObjectPtr<UFunction> OnSequenceFinished;

	TArray<uint16> GetNextSequences() const override { return NextSequences; }

	void WhenSequenceActivated(bool bHasAuthority) override;
	void WhenSequenceDeactivated(bool bHasAuthority) override;

	void WhenElementFinished(FGameQuestElementBase* FinishedElement, const FGameQuestFinishEvent& OnElementFinishedEvent) override;
	void WhenElementUnfinished(FGameQuestElementBase* FinishedElement) override {}
};

USTRUCT(BlueprintType)
struct GAMEQUESTGRAPH_API FGameQuestSequenceBranchElement
{
	GENERATED_BODY()
public:
	FGameQuestSequenceBranchElement()
		: bAutoDeactivateOtherBranch(true)
		, bInterrupted(false)
	{}

	UPROPERTY(VisibleAnywhere, NotReplicated)
	uint16 Element = 0;
	UPROPERTY(VisibleAnywhere, NotReplicated)
	uint8 bAutoDeactivateOtherBranch : 1;
	UPROPERTY(VisibleAnywhere, NotReplicated)
	uint8 bInterrupted : 1;
	UPROPERTY(VisibleAnywhere)
	TArray<uint16> NextSequences;
};

USTRUCT(meta = (DisplayName = "Sequence Branch"))
struct GAMEQUESTGRAPH_API FGameQuestSequenceBranch : public FGameQuestSequenceBase
{
	GENERATED_BODY()
public:
	FGameQuestSequenceBranch()
		: bIsBranchesActivated(false)
	{}

	UPROPERTY(NotReplicated)
	TArray<uint16> Elements;

	UPROPERTY()
	TArray<FGameQuestSequenceBranchElement> Branches;

	uint8 bIsBranchesActivated : 1;

	TArray<uint16> GetNextSequences() const override;

	void WhenSequenceActivated(bool bHasAuthority) override;
	void WhenSequenceDeactivated(bool bHasAuthority) override;

	void WhenElementFinished(FGameQuestElementBase* FinishedElement, const FGameQuestFinishEvent& OnElementFinishedEvent) override;
	void WhenElementUnfinished(FGameQuestElementBase* FinishedElement) override;

	bool CanActivateBranchElement() const;
	void ActivateBranches(bool bHasAuthority);
	void DeactivateBranches(bool bHasAuthority);
};

USTRUCT(BlueprintType, BlueprintInternalUseOnly)
struct GAMEQUESTGRAPH_API FGameQuestSequenceSubQuestFinishedTag
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere)
	FName TagName;
	UPROPERTY(VisibleAnywhere)
	uint16 PreSubQuestSequence = GameQuest::IdNone;
	UPROPERTY(VisibleAnywhere)
	uint16 PreSubQuestBranch = GameQuest::IdNone;
	UPROPERTY(VisibleAnywhere)
	FName PreFinishedTagName = NAME_None;
	UPROPERTY(VisibleAnywhere)
	TArray<uint16> NextSequences;
};

USTRUCT(meta = (DisplayName = "Sub Quest"))
struct GAMEQUESTGRAPH_API FGameQuestSequenceSubQuest : public FGameQuestSequenceBase
{
	GENERATED_BODY()
public:
	bool ShouldReplicatedSubobject() const override { return true; }
	bool ReplicateSubobject(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

	UPROPERTY(NotReplicated)
	TSoftClassPtr<UGameQuestGraphBase> SubQuestClass;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UGameQuestGraphBase> SubQuestInstance;

	UPROPERTY()
	TArray<FGameQuestSequenceSubQuestFinishedTag> FinishedTags;

	void WhenSequenceActivated(bool bHasAuthority) override;
	void WhenSequenceDeactivated(bool bHasAuthority) override;
	void WhenOnRepValue(const FGameQuestNodeBase& PreValue) override;
	bool IsTickable() const override { return true; }
	void WhenTick(float DeltaSeconds) override;
	TArray<uint16> GetNextSequences() const override;

	void ProcessFinishedTag(const FName& FinishedTagName, const FGameQuestFinishedTag& FinishedTag);
	void WhenSubQuestFinished();
	void WhenSubQuestInterrupted();
private:
	void WhenElementFinished(FGameQuestElementBase* FinishedElement, const FGameQuestFinishEvent& OnElementFinishedEvent) override { checkNoEntry(); }
	void WhenElementUnfinished(FGameQuestElementBase* FinishedElement) override { checkNoEntry(); }
};
