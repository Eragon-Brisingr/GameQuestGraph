// Fill out your copyright notice in the Description page of Project Settings.


#include "GameQuestTreeList.h"

#include "GameQuestElementBase.h"
#include "GameQuestGraphBase.h"
#include "GameQuestSequenceBase.h"
#include "Blueprint/UserWidget.h"
#include "Slate/SObjectWidget.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SSeparator.h"

#define LOCTEXT_NAMESPACE "GameQuestGraph"

namespace GameQuest
{
	const FMargin TreeNodePadding{ 20.f, 0.f, 0.f, 0.f };
	struct FFinishedTagNode
	{
		FGameQuestSequenceSubQuestFinishedTag Tag;
		TSharedRef<SWidget> TagWidget;
		struct FSequence
		{
			uint16 Id;
			FGameQuestSequenceBase* Ref;
			TSharedRef<SWidget> Header;
			TSharedRef<SWidget> Content;
		};
		TArray<FSequence> Sequences;
	};

	class FNextSequenceListNode
	{
	public:
		TSharedPtr<SVerticalBox> SequenceList;
		struct FManagedSequence
		{
			uint16 Id;
			FGameQuestSequenceBase* Ref;
			TSharedRef<SWidget> Header;
			TSharedRef<SWidget> Content;
		};
		TArray<FManagedSequence> ManagedSequences;
		TArray<FFinishedTagNode> FinishedTagNodes;

