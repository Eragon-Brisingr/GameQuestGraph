// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BPNode_GameQuestElementBase.h"
#include "BPNode_GameQuestNodeBase.h"
#include "Editor.h"
#include "GameQuestElementBase.h"
#include "GameQuestGraphBase.h"
#include "GameQuestGraphEditorStyle.h"
#include "GameQuestType.h"
#include "ScopedTransaction.h"
#include "SGraphActionMenu.h"
#include "SGraphPanel.h"
#include "ToolMenu.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"

#define LOCTEXT_NAMESPACE "GameQuestGraphEditor"

template<typename TElementList>
class SNode_GameQuestListDropArea : public SCompoundWidget
{
	using Super = SCompoundWidget;
public:
	SLATE_BEGIN_ARGS(SNode_GameQuestListDropArea)
	{}
		SLATE_DEFAULT_SLOT( FArguments, Content )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, int32 Idx, const TWeakObjectPtr<TElementList>& InList)
	{
		DropIdx = Idx;
		List = InList;

		ChildSlot
			[
				InArgs._Content.Widget
			];
	}
	FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
	{
		if (const TSharedPtr<FGameQuestElementDragDropOp> ElementNodeDragDropOp = DragDropEvent.GetOperationAs<FGameQuestElementDragDropOp>())
		{
			FScopedTransaction ScopedTransaction(LOCTEXT("AdjustQuestElementPosition", "AdjustQuestElementPosition"));

			bIsDragOver = false;
			List->MoveElementNode(ElementNodeDragDropOp->ElementNode.Get(), DropIdx);

			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(List->GetBlueprint());
			return FReply::Handled();
		}
		return Super::OnDrop(MyGeometry, DragDropEvent);
	}
	FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
	{
		if (const TSharedPtr<FGameQuestElementDragDropOp> ElementNodeDragDropOp = DragDropEvent.GetOperationAs<FGameQuestElementDragDropOp>())
		{
			bIsDragOver = true;
			return FReply::Handled();
		}
		return Super::OnDragOver(MyGeometry, DragDropEvent);
	}
	void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
	{
		Super::OnDragEnter(MyGeometry, DragDropEvent);
		if (const TSharedPtr<FGameQuestElementDragDropOp> ElementNodeDragDropOp = DragDropEvent.GetOperationAs<FGameQuestElementDragDropOp>())
		{
			bIsDragOver = true;
			ElementNodeDragDropOp->SetAllowedTooltip();
		}
	}
	void OnDragLeave(FDragDropEvent const& DragDropEvent) override
	{
		Super::OnDragLeave(DragDropEvent);
		if (const TSharedPtr<FGameQuestElementDragDropOp> ElementNodeDragDropOp = DragDropEvent.GetOperationAs<FGameQuestElementDragDropOp>())
		{
			bIsDragOver = false;
			ElementNodeDragDropOp->SetErrorTooltip();
		}
	}
	int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		LayerId = Super::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
		if (bIsDragOver)
		{
			const FVector2D LocalSize(AllottedGeometry.GetLocalSize());
			const FVector2D Pivot(LocalSize * 0.5f);
			const FVector2D RotatedLocalSize(LocalSize.Y, LocalSize.X);
			const FSlateLayoutTransform RotatedTransform(Pivot - RotatedLocalSize * 0.5f);	// Make the box centered to the alloted geometry, so that it can be rotated around the center.

			static const FSlateBrush* DropIndicatorBrush = &FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row").DropIndicator_Onto;

			FSlateDrawElement::MakeBox
			(
				OutDrawElements,
				LayerId++,
				AllottedGeometry.ToPaintGeometry(),
				DropIndicatorBrush,
				ESlateDrawEffect::None,
				DropIndicatorBrush->GetTint(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
			);
		}
		return LayerId;
	}

	int32 DropIdx;
	uint8 bIsDragOver : 1;
	TWeakObjectPtr<TElementList> List;
};

template<typename TElementList>
class SNode_GameQuestList : public SNode_GameQuestNodeBase
{
	using Super = SNode_GameQuestNodeBase;
public:
	void Construct(const FArguments& InArgs, TElementList* InNode)
	{
		List = InNode;
		Super::Construct(InArgs, InNode);
	}

