﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "GameQuestGraphEditor.h"

#include "BlueprintEditorModes.h"
#include "BPNode_GameQuestElementBase.h"
#include "BPNode_GameQuestEntryEvent.h"
#include "BPNode_GameQuestRerouteTag.h"
#include "BPNode_GameQuestNodeBase.h"
#include "BPNode_GameQuestSequenceBase.h"
#include "EdGraphSchemaGameQuest.h"
#include "GameQuestElementBase.h"
#include "GameQuestGraphBase.h"
#include "GameQuestGraphBlueprint.h"
#include "GameQuestSequenceBase.h"
#include "GameQuestGraphEditorStyle.h"
#include "GameQuestTreeList.h"
#include "GraphEditorActions.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"

#define LOCTEXT_NAMESPACE "GameQuestGraphEditor"

FGraphAppearanceInfo FGameQuestGraphEditor::GetGraphAppearance(UEdGraph* InGraph) const
{
	FGraphAppearanceInfo AppearanceInfo = Super::GetGraphAppearance(InGraph);
	if (InGraph->Schema == UEdGraphSchemaGameQuest::StaticClass())
	{
		AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_GameQuestGraph", "GameQuest");
	}
	return AppearanceInfo;
}

void FGameQuestGraphEditor::OpenEditor(const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost>& InToolkitHost, UGameQuestGraphBlueprint* GameQuestBlueprint)
{
	const TSharedRef<FGameQuestGraphEditor> Editor = MakeShared<FGameQuestGraphEditor>();
	Editor->InitGameQuestGraphEditor(InMode, InToolkitHost, GameQuestBlueprint);
}

void FGameQuestGraphEditor::InitGameQuestGraphEditor(const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost>& InToolkitHost, UGameQuestGraphBlueprint* InGameQuestBlueprint)
{
	GameQuestBlueprint = InGameQuestBlueprint;
	InitBlueprintEditor(InMode, InToolkitHost, { InGameQuestBlueprint }, false);

	FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>(TEXT("Kismet"));
	BlueprintEditorModule.OnBlueprintEditorOpened().Broadcast(GameQuestBlueprint->BlueprintType);
}

void FGameQuestGraphEditor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const UGameQuestGraphBlueprint* QuestBlueprint = GameQuestBlueprint.Get();
	if (QuestBlueprint == nullptr)
	{
		return;
	}
	const UGameQuestGraphBase* WatchedQuest = Cast<UGameQuestGraphBase>(QuestBlueprint->GetObjectBeingDebugged());
	if (WatchedQuest == nullptr)
	{
		return;
	}
	const UClass* QuestClass = WatchedQuest->GetClass();
	TArray<UEdGraph*> Graphs{ QuestBlueprint->GameQuestGraph };
	QuestBlueprint->GameQuestGraph->GetAllChildrenGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* GraphNode : Graph->Nodes)
		{
			UBPNode_GameQuestNodeBase* QuestNode = Cast<UBPNode_GameQuestNodeBase>(GraphNode);
			if (QuestNode == nullptr)
			{
				continue;
			}
			const FStructProperty* StructProperty = FindFProperty<FStructProperty>(QuestClass, QuestNode->GetRefVarName());
			if (StructProperty == nullptr)
			{
				continue;
			}
			using EDebugState = UBPNode_GameQuestNodeBase::EDebugState;
			if (StructProperty->Struct->IsChildOf(FGameQuestElementBase::StaticStruct()))
			{
				const FGameQuestElementBase* Element = StructProperty->ContainerPtrToValuePtr<FGameQuestElementBase>(WatchedQuest);
				if (Element->bIsFinished)
				{
					QuestNode->DebugState = EDebugState::Finished;
				}
				else if (Element->bIsActivated)
				{
					QuestNode->DebugState = EDebugState::Activated;
				}
				else if (Element->IsInterrupted())
				{
					QuestNode->DebugState = EDebugState::Interrupted;
				}
				else
				{
					QuestNode->DebugState = EDebugState::Deactivated;
				}
			}
			else if (StructProperty->Struct->IsChildOf(FGameQuestSequenceBase::StaticStruct()))
			{
				const FGameQuestSequenceBase* Sequence = StructProperty->ContainerPtrToValuePtr<FGameQuestSequenceBase>(WatchedQuest);
				switch (Sequence->GetSequenceState())
				{
				case FGameQuestSequenceBase::EState::Deactivated:
					QuestNode->DebugState = EDebugState::Deactivated;
					break;
				case FGameQuestSequenceBase::EState::Activated:
					QuestNode->DebugState = EDebugState::Activated;
					break;
				case FGameQuestSequenceBase::EState::Finished:
					QuestNode->DebugState = EDebugState::Finished;
					break;
				case FGameQuestSequenceBase::EState::Interrupted:
					QuestNode->DebugState = EDebugState::Interrupted;
					break;
				default: ;
				}
			}
		}
	}
}