		TSharedRef<SWidget> Construct(SGameQuestTreeListBase* TreeList,  const UGameQuestGraphBase* Quest, const TArray<uint16>& Sequences)
		{
			SequenceList = SNew(SVerticalBox);
			AddNextSequences(TreeList, Quest, Sequences);
			return SequenceList.ToSharedRef();
		}
		void AddNextSequences(SGameQuestTreeListBase* TreeList, const UGameQuestGraphBase* Quest, const TArray<uint16>& SequenceIds)
		{
			if (SequenceIds.Num() == 0)
			{
				return;
			}
			for (const uint16 SequenceId : SequenceIds)
			{
				if (ManagedSequences.ContainsByPredicate([SequenceId](const FManagedSequence& E){ return E.Id == SequenceId; }))
				{
					continue;
				}
				FGameQuestSequenceBase* Sequence = Quest->GetSequencePtr(SequenceId);
				const TSharedRef<SWidget> Header = TreeList->CreateSequenceHeader(Quest, SequenceId, Sequence);
				const TSharedRef<SWidget> Content = TreeList->CreateSequenceContent(Quest, SequenceId, Sequence);
				ManagedSequences.Add({ SequenceId, Sequence, Header, Content });
			}
			RefreshManagedSequences(TreeList);
		}
		void RefreshManagedSequences(SGameQuestTreeListBase* TreeList)
		{
			SequenceList->ClearChildren();
			if (ManagedSequences.Num() == 1 && FinishedTagNodes.Num() == 0)
			{
				const FManagedSequence& Sequence = ManagedSequences[0];
				SequenceList->AddSlot()
				.AutoHeight()
				[
					TreeList->ApplySequenceWrapper(Sequence.Ref->OwnerQuest, Sequence.Id, Sequence.Ref,
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f)
						[
							Sequence.Header
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							Sequence.Content
						])
				];
			}
			else if (ManagedSequences.Num() == 0 && FinishedTagNodes.Num() == 1 && FinishedTagNodes[0].Sequences.Num() == 1)
			{
				const auto& Sequence = FinishedTagNodes[0].Sequences[0];
				SequenceList->AddSlot()
				.AutoHeight()
				.Padding(TreeNodePadding)
				[
					FinishedTagNodes[0].TagWidget
				];
				SequenceList->AddSlot()
				.AutoHeight()
				[
					TreeList->ApplySequenceWrapper(Sequence.Ref->OwnerQuest, Sequence.Id, Sequence.Ref,
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(2.f, TreeNodePadding.Top, TreeNodePadding.Right, TreeNodePadding.Bottom)
						[
							Sequence.Header
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							Sequence.Content
						])
				];
			}
			else
			{
				const FMargin HeaderPadding{ 1.f, TreeNodePadding.Top, TreeNodePadding.Right, TreeNodePadding.Bottom };
				for (const FManagedSequence& Sequence : ManagedSequences)
				{
					SequenceList->AddSlot()
					.AutoHeight()
					[
						TreeList->ApplySequenceWrapper(Sequence.Ref->OwnerQuest, Sequence.Id, Sequence.Ref,
							SNew(SExpandableArea)
							.InitiallyCollapsed(false)
							.BorderImage(FStyleDefaults::GetNoBrush())
							.BorderBackgroundColor(FLinearColor::Transparent)
							.HeaderPadding(HeaderPadding)
							.HeaderContent()
							[
								Sequence.Header
							]
							.Padding(0.f)
							.BodyContent()
							[
								Sequence.Content
							])
					];
				}
				for (const FFinishedTagNode& Node : FinishedTagNodes)
				{
					SequenceList->AddSlot()
					.AutoHeight()
					.Padding(TreeNodePadding)
					[
						Node.TagWidget
					];
					for (const auto& Sequence : Node.Sequences)
					{
						SequenceList->AddSlot()
						.AutoHeight()
						[
							TreeList->ApplySequenceWrapper(Sequence.Ref->OwnerQuest, Sequence.Id, Sequence.Ref,
								SNew(SExpandableArea)
								.InitiallyCollapsed(false)
								.BorderImage(FStyleDefaults::GetNoBrush())
								.BorderBackgroundColor(FLinearColor::Transparent)
								.HeaderPadding(HeaderPadding)
								.HeaderContent()
								[
									Sequence.Header
								]
								.Padding(0.f)
								.BodyContent()
								[
									Sequence.Content
								])
						];
					}
				}
			}
		}
		void AddFinishedTagSequences(SGameQuestTreeListBase* TreeList, const FFinishedTagNode& FinishedTagNode)
		{
			FinishedTagNodes.Add(FinishedTagNode);
			RefreshManagedSequences(TreeList);
		}
		void RemoveFinishedTagSequence(SGameQuestTreeListBase* TreeList, const FFinishedTagNode& FinishedTagNode)
		{
			const int32 Idx = FinishedTagNodes.IndexOfByPredicate([&](const FFinishedTagNode& E){ return E.Tag.TagName == FinishedTagNode.Tag.TagName; });
			if (ensure(Idx != INDEX_NONE))
			{
				FinishedTagNodes.RemoveAt(Idx);
				RefreshManagedSequences(TreeList);
			}
		}
	};

	class FSequenceNodeBase
	{
	public:
		virtual ~FSequenceNodeBase() = default;
		virtual void BuildNextSequences(SGameQuestTreeListBase* TreeList, const UGameQuestGraphBase* Quest) = 0;
		virtual void ShowFinishedTagSequences(const FGameQuestSequenceSubQuestFinishedTag& FinishedTag, SGameQuestTreeListBase* OwnerTreeList, const FFinishedTagNode& FinishedTagNode) = 0;
		virtual void HiddenFinishedTagSequences(const FGameQuestSequenceSubQuestFinishedTag& FinishedTag, SGameQuestTreeListBase* OwnerTreeList, const FFinishedTagNode& FinishedTagNode) = 0;
	};

	class FSequenceSingleNode : public FSequenceNodeBase
	{
	public:
		FGameQuestSequenceSingle* SequenceSingle;
		FNextSequenceListNode NextSequenceListNode;

		TSharedRef<SWidget> Construct(SGameQuestTreeListBase* TreeList, const UGameQuestGraphBase* Quest, FGameQuestSequenceSingle* InSequenceSingle)
		{
			SequenceSingle = InSequenceSingle;

			FGameQuestElementBase* Element = Quest->GetElementPtr(SequenceSingle->Element);
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(TreeNodePadding)
				[
					TreeList->CreateElementWidget(Quest, SequenceSingle->Element, Element, SequenceSingle)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					NextSequenceListNode.Construct(TreeList, Quest, SequenceSingle->NextSequences)
				];
		}

		void BuildNextSequences(SGameQuestTreeListBase* TreeList, const UGameQuestGraphBase* Quest) override
		{
			NextSequenceListNode.AddNextSequences(TreeList, Quest, SequenceSingle->NextSequences);
		}
		void ShowFinishedTagSequences(const FGameQuestSequenceSubQuestFinishedTag& FinishedTag, SGameQuestTreeListBase* OwnerTreeList, const FFinishedTagNode& FinishedTagNode) override
		{
			NextSequenceListNode.AddFinishedTagSequences(OwnerTreeList, FinishedTagNode);
		}
		void HiddenFinishedTagSequences(const FGameQuestSequenceSubQuestFinishedTag& FinishedTag, SGameQuestTreeListBase* OwnerTreeList, const FFinishedTagNode& FinishedTagNode) override
		{
			NextSequenceListNode.RemoveFinishedTagSequence(OwnerTreeList, FinishedTagNode);
		}
	};

	class FSequenceListNode : public FSequenceNodeBase
	{
	public:
		FGameQuestSequenceList* SequenceList;
		FNextSequenceListNode NextSequenceListNode;

		TSharedRef<SWidget> Construct(SGameQuestTreeListBase* TreeList, const UGameQuestGraphBase* Quest, FGameQuestSequenceList* InSequenceList)
		{
			SequenceList = InSequenceList;
			return SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(TreeNodePadding)
				[
					TreeList->CreateElementList(Quest, SequenceList->Elements, Quest->GetLogicList(SequenceList), SequenceList)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					NextSequenceListNode.Construct(TreeList, Quest, SequenceList->NextSequences)
				];
		}

		void BuildNextSequences(SGameQuestTreeListBase* TreeList, const UGameQuestGraphBase* Quest) override
		{
			NextSequenceListNode.AddNextSequences(TreeList, Quest, SequenceList->NextSequences);
		}
		void ShowFinishedTagSequences(const FGameQuestSequenceSubQuestFinishedTag& FinishedTag, SGameQuestTreeListBase* OwnerTreeList, const FFinishedTagNode& FinishedTagNode) override
		{
			NextSequenceListNode.AddFinishedTagSequences(OwnerTreeList, FinishedTagNode);
		}
		void HiddenFinishedTagSequences(const FGameQuestSequenceSubQuestFinishedTag& FinishedTag, SGameQuestTreeListBase* OwnerTreeList, const FFinishedTagNode& FinishedTagNode) override
		{
			NextSequenceListNode.RemoveFinishedTagSequence(OwnerTreeList, FinishedTagNode);
		}
	};

	class FSequenceBranchNode : public FSequenceNodeBase
	{
	public:
		FGameQuestSequenceBranch* SequenceBranch;

		class FBranchElementNode
		{
		public:
			struct FBranchWidget
			{
				FNextSequenceListNode NextSequenceListNode;
				uint8 bBuilt : 1;
			};
			TArray<FBranchWidget> BranchWidgets;
			TSharedRef<SWidget> Construct(SGameQuestTreeListBase* TreeList, const UGameQuestGraphBase* Quest, FGameQuestSequenceBranch* InSequenceBranch)
			{
				const TArray<FGameQuestSequenceBranchElement>& Branches = InSequenceBranch->Branches;
				BranchWidgets.SetNum(Branches.Num());
				auto CreateElementWidget = [&](uint16 ElementId, FGameQuestElementBase* Element)->TSharedRef<SWidget>
				{
					if (const FGameQuestElementBranchList* BranchList = GameQuestCast<FGameQuestElementBranchList>(Element))
					{
						return TreeList->CreateElementList(Quest, BranchList->Elements, BranchList->OwnerQuest->GetLogicList(BranchList), InSequenceBranch);
					}
					return TreeList->CreateElementWidget(Quest, ElementId, Element, InSequenceBranch);
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
						.Padding(TreeNodePadding)
						[
							TreeList->ApplyBranchElementWrapper(Quest, InSequenceBranch, Idx, Branch.Element, Element, CreateElementWidget(Branch.Element, Element))
						];
					TSharedRef<SWidget> NextSequenceList = BranchVisualData.NextSequenceListNode.Construct(TreeList, Quest, Branch.NextSequences);
					NextSequenceList->SetVisibility(BranchVisualData.bBuilt ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
					BranchList->AddSlot()
						.AutoHeight()
						.Padding(FMargin(Branches.Num() > 1 ? TreeNodePadding.Left : 0.f, 0.f, 0.f, 0.f))
						[
							NextSequenceList
						];
				}
				return BranchList;
			}
		};

		FBranchElementNode BranchElement;
		TSharedRef<SWidget> Construct(SGameQuestTreeListBase* TreeList, const UGameQuestGraphBase* Quest, FGameQuestSequenceBranch* InSequenceBranch)
		{
			SequenceBranch = InSequenceBranch;

			return SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(TreeNodePadding)
				[
					TreeList->CreateElementList(SequenceBranch->OwnerQuest, SequenceBranch->Elements, SequenceBranch->OwnerQuest->GetLogicList(SequenceBranch), SequenceBranch)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					BranchElement.Construct(TreeList, Quest, InSequenceBranch)
				];
		}

		void BuildNextSequences(SGameQuestTreeListBase* TreeList, const UGameQuestGraphBase* Quest) override
		{
			for (int32 Idx = 0; Idx < SequenceBranch->Branches.Num(); ++Idx)
			{
				const FGameQuestSequenceBranchElement& Branch = SequenceBranch->Branches[Idx];
				FBranchElementNode::FBranchWidget& BranchVisualData = BranchElement.BranchWidgets[Idx];
				if (BranchVisualData.bBuilt == false && Branch.NextSequences.Num() > 0)
				{
					BranchVisualData.bBuilt = true;
					BranchVisualData.NextSequenceListNode.AddNextSequences(TreeList, Quest, Branch.NextSequences);
					BranchVisualData.NextSequenceListNode.SequenceList->SetVisibility(EVisibility::SelfHitTestInvisible);
				}
			}
		}
		void ShowFinishedTagSequences(const FGameQuestSequenceSubQuestFinishedTag& FinishedTag, SGameQuestTreeListBase* OwnerTreeList, const FFinishedTagNode& FinishedTagNode) override
		{
			for (int32 Idx = 0; Idx < SequenceBranch->Branches.Num(); ++Idx)
			{
				const FGameQuestSequenceBranchElement& Branch = SequenceBranch->Branches[Idx];
				FBranchElementNode::FBranchWidget& BranchVisualData = BranchElement.BranchWidgets[Idx];
				if (Branch.Element != FinishedTag.PreSubQuestBranch)
				{
					continue;
				}
				BranchVisualData.NextSequenceListNode.AddFinishedTagSequences(OwnerTreeList, FinishedTagNode);
				BranchVisualData.NextSequenceListNode.SequenceList->SetVisibility(EVisibility::SelfHitTestInvisible);
			}
		}
		void HiddenFinishedTagSequences(const FGameQuestSequenceSubQuestFinishedTag& FinishedTag, SGameQuestTreeListBase* OwnerTreeList, const FFinishedTagNode& FinishedTagNode) override
		{
			for (int32 Idx = 0; Idx < SequenceBranch->Branches.Num(); ++Idx)
			{
				const FGameQuestSequenceBranchElement& Branch = SequenceBranch->Branches[Idx];
				FBranchElementNode::FBranchWidget& BranchVisualData = BranchElement.BranchWidgets[Idx];
				if (Branch.Element != FinishedTag.PreSubQuestBranch)
				{
					continue;
				}
				BranchVisualData.NextSequenceListNode.RemoveFinishedTagSequence(OwnerTreeList, FinishedTagNode);
			}
		}
	};

	class FSequenceSubQuestNode : public FSequenceNodeBase
	{
	public:
		FGameQuestSequenceSubQuest* SequenceSubQuest;
		TSharedPtr<SVerticalBox> ContentBox;
		FNextSequenceListNode NextSequenceListNode;
		bool bIsExpand = true;
		TSharedPtr<SGameQuestTreeListBase> SubTreeList;
		FTSTicker::FDelegateHandle TickerHandle;
		TMap<FName, FFinishedTagNode> FinishedTagMap;

		TSharedRef<SWidget> Construct(SGameQuestTreeListBase* TreeList, const UGameQuestGraphBase* Quest, FGameQuestSequenceSubQuest* InSequenceSubQuest)
		{
			SequenceSubQuest = InSequenceSubQuest;
			ContentBox = SNew(SVerticalBox);
			auto CreateSubQuestTree = [this, TreeList, Quest]
			{
				const FMargin HeaderPadding{ 1.f, TreeNodePadding.Top, TreeNodePadding.Right, TreeNodePadding.Bottom };
				SubTreeList = TreeList->CreateSubTreeList(SequenceSubQuest->SubQuestInstance);
				ContentBox->AddSlot()
				.AutoHeight()
				[
					SNew(SExpandableArea)
					.InitiallyCollapsed(!bIsExpand)
					.OnAreaExpansionChanged_Lambda([this, TreeList](bool bExpand)
					{
						bIsExpand = bExpand;
						if (bIsExpand)
						{
							for (const auto& [Tag, Node] : FinishedTagMap)
							{
								NextSequenceListNode.RemoveFinishedTagSequence(TreeList, Node);
								const TSharedPtr<FSequenceNodeBase> SequenceWidget = SubTreeList->SequenceWidgetMap.FindRef(Node.Tag.PreSubQuestSequence);
								SequenceWidget->ShowFinishedTagSequences(Node.Tag, TreeList, Node);
							}
						}
						else
						{
							for (const auto& [Tag, Node] : FinishedTagMap)
							{
								const TSharedPtr<FSequenceNodeBase> SequenceWidget = SubTreeList->SequenceWidgetMap.FindRef(Node.Tag.PreSubQuestSequence);
								SequenceWidget->HiddenFinishedTagSequences(Node.Tag, TreeList, Node);
								NextSequenceListNode.AddFinishedTagSequences(TreeList, Node);
							}
						}
					})
					.BorderImage(FStyleDefaults::GetNoBrush())
					.BorderBackgroundColor(FLinearColor::Transparent)
					.HeaderPadding(HeaderPadding)
					.HeaderContent()
					[
						TreeList->CreateSubQuestHeader(Quest, SequenceSubQuest)
					]
					.Padding(FMargin{ TreeNodePadding.Left, 0.f, 0.f, 0.f })
					.BodyContent()
					[
						SubTreeList.ToSharedRef()
					]
				];
				ContentBox->AddSlot()
				.AutoHeight()
				[
					NextSequenceListNode.Construct(TreeList, Quest, {})
				];
				BuildNextSequences(TreeList, Quest);
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
			return ContentBox.ToSharedRef();
		}
		~FSequenceSubQuestNode() override
		{
			if (TickerHandle.IsValid())
			{
				FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
			}
		}
		void BuildNextSequences(SGameQuestTreeListBase* TreeList, const UGameQuestGraphBase* Quest) override
		{
			for (const FGameQuestSequenceSubQuestFinishedTag& FinishedTag : SequenceSubQuest->FinishedTags)
			{
				if (FinishedTagMap.Contains(FinishedTag.TagName))
				{
					continue;
				}
				const TSharedRef<SWidget> TagWidget = TreeList->CreateFinishedTagWidget(Quest, SequenceSubQuest, FinishedTag);
				FFinishedTagNode& FinishedTagNode = FinishedTagMap.Add(FinishedTag.TagName, { FinishedTag, TagWidget });

				const TSharedPtr<FSequenceNodeBase> SequenceWidget = SubTreeList->SequenceWidgetMap.FindRef(FinishedTag.PreSubQuestSequence);
				for (const uint16 SequenceId : FinishedTag.NextSequences)
				{
					FGameQuestSequenceBase* Sequence = Quest->GetSequencePtr(SequenceId);
					const TSharedRef<SWidget> Header = TreeList->CreateSequenceHeader(Quest, SequenceId, Sequence);
					const TSharedRef<SWidget> Content = TreeList->CreateSequenceContent(Quest, SequenceId, Sequence);
					FinishedTagNode.Sequences.Add({ SequenceId, Sequence, Header, Content });
				}
				if (bIsExpand)
				{
					SequenceWidget->ShowFinishedTagSequences(FinishedTag, TreeList, FinishedTagNode);
				}
				else
				{
					NextSequenceListNode.AddFinishedTagSequences(TreeList, FinishedTagNode);
				}
			}
		}
		void ShowFinishedTagSequences(const FGameQuestSequenceSubQuestFinishedTag& FinishedTag, SGameQuestTreeListBase* OwnerTreeList, const FFinishedTagNode& FinishedTagNode) override
		{
			const FGameQuestSequenceSubQuestFinishedTag* SubFinishedTag = SequenceSubQuest->FinishedTags.FindByPredicate([&](const FGameQuestSequenceSubQuestFinishedTag& E){ return E.TagName == FinishedTag.PreFinishedTagName; });
			check(SubFinishedTag);
			const TSharedPtr<FSequenceNodeBase> SequenceWidget = SubTreeList->SequenceWidgetMap.FindRef(SubFinishedTag->PreSubQuestSequence);
			SequenceWidget->ShowFinishedTagSequences(*SubFinishedTag, OwnerTreeList, FinishedTagNode);
		}
		void HiddenFinishedTagSequences(const FGameQuestSequenceSubQuestFinishedTag& FinishedTag, SGameQuestTreeListBase* OwnerTreeList, const FFinishedTagNode& FinishedTagNode) override
		{
			const FGameQuestSequenceSubQuestFinishedTag* SubFinishedTag = SequenceSubQuest->FinishedTags.FindByPredicate([&](const FGameQuestSequenceSubQuestFinishedTag& E){ return E.TagName == FinishedTag.PreFinishedTagName; });
			check(SubFinishedTag);
			const TSharedPtr<FSequenceNodeBase> SequenceWidget = SubTreeList->SequenceWidgetMap.FindRef(SubFinishedTag->PreSubQuestSequence);
			SequenceWidget->HiddenFinishedTagSequences(*SubFinishedTag, OwnerTreeList, FinishedTagNode);
		}
	};

	class FSequenceGenericNode : public FSequenceNodeBase
	{
	public:
		FGameQuestSequenceBase* Sequence;
		FNextSequenceListNode NextSequenceListNode;

		TSharedRef<SWidget> Construct(SGameQuestTreeListBase* TreeList, const UGameQuestGraphBase* Quest, FGameQuestSequenceBase* InSequence)
		{
			Sequence = InSequence;
			return NextSequenceListNode.Construct(TreeList, Quest, Sequence->GetNextSequences());
		}

		void BuildNextSequences(SGameQuestTreeListBase* TreeList, const UGameQuestGraphBase* Quest) override
		{
			NextSequenceListNode.AddNextSequences(TreeList, Quest, Sequence->GetNextSequences());
		}
		void ShowFinishedTagSequences(const FGameQuestSequenceSubQuestFinishedTag& FinishedTag, SGameQuestTreeListBase* OwnerTreeList, const FFinishedTagNode& FinishedTagNode) override
		{
			NextSequenceListNode.AddFinishedTagSequences(OwnerTreeList, FinishedTagNode);
		}
		void HiddenFinishedTagSequences(const FGameQuestSequenceSubQuestFinishedTag& FinishedTag, SGameQuestTreeListBase* OwnerTreeList, const FFinishedTagNode& FinishedTagNode) override
		{
			NextSequenceListNode.RemoveFinishedTagSequence(OwnerTreeList, FinishedTagNode);
		}
	};

	TSharedRef<SWidget> CreateDefaultSequenceContent(const UGameQuestGraphBase* Quest, uint16 SequenceId, FGameQuestSequenceBase* Sequence, SGameQuestTreeListBase* TreeList)
	{
		if (FGameQuestSequenceSingle* Single = GameQuestCast<FGameQuestSequenceSingle>(Sequence))
		{
			const TSharedRef<FSequenceSingleNode> SingleNode = MakeShared<FSequenceSingleNode>();
			TreeList->SequenceWidgetMap.Add(SequenceId, SingleNode);
			return SingleNode->Construct(TreeList, Quest, Single);
		}
		else if (FGameQuestSequenceList* List = GameQuestCast<FGameQuestSequenceList>(Sequence))
		{
			const TSharedRef<FSequenceListNode> ListNode = MakeShared<FSequenceListNode>();
			TreeList->SequenceWidgetMap.Add(SequenceId, ListNode);
			return ListNode->Construct(TreeList, Quest, List);
		}
		else if (FGameQuestSequenceBranch* Branch = GameQuestCast<FGameQuestSequenceBranch>(Sequence))
		{
			const TSharedRef<FSequenceBranchNode> BranchNode = MakeShared<FSequenceBranchNode>();
			TreeList->SequenceWidgetMap.Add(SequenceId, BranchNode);
			return BranchNode->Construct(TreeList, Quest, Branch);
		}
		else if (FGameQuestSequenceSubQuest* SubQuest = GameQuestCast<FGameQuestSequenceSubQuest>(Sequence))
		{
			const TSharedRef<FSequenceSubQuestNode> SubQuestNode = MakeShared<FSequenceSubQuestNode>();
			TreeList->SequenceWidgetMap.Add(SequenceId, SubQuestNode);
			return SubQuestNode->Construct(TreeList, Quest, SubQuest);
		}
		else
		{
			const TSharedRef<FSequenceGenericNode> GenericNode = MakeShared<FSequenceGenericNode>();
			TreeList->SequenceWidgetMap.Add(SequenceId, GenericNode);
			return GenericNode->Construct(TreeList, Quest, SubQuest);
		}
	}
}

void SGameQuestTreeListBase::Construct(const FArguments& InArgs)
{

}

SGameQuestTreeListBase::~SGameQuestTreeListBase()
{
	if (UGameQuestGraphBase* CurGameQuest = GameQuest.Get())
	{
		CurGameQuest->OnPreSequenceActivatedNative.Remove(OnPreSequenceActivatedHandle);
		CurGameQuest->OnPostSequenceDeactivatedNative.Remove(OnPostSequenceDeactivatedHandle);
	}
}

void SGameQuestTreeListBase::SetQuest(UGameQuestGraphBase* InGameQuest)
{
	if (UGameQuestGraphBase* PreGameQuest = GameQuest.Get())
	{
		PreGameQuest->OnPreSequenceActivatedNative.Remove(OnPreSequenceActivatedHandle);
		PreGameQuest->OnPostSequenceDeactivatedNative.Remove(OnPostSequenceDeactivatedHandle);
	}
	SequenceList->ClearChildren();
	SequenceWidgetMap.Empty();
	GameQuest = InGameQuest;
	if (UGameQuestGraphBase* Quest = GameQuest.Get())
	{
		OnPreSequenceActivatedHandle = Quest->OnPreSequenceActivatedNative.AddSP(this, &SGameQuestTreeListBase::WhenSequenceActivated);
		OnPostSequenceDeactivatedHandle = Quest->OnPostSequenceDeactivatedNative.AddSP(this, &SGameQuestTreeListBase::WhenSequenceDeactivated);
		SequenceList->AddSlot()
		[
			GameQuest::FNextSequenceListNode{}.Construct(this, Quest, Quest->GetStartSequencesIds())
		];
	}
}

TSharedRef<SWidget> SGameQuestTreeListBase::CreateSequenceContent(const UGameQuestGraphBase* Quest, uint16 SequenceId, FGameQuestSequenceBase* Sequence)
{
	return GameQuest::CreateDefaultSequenceContent(Quest, SequenceId, Sequence, this);
}

TSharedRef<SWidget> SGameQuestTreeListBase::ApplySequenceWrapper(const UGameQuestGraphBase* Quest, uint16 SequenceId, FGameQuestSequenceBase* Sequence, const TSharedRef<SWidget>& SequenceWidget) const
{
	return SequenceWidget;
}

TSharedRef<SWidget> SGameQuestTreeListBase::ApplyBranchElementWrapper(const UGameQuestGraphBase* Quest, FGameQuestSequenceBranch* SequenceBranch, int32 BranchIdx, uint16 ElementId, FGameQuestElementBase* Element, const TSharedRef<SWidget>& ElementWidget) const
{
	return ElementWidget;
}

void SGameQuestTreeListBase::WhenSequenceActivated(FGameQuestSequenceBase* Sequence, uint16 SequenceId)
{
	const UGameQuestGraphBase* Quest = GetQuest();
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
			GameQuest::FNextSequenceListNode{}.Construct(this, Quest, Quest->GetStartSequencesIds())
		];
		return;
	}
	TSharedPtr<GameQuest::FSequenceNodeBase> ExistedWidget;
	const FGameQuestSequenceBase* PreSequence;
	for (uint16 PreSequenceId = Sequence->PreSequence; PreSequenceId != GameQuest::IdNone; PreSequenceId = PreSequence->PreSequence)
	{
		PreSequence = Quest->GetSequencePtr(PreSequenceId);
		if (const TSharedPtr<GameQuest::FSequenceNodeBase> FindWidget = SequenceWidgetMap.FindRef(PreSequenceId))
		{
			ExistedWidget = FindWidget;
			break;
		}
	}
	if (ensure(ExistedWidget))
	{
		ExistedWidget->BuildNextSequences(this, Quest);
	}
}

