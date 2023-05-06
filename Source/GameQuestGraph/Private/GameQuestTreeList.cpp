// Fill out your copyright notice in the Description page of Project Settings.


#include "GameQuestTreeList.h"

#include "GameQuestElementBase.h"
#include "GameQuestGraphBase.h"
#include "GameQuestSequenceBase.h"
#include "Widgets/Layout/SExpandableArea.h"

#define LOCTEXT_NAMESPACE "GameQuestGraph"

namespace GameQuest
{
	class SNextSequenceList : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SNextSequenceList)
		{}
		SLATE_END_ARGS()

		TSharedPtr<SVerticalBox> SequenceList;
		TArray<FGameQuestSequenceBase*> ManagedSequences;

		void Construct(const FArguments& InArgs, SGameQuestTreeListBase* TreeList, const TArray<uint16>& Sequences)
		{
			AddNextSequences(TreeList, Sequences);
		}
		void AddNextSequences(SGameQuestTreeListBase* TreeList, const TArray<uint16>& SequenceIds)
		{
			if (SequenceIds.Num() == 0)
			{
				return;
			}
			const UGameQuestGraphBase* Quest = TreeList->GetGameQuest();
			if (SequenceList == nullptr && SequenceIds.Num() == 1 && ManagedSequences.Num() == 0)
			{
				const uint16 SequenceId = SequenceIds[0];
				FGameQuestSequenceBase* Sequence = Quest->GetSequencePtr(SequenceId);
				ManagedSequences.Add(Sequence);
				ChildSlot
					[
						TreeList->ApplySequenceWrapper(SequenceId, Sequence,
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(TreeList->ElementPadding.Left - 18.f, TreeList->ElementPadding.Top, TreeList->ElementPadding.Right, TreeList->ElementPadding.Bottom)
							[
								TreeList->CreateSequenceHeader(SequenceId, Sequence)
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								TreeList->CreateSequenceContent(SequenceId, Sequence)
							])
					];
			}
			else
			{
				const FMargin HeaderPadding{ TreeList->ElementPadding.Left - 19.f, TreeList->ElementPadding.Top, TreeList->ElementPadding.Right, TreeList->ElementPadding.Bottom };
				auto AddSequenceToList = [&](uint16 SequenceId, FGameQuestSequenceBase* Sequence)
				{
					SequenceList->AddSlot()
					.AutoHeight()
					[
						TreeList->ApplySequenceWrapper(SequenceId, Sequence,
							SNew(SExpandableArea)
							.InitiallyCollapsed(false)
							.BorderImage(FStyleDefaults::GetNoBrush())
							.BorderBackgroundColor(FLinearColor::Transparent)
							.HeaderPadding(HeaderPadding)
							.HeaderContent()
							[
								TreeList->CreateSequenceHeader(SequenceId, Sequence)
							]
							.Padding(FMargin(TreeList->ElementPadding.Left, 0.f, 0.f, 0.f))
							.BodyContent()
							[
								TreeList->CreateSequenceContent(SequenceId, Sequence)
							])
					];
				};
				if (SequenceList == nullptr)
				{
					SequenceList = SNew(SVerticalBox);
					ChildSlot
						[
							SequenceList.ToSharedRef()
						];
					if (ManagedSequences.Num() > 0)
					{
						for (FGameQuestSequenceBase* Sequence : ManagedSequences)
						{
							AddSequenceToList(Sequence->OwnerQuest->GetSequenceId(Sequence), Sequence);
						}
					}
				}
				for (const uint16 SequenceId : SequenceIds)
				{
					FGameQuestSequenceBase* Sequence = Quest->GetSequencePtr(SequenceId);
					if (ManagedSequences.Contains(Sequence))
					{
						continue;
					}
					AddSequenceToList(SequenceId, Sequence);
				};
			}
		}
	};

	class FNodeSequenceBase
	{
	public:
		virtual ~FNodeSequenceBase() = default;
		virtual void BuildNextSequences(SGameQuestTreeListBase* TreeList) = 0;
		virtual void BuildLinkedFinishedTagSequences(const FGameQuestSequenceSubQuestFinishedTag& FinishedTag, SGameQuestTreeListBase* OwnerTreeList, const TArray<uint16>& NextSequences) = 0;
	};

	class FNodeSequenceSingle : public FNodeSequenceBase
	{
	public:
		FGameQuestSequenceSingle* SequenceSingle;
		TSharedPtr<SNextSequenceList> SequenceListWidget;

		TSharedRef<SWidget> Construct(SGameQuestTreeListBase* TreeList, FGameQuestSequenceSingle* InSequenceSingle)
		{
			SequenceSingle = InSequenceSingle;

			const UGameQuestGraphBase* Quest = TreeList->GetGameQuest();
			FGameQuestElementBase* Element = Quest->GetElementPtr(SequenceSingle->Element);
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(TreeList->ElementPadding)
				[
					TreeList->CreateElementWidget(SequenceSingle->Element, Element)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(SequenceListWidget, SNextSequenceList, TreeList, SequenceSingle->NextSequences)
				];
		}

		void BuildNextSequences(SGameQuestTreeListBase* TreeList) override
		{
			SequenceListWidget->AddNextSequences(TreeList, SequenceSingle->NextSequences);
		}
		void BuildLinkedFinishedTagSequences(const FGameQuestSequenceSubQuestFinishedTag& FinishedTag, SGameQuestTreeListBase* OwnerTreeList, const TArray<uint16>& NextSequences) override
		{
			SequenceListWidget->AddNextSequences(OwnerTreeList, NextSequences);
		}
	};

	class FNodeSequenceList : public FNodeSequenceBase
	{
	public:
		FGameQuestSequenceList* SequenceList;
		TSharedPtr<SNextSequenceList> SequenceListWidget;

		TSharedRef<SWidget> Construct(SGameQuestTreeListBase* TreeList, FGameQuestSequenceList* InSequenceList)
		{
			SequenceList = InSequenceList;
			const UGameQuestGraphBase* Quest = TreeList->GetGameQuest();
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(TreeList->ElementPadding)
				[
					TreeList->CreateElementList(SequenceList->Elements, Quest->GetLogicList(SequenceList))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(SequenceListWidget, SNextSequenceList, TreeList, SequenceList->NextSequences)
				];
		}

		void BuildNextSequences(SGameQuestTreeListBase* TreeList) override
		{
			SequenceListWidget->AddNextSequences(TreeList, SequenceList->NextSequences);
		}
		void BuildLinkedFinishedTagSequences(const FGameQuestSequenceSubQuestFinishedTag& FinishedTag, SGameQuestTreeListBase* OwnerTreeList, const TArray<uint16>& NextSequences) override
		{
			SequenceListWidget->AddNextSequences(OwnerTreeList, NextSequences);
		}
	};

	class FNodeSequenceBranch : public FNodeSequenceBase
	{
	public:
		FGameQuestSequenceBranch* SequenceBranch;

		class FNodeBranchElement
		{
		public:
			struct FBranchWidget
			{
				FBranchWidget() = default;
				TSharedPtr<SNextSequenceList> SequenceList;
				uint8 bBuilt : 1;
			};
			TArray<FBranchWidget> BranchWidgets;
			TSharedRef<SWidget> Construct(SGameQuestTreeListBase* TreeList, FGameQuestSequenceBranch* InSequenceBranch)
			{
				const TArray<FGameQuestSequenceBranchElement>& Branches = InSequenceBranch->Branches;
				BranchWidgets.SetNum(Branches.Num());
				const UGameQuestGraphBase* Quest = TreeList->GetGameQuest();
				auto CreateElementWidget = [&](uint16 ElementId, FGameQuestElementBase* Element)->TSharedRef<SWidget>
				{
					if (const FGameQuestElementBranchList* BranchList = GameQuestCast<FGameQuestElementBranchList>(Element))
					{
						return TreeList->CreateElementList(BranchList->Elements, BranchList->OwnerQuest->GetLogicList(BranchList));
					}
					return TreeList->CreateElementWidget(ElementId, Element);
				};

				const TSharedRef<SVerticalBox> BranchList = SNew(SVerticalBox)
						.Visibility_Lambda([InSequenceBranch]
						{
							return InSequenceBranch->bIsActivated == false || InSequenceBranch->bIsBranchesActivated ? EVisibility::Visible : EVisibility::Collapsed;
						});
				for (int32 Idx = 0; Idx < Branches.Num(); ++Idx)
				{
					const FGameQuestSequenceBranchElement& Branch = Branches[Idx];
					FGameQuestElementBase* Element = Quest->GetElementPtr(Branch.Element);
					FBranchWidget& BranchVisualData = BranchWidgets[Idx];
					BranchVisualData.bBuilt = Branch.NextSequences.Num() != 0;
					BranchList->AddSlot()
						.AutoHeight()
						.Padding(TreeList->ElementPadding)
						[
							TreeList->ApplyBranchElementWrapper(InSequenceBranch, Idx, Branch.Element, Element, CreateElementWidget(Branch.Element, Element))
						];
					BranchList->AddSlot()
						.AutoHeight()
						.Padding(FMargin(Branches.Num() > 1 ? TreeList->ElementPadding.Left : 0.f, 0.f, 0.f, 0.f))
						[
							SAssignNew(BranchVisualData.SequenceList, SNextSequenceList, TreeList, Branch.NextSequences)
								.Visibility(BranchVisualData.bBuilt ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed)
						];
				}
				return BranchList;
			}
		};

		FNodeBranchElement BranchElement;
		TSharedRef<SWidget> Construct(SGameQuestTreeListBase* TreeList, FGameQuestSequenceBranch* InSequenceBranch)
		{
			SequenceBranch = InSequenceBranch;

			return SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(TreeList->ElementPadding)
				[
					TreeList->CreateElementList(SequenceBranch->Elements, SequenceBranch->OwnerQuest->GetLogicList(SequenceBranch))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					BranchElement.Construct(TreeList, InSequenceBranch)
				];
		}

		void BuildNextSequences(SGameQuestTreeListBase* TreeList) override
		{
			for (int32 Idx = 0; Idx < SequenceBranch->Branches.Num(); ++Idx)
			{
				const FGameQuestSequenceBranchElement& Branch = SequenceBranch->Branches[Idx];
				FNodeBranchElement::FBranchWidget& BranchVisualData = BranchElement.BranchWidgets[Idx];
				if (BranchVisualData.bBuilt == false && Branch.NextSequences.Num() > 0)
				{
					BranchVisualData.bBuilt = true;
					BranchVisualData.SequenceList->AddNextSequences(TreeList, Branch.NextSequences);
					BranchVisualData.SequenceList->SetVisibility(EVisibility::SelfHitTestInvisible);
				}
			}
		}
		void BuildLinkedFinishedTagSequences(const FGameQuestSequenceSubQuestFinishedTag& FinishedTag, SGameQuestTreeListBase* OwnerTreeList, const TArray<uint16>& NextSequences) override
		{
			for (int32 Idx = 0; Idx < SequenceBranch->Branches.Num(); ++Idx)
			{
				const FGameQuestSequenceBranchElement& Branch = SequenceBranch->Branches[Idx];
				FNodeBranchElement::FBranchWidget& BranchVisualData = BranchElement.BranchWidgets[Idx];
				if (Branch.Element != FinishedTag.PreSubQuestBranch)
				{
					continue;
				}
				BranchVisualData.bBuilt = true;
				BranchVisualData.SequenceList->AddNextSequences(OwnerTreeList, NextSequences);
				BranchVisualData.SequenceList->SetVisibility(EVisibility::SelfHitTestInvisible);
			}
		}
	};

	class FNodeSequenceSubQuest : public FNodeSequenceBase
	{
	public:
		FGameQuestSequenceSubQuest* SequenceSubQuest;
		TSharedPtr<SVerticalBox> VerticalBox;
		TSharedPtr<SGameQuestTreeListBase> SubTreeList;
		FTSTicker::FDelegateHandle TickerHandle;
		TMap<FName, TSharedPtr<FNodeSequenceBase>> NextSequenceMap;

		TSharedRef<SWidget> Construct(SGameQuestTreeListBase* TreeList, FGameQuestSequenceSubQuest* InSequenceSubQuest)
		{
			SequenceSubQuest = InSequenceSubQuest;
			VerticalBox = SNew(SVerticalBox);
			auto CreateSubQuestTree = [this, TreeList]
			{
				SubTreeList = TreeList->CreateSubTreeList(SequenceSubQuest->SubQuestInstance);
				VerticalBox->AddSlot()
				.AutoHeight()
				.Padding(FMargin(0.f))
				[
					SubTreeList.ToSharedRef()
				];
				BuildNextSequences(TreeList);
			};
			if (SequenceSubQuest->SubQuestInstance)
			{
				CreateSubQuestTree();
			}
			else
			{
				TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(SequenceSubQuest->OwnerQuest.Get(), [this, CreateSubQuestTree](float)
				{
					if (SequenceSubQuest->SubQuestInstance)
					{
						CreateSubQuestTree();
						return false;
					}
					return true;
				}));
			}
			return VerticalBox.ToSharedRef();
		}
		~FNodeSequenceSubQuest() override
		{
			if (TickerHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
			}
		}

		void BuildNextSequences(SGameQuestTreeListBase* TreeList) override
		{
			for (const FGameQuestSequenceSubQuestFinishedTag& FinishedTag : SequenceSubQuest->FinishedTags)
			{
				if (NextSequenceMap.Contains(FinishedTag.TagName))
				{
					continue;
				}
				TSharedPtr<FNodeSequenceBase> SequenceWidget = SubTreeList->SequenceWidgetMap.FindRef(FinishedTag.PreSubQuestSequence);
				NextSequenceMap.Add(FinishedTag.TagName, SequenceWidget);
				SequenceWidget->BuildLinkedFinishedTagSequences(FinishedTag, TreeList, FinishedTag.NextSequences);
			}
		}
		void BuildLinkedFinishedTagSequences(const FGameQuestSequenceSubQuestFinishedTag& FinishedTag, SGameQuestTreeListBase* OwnerTreeList, const TArray<uint16>& NextSequences) override
		{
			const FGameQuestSequenceSubQuestFinishedTag* SubFinishedTag = SequenceSubQuest->FinishedTags.FindByPredicate([&](const FGameQuestSequenceSubQuestFinishedTag& E){ return E.TagName == FinishedTag.PreFinishedTagName; });
			if (!ensure(SubFinishedTag))
			{
				return;
			}
			const TSharedPtr<FNodeSequenceBase> SequenceWidget = SubTreeList->SequenceWidgetMap.FindRef(SubFinishedTag->PreSubQuestSequence);
			SequenceWidget->BuildLinkedFinishedTagSequences(*SubFinishedTag, OwnerTreeList, NextSequences);
		}
	};

	TSharedRef<SWidget> CreateDefaultSequenceContent(uint16 SequenceId, FGameQuestSequenceBase* Sequence, SGameQuestTreeListBase* TreeList)
	{
		if (FGameQuestSequenceSingle* Single = GameQuestCast<FGameQuestSequenceSingle>(Sequence))
		{
			const TSharedRef<FNodeSequenceSingle> SingleNode = MakeShared<FNodeSequenceSingle>();
			TreeList->SequenceWidgetMap.Add(SequenceId, SingleNode);
			return SingleNode->Construct(TreeList, Single);
		}
		else if (FGameQuestSequenceList* List = GameQuestCast<FGameQuestSequenceList>(Sequence))
		{
			const TSharedRef<FNodeSequenceList> ListNode = MakeShared<FNodeSequenceList>();
			TreeList->SequenceWidgetMap.Add(SequenceId, ListNode);
			return ListNode->Construct(TreeList, List);
		}
		else if (FGameQuestSequenceBranch* Branch = GameQuestCast<FGameQuestSequenceBranch>(Sequence))
		{
			const TSharedRef<FNodeSequenceBranch> BranchNode = MakeShared<FNodeSequenceBranch>();
			TreeList->SequenceWidgetMap.Add(SequenceId, BranchNode);
			return BranchNode->Construct(TreeList, Branch);
		}
		else if (FGameQuestSequenceSubQuest* SubQuest = GameQuestCast<FGameQuestSequenceSubQuest>(Sequence))
		{
			const TSharedRef<FNodeSequenceSubQuest> SubQuestNode = MakeShared<FNodeSequenceSubQuest>();
			TreeList->SequenceWidgetMap.Add(SequenceId, SubQuestNode);
			return SubQuestNode->Construct(TreeList, SubQuest);
		}
		checkNoEntry();
		return SNullWidget::NullWidget;
	}
}

