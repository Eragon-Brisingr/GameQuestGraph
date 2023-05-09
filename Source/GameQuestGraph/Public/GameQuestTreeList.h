// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameQuestType.h"
#include "Components/Widget.h"
#include "UObject/Object.h"
#include "GameQuestTreeList.generated.h"

namespace GameQuest
{
	class FSequenceNodeBase;
}

class SVerticalBox;
class UGameQuestGraphBase;
struct FGameQuestNodeBase;
struct FGameQuestSequenceBase;
struct FGameQuestSequenceBranch;
struct FGameQuestElementBase;
struct FGameQuestSequenceSubQuest;
struct FGameQuestSequenceSubQuestFinishedTag;

class GAMEQUESTGRAPH_API SGameQuestTreeListBase : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGameQuestTreeListBase)
	{}
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);
	~SGameQuestTreeListBase() override;

	TSharedPtr<SVerticalBox> SequenceList;

	void SetGameQuest(UGameQuestGraphBase* InGameQuest);
	UGameQuestGraphBase* GetGameQuest() const { return GameQuest.Get(); }

	TMap<uint16, TSharedPtr<GameQuest::FSequenceNodeBase>> SequenceWidgetMap;
	virtual TSharedRef<SWidget> CreateElementWidget(const UGameQuestGraphBase* Quest, uint16 ElementId, FGameQuestElementBase* Element, const FGameQuestNodeBase* OwnerNode) const = 0;
	virtual TSharedRef<SWidget> CreateElementList(const UGameQuestGraphBase* Quest, const TArray<uint16>& ElementIds, const GameQuest::FLogicList* ElementLogics, const FGameQuestNodeBase* OwnerNode) const = 0;
	virtual TSharedRef<SWidget> CreateSequenceHeader(const UGameQuestGraphBase* Quest, uint16 SequenceId, FGameQuestSequenceBase* Sequence) const = 0;
	virtual TSharedRef<SWidget> CreateSequenceContent(const UGameQuestGraphBase* Quest, uint16 SequenceId, FGameQuestSequenceBase* Sequence);
	virtual TSharedRef<SWidget> CreateSubQuestHeader(const UGameQuestGraphBase* Quest, FGameQuestSequenceSubQuest* SequenceSubQuest) const = 0;
	virtual TSharedRef<SWidget> CreateFinishedTagWidget(const UGameQuestGraphBase* Quest, FGameQuestSequenceSubQuest* SequenceSubQuest, const FGameQuestSequenceSubQuestFinishedTag& FinishedTag) const = 0;
	virtual TSharedRef<SWidget> ApplySequenceWrapper(const UGameQuestGraphBase* Quest, uint16 SequenceId, FGameQuestSequenceBase* Sequence, const TSharedRef<SWidget>& SequenceWidget) const;
	virtual TSharedRef<SWidget> ApplyBranchElementWrapper(const UGameQuestGraphBase* Quest, FGameQuestSequenceBranch* SequenceBranch, int32 BranchIdx, uint16 ElementId, FGameQuestElementBase* Element, const TSharedRef<SWidget>& ElementWidget) const;
	virtual TSharedRef<SGameQuestTreeListBase> CreateSubTreeList(UGameQuestGraphBase* SubQuest) const = 0;
private:
	void WhenSequenceActivated(FGameQuestSequenceBase* Sequence, uint16 SequenceId);
	void WhenSequenceDeactivated(FGameQuestSequenceBase* Sequence, uint16 SequenceId);

	TWeakObjectPtr<UGameQuestGraphBase> GameQuest;
	FDelegateHandle OnPreSequenceActivatedHandle;
	FDelegateHandle OnPostSequenceDeactivatedHandle;
};

UCLASS()
class GAMEQUESTGRAPH_API UGameQuestTreeList : public UWidget
{
	GENERATED_BODY()
};
