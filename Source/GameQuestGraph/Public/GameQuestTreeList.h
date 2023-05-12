// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameQuestType.h"
#include "Components/Widget.h"
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

	void SetQuest(UGameQuestGraphBase* InGameQuest);
	UGameQuestGraphBase* GetQuest() const { return GameQuest.Get(); }

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
public:
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UGameQuestGraphBase> Quest;
	UFUNCTION(BlueprintCallable)
	void SetQuest(UGameQuestGraphBase* NewQuest);

	// Must Implement GameQuestTreeListElement Interface
	UPROPERTY(EditAnywhere, meta = (MustImplement = "/Script/GameQuestGraph.GameQuestTreeListElement"))
	TSubclassOf<UUserWidget> ElementWidget;
	// Must Implement GameQuestTreeListSequence Interface
	UPROPERTY(EditAnywhere, meta = (MustImplement = "/Script/GameQuestGraph.GameQuestTreeListSequence"))
	TSubclassOf<UUserWidget> SequenceHeader;
	// Must Implement GameQuestTreeListSubQuest Interface
	UPROPERTY(EditAnywhere, meta = (MustImplement = "/Script/GameQuestGraph.GameQuestTreeListSubQuest"))
	TSubclassOf<UUserWidget> SubQuestHeader;
	// Must Implement GameQuestTreeListFinishedTag Interface
	UPROPERTY(EditAnywhere, meta = (MustImplement = "/Script/GameQuestGraph.GameQuestTreeListFinishedTag"))
	TSubclassOf<UUserWidget> FinishedTagWidget;

	class SGameQuestTreeListUMG;
	class SGameQuestTreeListSubUMG;
	class SGameQuestTreeListMainUMG;
	TSharedPtr<SGameQuestTreeListMainUMG> MainQuestTree;
	TSharedRef<SWidget> RebuildWidget() override;
	void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	const FText GetPaletteCategory() override;
#endif
};

UINTERFACE(MinimalAPI)
class UGameQuestTreeListElement : public UInterface
{
	GENERATED_BODY()
};
class GAMEQUESTGRAPH_API IGameQuestTreeListElement
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, Category = GameQuest)
	void WhenSetElement(const UGameQuestGraphBase* Quest, const FGameQuestElementPtr& Element);
};

UINTERFACE(MinimalAPI)
class UGameQuestTreeListSequence : public UInterface
{
	GENERATED_BODY()
};
class GAMEQUESTGRAPH_API IGameQuestTreeListSequence
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, Category = GameQuest)
	void WhenSetSequence(const UGameQuestGraphBase* Quest, const FGameQuestSequencePtr& Sequence);
};

UINTERFACE(MinimalAPI)
class UGameQuestTreeListSubQuest : public UInterface
{
	GENERATED_BODY()
};
class GAMEQUESTGRAPH_API IGameQuestTreeListSubQuest
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, Category = GameQuest)
	void WhenSetSubQuest(const UGameQuestGraphBase* Quest, const FGameQuestSequencePtr& Sequence, const UGameQuestGraphBase* SubQuest);
};

UINTERFACE(MinimalAPI)
class UGameQuestTreeListFinishedTag : public UInterface
{
	GENERATED_BODY()
};
class GAMEQUESTGRAPH_API IGameQuestTreeListFinishedTag
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, Category = GameQuest)
	void WhenSetFinishedTag(const UGameQuestGraphBase* Quest, const FName& FinishedTagName);
};
