// Fill out your copyright notice in the Description page of Project Settings.


#include "BPNode_GameQuestElementBase.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "BPNode_GameQuestListSequenceUtils.h"
#include "BPNode_GameQuestSequenceBase.h"
#include "GameQuestElementBase.h"
#include "GameQuestGraphBase.h"
#include "GameQuestGraphBlueprint.h"
#include "GameQuestGraphEditorSettings.h"
#include "GameQuestGraphEditorStyle.h"
#include "GraphEditorSettings.h"
#include "K2Node_StructMemberGet.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "KismetCompiler.h"
#include "SGraphPanel.h"
#include "ToolMenu.h"
#include "ToolMenuMisc.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Widgets/Layout/SSpacer.h"

#define LOCTEXT_NAMESPACE "GameQuestGraphEditor"

void UBPNode_GameQuestElementBase::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	using namespace GameQuestUtils;

	if (IsListMode())
	{
		CreatePin(EGPD_Input, Pin::ListPinCategory, Pin::ListPinName);
	}
	else
	{
		UEdGraphPin* BranchPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, Pin::BranchPinSubCategory, Pin::BranchPinName);
		BranchPin->PinFriendlyName = LOCTEXT("BranchPinName", "BranchPin");
	}
	CreateExposePins();

	if (IsListMode() == false)
	{
		CreateFinishEventPins();
	}

	UEdGraphPin* ReturnValuePin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, GetNodeStruct(), UEdGraphSchema_K2::PN_ReturnValue);
	ReturnValuePin->bAdvancedView = true;
}

FText UBPNode_GameQuestElementBase::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::MenuTitle)
	{
		if (const UStruct* NodeType = GetNodeImplStruct())
		{
			return FText::Format(LOCTEXT("GameQuestElementNodeTitle", "Add Quest Branch {0}"), NodeType->GetDisplayNameText());
		}
	}
	if (TitleType == ENodeTitleType::FullTitle)
	{
		const UStruct* NodeType = GetNodeImplStruct();
		const FText NodeTypeText = NodeType ? NodeType->GetDisplayNameText() : LOCTEXT("MissingType", "Missing Type");
		if (bListMode && bIsOptional)
		{
			return FText::Format(LOCTEXT("GameQuestElementOptionalFullTitle", "{0}\n{1} [Optional]"), FText::FromString(GameQuestNodeName), NodeTypeText);
		}
		return FText::Format(LOCTEXT("GameQuestElementFullTitle", "{0}\n{1}"), FText::FromString(GameQuestNodeName), NodeTypeText);
	}
	return Super::GetNodeTitle(TitleType);
}

class SNode_GameQuestListElement : public SNode_GameQuestNodeBase
{
	using Super = SNode_GameQuestNodeBase;
public:
	void Construct(const FArguments& InArgs, UBPNode_GameQuestElementBase* InNode)
	{
		ElementNode = InNode;
		Super::Construct(InArgs, InNode);
	}

	void CreateBelowWidgetControls(TSharedPtr<SVerticalBox> MainBox) override
	{
		MainBox->AddSlot()
			[
				SNew(SSpacer)
				.Size(FVector2D(200.f, 0.f))
			];
	}
	FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.IsMouseButtonDown(EKeys::MiddleMouseButton))
		{
			if (FReply::Handled().GetDragDropContent().IsValid() == false)
			{
				return FReply::Handled().DetectDrag(AsShared(), EKeys::MiddleMouseButton);
			}
		}
		return Super::OnMouseMove(MyGeometry, MouseEvent);
	}
	void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty) override { /* disable dragging */ }
	int32 GetSortDepth() const override { return 1; }
	void CacheDesiredSize(float InLayoutScaleMultiplier) override
	{
		Super::CacheDesiredSize(InLayoutScaleMultiplier);
		ElementNode->NodeSize = GetDesiredSize();
	}
	FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return FReply::Handled().BeginDragDrop(FGameQuestElementDragDropOp::New(ElementNode));
	}

	UBPNode_GameQuestElementBase* ElementNode = nullptr;
};