void SGameQuestTreeListBase::WhenSequenceDeactivated(FGameQuestSequenceBase* Sequence, uint16 SequenceId)
{
	if (const TSharedPtr<GameQuest::FSequenceNodeBase> FindWidget = SequenceWidgetMap.FindRef(SequenceId))
	{
		FindWidget->BuildNextSequences(this, Sequence->OwnerQuest);
	}
}

class UGameQuestTreeList::SGameQuestTreeListUMG : public SGameQuestTreeListBase
{
	using Super = SGameQuestTreeListBase;
protected:
	TObjectPtr<UGameQuestTreeList> Owner;
public:
	void Construct(const FArguments& InArgs, UGameQuestTreeList* InOwner)
	{
		Super::Construct(InArgs);
		Owner = InOwner;

		ChildSlot
		[
			SAssignNew(SequenceList, SVerticalBox)
		];
	}
	TSharedRef<SWidget> CreateElementWidget(const UGameQuestGraphBase* Quest, uint16 ElementId, FGameQuestElementBase* Element, const FGameQuestNodeBase* OwnerNode) const override
	{
		if (Owner->ElementWidget == nullptr || !ensure(Owner->ElementWidget->ImplementsInterface(UGameQuestTreeListElement::StaticClass())))
		{
			return SNew(STextBlock).Text(FText::FromName(Element->GetNodeName()));
		}
		UUserWidget* UserWidget = CreateWidget<UUserWidget>(Owner, Owner->ElementWidget);
		IGameQuestTreeListElement::Execute_WhenSetElement(UserWidget, Quest, *Element);
		return UserWidget->TakeWidget();
	}
	TSharedRef<SWidget> CreateElementList(const UGameQuestGraphBase* Quest, const TArray<uint16>& ElementIds, const GameQuest::FLogicList& ElementLogics, const FGameQuestNodeBase* OwnerNode) const override
	{
		static FTextBlockStyle LogicHintTextStyle{ FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("NormalText")) };
		LogicHintTextStyle.Font.OutlineSettings.OutlineSize = 1;