void SGameQuestTreeListBase::Construct(const FArguments& InArgs)
{
	ElementPadding = InArgs._ElementPadding;
}

SGameQuestTreeListBase::~SGameQuestTreeListBase()
{
	if (UGameQuestGraphBase* CurGameQuest = GameQuest.Get())
	{
		CurGameQuest->OnPreSequenceActivatedNative.Remove(OnPreSequenceActivatedHandle);
		CurGameQuest->OnPostSequenceDeactivatedNative.Remove(OnPostSequenceDeactivatedHandle);
	}
}

void SGameQuestTreeListBase::SetGameQuest(UGameQuestGraphBase* InGameQuest)
{
	if (UGameQuestGraphBase* PreGameQuest = GameQuest.Get())
	{
		PreGameQuest->OnPreSequenceActivatedNative.Remove(OnPreSequenceActivatedHandle);
		PreGameQuest->OnPostSequenceDeactivatedNative.Remove(OnPostSequenceDeactivatedHandle);
	}
	SequenceList->ClearChildren();
	SequenceWidgetMap.Empty();
	GameQuest = InGameQuest;
	if (UGameQuestGraphBase* CurGameQuest = GameQuest.Get())
	{
		OnPreSequenceActivatedHandle = CurGameQuest->OnPreSequenceActivatedNative.AddSP(this, &SGameQuestTreeListBase::WhenSequenceActivated);
		OnPostSequenceDeactivatedHandle = CurGameQuest->OnPostSequenceDeactivatedNative.AddSP(this, &SGameQuestTreeListBase::WhenSequenceDeactivated);
		SequenceList->AddSlot()
		[
			SNew(GameQuest::SNextSequenceList, this, CurGameQuest->GetStartSequencesIds())
		];
	}
}