class FGameQuestGraphEditor::SGameQuestTreeViewerBase : public SGameQuestTreeListBase
{
	using Super = SGameQuestTreeListBase;
public:
	void Construct(const FArguments& InArgs, UGameQuestGraphBase* DebugGameQuest)
	{
		using namespace GameQuestGraphEditorStyle;
		Super::Construct(InArgs);

		if (DebugGameQuest)
		{
			SetQuest(DebugGameQuest);
		}
	}

	TSharedRef<SWidget> CreateElementWidget(const UGameQuestGraphBase* Quest, uint16 ElementId, FGameQuestElementBase* Element, const FGameQuestNodeBase* OwnerNode) const override
	{
		const FString ElementName = Element->GetNodeName().ToString();
		FString ShortName = ElementName.Right(ElementName.Len() - OwnerNode->GetNodeName().GetStringLength() - 1);
		if (Element->bIsOptional)
		{
			ShortName += TEXT(" [Optional]");
		}

		return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.f)
				[
					SNew(SCheckBox)
					.IsEnabled(false)
					.IsChecked_Lambda([=] { return Element->bIsFinished ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(2.f)
				.FillWidth(1.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(ShortName))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
					.ToolTipText(LOCTEXT("BrowseToElementNode", "Browse To Element Node"))
					.OnClicked_Lambda([this, Element]
					{
						JumpToQuestNode(Element->OwnerQuest, Element->GetNodeName());
						return FReply::Handled();
					})
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush(TEXT("Symbols.SearchGlass")))
					]
				];
	}
	TSharedRef<SWidget> CreateElementList(const UGameQuestGraphBase* Quest, const TArray<uint16>& ElementIds, const GameQuest::FLogicList& ElementLogics, const FGameQuestNodeBase* OwnerNode) const override
	{
		static const auto SeparatorBrush = FGameQuestGraphSlateStyle::Get().GetBrush(TEXT("ThinLine.Horizontal"));
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
								.SeparatorImage(SeparatorBrush)
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
		return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(2.f)
				[
					SNew(SImage)
					.Image_Lambda([Sequence]
					{
						static const FSlateBrush* QuestBrush = FGameQuestGraphSlateStyle::Get().GetBrush(TEXT("Graph.Quest"));
						static const FSlateBrush* InterruptBrush = FGameQuestGraphSlateStyle::Get().GetBrush(TEXT("Graph.Event.Interrupt"));
						return Sequence->GetSequenceState() == FGameQuestSequenceBase::EState::Interrupted ? InterruptBrush : QuestBrush;
					})
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(2.f)
				.FillWidth(1.f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text_Lambda([=]
					{
						const FText Describe = FText::FromName(Sequence->GetNodeName());
						return Describe;
					})
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(2.f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
					.ToolTipText(LOCTEXT("BrowseToSequenceNode", "Browse To Sequence Node"))
					.OnClicked_Lambda([this, Sequence]
					{
						JumpToQuestNode(Sequence->OwnerQuest, Sequence->GetNodeName());
						return FReply::Handled();
					})
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush(TEXT("Symbols.SearchGlass")))
					]
				];
	}
	TSharedRef<SWidget> CreateSubQuestHeader(const UGameQuestGraphBase* Quest, FGameQuestSequenceSubQuest* SequenceSubQuest) const override
	{
		static const FSlateBrush* SubQuestBrush = FGameQuestGraphSlateStyle::Get().GetBrush(TEXT("Graph.Event.Start"));
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(4.f)
			[
				SNew(SImage)
				.Image(SubQuestBrush)
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(1.f)
			.Padding(2.f)
			[
				SNew(STextBlock)
				.Text(SequenceSubQuest->SubQuestInstance ? SequenceSubQuest->SubQuestInstance->GetClass()->GetDisplayNameText() : LOCTEXT("None", "None"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.ToolTipText(LOCTEXT("BrowseToSubQuestClass", "Browse To Sub Quest Class"))
				.OnClicked_Lambda([this, SequenceSubQuest]
				{
					if (UObject* Blueprint = SequenceSubQuest->SubQuestInstance->GetClass()->ClassGeneratedBy)
					{
						TArray<UObject*> BrowserToObjects{ Blueprint };
						GEditor->SyncBrowserToObjects(BrowserToObjects);
					}
					return FReply::Handled();
				})
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush(TEXT("Symbols.SearchGlass")))
				]
			];
	}
	TSharedRef<SWidget> CreateRerouteTagWidget(const UGameQuestGraphBase* Quest, FGameQuestSequenceSubQuest* SequenceSubQuest, const FGameQuestSequenceSubQuestRerouteTag& RerouteTag) const override
	{
		static const FSlateBrush* RouteBrush = FGameQuestGraphSlateStyle::Get().GetBrush(TEXT("Graph.Reroute"));
		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(4.f)
			[
				SNew(SImage)
				.Image(RouteBrush)
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(1.f)
			.Padding(2.f)
			[
				SNew(STextBlock)
				.Text(FText::FromName(RerouteTag.TagName))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.f)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
				.ToolTipText(RerouteTag.TagName != FGameQuestRerouteTag::FinishCompletedTagName ? LOCTEXT("BrowseToRerouteTagNode", "Browse To Reroute Tag Node") : LOCTEXT("BrowseToRerouteTagPreNode", "Browse To Finished Node") )
				.OnClicked_Lambda([this, SequenceSubQuest, RerouteTag]
				{
					if (RerouteTag.TagName != FGameQuestRerouteTag::FinishCompletedTagName)
					{
						JumpToRerouteTagNode(SequenceSubQuest->SubQuestInstance, RerouteTag);
					}
					else if (RerouteTag.PreSubQuestBranch != GameQuest::IdNone)
					{
						const FGameQuestElementBase* BranchElement = SequenceSubQuest->SubQuestInstance->GetElementPtr(RerouteTag.PreSubQuestBranch);
						JumpToQuestNode(SequenceSubQuest->SubQuestInstance, BranchElement->GetNodeName());
					}
					else if (RerouteTag.PreSubQuestSequence != GameQuest::IdNone)
					{
						const FGameQuestSequenceBase* Sequence = SequenceSubQuest->SubQuestInstance->GetSequencePtr(RerouteTag.PreSubQuestSequence);
						JumpToQuestNode(SequenceSubQuest->SubQuestInstance, Sequence->GetNodeName());
					}
					return FReply::Handled();
				})
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush(TEXT("Symbols.SearchGlass")))
				]
			];
	}
	TSharedRef<SWidget> ApplySequenceWrapper(const UGameQuestGraphBase* Quest, uint16 SequenceId, FGameQuestSequenceBase* Sequence, const TSharedRef<SWidget>& SequenceWidget) const override
	{
		static const auto BodyBrush = FGameQuestGraphSlateStyle::Get().GetBrush(TEXT("GroupBorder"));
		static const auto BorderImage = FGameQuestGraphSlateStyle::Get().GetBrush(TEXT("Border"));
		return SNew(SBorder)
		.BorderImage(BodyBrush)
		.BorderBackgroundColor_Lambda([Sequence]
		{
			using namespace GameQuestGraphEditorStyle;
			const FGameQuestSequenceBase::EState State = Sequence->GetSequenceState();
			switch (State) {
				case FGameQuestSequenceBase::EState::Deactivated:
					return Debug::Deactivated;
				case FGameQuestSequenceBase::EState::Activated:
					return Debug::Activated;
				case FGameQuestSequenceBase::EState::Finished:
					return Debug::Finished;
				case FGameQuestSequenceBase::EState::Interrupted:
					return Debug::Interrupted;
				default: ;
			}
			return Debug::Deactivated;
		})
		.Padding(0.f)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SequenceWidget
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SImage)
				.Image(BorderImage)
				.Visibility(EVisibility::HitTestInvisible)
			]
		];
	}
	TSharedRef<SWidget> ApplyBranchElementWrapper(const UGameQuestGraphBase* Quest, FGameQuestSequenceBranch* SequenceBranch, int32 BranchIdx, uint16 ElementId, FGameQuestElementBase* Element, const TSharedRef<SWidget>& ElementWidget) const override
	{
		static const auto BodyBrush = FGameQuestGraphSlateStyle::Get().GetBrush(TEXT("GroupBorder"));
		return SNew(SBorder)
		.BorderImage(BodyBrush)
		.Padding(0.f)
		.BorderBackgroundColor_Lambda([SequenceBranch, Element, BranchIdx]
		{
			using namespace GameQuestGraphEditorStyle;
			const FGameQuestSequenceBranchElement& Branch = SequenceBranch->Branches[BranchIdx];
			if (Branch.bInterrupted)
			{
				return Debug::Interrupted;
			}
			if (Element->bIsFinished)
			{
				return Debug::Finished;
			}
			if (SequenceBranch->bIsActivated)
			{
				return Debug::Activated;
			}
			return Debug::Deactivated;
		})
		[
			ElementWidget
		];
	}
	static void JumpToQuestNode(const UGameQuestGraphBase* Quest, const FName& NodeName)
	{
		const UGameQuestGraphBlueprint* Blueprint = Cast<UGameQuestGraphBlueprint>(Quest->GetClass()->ClassGeneratedBy);
		if (Blueprint == nullptr || Blueprint->GameQuestGraph == nullptr)
		{
			return;
		}
		TArray<UEdGraph*> Graphs{ Blueprint->GameQuestGraph };
		Blueprint->GameQuestGraph->GetAllChildrenGraphs(Graphs);
		for (const UEdGraph* Graph : Graphs)
		{
			for (const UEdGraphNode* Node : Graph->Nodes)
			{
				if (const UBPNode_GameQuestNodeBase* QuestNode = Cast<UBPNode_GameQuestNodeBase>(Node))
				{
					if (QuestNode->GetRefVarName() == NodeName)
					{
						FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Node);
						return;
					}
				}
			}
		}
	}
	static void JumpToRerouteTagNode(const UGameQuestGraphBase* Quest, const FGameQuestSequenceSubQuestRerouteTag& RerouteTag)
	{
		const UGameQuestGraphBlueprint* Blueprint = Cast<UGameQuestGraphBlueprint>(Quest->GetClass()->ClassGeneratedBy);
		if (Blueprint == nullptr || Blueprint->GameQuestGraph == nullptr)
		{
			return;
		}
		for (const UEdGraphNode* Node : Blueprint->GameQuestGraph->Nodes)
		{
			const UBPNode_GameQuestRerouteTag* RerouteTagNode = Cast<UBPNode_GameQuestRerouteTag>(Node);
			if (RerouteTagNode == nullptr)
			{
				continue;
			}
			struct FPreNodeFinder
			{
				uint16 PreNodeId;
				TArray<const UEdGraphNode*> Visited;
				const UBPNode_GameQuestNodeBase* Do(const UEdGraphNode* Node)
				{
					if (Visited.Contains(Node))
					{
						return nullptr;
					}
					Visited.Add(Node);
					if (const UBPNode_GameQuestNodeBase* QuestNode = Cast<UBPNode_GameQuestNodeBase>(Node))
					{
						if (QuestNode->NodeId == PreNodeId)
						{
							return QuestNode;
						}
					}
					for (UEdGraphPin* Pin : Node->Pins)
					{
						if (Pin->Direction == EGPD_Input && (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec))
						{
							for (UEdGraphPin* LinkToPin : Pin->LinkedTo)
							{
								if (const UBPNode_GameQuestNodeBase* QuestNode = Do(TunnelPin::Redirect(LinkToPin)->GetOwningNode()))
								{
									return QuestNode;
								}
							}
						}
					}
					return nullptr;
				};
			};
			if (RerouteTagNode->RerouteTag == RerouteTag.TagName
				&& FPreNodeFinder{ RerouteTag.PreSubQuestBranch != GameQuest::IdNone ? RerouteTag.PreSubQuestBranch : RerouteTag.PreSubQuestSequence }.Do(RerouteTagNode) != nullptr)
			{
				FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(RerouteTagNode);
				return;
			}
		}
	}
};