		const TSharedRef<SVerticalBox> ElementList = SNew(SVerticalBox);
		for (int32 Idx = 0; Idx < ElementIds.Num(); ++Idx)
		{
			if (Idx >= 1)
			{
				const EGameQuestSequenceLogic Logic = ElementLogics[Idx - 1];
				if (Logic == EGameQuestSequenceLogic::Or)
				{
					ElementList->AddSlot()
						.AutoHeight()
						.Padding(2.f)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Center)
							[
								SNew(SSeparator)
								.Orientation(Orient_Horizontal)
								.Thickness(2.0f)
								.ColorAndOpacity(FSlateColor{ FLinearColor::Gray })
							]
							+ SOverlay::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Visibility(EVisibility::HitTestInvisible)
								.Justification(ETextJustify::Center)
								.TextStyle(&LogicHintTextStyle)
								.Text(LOCTEXT("Or", "Or"))
							]
						];
				}
			}
			const uint16 ElementId = ElementIds[Idx];
			FGameQuestElementBase* Element = Quest->GetElementPtr(ElementId);
			ElementList->AddSlot()
				.AutoHeight()
				.Padding(0.f)
				[
					CreateElementWidget(Quest, ElementId, Element, OwnerNode)
				];
		}
		return ElementList;
	}
	TSharedRef<SWidget> CreateSequenceHeader(const UGameQuestGraphBase* Quest, uint16 SequenceId, FGameQuestSequenceBase* Sequence) const override
	{
		if (Owner->SequenceHeader == nullptr || !ensure(Owner->SequenceHeader->ImplementsInterface(UGameQuestTreeListSequence::StaticClass())))
		{
			return SNew(STextBlock).Text(FText::FromName(Sequence->GetNodeName()));
		}
		UUserWidget* UserWidget = CreateWidget<UUserWidget>(Owner, Owner->SequenceHeader);
		IGameQuestTreeListSequence::Execute_WhenSetSequence(UserWidget, Quest, *Sequence);
		return UserWidget->TakeWidget();
	}
	TSharedRef<SWidget> CreateSubQuestHeader(const UGameQuestGraphBase* Quest, FGameQuestSequenceSubQuest* SequenceSubQuest) const override
	{
		if (Owner->SubQuestHeader == nullptr || !ensure(Owner->SubQuestHeader->ImplementsInterface(UGameQuestTreeListSubQuest::StaticClass())))
		{
			return SNew(STextBlock).Text(FText::FromName(SequenceSubQuest->GetNodeName()));
		}
		UUserWidget* UserWidget = CreateWidget<UUserWidget>(Owner, Owner->SubQuestHeader);
		IGameQuestTreeListSubQuest::Execute_WhenSetSubQuest(UserWidget, Quest, *SequenceSubQuest, SequenceSubQuest->SubQuestInstance);
		return UserWidget->TakeWidget();
	}
	TSharedRef<SWidget> CreateFinishedTagWidget(const UGameQuestGraphBase* Quest, FGameQuestSequenceSubQuest* SequenceSubQuest, const FGameQuestSequenceSubQuestFinishedTag& FinishedTag) const override
	{
		if (Owner->FinishedTagWidget == nullptr || !ensure(Owner->FinishedTagWidget->ImplementsInterface(UGameQuestTreeListFinishedTag::StaticClass())))
		{
			return SNew(STextBlock).Text(FText::FromName(FinishedTag.TagName));
		}
		UUserWidget* UserWidget = CreateWidget<UUserWidget>(Owner, Owner->FinishedTagWidget);
		IGameQuestTreeListFinishedTag::Execute_WhenSetFinishedTag(UserWidget, Quest, FinishedTag.TagName);
		return UserWidget->TakeWidget();
	}
};