TSharedRef<SWidget> SGameQuestTreeListBase::CreateSequenceContent(uint16 SequenceId, FGameQuestSequenceBase* Sequence)
{
	return GameQuest::CreateDefaultSequenceContent(SequenceId, Sequence, this);
}

TSharedRef<SWidget> SGameQuestTreeListBase::ApplySequenceWrapper(uint16 SequenceId, FGameQuestSequenceBase* Sequence, const TSharedRef<SWidget>& SequenceWidget)
{
	return SequenceWidget;
}

TSharedRef<SWidget> SGameQuestTreeListBase::ApplyBranchElementWrapper(FGameQuestSequenceBranch* SequenceBranch, int32 BranchIdx, uint16 ElementId, FGameQuestElementBase* Element, const TSharedRef<SWidget>& ElementWidget)
{
	return ElementWidget;
}

void SGameQuestTreeListBase::WhenSequenceActivated(FGameQuestSequenceBase* Sequence, uint16 SequenceId)
{
	const UGameQuestGraphBase* Quest = GetGameQuest();
	if (Sequence->PreSequence == GameQuest::IdNone)
	{
		if (SequenceWidgetMap.Contains(SequenceId))
		{
			return;
		}
		SequenceList->AddSlot()
		.AutoHeight()
		.Padding(FMargin{ 0.f, 0.f, 0.f, 0.f })
		[
			SNew(GameQuest::SNextSequenceList, this, Quest->GetStartSequencesIds())
		];
		return;
	}
	TSharedPtr<GameQuest::FNodeSequenceBase> ExistedWidget;
	const FGameQuestSequenceBase* PreSequence;
	for (uint16 PreSequenceId = Sequence->PreSequence; PreSequenceId != GameQuest::IdNone; PreSequenceId = PreSequence->PreSequence)
	{
		PreSequence = Quest->GetSequencePtr(PreSequenceId);
		if (const TSharedPtr<GameQuest::FNodeSequenceBase> FindWidget = SequenceWidgetMap.FindRef(PreSequenceId))
		{
			ExistedWidget = FindWidget;
			break;
		}
	}
	if (ensure(ExistedWidget))
	{
		ExistedWidget->BuildNextSequences(this);
	}
}

void SGameQuestTreeListBase::WhenSequenceDeactivated(FGameQuestSequenceBase* Sequence, uint16 SequenceId)
{
	if (const TSharedPtr<GameQuest::FNodeSequenceBase> FindWidget = SequenceWidgetMap.FindRef(SequenceId))
	{
		FindWidget->BuildNextSequences(this);
	}
}

#undef LOCTEXT_NAMESPACE