class SNode_GameQuestSingleElement : public SNode_GameQuestNodeBase
{
	using Super = SNode_GameQuestNodeBase;
public:
	void Construct(const FArguments& InArgs, UBPNode_GameQuestElementBase* InNode, UBPNode_GameQuestSequenceSingle* InSequenceSingle)
	{
		ElementNode = InNode;
		SequenceSingle = InSequenceSingle;
		Super::Construct(InArgs, InNode);
	}
	void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty) override
	{
		Super::MoveTo(NewPosition, NodeFilter, bMarkDirty);
		SyncElementPosition();
	}
	void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		Super::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
		SyncElementPosition();
	}
	void SyncElementPosition()
	{
		SequenceSingle->NodePosX = ElementNode->NodePosX;
		SequenceSingle->NodePosY = ElementNode->NodePosY;
	}
	void CreateStandardPinWidget(UEdGraphPin* Pin) override
	{
		if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->PinType.PinSubCategory == GameQuestUtils::Pin::BranchPinSubCategory)
		{
			// Hidden branch pin
		}
		else
		{
			Super::CreateStandardPinWidget(Pin);
		}
	}
	void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override
	{
		const UEdGraphPin* Pin = PinToAdd->GetPinObj();
		if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			PinToAdd->SetOwner(SharedThis(this));
			RightExecNodeBox->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(Settings->GetOutputPinPadding())
			[
				PinToAdd
			];
			OutputPins.Add(PinToAdd);
		}
		else
		{
			Super::AddPin(PinToAdd);
		}
	}
	void CreateBelowWidgetControls(TSharedPtr<SVerticalBox> MainBox) override
	{
		Super::CreateBelowWidgetControls(MainBox);
		using namespace GameQuestGraphEditorStyle;

		MainBox->InsertSlot(0)
		[
			SNew(SBorder)
			.BorderImage(GetNodeBodyBrush())
			.BorderBackgroundColor(ElementNode->GetNodeBodyTintColor())
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Top)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("Graph.StateNode.Body"))
					.Padding(FMargin(0.f, 0.f, 2.5f, 2.5f))
					.BorderBackgroundColor_Lambda([this]
					{
						return GetOwnerPanel()->SelectionManager.SelectedNodes.Contains(SequenceSingle) ? ElementBorder::Selected : ElementBorder::Default;
					})
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						.Padding(0.f, 0.f, 2.f, 0.f)
						[
							SNew(SBorder)
							.BorderImage(FAppStyle::GetBrush("Graph.StateNode.Body"))
							.BorderBackgroundColor(FLinearColor(0.006f, 0.006f, 0.006f))
						]
						+ SOverlay::Slot()
						[
							SNew(SSpacer)
							.Size_Lambda([this]
							{
								return SequenceSingle->NodeSize;
							})
						]
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Top)
				.Padding(FMargin{ 0.f, 41.f, 0.f, 0.f })
				[
					SAssignNew(RightExecNodeBox, SVerticalBox)
				]
			]
		];
	}
	UBPNode_GameQuestElementBase* ElementNode = nullptr;
	UBPNode_GameQuestSequenceSingle* SequenceSingle = nullptr;
	TSharedPtr<SVerticalBox> RightExecNodeBox;
};

class SNode_GameQuestBranchElement : public SNode_GameQuestNodeBase
{
	using Super = SNode_GameQuestNodeBase;
};

TSharedPtr<SGraphNode> UBPNode_GameQuestElementBase::CreateVisualWidget()
{
	if (IsListMode())
	{
		return SNew(SNode_GameQuestListElement, this);
	}
	if (UBPNode_GameQuestSequenceSingle* SequenceSingle = Cast<UBPNode_GameQuestSequenceSingle>(OwnerNode))
	{
		return SNew(SNode_GameQuestSingleElement, this, SequenceSingle);
	}
	return SNew(SNode_GameQuestBranchElement, this);
}

FText UBPNode_GameQuestElementBase::GetMenuCategory() const
{
	FText Category;
	if (const UStruct* NodeType = GetNodeImplStruct())
	{
		Category = NodeType->GetMetaDataText(TEXT("Category"), TEXT("UObjectCategory"), NodeType->GetFullGroupName(false));
	}
	if (Category.IsEmpty())
	{
		return LOCTEXT("GameQuestElementDefaultCategory", "GameQuest|Element");
	}
	return FText::Format(LOCTEXT("GameQuestElementCategory", "GameQuest|Element|{0}"), Category);
}

FName UBPNode_GameQuestElementBase::GetCornerIcon() const
{
	if (IsListMode())
	{
		return NAME_None;
	}
	return Super::GetCornerIcon();
}

FSlateIcon UBPNode_GameQuestElementBase::GetIconAndTint(FLinearColor& OutColor) const
{
	if (IsListMode())
	{
		static FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "Icons.BulletPoint");
		return Icon;
	}
	static FSlateIcon Icon("EditorStyle", "GraphEditor.Macro.DoOnce_16x");
	return Icon;
}