class FGameQuestGraphEditor::SGameQuestTreeViewerSubQuest : public SGameQuestTreeViewerBase
{
	using Super = SGameQuestTreeViewerBase;
public:
	void Construct(const FArguments& InArgs, UGameQuestGraphBase* SubQuest)
	{
		ChildSlot
		[
			SAssignNew(SequenceList, SVerticalBox)
		];

		using namespace GameQuestGraphEditorStyle;
		Super::Construct(InArgs, SubQuest);
	}
	TSharedRef<SGameQuestTreeListBase> CreateSubTreeList(UGameQuestGraphBase* SubQuest) const override
	{
		return SNew(FGameQuestGraphEditor::SGameQuestTreeViewerSubQuest, SubQuest);
	}
};

class FGameQuestGraphEditor::SGameQuestTreeViewer : public SGameQuestTreeViewerBase
{
	using Super = SGameQuestTreeViewerBase;
public:
	void Construct(const FArguments& InArgs, FGameQuestGraphEditor* InGameQuestEditor)
	{
		ChildSlot
		[
			SAssignNew(ScrollBox, SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(SequenceList, SVerticalBox)
			]
		];

		using namespace GameQuestGraphEditorStyle;
		GameQuestBlueprint = InGameQuestEditor->GameQuestBlueprint;
		GameQuestEditor = InGameQuestEditor;
		OnSetObjectBeingDebuggedHandle = GameQuestBlueprint->OnSetObjectBeingDebugged().AddLambda([this](UObject* DebugObject)
		{
			SetQuest(Cast<UGameQuestGraphBase>(DebugObject));
		});
		Super::Construct(InArgs, Cast<UGameQuestGraphBase>(GameQuestBlueprint->GetObjectBeingDebugged()));
	}
	~SGameQuestTreeViewer() override
	{
		GameQuestBlueprint->OnSetObjectBeingDebugged().Remove(OnSetObjectBeingDebuggedHandle);
	}
	TSharedRef<SGameQuestTreeListBase> CreateSubTreeList(UGameQuestGraphBase* SubQuest) const override
	{
		return SNew(FGameQuestGraphEditor::SGameQuestTreeViewerSubQuest, SubQuest);
	}
private:
	FDelegateHandle OnSetObjectBeingDebuggedHandle;
	TWeakObjectPtr<UGameQuestGraphBlueprint> GameQuestBlueprint = nullptr;
	FGameQuestGraphEditor* GameQuestEditor = nullptr;
	TSharedPtr<SScrollBox> ScrollBox;
};

