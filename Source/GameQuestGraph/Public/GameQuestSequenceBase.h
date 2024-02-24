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
	friend struct FGameQuestPrivateVisitor;
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
	UPROPERTY(SaveGame)
	uint16 PreSequence{ GameQuest::IdNone };
	uint8 bIsActivated : 1;
	UPROPERTY(SaveGame)
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
	virtual TArray<uint16> GetElementIds() const { unimplemented(); return {}; }
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
	uint16 Element{ 0 };

	UPROPERTY(SaveGame)
	TArray<uint16> NextSequences;

	UScriptStruct* GetNodeStruct() const override { return StaticStruct(); }
	TArray<uint16> GetNextSequences() const override { return NextSequences; }
	TArray<uint16> GetElementIds() const override { return { Element }; }

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

	UPROPERTY(SaveGame)
	TArray<uint16> NextSequences;

	TObjectPtr<UFunction> OnSequenceFinished;

	UScriptStruct* GetNodeStruct() const override { return StaticStruct(); }
	TArray<uint16> GetNextSequences() const override { return NextSequences; }
	TArray<uint16> GetElementIds() const override { return Elements; }

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

	UPROPERTY(VisibleAnywhere, NotReplicated, Category = "GameQuest")
	uint16 Element = 0;
	UPROPERTY(VisibleAnywhere, NotReplicated, Category = "GameQuest")
	uint8 bAutoDeactivateOtherBranch : 1;
	UPROPERTY(VisibleAnywhere, NotReplicated, SaveGame, Category = "GameQuest")
	uint8 bInterrupted : 1;
	UPROPERTY(VisibleAnywhere, SaveGame, Category = "GameQuest")
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

	UPROPERTY(SaveGame)
	TArray<FGameQuestSequenceBranchElement> Branches;

	uint8 bIsBranchesActivated : 1;

	UScriptStruct* GetNodeStruct() const override { return StaticStruct(); }
	TArray<uint16> GetNextSequences() const override;
	TArray<uint16> GetElementIds() const override;

	void WhenSequenceActivated(bool bHasAuthority) override;
	void WhenSequenceDeactivated(bool bHasAuthority) override;

	void WhenElementFinished(FGameQuestElementBase* FinishedElement, const FGameQuestFinishEvent& OnElementFinishedEvent) override;
	void WhenElementUnfinished(FGameQuestElementBase* FinishedElement) override;

	bool CanActivateBranchElement() const;
	void ActivateBranches(bool bHasAuthority);
	void DeactivateBranches(bool bHasAuthority);
};

USTRUCT(BlueprintType, BlueprintInternalUseOnly)
struct GAMEQUESTGRAPH_API FGameQuestSequenceSubQuestRerouteTag
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere, SaveGame, Category = "GameQuest")
	FName TagName;
	UPROPERTY(VisibleAnywhere, SaveGame, Category = "GameQuest")
	uint16 PreSubQuestSequence = GameQuest::IdNone;
	UPROPERTY(VisibleAnywhere, SaveGame, Category = "GameQuest")
	uint16 PreSubQuestBranch = GameQuest::IdNone;
	UPROPERTY(VisibleAnywhere, SaveGame, Category = "GameQuest")
	FName PreRerouteTagName = NAME_None;
	UPROPERTY(VisibleAnywhere, SaveGame, Category = "GameQuest")
	TArray<uint16> NextSequences;
};

struct FStreamableHandle;

USTRUCT(meta = (DisplayName = "Sub Quest"))
struct GAMEQUESTGRAPH_API FGameQuestSequenceSubQuest : public FGameQuestSequenceBase
{
	GENERATED_BODY()
public:
	bool ShouldReplicatedSubobject() const override { return true; }
	bool ReplicateSubobject(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

	UPROPERTY(NotReplicated)
	TSoftClassPtr<UGameQuestGraphBase> SubQuestClass;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "GameQuest")
	TObjectPtr<UGameQuestGraphBase> SubQuestInstance;

	UPROPERTY()
	FName CustomEntryName = NAME_None;

	UPROPERTY(SaveGame)
	TArray<FGameQuestSequenceSubQuestRerouteTag> RerouteTags;

	void WhenSequenceActivated(bool bHasAuthority) override;
	void WhenSequenceDeactivated(bool bHasAuthority) override;
	void WhenOnRepValue(const FGameQuestNodeBase& PreValue) override;
	bool IsTickable() const override { return true; }
	void WhenTick(float DeltaSeconds) override;
	TArray<uint16> GetNextSequences() const override;
	TArray<uint16> GetElementIds() const override { return {}; }

	void ProcessRerouteTag(const FName& RerouteTagName, const FGameQuestRerouteTag& RerouteTag);
	void WhenSubQuestFinished();
	void WhenSubQuestInterrupted();
private:
	TSharedPtr<FStreamableHandle> AsyncLoadHandle;
	void WhenElementFinished(FGameQuestElementBase* FinishedElement, const FGameQuestFinishEvent& OnElementFinishedEvent) override { checkNoEntry(); }
	void WhenElementUnfinished(FGameQuestElementBase* FinishedElement) override { checkNoEntry(); }
};
