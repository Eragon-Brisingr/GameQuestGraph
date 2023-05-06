// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameQuestType.h"
#include "Components/Widget.h"
#include "UObject/Object.h"
#include "GameQuestTreeList.generated.h"

namespace GameQuest
{
	class FNodeSequenceBase;
}

class SVerticalBox;
class UGameQuestGraphBase;
struct FGameQuestSequenceBase;
struct FGameQuestSequenceBranch;
struct FGameQuestElementBase;

class GAMEQUESTGRAPH_API SGameQuestTreeListBase : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGameQuestTreeListBase)
		: _ElementPadding(FMargin(20.f, 2.f, 4.f, 0.f))
	{}
		SLATE_ARGUMENT(FMargin, ElementPadding)
	SLATE_END_ARGS()
	void Construct(const FArguments& InArgs);
	~SGameQuestTreeListBase();

	TSharedPtr<SVerticalBox> SequenceList;

	void SetGameQuest(UGameQuestGraphBase* InGameQuest);
	UGameQuestGraphBase* GetGameQuest() const { return GameQuest.Get(); }

	TMap<uint16, TSharedPtr<GameQuest::FNodeSequenceBase>> SequenceWidgetMap;
	virtual TSharedRef<SWidget> CreateElementWidget(uint16 ElementId, FGameQuestElementBase* Element) const = 0;
	virtual TSharedRef<SWidget> CreateElementList(const TArray<uint16>& ElementIds, const GameQuest::FLogicList* ElementLogics) const = 0;
	virtual TSharedRef<SWidget> CreateSequenceHeader(uint16 SequenceId, FGameQuestSequenceBase* Sequence) const = 0;
	virtual TSharedRef<SWidget> CreateSequenceContent(uint16 SequenceId, FGameQuestSequenceBase* Sequence);
	virtual TSharedRef<SWidget> ApplySequenceWrapper(uint16 SequenceId, FGameQuestSequenceBase* Sequence, const TSharedRef<SWidget>& SequenceWidget);
	virtual TSharedRef<SWidget> ApplyBranchElementWrapper(FGameQuestSequenceBranch* SequenceBranch, int32 BranchIdx, uint16 ElementId, FGameQuestElementBase* Element, const TSharedRef<SWidget>& ElementWidget);
	virtual TSharedRef<SGameQuestTreeListBase> CreateSubTreeList(UGameQuestGraphBase* SubQuest) const = 0;
private:
	void WhenSequenceActivated(FGameQuestSequenceBase* Sequence, uint16 SequenceId);
	void WhenSequenceDeactivated(FGameQuestSequenceBase* Sequence, uint16 SequenceId);

	TWeakObjectPtr<UGameQuestGraphBase> GameQuest;
	FDelegateHandle OnPreSequenceActivatedHandle;
	FDelegateHandle OnPostSequenceDeactivatedHandle;
public:
	FMargin ElementPadding;
};

UCLASS()
class GAMEQUESTGRAPH_API UGameQuestTreeList : public UWidget
{
	GENERATED_BODY()
};