class FGameQuestGraphEditor::FGameQuestGraphApplicationMode : public FBlueprintEditorUnifiedMode
{
	using Super = FBlueprintEditorUnifiedMode;
public:
	FGameQuestGraphApplicationMode(const TSharedPtr<FGameQuestGraphEditor>& InBlueprintEditor)
		: Super(InBlueprintEditor, ModeName, [](const FName){ return LOCTEXT("GameQuestGraphApplicationMode", "GameQuest"); }, false)
		, GameQuestEditor(InBlueprintEditor.Get())
	{}

	void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override
	{
		Super::RegisterTabFactories(InTabManager);
		WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("GameQuestEditorWorkspaceMenu", "Game Quest Editor"));
		InTabManager->RegisterTabSpawner(TEXT("GameQuestTreeViewer"), FOnSpawnTab::CreateLambda([this](const FSpawnTabArgs& Args)
		{
			TSharedRef<SDockTab> SpawnedTab =
				SNew(SDockTab)
				.Label(LOCTEXT("GameQuestTreeViewer_TabTitle", "Quest Tree Viewer"))
				[
					SNew(FGameQuestGraphEditor::SGameQuestTreeViewer, GameQuestEditor)
				];

			SpawnedTab->SetTabIcon(FAppStyle::GetBrush("ContentBrowser.Sources.Collections"));
			return SpawnedTab;
		}))
		.SetDisplayName(LOCTEXT("GameQuestTreeViewerTab", "Quest Tree Viewer"))
		.SetIcon(FSlateIcon(FAppStyle::Get().GetStyleSetName(), "LevelEditor.Tabs.Outliner"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
	}
	FGameQuestGraphEditor* GameQuestEditor;

	static const FName ModeName;
};
const FName FGameQuestGraphEditor::FGameQuestGraphApplicationMode::ModeName = TEXT("GameQuestGraphApplicationMode");

void FGameQuestGraphEditor::RegisterApplicationModes(const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode, bool bNewlyCreated)
{
	AddApplicationMode(
		FGameQuestGraphApplicationMode::ModeName,
		MakeShared<FGameQuestGraphApplicationMode>(SharedThis(this)));
	SetCurrentMode(FGameQuestGraphApplicationMode::ModeName);
}

void FGameQuestGraphEditor::OnCreateGraphEditorCommands(TSharedPtr<FUICommandList> GraphEditorCommandsList)
{
	GraphEditorCommandsList->MapAction(FGraphEditorCommands::Get().CollapseNodes,
		FExecuteAction::CreateSP(this, &FGameQuestGraphEditor::OnCollapseNodes),
		FCanExecuteAction::CreateSP(this, &FGameQuestGraphEditor::CanCollapseGameQuestEditorNodes)
	);
}

bool FGameQuestGraphEditor::CanCollapseGameQuestEditorNodes() const
{
	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();

	TArray<UBPNode_GameQuestNodeBase*> SelectedGameQuestNodes;

	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		if (UEdGraphNode* SelectedNode = Cast<UEdGraphNode>(*NodeIt))
		{
			if (SelectedNode->IsA<UBPNode_GameQuestEntryEvent>())
			{
				return false;
			}
			if (SelectedNode->IsA<UBPNode_GameQuestRerouteTag>())
			{
				return false;
			}
			if (UBPNode_GameQuestNodeBase* GameQuestNode = Cast<UBPNode_GameQuestNodeBase>(SelectedNode))
			{
				SelectedGameQuestNodes.Add(GameQuestNode);
			}
		}
	}