void UBPNode_GameQuestElementBase::PinConnectionListChanged(UEdGraphPin* Pin)
{
	if (IsListMode() == false)
	{
		if (Pin->GetFName() == GameQuestUtils::Pin::BranchPinName)
		{
			if (Pin->LinkedTo.Num() > 0)
			{
				OwnerNode = Cast<UBPNode_GameQuestNodeBase>(Pin->LinkedTo[0]->GetOwningNode());
				const UBlueprint* Blueprint = GetBlueprint();
				const UStruct* Struct = GetNodeImplStruct();
				if (Struct && Blueprint)
				{
					if (GameQuestNodeName.IsEmpty())
					{
						GameQuestNodeName = *MakeUniqueRefVarName(Blueprint, Struct->GetDisplayNameText().ToString(), GetRefVarPrefix());
					}
					else
					{
						FString BaseName = GameQuestNodeName;
						const UObject* ExistNode = FindNodeByRefVarName(Blueprint, GetRefVarName());
						if (ExistNode != nullptr && ExistNode != this)
						{
							int32 SplitIndex = INDEX_NONE;
							if (GameQuestNodeName.FindLastChar(TEXT('_'), SplitIndex))
							{
								const FString TestNumberString = GameQuestNodeName.Right(GameQuestNodeName.Len() - SplitIndex - 1);
								if (TestNumberString.IsNumeric())
								{
									BaseName = GameQuestNodeName.Left(SplitIndex);
								}
							}
							GameQuestNodeName = *MakeUniqueRefVarName(Blueprint, BaseName, GetRefVarPrefix());
						}
					}
				}
				else
				{
					BreakAllNodeLinks();
				}
			}
			else if (Cast<UBPNode_GameQuestSequenceBranch>(OwnerNode) != nullptr)
			{
				OwnerNode = nullptr;
			}
		}
	}
	Super::PinConnectionListChanged(Pin);
}

void UBPNode_GameQuestElementBase::DestroyNode()
{
	if (UBPNode_GameQuestSequenceListBase* SequenceList = Cast<UBPNode_GameQuestSequenceListBase>(OwnerNode))
	{
		SequenceList->RemoveElement(this);
		SequenceList->ReconstructNode();
	}
	else if (UBPNode_GameQuestElementBranchList* BranchList = Cast<UBPNode_GameQuestElementBranchList>(OwnerNode))
	{
		BranchList->RemoveElement(this);
		BranchList->ReconstructNode();
	}
	else if (UBPNode_GameQuestSequenceSingle* SequenceSingle = Cast<UBPNode_GameQuestSequenceSingle>(OwnerNode))
	{
		SequenceSingle->Element = nullptr;
		FBlueprintEditorUtils::RemoveNode(GetBlueprint(), SequenceSingle, true);
	}
	Super::DestroyNode();
}

void UBPNode_GameQuestElementBase::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const TSubclassOf<UGameQuestGraphBase> SupportQuest = GetSupportQuest();
	if (SupportQuest == nullptr || CompilerContext.NewClass->IsChildOf(SupportQuest) == false)
	{
		CompilerContext.MessageLog.Warning(TEXT("@@not support this graph type"), this);
	}

	if (IsListMode() == false)
	{
		ExpandNodeForFinishEvents(CompilerContext, SourceGraph);
	}

	const FName RefVarName = GetRefVarName();
	UK2Node_VariableGet* GetStructNode = CompilerContext.SpawnIntermediateNode<UK2Node_VariableGet>(this);
	GetStructNode->VariableReference.SetSelfMember(RefVarName);
	GetStructNode->AllocateDefaultPins();
	UEdGraphPin* RefVarPin = GetStructNode->FindPinChecked(RefVarName);
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(UEdGraphSchema_K2::PN_ReturnValue), *RefVarPin);
}

bool UBPNode_GameQuestElementBase::CanPasteHere(const UEdGraph* TargetGraph) const
{
	using namespace GameQuestUtils::Pin;
	if (GetNodeImplStruct() == nullptr)
	{
		return false;
	}
	if (IsListMode())
	{
		const UEdGraphPin* ListPin = FindPinChecked(ListPinName);
		if (ListPin->LinkedTo.Num() == 0 || ListPin->LinkedTo[0] == nullptr)
		{
			return false;
		}
		if (const UBPNode_GameQuestSequenceListBase* SequenceList = Cast<UBPNode_GameQuestSequenceListBase>(ListPin->LinkedTo[0]->GetOwningNode()))
		{
			return SequenceList->Elements.Contains(this);
		}
		else if (const UBPNode_GameQuestElementBranchList* BranchList = Cast<UBPNode_GameQuestElementBranchList>(ListPin->LinkedTo[0]->GetOwningNode()))
		{
			return BranchList->Elements.Contains(this);
		}
	}
	else
	{
		const UEdGraphPin* BranchPin = FindPinChecked(BranchPinName);
		if (BranchPin->LinkedTo.Num() > 0 && BranchPin->LinkedTo[0] != nullptr && Cast<UBPNode_GameQuestSequenceSingle>(BranchPin->LinkedTo[0]->GetOwningNode()))
		{
			return false;
		}
	}
	if (const UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph))
	{
		if (ensure(Blueprint->GeneratedClass))
		{
			const UClass* SupportGameQuest = GetSupportQuest();
			if (SupportGameQuest == nullptr || Blueprint->GeneratedClass->IsChildOf(SupportGameQuest) == false)
			{
				return false;
			}
		}
	}
	else
	{
		return false;
	}
	return Super::CanPasteHere(TargetGraph);
}