	void CreateBelowWidgetControls(TSharedPtr<SVerticalBox> MainBox) override
	{
		ElementContent = SNew(SVerticalBox);
		MainBox->AddSlot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Graph.StateNode.Body"))
				.Padding(FMargin(2.5f, 0.f))
				.BorderBackgroundColor(GameQuestGraphEditorStyle::ElementContentBorder::Default)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(2.f, 2.f))
					[
						SNew(SNode_GameQuestListDropArea<TElementList>, 0, List)
						[
							SNew(SBox)
							.MinDesiredWidth(200.f)
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("Graph.StateNode.Body"))
								.Padding(FMargin(6.f, 2.f))
								.BorderBackgroundColor(FLinearColor(0.006f, 0.006f, 0.006f))
								[
									SNew(STextBlock)
									.Text(LOCTEXT("QuestElementListTitle", "Quest Element List:"))
								]
							]
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						ElementContent.ToSharedRef()
					]
				]
			];

		const TArray<UBPNode_GameQuestElementBase*>& Elements = List->Elements;
		for (int32 Idx = 0; Idx < Elements.Num(); ++Idx)
		{
			const int32 LastIndex = Elements.Num() - 1;
			UBPNode_GameQuestElementBase* Element = Elements[Idx];

			const ISlateStyle& SlateStyle = FGameQuestGraphSlateStyle::Get();

			if (Idx == 0)
			{
				ElementContent->AddSlot()
					.AutoHeight()
					[
						SNew(SNode_GameQuestListDropArea<TElementList>, 0, List)
						[
							SNew(SImage)
							.Image(SlateStyle.GetBrush(TEXT("ElementLogicTop")))
						]
					];
			}

			if (Idx > 0)
			{
				static FTextBlockStyle LogicHintTextStyle{ FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("NormalText")) };
				LogicHintTextStyle.Font.OutlineSettings.OutlineSize = 1;

				ElementContent->AddSlot()
					.AutoHeight()
					[
						SNew(SNode_GameQuestListDropArea<TElementList>, Idx, List)
						[
							SNew(SBox)
							.HeightOverride(16.f)
							[
								SNew(SOverlay)
								+ SOverlay::Slot()
								.Padding(0.f)
								.HAlign(HAlign_Fill)
								.VAlign(VAlign_Fill)
								[
									SNew(SImage)
									.ToolTipText_Lambda([this, Idx]()
									{
										const EGameQuestSequenceLogic Logic = List->ElementLogics[Idx - 1];
										return Logic == EGameQuestSequenceLogic::And ? LOCTEXT("SetElementOrLogic", "Set Element Or Logic") : LOCTEXT("SetElementAndLogic", "Set Element And Logic");
									})
									.Image_Lambda([this, Idx]
									{
										const ISlateStyle& SlateStyle = FGameQuestGraphSlateStyle::Get();
										const EGameQuestSequenceLogic Logic = List->ElementLogics[Idx - 1];
										static const FSlateBrush* AndBrush = SlateStyle.GetBrush(TEXT("ElementLogicAnd"));
										static const FSlateBrush* OrBrush = SlateStyle.GetBrush(TEXT("ElementLogicOr"));
										return Logic == EGameQuestSequenceLogic::And ? AndBrush : OrBrush;
									})
									.OnMouseButtonDown_Lambda([this, Idx](const FGeometry&, const FPointerEvent& PointerEvent)
									{
										if (PointerEvent.IsMouseButtonDown(EKeys::LeftMouseButton) == false)
										{
											return FReply::Handled();
										}
										const FScopedTransaction Transaction(LOCTEXT("ModifyElementLogic", "ModifyElementLogic"));
										List->Modify();

										EGameQuestSequenceLogic& Logic = List->ElementLogics[Idx - 1];
										UBlueprint* Blueprint = List->GetBlueprint();
										Blueprint->Status = BS_Dirty;
										Logic = Logic == EGameQuestSequenceLogic::And ? EGameQuestSequenceLogic::Or : EGameQuestSequenceLogic::And;
										return FReply::Handled();
									})
									.Visibility_Lambda([=]()
									{
										if (GEditor->PlayWorld != nullptr)
										{
											return EVisibility::HitTestInvisible;
										}
										return EVisibility::Visible;
									})
								]
								+ SOverlay::Slot()
								.Padding(0.f)
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Visibility(EVisibility::HitTestInvisible)
									.TextStyle(&LogicHintTextStyle)
									.Text_Lambda([this, Idx]
									{
										const EGameQuestSequenceLogic Logic = List->ElementLogics[Idx - 1];
										return Logic == EGameQuestSequenceLogic::And ? LOCTEXT("And", "And") : LOCTEXT("Or", "Or");
									})
								]
							]
						]
					];
			}

			ElementContent->AddSlot()
				.AutoHeight()
				.HAlign(HAlign_Fill)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("Graph.StateNode.Body"))
					.Padding(FMargin(2.5f, 2.5f))
					.BorderBackgroundColor_Lambda([this, Element]
					{
						using namespace GameQuestGraphEditorStyle;
						return GetOwnerPanel()->SelectionManager.SelectedNodes.Contains(Element) ? ElementBorder::Selected : ElementBorder::Default;
					})
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						.Padding(2.f, 2.f, 2.f, 0.f)
						[
							SNew(SBorder)
							.BorderImage(FAppStyle::GetBrush("Graph.StateNode.Body"))
							.BorderBackgroundColor(FLinearColor(0.006f, 0.006f, 0.006f))
						]
						+ SOverlay::Slot()
						[
							SNew(SSpacer)
							.Size_Lambda([Element]
							{
								return Element->NodeSize;
							})
						]
					]
				];

			if (Idx == LastIndex)
			{
				ElementContent->AddSlot()
					.AutoHeight()
					[
						SNew(SNode_GameQuestListDropArea<TElementList>, Idx, List)
						[
							SNew(SImage)
							.Image(SlateStyle.GetBrush(TEXT("ElementLogicDown")))
						]
					];
			}
		}
	}
	void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty) override
	{
		Super::MoveTo(NewPosition, NodeFilter, bMarkDirty);
		SyncChildrenPosition();
	}
	void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		Super::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
		SyncChildrenPosition();
	}