	for (UBPNode_GameQuestNodeBase* GameQuestNode : SelectedGameQuestNodes)
	{
		if (UBPNode_GameQuestSequenceListBase* SequenceList = Cast<UBPNode_GameQuestSequenceListBase>(GameQuestNode))
		{
			for (UBPNode_GameQuestElementBase* Element : SequenceList->Elements)
			{
				if (SelectedGameQuestNodes.Contains(Element) == false)
				{
					return false;
				}
			}
			if (const UBPNode_GameQuestSequenceBranch* SequenceBranch = Cast<UBPNode_GameQuestSequenceBranch>(SequenceList))
			{
				UEdGraphPin* BranchPin = SequenceBranch->FindPinChecked(GameQuestUtils::Pin::BranchPinName);
				for (const UEdGraphPin* LinkTo : BranchPin->LinkedTo)
				{
					if (UBPNode_GameQuestNodeBase* BranchElementNode = Cast<UBPNode_GameQuestNodeBase>(LinkTo->GetOwningNode()))
					{
						if (SelectedGameQuestNodes.Contains(BranchElementNode) == false)
						{
							return false;
						}
					}
				}
			}
		}
		else if (const UBPNode_GameQuestSequenceSingle* SequenceSingle = Cast<UBPNode_GameQuestSequenceSingle>(GameQuestNode))
		{
			if (SelectedGameQuestNodes.Contains(SequenceSingle->Element) == false)
			{
				return false;
			}
		}
		else if (const UBPNode_GameQuestElementBase* ElementNode = Cast<UBPNode_GameQuestElementBase>(GameQuestNode))
		{
			if (UBPNode_GameQuestNodeBase* OwnerNode = ElementNode->OwnerNode)
			{
				if (SelectedGameQuestNodes.Contains(OwnerNode) == false)
				{
					return false;
				}
			}
			if (const UBPNode_GameQuestElementBranchList* BranchList = Cast<UBPNode_GameQuestElementBranchList>(GameQuestNode))
			{
				for (UBPNode_GameQuestElementBase* Element : BranchList->Elements)
				{
					if (SelectedGameQuestNodes.Contains(Element) == false)
					{
						return false;
					}
				}
			}
		}
	}

	return CanCollapseNodes();
}

#undef LOCTEXT_NAMESPACE