void UBPNode_GameQuestElementBase::PostPasteNode()
{
	Super::PostPasteNode();

	using namespace GameQuestUtils::Pin;
	if (IsListMode() == false)
	{
		const UEdGraphPin* BranchPin = FindPinChecked(BranchPinName);
		if (BranchPin->LinkedTo.Num() == 0)
		{
			OwnerNode = nullptr;
		}
	}
}

bool UBPNode_GameQuestElementBase::CanEditChange(const FProperty* InProperty) const
{
	const FName PropertyName = InProperty ? InProperty->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, bIsOptional))
	{
		return bListMode;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, bAutoDeactivateOtherBranch))
	{
		const UEdGraphPin* BranchPin = FindPin(GameQuestUtils::Pin::BranchPinName);
		return BranchPin && BranchPin->LinkedTo.Num() > 0 && Cast<UBPNode_GameQuestSequenceBranch>(BranchPin->LinkedTo[0]->GetOwningNode()) != nullptr;
	}
	return Super::CanEditChange(InProperty);
}

void UBPNode_GameQuestElementBase::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	Super::GetNodeContextMenuActions(Menu, Context);

	UGameQuestGraphBase* DebuggedQuest = Cast<UGameQuestGraphBase>(GetBlueprint()->GetObjectBeingDebugged());
	if (DebuggedQuest == nullptr || Context->bIsDebugging == false)
	{
		return;
	}
	if (DebuggedQuest->GetQuestState() != UGameQuestGraphBase::EState::Activated)
	{
		return;
	}
	const FStructProperty* ElementProperty = FindFProperty<FStructProperty>(DebuggedQuest->GetClass(), GetRefVarName());
	if (ElementProperty == nullptr || ElementProperty->Struct->IsChildOf(FGameQuestElementBase::StaticStruct()) == false)
	{
		return;
	}
	FGameQuestElementBase* ElementInstance = ElementProperty->ContainerPtrToValuePtr<FGameQuestElementBase>(DebuggedQuest);
	if (ElementInstance->bIsFinished)
	{
		return;
	}
	if (ElementInstance->bIsActivated == false)
	{
		const UBPNode_GameQuestSequenceBranch* SequenceBranchNode = Cast<UBPNode_GameQuestSequenceBranch>(OwnerNode);
		if (SequenceBranchNode == nullptr)
		{
			return;
		}
		FToolMenuSection& Section = Menu->AddSection(TEXT("Debug"), LOCTEXT("Debug", "Debug"), FToolMenuInsert(TEXT("GraphNodeComment"), EToolMenuInsertType::Before));
		Section.AddMenuEntry(
			TEXT("ForceActivateBranch"),
			LOCTEXT("ForceActivateBranch", "Force activate branch"),
			LOCTEXT("ForceActivateBranchDesc", "Force activate selected branch"),
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateWeakLambda(DebuggedQuest, [ElementInstance, DebuggedQuest]
			{
				if (DebuggedQuest->GetQuestState() == UGameQuestGraphBase::EState::Activated
					&& ElementInstance->bIsFinished == false && ElementInstance->bIsActivated == false)
				{
					DebuggedQuest->ForceActivateBranchToServer(DebuggedQuest->GetElementId(ElementInstance));
				}
			}))
		);
		return;
	}
	const auto EventNames = ElementInstance->GetActivatedFinishEventNames();
	FToolMenuSection& Section = Menu->AddSection(TEXT("Debug"), LOCTEXT("Debug", "Debug"), FToolMenuInsert(TEXT("GraphNodeComment"), EToolMenuInsertType::Before));
	if (EventNames.Num() > 0)
	{
		for (const FName& EventName : EventNames)
		{
			Section.AddMenuEntry(
				*FString::Printf(TEXT("ForceFinishElementByEvent[%s]"), *EventName.ToString()),
				FText::Format(LOCTEXT("ForceFinishElementByEvent", "Force Finish Element By [{0}]"), FText::FromName(EventName)),
				LOCTEXT("ForceFinishBranchDesc", "Force finish selected element by event"),
				FSlateIcon(),
				FUIAction(
				FExecuteAction::CreateWeakLambda(DebuggedQuest, [ElementInstance, DebuggedQuest, EventName]
				{
					if (ElementInstance->bIsFinished == false && ElementInstance->bIsActivated)
					{
						DebuggedQuest->ForceFinishElementToServer(DebuggedQuest->GetElementId(ElementInstance), EventName);
					}
				}))
			);
		}
	}
	else
	{
		Section.AddMenuEntry(
			TEXT("ForceFinishElement"),
			LOCTEXT("ForceFinishElement", "Force finish element"),
			LOCTEXT("ForceFinishElementDesc", "Force finish selected element"),
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateWeakLambda(DebuggedQuest, [ElementInstance, DebuggedQuest]
			{
				if (DebuggedQuest->GetQuestState() == UGameQuestGraphBase::EState::Activated
					&& ElementInstance->bIsFinished == false && ElementInstance->bIsActivated)
				{
					DebuggedQuest->ForceFinishElementToServer(DebuggedQuest->GetElementId(ElementInstance), NAME_None);
				}
			}))
		);
	}
}