class UGameQuestTreeList::SGameQuestTreeListSubUMG : public SGameQuestTreeListUMG
{
	using Super = SGameQuestTreeListUMG;
public:
	void Construct(const FArguments& InArgs, UGameQuestTreeList* InOwner, UGameQuestGraphBase* SubQuest)
	{
		Super::Construct(InArgs, InOwner);
		SetQuest(SubQuest);
	}
	TSharedRef<SGameQuestTreeListBase> CreateSubTreeList(UGameQuestGraphBase* SubQuest) const override
	{
		return SNew(SGameQuestTreeListSubUMG, Owner.Get(), SubQuest);
	}
};

class UGameQuestTreeList::SGameQuestTreeListMainUMG : public SGameQuestTreeListUMG
{
	TSharedRef<SGameQuestTreeListBase> CreateSubTreeList(UGameQuestGraphBase* SubQuest) const override
	{
		return SNew(SGameQuestTreeListSubUMG, Owner.Get(), SubQuest);
	}
};

void UGameQuestTreeList::SetQuest(UGameQuestGraphBase* NewQuest)
{
	if (Quest == NewQuest)
	{
		return;
	}
	Quest = NewQuest;
	if (Quest && MainQuestTree)
	{
		MainQuestTree->SetQuest(Quest);
	}
}

TSharedRef<SWidget> UGameQuestTreeList::RebuildWidget()
{
	MainQuestTree = SNew(SGameQuestTreeListMainUMG, this);
	if (Quest)
	{
		MainQuestTree->SetQuest(Quest);
	}
	return MainQuestTree.ToSharedRef();
}

void UGameQuestTreeList::ReleaseSlateResources(bool bReleaseChildren)
{
	MainQuestTree.Reset();
	Super::ReleaseSlateResources(bReleaseChildren);
}

#if WITH_EDITOR
const FText UGameQuestTreeList::GetPaletteCategory()
{
	return LOCTEXT("Misc", "Misc");
}
#endif

#undef LOCTEXT_NAMESPACE