private:
	TSharedPtr<SVerticalBox> ElementContent;
	TElementList* List = nullptr;

	void SyncChildrenPosition()
	{
		const FVector2D ParentSize = GetDesiredSize();

		constexpr int32 LeftPadding = 5;
		constexpr int32 TopPadding = 5 + 16 + 4;

		const int32 PosX = List->NodePosX + LeftPadding;
		int32 PosY = List->NodePosY + ParentSize.Y - (ElementContent->GetDesiredSize().Y + 5) + TopPadding;

		for (UBPNode_GameQuestElementBase* Element : List->Elements)
		{
			Element->NodePosX = PosX;
			Element->NodePosY = PosY;

			constexpr int32 LogicPadding = 5 + 16;
			PosY += Element->NodeSize.Y + LogicPadding;
		}
	}
};

template<typename TElementList>
class SGameQuestList_AddElement : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGameQuestList_AddElement)
	{}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TElementList* InBPNode_GameQuestList)
	{
		List = InBPNode_GameQuestList;

		ChildSlot
		[
			SNew(SBorder)
			.BorderImage( FAppStyle::GetBrush("Menu.Background") )
			.Padding(5)
			[
				SNew(SBox)
				.WidthOverride(400)
				.HeightOverride(400)
				[
					SNew(SGraphActionMenu)
					.OnActionSelected(this, &SGameQuestList_AddElement::OnActionSelected)
					.OnCollectAllActions(this, &SGameQuestList_AddElement::CollectAllActions)
					.AutoExpandActionMenu(true)
				]
			]
		];
	}