FGameQuestElementBase* UBPNode_GameQuestElementBase::GetElementNode() const
{
	return const_cast<FInstancedStruct&>(StructNodeInstance).GetMutablePtr<FGameQuestElementBase>();
}

void UBPNode_GameQuestElementBase::CopyTermDefaultsToDefaultNode(FGameQuestGraphCompilerContext& CompilerContext, UObject* DefaultObject, UGameQuestGraphGeneratedClass* ObjectClass, FStructProperty* NodeProperty)
{
	Super::CopyTermDefaultsToDefaultNode(CompilerContext, DefaultObject, ObjectClass, NodeProperty);

	if (ensure(OwnerNode))
	{
		FGameQuestElementBase* Element = NodeProperty->ContainerPtrToValuePtr<FGameQuestElementBase>(DefaultObject);
		if (const UBPNode_GameQuestElementBranchList* BranchList = Cast<UBPNode_GameQuestElementBranchList>(OwnerNode))
		{
			Element->Sequence = BranchList->OwnerNode->NodeId;
		}
		else
		{
			Element->Sequence = OwnerNode->NodeId;
		}
		Element->bIsOptional = bListMode ? bIsOptional : false;
	}
	ObjectClass->NodeNameIdMap.Add(NodeProperty->GetFName(), NodeId);
}

FString UBPNode_GameQuestElementBase::GetRefVarPrefix() const
{
	return FString::Printf(TEXT("%s_"), *(OwnerNode ? OwnerNode->GetRefVarName() : NAME_None).ToString());
}

TSubclassOf<UGameQuestGraphBase> UBPNode_GameQuestElementBase::GetSupportQuest() const
{
	const FGameQuestElementBase* ElementInstance = StructNodeInstance.GetPtr<FGameQuestElementBase>();
	return ElementInstance ? ElementInstance->GetSupportQuestGraph() : nullptr;
}

void UBPNode_GameQuestElementBranchList::AllocateDefaultPins()
{
	using namespace GameQuestUtils;
	CreatePin(EGPD_Input, Pin::ListPinCategory, Pin::ListPinName);
	Super::AllocateDefaultPins();
}

TSharedPtr<SGraphNode> UBPNode_GameQuestElementBranchList::CreateVisualWidget()
{
	return SNew(SNode_GameQuestList<UBPNode_GameQuestElementBranchList>, this);
}

void UBPNode_GameQuestElementBranchList::DestroyNode()
{
	for (int32 Idx = Elements.Num() - 1; Idx >= 0; --Idx)
	{
		UBPNode_GameQuestElementBase* Element = Elements[Idx];
		if (Element == nullptr)
		{
			continue;
		}
		FBlueprintEditorUtils::RemoveNode(GetBlueprint(), Element, true);
	}
	Super::DestroyNode();
}

void UBPNode_GameQuestElementBranchList::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	Super::GetNodeContextMenuActions(Menu, Context);

	CreateAddElementMenu(const_cast<UBPNode_GameQuestElementBranchList*>(this), Menu, Context);
}

void UBPNode_GameQuestElementBranchList::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	if (Elements.Num() == 0)
	{
		CompilerContext.MessageLog.Error(TEXT("@@Must have one or more elements"), this);
	}

	CheckOptionalIsValid(this, CompilerContext.MessageLog);
}

void UBPNode_GameQuestElementBranchList::PostPasteNode()
{
	Super::PostPasteNode();

	QuestListPostPasteNode(this);
}

void UBPNode_GameQuestElementBranchList::CreateClassVariablesFromNode(FGameQuestGraphCompilerContext& CompilerContext)
{
	for (UBPNode_GameQuestElementBase* Element : Elements)
	{
		if (!ensure(Element))
		{
			continue;
		}
		Element->CreateClassVariablesFromNode(CompilerContext);
	}

	Super::CreateClassVariablesFromNode(CompilerContext);
}

void UBPNode_GameQuestElementBranchList::CopyTermDefaultsToDefaultNode(FGameQuestGraphCompilerContext& CompilerContext, UObject* DefaultObject, UGameQuestGraphGeneratedClass* ObjectClass, FStructProperty* NodeProperty)
{
	Super::CopyTermDefaultsToDefaultNode(CompilerContext, DefaultObject, ObjectClass, NodeProperty);

	GameQuest::FLogicList& LogicList = ObjectClass->NodeIdLogicsMap.Add(NodeId);
	ensure(Elements.Num() == 0 || ElementLogics.Num() == Elements.Num() - 1);
	for (EGameQuestSequenceLogic Logic : ElementLogics)
	{
		LogicList.Add(Logic);
	}

	FGameQuestElementBranchList* BranchList = NodeProperty->ContainerPtrToValuePtr<FGameQuestElementBranchList>(DefaultObject);
	BranchList->Elements.Empty();
	for (UBPNode_GameQuestElementBase* Element : Elements)
	{
		if (!ensure(Element))
		{
			continue;
		}
		FStructProperty* ElementProperty = FindFProperty<FStructProperty>(DefaultObject->GetClass(), Element->GetRefVarName());
		if (!ensure(ElementProperty))
		{
			continue;
		}
		BranchList->Elements.Add(Element->NodeId);
		Element->CopyTermDefaultsToDefaultNode(CompilerContext, DefaultObject, ObjectClass, ElementProperty);
	}
}

UScriptStruct* UBPNode_GameQuestElementBranchList::GetBaseNodeStruct() const
{
	return FGameQuestElementBranchList::StaticStruct();
}

UScriptStruct* UBPNode_GameQuestElementBranchList::GetNodeStruct() const
{
	return GetDefault<UGameQuestGraphEditorSettings>()->ElementBranchListType.Get();
}

void UBPNode_GameQuestElementBranchList::AddElement(UBPNode_GameQuestElementBase* Element)
{
	QuestListAddElement(this, Element);
}

void UBPNode_GameQuestElementBranchList::InsertElementNode(UBPNode_GameQuestElementBase* Element, int32 Idx)
{
	QuestListInsertElement(this, Element, Idx);
}

void UBPNode_GameQuestElementBranchList::RemoveElement(UBPNode_GameQuestElementBase* Element)
{
	QuestListRemoveElement(this, Element);
}

void UBPNode_GameQuestElementBranchList::MoveElementNode(UBPNode_GameQuestElementBase* Element, int32 Idx)
{
	QuestListMoveElement(this, Element, Idx);
}

UScriptStruct* UBPNode_GameQuestElementStruct::GetBaseNodeStruct() const
{
	return FGameQuestElementBase::StaticStruct();
}

bool UBPNode_GameQuestElementStruct::IsActionFilteredOut(FBlueprintActionFilter const& Filter)
{
	const FGameQuestElementBase* Element = StructNodeInstance.GetPtr<FGameQuestElementBase>();
	if (Element == nullptr)
	{
		return true;
	}
	const TSubclassOf<UGameQuestGraphBase> SupportType = Element->GetSupportQuestGraph();
	if (SupportType == nullptr)
	{
		return true;
	}

	for (const UBlueprint* Blueprint : Filter.Context.Blueprints)
	{
		if (UClass* Class = Blueprint->GeneratedClass)
		{
			if (Class == nullptr || !Class->IsChildOf(SupportType))
			{
				return true;
			}
		}
	}
	return false;
}

void UBPNode_GameQuestElementScript::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	if (ScriptInstance == nullptr)
	{
		return;
	}

	FQuestNodeOptionalPinManager QuestNodeOptionalPinManager;
	QuestNodeOptionalPinManager.RebuildPropertyList(ShowPinForScript, ScriptInstance->GetClass());
	QuestNodeOptionalPinManager.CreateVisiblePins(ShowPinForScript, ScriptInstance->GetClass(), EGPD_Input, this);
}

FText UBPNode_GameQuestElementScript::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::MenuTitle)
	{
		if (UnloadNodeData.IsSet() && UnloadNodeData->Class.Get() == nullptr)
		{
			FString ClassName = UnloadNodeData->Class.GetAssetName();
			ClassName.RemoveFromEnd(TEXT("_C"));
			return FText::Format(LOCTEXT("GameQuestElementNodeUnloadTitle", "Add Quest Branch [{0}]"), FText::FromString(ClassName));
		}
	}
	return Super::GetNodeTitle(TitleType);
}

FText UBPNode_GameQuestElementScript::GetMenuCategory() const
{
	if (UnloadNodeData.IsSet() && UnloadNodeData->Class.Get() == nullptr)
	{
		return LOCTEXT("GameQuestElementUnloadCategory", "GameQuest|Element|Unload");
	}
	return Super::GetMenuCategory();
}