protected:
	TWeakObjectPtr<TElementList> List;

	void OnActionSelected(const TArray<TSharedPtr<FEdGraphSchemaAction>>& SelectedAction, ESelectInfo::Type InSelectionType)
	{
		if (InSelectionType == ESelectInfo::OnMouseClick || InSelectionType == ESelectInfo::OnKeyPress || SelectedAction.Num() == 0)
		{
			if (const TElementList* GameQuestListNode = List.Get())
			{
				for (int32 ActionIndex = 0; ActionIndex < SelectedAction.Num(); ActionIndex++)
				{
					TSharedPtr<FEdGraphSchemaAction> CurrentAction = SelectedAction[ActionIndex];
					if (CurrentAction.IsValid())
					{
						CurrentAction->PerformAction(GameQuestListNode->GetGraph(), nullptr, FVector2D::ZeroVector);
					}
				}
			}
		}
		FSlateApplication::Get().DismissAllMenus();
	}
	void CollectAllActions(FGraphActionListBuilderBase& OutAllActions)
	{
		TElementList* GameQuestListNode = List.Get();
		if (GameQuestListNode == nullptr)
		{
			return;
		}

		const UBlueprint* Blueprint = GameQuestListNode->GetBlueprint();
		if (ensure(Blueprint) == false)
		{
			return;
		}
		const UClass* Class = Blueprint->GeneratedClass;
		if (ensure(Class) == false)
		{
			return;
		}

		FGraphContextMenuBuilder ContextMenuBuilder(GameQuestListNode->GetGraph());

		const FGameQuestStructCollector& TypeCollector = FGameQuestStructCollector::Get();
		struct FNewStructElement_SchemaAction : FEdGraphSchemaAction
		{
			FNewStructElement_SchemaAction(const FText& Category, const FText& MenuDesc, const FText& Tooltip, const int32 InGrouping, TElementList* ListNode, const UScriptStruct* ElementStruct, TSubclassOf<UBPNode_GameQuestNodeBase> ElementNodeClass)
				: FEdGraphSchemaAction{ Category, MenuDesc, Tooltip, InGrouping }
			, QuestListNode(ListNode)
			, ElementStruct(ElementStruct)
			, ElementNodeClass(ElementNodeClass)
			{}

			TWeakObjectPtr<TElementList> QuestListNode;
			const UScriptStruct* ElementStruct;
			TSubclassOf<UBPNode_GameQuestNodeBase> ElementNodeClass;

			UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode) override
			{
				if (TElementList* ListNode = QuestListNode.Get())
				{
					const FScopedTransaction Transaction(GetTooltipDescription());

					ParentGraph->Modify();
					ListNode->Modify();

					UBPNode_GameQuestElementStruct* ElementNode = NewObject<UBPNode_GameQuestElementStruct>(ParentGraph, ElementNodeClass, NAME_None, RF_Transactional);
					ElementNode->CreateNewGuid();
					ElementNode->StructNodeInstance.InitializeAs(ElementStruct);
					ElementNode->bListMode = true;
					ElementNode->AllocateDefaultPins();
					ListNode->AddElement(ElementNode);
					ListNode->ReconstructNode();
					ParentGraph->AddNode(ElementNode);
					ElementNode->PostPlacedNewNode();

					FBlueprintEditorUtils::MarkBlueprintAsModified(ListNode->GetBlueprint());

					return ElementNode;
				}
				return nullptr;
			}
		};
		for (const auto& [Struct, StructDefaultValue] : TypeCollector.ValidNodeStructMap)
		{
			if (Struct->IsChildOf(FGameQuestElementBase::StaticStruct()) == false)
			{
				continue;
			}
			static FName MD_BranchElement = TEXT("BranchElement");
			if (Struct->HasMetaData(MD_BranchElement))
			{
				continue;
			}
			const TSubclassOf<UBPNode_GameQuestNodeBase> BPNode = TypeCollector.GetBPNodeClass(Struct);
			if (!ensure(BPNode))
			{
				continue;
			}

			const TSubclassOf<UGameQuestGraphBase> SupportType = StructDefaultValue.Get<FGameQuestElementBase>().GetSupportQuestGraph();
			if (Class->IsChildOf(SupportType) == false)
			{
				continue;
			}

			const FText Category = Struct->GetMetaDataText(TEXT("Category"), TEXT("UObjectCategory"), Struct->GetFullGroupName(false));
			const TSharedPtr<FNewStructElement_SchemaAction> NewNodeAction = MakeShared<FNewStructElement_SchemaAction>(Category, Struct->GetDisplayNameText(), Struct->GetToolTipText(), 0, GameQuestListNode, Struct, *BPNode);
			ContextMenuBuilder.AddAction(NewNodeAction);
		}

		struct FNewClassElement_SchemaAction : FEdGraphSchemaAction
		{
			FNewClassElement_SchemaAction(const FText& Category, const FText& MenuDesc, const FText& Tooltip, const int32 InGrouping, TElementList* ListNode, const TSoftClassPtr<UGameQuestElementScriptable>& ElementClass)
				: FEdGraphSchemaAction{ Category, MenuDesc, Tooltip, InGrouping }
				, QuestListNode(ListNode)
				, ElementClass(ElementClass)
			{}

			TWeakObjectPtr<TElementList> QuestListNode;
			const TSoftClassPtr<UGameQuestElementScriptable> ElementClass;
			TSubclassOf<UBPNode_GameQuestNodeBase> ElementNodeClass;

			UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode) override
			{
				const TSubclassOf<UGameQuestElementScriptable> Class = ElementClass.LoadSynchronous();
				if (Class == nullptr)
				{
					return nullptr;
				}

				if (TElementList* ListNode = QuestListNode.Get())
				{
					const FScopedTransaction Transaction(GetTooltipDescription());

					ParentGraph->Modify();
					ListNode->Modify();

					UBPNode_GameQuestElementScript* ElementNode = NewObject<UBPNode_GameQuestElementScript>(ParentGraph, NAME_None, RF_Transactional);
					ElementNode->CreateNewGuid();
					ElementNode->InitialByClass(Class);
					ElementNode->bListMode = true;
					ElementNode->AllocateDefaultPins();
					ListNode->AddElement(ElementNode);
					ListNode->ReconstructNode();
					ParentGraph->AddNode(ElementNode);
					ElementNode->PostPlacedNewNode();

					FBlueprintEditorUtils::MarkBlueprintAsModified(ListNode->GetBlueprint());

					return ElementNode;
				}
				return nullptr;
			}
		};
		FGameQuestClassCollector::Get().ForEachDerivedClasses(UGameQuestElementScriptable::StaticClass(),
		[&](const TSubclassOf<UGameQuestElementScriptable>& Class, const TSoftClassPtr<UGameQuestGraphBase>&)
		{
			const FText Category = Class->GetMetaDataText(TEXT("Category"), TEXT("UObjectCategory"), Class->GetFullGroupName(false));
			const TSharedPtr<FNewClassElement_SchemaAction> NewNodeAction = MakeShared<FNewClassElement_SchemaAction>(Category, Class->GetDisplayNameText(), Class->GetToolTipText(), 0, GameQuestListNode, TSoftClassPtr<UGameQuestElementScriptable>{ Class });
			ContextMenuBuilder.AddAction(NewNodeAction);
		},
		[&](const TSoftClassPtr<UGameQuestElementScriptable>& SoftClass, const TSoftClassPtr<UGameQuestGraphBase>&)
		{
			FString ClassName = SoftClass.GetAssetName();
			ClassName.RemoveFromEnd(TEXT("_C"));
			const TSharedPtr<FNewClassElement_SchemaAction> NewNodeAction = MakeShared<FNewClassElement_SchemaAction>(LOCTEXT("Unload", "Unload"), FText::FromString(ClassName), FText::GetEmpty(), 0, GameQuestListNode, SoftClass);
			ContextMenuBuilder.AddAction(NewNodeAction);
		});

		OutAllActions.Append(ContextMenuBuilder);
	}
};