UScriptStruct* UBPNode_GameQuestElementScript::GetBaseNodeStruct() const
{
	return FGameQuestElementScript::StaticStruct();
}

UScriptStruct* UBPNode_GameQuestElementScript::GetNodeStruct() const
{
	return GetDefault<UGameQuestGraphEditorSettings>()->ElementScriptType.Get();
}

UStruct* UBPNode_GameQuestElementScript::GetNodeImplStruct() const
{
	if (ScriptInstance)
	{
		return ScriptInstance->GetClass();
	}
	if (UnloadNodeData.IsSet())
	{
		if (UClass* Class = UnloadNodeData->Class.Get())
		{
			return Class;
		}
	}
	return Super::GetNodeImplStruct();
}

void UBPNode_GameQuestElementScript::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	const UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey) == false)
	{
		return;
	}

	FGameQuestClassCollector::Get().ForEachDerivedClasses(UGameQuestElementScriptable::StaticClass(),
	[&](const TSubclassOf<UGameQuestElementScriptable>& Class, const TSoftClassPtr<UGameQuestGraphBase>& SupportType)
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateLambda([=](UEdGraphNode* NewNode, bool bIsTemplateNode) mutable
		{
			UBPNode_GameQuestElementScript* ElementScriptNode = CastChecked<UBPNode_GameQuestElementScript>(NewNode);
			ElementScriptNode->InitialByClass(Class);
		});
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	},
	[&](const TSoftClassPtr<UGameQuestElementScriptable>& SoftClass, const TSoftClassPtr<UGameQuestGraphBase>& SupportType)
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateLambda([=](UEdGraphNode* NewNode, bool bIsTemplateNode) mutable
		{
			if (bIsTemplateNode)
			{
				UBPNode_GameQuestElementScript* ElementScriptNode = CastChecked<UBPNode_GameQuestElementScript>(NewNode);
				ElementScriptNode->UnloadNodeData = FUnloadNodeData{ SoftClass, SupportType };
			}
			else if (const TSubclassOf<UGameQuestElementScriptable> Class = SoftClass.LoadSynchronous())
			{
				UBPNode_GameQuestElementScript* ElementScriptNode = CastChecked<UBPNode_GameQuestElementScript>(NewNode);
				ElementScriptNode->InitialByClass(Class);
			}
		});
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	});
}

UObject* UBPNode_GameQuestElementScript::GetJumpTargetForDoubleClick() const
{
	if (ScriptInstance)
	{
		return ScriptInstance->GetClass()->ClassGeneratedBy;
	}
	return nullptr;
}

bool UBPNode_GameQuestElementScript::IsActionFilteredOut(FBlueprintActionFilter const& Filter)
{
	TSubclassOf<UGameQuestGraphBase> SupportType;
	if (ScriptInstance)
	{
		SupportType = ScriptInstance->SupportType.Get();
	}
	else if (UnloadNodeData.IsSet())
	{
		SupportType = UnloadNodeData->SupportType.Get();
	}
	if (SupportType == nullptr)
	{
		return true;
	}

	for (const UBlueprint* Blueprint : Filter.Context.Blueprints)
	{
		if (UClass* Class = Blueprint->GeneratedClass)
		{
			if (Class == nullptr || !Class->IsChildOf(SupportType))
			{
				return true;
			}
		}
	}
	return false;
}

bool UBPNode_GameQuestElementScript::HasExternalDependencies(TArray<UStruct*>* OptionalOutput) const
{
	bool Res = Super::HasExternalDependencies(OptionalOutput);
	Res |= ScriptInstance != nullptr;
	if (OptionalOutput)
	{
		OptionalOutput->AddUnique(ScriptInstance->GetClass());
	}
	return Res;
}

bool UBPNode_GameQuestElementScript::HasEvaluateActionParams() const
{
	return ShowPinForScript.ContainsByPredicate([](const FOptionalPinFromProperty& E){ return E.bShowPin; }) || Super::HasEvaluateActionParams();
}