template<typename TElementList>
void QuestListInsertElement(TElementList* List, UBPNode_GameQuestElementBase* Element, int32 Idx)
{
	using namespace GameQuestUtils;
	check(Element && Element->OwnerNode != List);
	check(List->Elements.Contains(Element) == false);

	Element->Modify();
	Element->OwnerNode = List;
	Element->bListMode = true;
	UEdGraphPin* ListPin = List->FindPinChecked(Pin::ListPinName);
	Element->FindPinChecked(Pin::ListPinName)->MakeLinkTo(ListPin);
	List->Elements.Insert(Element, Idx);
	if (List->Elements.Num() >= 2)
	{
		List->ElementLogics.Insert(EGameQuestSequenceLogic::And, FMath::Max(Idx - 1, 0));
	}
}

template<typename TElementList>
void QuestListRemoveElement(TElementList* List, UBPNode_GameQuestElementBase* Element)
{
	using namespace GameQuestUtils;
	check(Element && Element->OwnerNode == List);
	check(List->Elements.Contains(Element));

	List->Modify();
	Element->Modify();
	Element->OwnerNode = nullptr;
	Element->bListMode = false;
	const int32 Idx = List->Elements.IndexOfByKey(Element);
	List->Elements.RemoveAt(Idx);
	if (List->ElementLogics.Num() > 0)
	{
		List->ElementLogics.RemoveAt(FMath::Max(Idx - 1, 0));
	}
}