void UBPNode_GameQuestElementScript::ExpandNodeForEvaluateActionParams(UEdGraphPin*& AuthorityThenPin, UEdGraphPin*& ClientThenPin, FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNodeForEvaluateActionParams(AuthorityThenPin, ClientThenPin, CompilerContext, SourceGraph);

	if (ScriptInstance == nullptr)
	{
		return;
	}
	if (ShowPinForScript.ContainsByPredicate([](const FOptionalPinFromProperty& E){ return E.bShowPin; }) == false)
	{
		return;
	}

	UClass* Class = ScriptInstance->GetClass();

	UK2Node_StructMemberGet* StructMemberGetNode = CompilerContext.SpawnIntermediateNode<UK2Node_StructMemberGet>(this, SourceGraph);
	StructMemberGetNode->VariableReference.SetSelfMember(GetRefVarName());
	StructMemberGetNode->StructType = GetNodeStruct();
	StructMemberGetNode->AllocatePinsForSingleMemberGet(GET_MEMBER_NAME_CHECKED(FGameQuestElementScript, Instance));
	UEdGraphPin* ScriptInstancePin = StructMemberGetNode->FindPinChecked(GET_MEMBER_NAME_CHECKED(FGameQuestElementScript, Instance));
	ScriptInstancePin->PinType.PinSubCategoryObject = Class;

	for (const FOptionalPinFromProperty& OptionalPin : ShowPinForScript)
	{
		if (OptionalPin.bShowPin == false)
		{
			continue;
		}
		UEdGraphPin* OriginPin = FindPinChecked(OptionalPin.PropertyName);
		{
			UK2Node_VariableSet* SetVariableNode = CompilerContext.SpawnIntermediateNode<UK2Node_VariableSet>(this, SourceGraph);
			SetVariableNode->VariableReference.SetExternalMember(OptionalPin.PropertyName, Class);
			SetVariableNode->AllocateDefaultPins();
			SetVariableNode->GetExecPin()->MakeLinkTo(AuthorityThenPin);
			ScriptInstancePin->MakeLinkTo(SetVariableNode->FindPinChecked(UEdGraphSchema_K2::PN_Self));
			AuthorityThenPin = SetVariableNode->FindPinChecked(UEdGraphSchema_K2::PN_Then);
			CompilerContext.CopyPinLinksToIntermediate(*OriginPin, *SetVariableNode->FindPinChecked(OptionalPin.PropertyName));
		}
		{
			const FProperty* PinProperty = Class->FindPropertyByName(OptionalPin.PropertyName);
			if (PinProperty && PinProperty->HasAnyPropertyFlags(CPF_Net) == false)
			{
				UK2Node_VariableSet* SetVariableNode = CompilerContext.SpawnIntermediateNode<UK2Node_VariableSet>(this, SourceGraph);
				SetVariableNode->VariableReference.SetExternalMember(OptionalPin.PropertyName, Class);
				SetVariableNode->AllocateDefaultPins();
				SetVariableNode->GetExecPin()->MakeLinkTo(ClientThenPin);
				ScriptInstancePin->MakeLinkTo(SetVariableNode->FindPinChecked(UEdGraphSchema_K2::PN_Self));
				ClientThenPin = SetVariableNode->FindPinChecked(UEdGraphSchema_K2::PN_Then);
				CompilerContext.CopyPinLinksToIntermediate(*OriginPin, *SetVariableNode->FindPinChecked(OptionalPin.PropertyName));
			}
		}
		OriginPin->BreakAllPinLinks();
	}
}

void UBPNode_GameQuestElementScript::CopyTermDefaultsToDefaultNode(FGameQuestGraphCompilerContext& CompilerContext, UObject* DefaultObject, UGameQuestGraphGeneratedClass* ObjectClass, FStructProperty* NodeProperty)
{
	Super::CopyTermDefaultsToDefaultNode(CompilerContext, DefaultObject, ObjectClass, NodeProperty);

	UGameQuestElementScriptable* Instance = ScriptInstance;
	if (!ensure(Instance))
	{
		return;
	}

	UBlueprint::ForceLoad(Instance);
	UBlueprint::ForceLoadMembers(Instance);
	FGameQuestElementScript* ActionScript = NodeProperty->ContainerPtrToValuePtr<FGameQuestElementScript>(DefaultObject);
	FObjectDuplicationParameters Parameters(Instance, DefaultObject);
	Parameters.DestName = GetRefVarName();
	Parameters.DestClass = Instance->GetClass();
	Parameters.ApplyFlags = RF_Public | RF_DefaultSubObject | RF_ArchetypeObject;
	ActionScript->Instance = CastChecked<UGameQuestElementScriptable>(::StaticDuplicateObjectEx(Parameters));
}

TSubclassOf<UGameQuestGraphBase> UBPNode_GameQuestElementScript::GetSupportQuest() const
{
	if (ScriptInstance)
	{
		return ScriptInstance->SupportType.Get();
	}
	if (UnloadNodeData.IsSet())
	{
		return UnloadNodeData->SupportType.Get();
	}
	return nullptr;
}

void UBPNode_GameQuestElementScript::InitialByClass(const TSubclassOf<UGameQuestElementScriptable>& Class)
{
	StructNodeInstance.InitializeAs(GetNodeStruct());
	ScriptInstance = NewObject<UGameQuestElementScriptable>(this, Class, NAME_None, RF_Transactional | RF_ArchetypeObject);
}

#undef LOCTEXT_NAMESPACE