template<typename TElementList>
void QuestListMoveElement(TElementList* List, UBPNode_GameQuestElementBase* Element, int32 Idx)
{
	using namespace GameQuestUtils;
	UBPNode_GameQuestNodeBase* OldOwnerNode = Element->OwnerNode;
	List->Modify();
	if (OldOwnerNode == List)
	{
		const int32 OldIdx = List->Elements.IndexOfByKey(Element);
		List->Elements.Swap(OldIdx, Idx);
		List->ReconstructNode();
	}
	else if (TElementList* SequenceList = Cast<TElementList>(OldOwnerNode))
	{
		OldOwnerNode->Modify();
		Element->Modify();

		SequenceList->RemoveElement(Element);
		List->InsertElementNode(Element, Idx);

		OldOwnerNode->ReconstructNode();
		List->ReconstructNode();
	}
}

template<typename TElementList>
void QuestListAddElement(TElementList* List, UBPNode_GameQuestElementBase* Element)
{
	QuestListInsertElement(List, Element, List->Elements.Num());
}

template<typename TElementList>
void CreateAddElementMenu(TElementList* ElementList, UToolMenu* Menu, UGraphNodeContextMenuContext* Context)
{
	if (Context->bIsDebugging)
	{
		return;
	}

	FToolMenuSection& Section = Menu->AddSection(TEXT("GameQuest"), LOCTEXT("GameQuest", "GameQuest"), FToolMenuInsert(TEXT("GraphNodeComment"), EToolMenuInsertType::Before));
	Section.AddSubMenu(
		TEXT("AddGameQuestElement"),
		LOCTEXT("AddGameQuestElement", "Add Quest Element"),
		LOCTEXT("AddGameQuestElement", "Add Quest Element"),
		FNewToolMenuWidget::CreateLambda([=](const FToolMenuContext& ToolMenuContext)
		{
			return SNew(SGameQuestList_AddElement<TElementList>, ElementList);
		}));
}

template<typename TElementList>
void CheckOptionalIsValid(TElementList* ElementList, FCompilerResultsLog& MessageLog)
{
	int32 MustElementNumber = 0;
	TArray<UBPNode_GameQuestElementBase*> InvalidOptionalElements;
	for (int32 Idx = 0; Idx < ElementList->Elements.Num(); ++Idx)
	{
		UBPNode_GameQuestElementBase* ElementNode = ElementList->Elements[Idx];
		if (ElementNode->bIsOptional)
		{
			InvalidOptionalElements.Add(ElementNode);
		}
		else
		{
			MustElementNumber += 1;
		}
		if (Idx == ElementList->Elements.Num() - 1 || ElementList->ElementLogics[Idx] == EGameQuestSequenceLogic::Or)
		{
			if (MustElementNumber == 0)
			{
				for (UBPNode_GameQuestElementBase* InvalidOptional : InvalidOptionalElements)
				{
					MessageLog.Error(TEXT("@@is optional element, group not contain must element"), InvalidOptional);
				}
			}
			MustElementNumber = 0;
			InvalidOptionalElements.Empty();
		}
	}
}

#undef LOCTEXT_NAMESPACE
