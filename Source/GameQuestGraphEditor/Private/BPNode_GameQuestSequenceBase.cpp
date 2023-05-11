// Fill out your copyright notice in the Description page of Project Settings.

#include "BPNode_GameQuestSequenceBase.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "BPNode_GameQuestElementBase.h"
#include "BPNode_GameQuestListSequenceUtils.h"
#include "GameQuestElementBase.h"
#include "GameQuestGraphBase.h"
#include "GameQuestGraphBlueprint.h"
#include "GameQuestGraphEditorSettings.h"
#include "GameQuestSequenceBase.h"
#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Knot.h"
#include "K2Node_StructMemberGet.h"
#include "K2Node_StructMemberSet.h"
#include "K2Node_VariableGet.h"
#include "KismetCompiler.h"
#include "ToolMenu.h"
#include "ToolMenuMisc.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "GameQuestGraphEditor"

FText UBPNode_GameQuestSequenceBase::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::MenuTitle)
	{
		if (const UStruct* NodeType = GetNodeImplStruct())
		{
			return FText::Format(LOCTEXT("GameQuestSequenceNodeTitle", "Add Quest {0}"), NodeType->GetDisplayNameText());
		}
	}
	return Super::GetNodeTitle(TitleType);
}

FText UBPNode_GameQuestSequenceBase::GetMenuCategory() const
{
	return LOCTEXT("GameQuestSequenceCategory", "GameQuest|Sequence");
}

void UBPNode_GameQuestSequenceBase::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
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
	const FStructProperty* SequenceProperty = FindFProperty<FStructProperty>(DebuggedQuest->GetClass(), GetRefVarName());
	if (SequenceProperty == nullptr || SequenceProperty->Struct->IsChildOf(FGameQuestSequenceBase::StaticStruct()) == false)
	{
		return;
	}
	const FGameQuestSequenceBase* SequenceInstance = SequenceProperty->ContainerPtrToValuePtr<FGameQuestSequenceBase>(DebuggedQuest);
	if (SequenceInstance->GetSequenceState() != FGameQuestSequenceBase::EState::Deactivated)
	{
		return;
	}
	FToolMenuSection& Section = Menu->AddSection(TEXT("Debug"), LOCTEXT("Debug", "Debug"), FToolMenuInsert(TEXT("GraphNodeComment"), EToolMenuInsertType::Before));
	Section.AddMenuEntry(
			TEXT("ForceActivateSequence"),
			LOCTEXT("ForceActivateSequence", "Force activate sequence"),
			LOCTEXT("ForceActivateSequenceDesc", "Force activate selected sequence"),
			FSlateIcon(),
			FUIAction(
			FExecuteAction::CreateWeakLambda(DebuggedQuest, [SequenceInstance, DebuggedQuest]
			{
				if (DebuggedQuest->GetQuestState() == UGameQuestGraphBase::EState::Activated
					&& SequenceInstance->GetSequenceState() == FGameQuestSequenceBase::EState::Deactivated)
				{
					DebuggedQuest->ForceActivateSequenceToServer(DebuggedQuest->GetSequenceId(SequenceInstance));
				}
			}))
		);
}

void UBPNode_GameQuestSequenceBase::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const FName RefVarName = GetRefVarName();
	UK2Node_VariableGet* GetStructNode = CompilerContext.SpawnIntermediateNode<UK2Node_VariableGet>(this);
	GetStructNode->VariableReference.SetSelfMember(RefVarName);
	GetStructNode->AllocateDefaultPins();
	UEdGraphPin* RefVarPin = GetStructNode->FindPinChecked(RefVarName);

	UK2Node_CallFunction* CallTryActivateNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this);
	CallTryActivateNode->SetFromFunction(UGameQuestGraphBase::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UGameQuestGraphBase, TryActivateSequence)));
	CallTryActivateNode->AllocateDefaultPins();
	UEdGraphPin* SequenceRefPin = CallTryActivateNode->FindPinChecked(TEXT("Sequence"));
	SequenceRefPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
	SequenceRefPin->PinType.PinSubCategoryObject = GetNodeStruct();
	SequenceRefPin->MakeLinkTo(RefVarPin);

	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *CallTryActivateNode->GetExecPin());
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(UEdGraphSchema_K2::PN_ReturnValue), *RefVarPin);
}

FText UBPNode_GameQuestSequenceSingle::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::MenuTitle)
	{
		static const FTextFormat NodeTitle = LOCTEXT("GameQuestSequenceSingleNodeTitle", "Add Quest Single {0}");
		if (ElementNodeStruct)
		{
			return FText::Format(NodeTitle, ElementNodeStruct->GetDisplayNameText());
		}
		if (ScriptNodeData.IsSet())
		{
			if (const UClass* Class = ScriptNodeData->Class.Get())
			{
				return FText::Format(NodeTitle, Class->GetDisplayNameText());
			}
			else
			{
				FString ClassName = ScriptNodeData->Class.GetAssetName();
				ClassName.RemoveFromEnd(TEXT("_C"));
				return FText::Format(NodeTitle, FText::FromString(ClassName));
			}
		}
	}
	return Super::GetNodeTitle(TitleType);
}

FText UBPNode_GameQuestSequenceSingle::GetMenuCategory() const
{
	return LOCTEXT("GameQuestSequenceSingleCategory", "GameQuest|Sequence|Single");
}

void UBPNode_GameQuestSequenceSingle::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	const UScriptStruct* SequenceSingleType = GetNodeStruct();
	if (SequenceSingleType == nullptr)
	{
		return;
	}

	const UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey) == false)
	{
		return;
	}

	const FGameQuestStructCollector& Collector = FGameQuestStructCollector::Get();
	for (const auto& [ElementStruct, _] : Collector.ValidNodeStructMap)
	{
		if (ElementStruct->IsChildOf(FGameQuestElementBase::StaticStruct()) == false)
		{
			continue;
		}
		static FName MD_BranchElement = TEXT("BranchElement");
		if (ElementStruct->HasMetaData(MD_BranchElement))
		{
			continue;
		}
		const TSubclassOf<UBPNode_GameQuestNodeBase> NodeClass = Collector.GetBPNodeClass(ElementStruct);
		if (NodeClass == nullptr)
		{
			continue;
		}

		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateLambda([ElementStruct, SequenceSingleType](UEdGraphNode* NewNode, bool bIsTemplateNode) mutable
		{
			UBPNode_GameQuestSequenceSingle* SingleNode = CastChecked<UBPNode_GameQuestSequenceSingle>(NewNode);
			SingleNode->StructNodeInstance.InitializeAs(SequenceSingleType);
			if (bIsTemplateNode)
			{
				SingleNode->ElementNodeStruct = ElementStruct;
				FInstancedStruct ElementInstance{ ElementStruct };

			}
			else if (const TSubclassOf<UBPNode_GameQuestNodeBase> NodeClass = FGameQuestStructCollector::Get().GetBPNodeClass(ElementStruct))
			{
				UEdGraph* ParentGraph = SingleNode->GetGraph();
				UBPNode_GameQuestElementStruct* ElementNode = NewObject<UBPNode_GameQuestElementStruct>(ParentGraph, NodeClass, NAME_None, RF_Transactional);
				ElementNode->CreateNewGuid();
				ElementNode->StructNodeInstance.InitializeAs(ElementStruct);
				ElementNode->AllocateDefaultPins();
				SingleNode->Element = ElementNode;
				ElementNode->OwnerNode = SingleNode;

				ParentGraph->AddNode(ElementNode);
				ElementNode->PostPlacedNewNode();
			}
		});

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}

	auto CreateScriptNode = [SequenceSingleType](UBPNode_GameQuestSequenceSingle* SingleNode, const TSubclassOf<UGameQuestElementScriptable>& Class)
	{
		SingleNode->StructNodeInstance.InitializeAs(SequenceSingleType);
		UEdGraph* ParentGraph = SingleNode->GetGraph();
		UBPNode_GameQuestElementScript* ElementNode = NewObject<UBPNode_GameQuestElementScript>(ParentGraph, NAME_None, RF_Transactional);
		ElementNode->CreateNewGuid();
		ElementNode->InitialByClass(Class);
		ElementNode->AllocateDefaultPins();
		SingleNode->Element = ElementNode;
		ElementNode->OwnerNode = SingleNode;

		ParentGraph->AddNode(ElementNode);
		ElementNode->PostPlacedNewNode();
	};
	FGameQuestClassCollector::Get().ForEachDerivedClasses(UGameQuestElementScriptable::StaticClass(),
	[&](const TSubclassOf<UGameQuestElementScriptable>& Class, const TSoftClassPtr<UGameQuestGraphBase>& SupportType)
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateLambda([=](UEdGraphNode* NewNode, bool bIsTemplateNode) mutable
		{
			if (bIsTemplateNode)
			{
				UBPNode_GameQuestSequenceSingle* ElementScriptNode = CastChecked<UBPNode_GameQuestSequenceSingle>(NewNode);
				ElementScriptNode->ScriptNodeData = FScriptNodeData{ TSoftClassPtr<UGameQuestElementScriptable>{ Class }, SupportType };
			}
			else
			{
				CreateScriptNode(CastChecked<UBPNode_GameQuestSequenceSingle>(NewNode), Class);
			}
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
				UBPNode_GameQuestSequenceSingle* ElementScriptNode = CastChecked<UBPNode_GameQuestSequenceSingle>(NewNode);
				ElementScriptNode->ScriptNodeData = FScriptNodeData{ SoftClass, SupportType };
			}
			else if (const TSubclassOf<UGameQuestElementScriptable> Class = SoftClass.LoadSynchronous())
			{
				CreateScriptNode(CastChecked<UBPNode_GameQuestSequenceSingle>(NewNode), Class);
			}
		});
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	});
}

void UBPNode_GameQuestSequenceSingle::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreateExposePins();
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, GameQuestUtils::Pin::BranchPinSubCategory, GameQuestUtils::Pin::BranchPinName);
	UEdGraphPin* ReturnValuePin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, GetNodeStruct(), UEdGraphSchema_K2::PN_ReturnValue);
	ReturnValuePin->bAdvancedView = true;
}

class SNode_GameQuestSequenceSingle : public SNode_GameQuestNodeBase
{
	using Super = SNode_GameQuestNodeBase;
public:
	void Construct(const FArguments& InArgs, UBPNode_GameQuestSequenceSingle* InNode)
	{
		SingleNode = InNode;
		Super::Construct(InArgs, InNode);
	}
	int32 GetSortDepth() const override { return 1; }
	void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter, bool bMarkDirty) override
	{
		Super::MoveTo(NewPosition, NodeFilter, bMarkDirty);
		if (UEdGraphNode* ElementNode = SingleNode->Element)
		{
			ElementNode->NodePosX = SingleNode->NodePosX;
			ElementNode->NodePosY = SingleNode->NodePosY;
		}
	}
	void CacheDesiredSize(float InLayoutScaleMultiplier) override
	{
		Super::CacheDesiredSize(InLayoutScaleMultiplier);
		SingleNode->NodeSize = GetDesiredSize();
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
	UBPNode_GameQuestSequenceSingle* SingleNode;
};

TSharedPtr<SGraphNode> UBPNode_GameQuestSequenceSingle::CreateVisualWidget()
{
	return SNew(SNode_GameQuestSequenceSingle, this);
}

FSlateIcon UBPNode_GameQuestSequenceSingle::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.Macro.DoOnce_16x");
	return Icon;
}

void UBPNode_GameQuestSequenceSingle::PostPlacedNewNode()
{
	Super::PostPlacedNewNode();

	if (Element)
	{
		Element->NodePosX = NodePosX;
		Element->NodePosY = NodePosY;
		FindPinChecked(GameQuestUtils::Pin::BranchPinName)->MakeLinkTo(Element->FindPinChecked(GameQuestUtils::Pin::BranchPinName));
	}
}

bool UBPNode_GameQuestSequenceSingle::CanPasteHere(const UEdGraph* TargetGraph) const
{
	using namespace GameQuestUtils::Pin;
	const UEdGraphPin* BranchPin = FindPinChecked(BranchPinName);
	if (BranchPin->LinkedTo.Num() == 0 || BranchPin->LinkedTo[0] == nullptr)
	{
		return false;
	}
	const UBPNode_GameQuestElementBase* LinkedElement = Cast<UBPNode_GameQuestElementBase>(BranchPin->LinkedTo[0]->GetOwningNode());
	if (LinkedElement == nullptr)
	{
		return false;
	}
	const UEdGraphPin* ElementPin = LinkedElement->FindPin(BranchPinName);
	if (ElementPin == nullptr || ElementPin->LinkedTo.Num() == 0 || ElementPin->LinkedTo[0]->GetOwningNode() != this)
	{
		return false;
	}
	return Super::CanPasteHere(TargetGraph);
}

void UBPNode_GameQuestSequenceSingle::DestroyNode()
{
	if (Element)
	{
		FBlueprintEditorUtils::RemoveNode(GetBlueprint(), Element, true);
		Element = nullptr;
	}
	Super::DestroyNode();
}

bool UBPNode_GameQuestSequenceSingle::IsActionFilteredOut(FBlueprintActionFilter const& Filter)
{
	TSubclassOf<UGameQuestGraphBase> SupportType;
	if (ElementNodeStruct)
	{
		const FGameQuestStructCollector& Collector = FGameQuestStructCollector::Get();
		const FInstancedStruct* StructDefaultValue = Collector.ValidNodeStructMap.Find(ElementNodeStruct);
		SupportType = StructDefaultValue ? StructDefaultValue->Get<FGameQuestElementBase>().GetSupportQuestGraph() : nullptr;
	}
	else if (ScriptNodeData.IsSet())
	{
		SupportType = ScriptNodeData->SupportType.Get();
	}
	if (SupportType == nullptr)
	{
		return true;
	}

	for (const UBlueprint* Blueprint : Filter.Context.Blueprints)
	{
		if (const UClass* Class = Blueprint->GeneratedClass)
		{
			if (Class == nullptr || !Class->IsChildOf(SupportType))
			{
				return true;
			}
		}
	}
	return false;
}

void UBPNode_GameQuestSequenceSingle::CreateClassVariablesFromNode(FGameQuestGraphCompilerContext& CompilerContext)
{
	if (ensure(Element))
	{
		Element->CreateClassVariablesFromNode(CompilerContext);
	}

	Super::CreateClassVariablesFromNode(CompilerContext);
}

void UBPNode_GameQuestSequenceSingle::CopyTermDefaultsToDefaultNode(FGameQuestGraphCompilerContext& CompilerContext, UObject* DefaultObject, UGameQuestGraphGeneratedClass* ObjectClass, FStructProperty* NodeProperty)
{
	Super::CopyTermDefaultsToDefaultNode(CompilerContext, DefaultObject, ObjectClass, NodeProperty);

	if (ensure(Element))
	{
		FGameQuestSequenceSingle* SequenceSingle = NodeProperty->ContainerPtrToValuePtr<FGameQuestSequenceSingle>(DefaultObject);
		SequenceSingle->Element = Element->NodeId;
		FStructProperty* ElementProperty = FindFProperty<FStructProperty>(DefaultObject->GetClass(), Element->GetRefVarName());
		if (ensure(ElementProperty))
		{
			Element->CopyTermDefaultsToDefaultNode(CompilerContext, DefaultObject, ObjectClass, ElementProperty);
		}
	}
}

UScriptStruct* UBPNode_GameQuestSequenceSingle::GetBaseNodeStruct() const
{
	return FGameQuestSequenceSingle::StaticStruct();
}

UScriptStruct* UBPNode_GameQuestSequenceSingle::GetNodeStruct() const
{
	return GetDefault<UGameQuestGraphEditorSettings>()->SequenceSingleType.Get();
}

TSharedPtr<SGraphNode> UBPNode_GameQuestSequenceListBase::CreateVisualWidget()
{
	return SNew(SNode_GameQuestList<UBPNode_GameQuestSequenceListBase>, this);
}

void UBPNode_GameQuestSequenceListBase::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	Super::GetNodeContextMenuActions(Menu, Context);

	CreateAddElementMenu(const_cast<UBPNode_GameQuestSequenceListBase*>(this), Menu, Context);
}

void UBPNode_GameQuestSequenceListBase::DestroyNode()
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

void UBPNode_GameQuestSequenceListBase::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	CheckOptionalIsValid(this, CompilerContext.MessageLog);
}

void UBPNode_GameQuestSequenceListBase::PostPasteNode()
{
	Super::PostPasteNode();

	QuestListPostPasteNode(this);
}

void UBPNode_GameQuestSequenceListBase::CopyTermDefaultsToDefaultNode(FGameQuestGraphCompilerContext& CompilerContext, UObject* DefaultObject, UGameQuestGraphGeneratedClass* ObjectClass, FStructProperty* NodeProperty)
{
	Super::CopyTermDefaultsToDefaultNode(CompilerContext, DefaultObject, ObjectClass, NodeProperty);

	GameQuest::FLogicList& LogicList = ObjectClass->NodeIdLogicsMap.Add(NodeId);
	ensure(Elements.Num() == 0 || ElementLogics.Num() == Elements.Num() - 1);
	for (EGameQuestSequenceLogic Logic : ElementLogics)
	{
		LogicList.Add(Logic);
	}
}

void UBPNode_GameQuestSequenceListBase::AddElement(UBPNode_GameQuestElementBase* Element)
{
	QuestListAddElement(this, Element);
}

void UBPNode_GameQuestSequenceListBase::InsertElementNode(UBPNode_GameQuestElementBase* Element, int32 Idx)
{
	QuestListInsertElement(this, Element, Idx);
}

void UBPNode_GameQuestSequenceListBase::RemoveElement(UBPNode_GameQuestElementBase* Element)
{
	QuestListRemoveElement(this, Element);
}

void UBPNode_GameQuestSequenceListBase::MoveElementNode(UBPNode_GameQuestElementBase* Element, int32 Idx)
{
	QuestListMoveElement(this, Element, Idx);
}

void UBPNode_GameQuestSequenceList::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreatePin(EGPD_Input, GameQuestUtils::Pin::ListPinCategory, GameQuestUtils::Pin::ListPinName);
	CreateExposePins();
	UEdGraphPin* FinishedPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, GET_MEMBER_NAME_CHECKED(FGameQuestSequenceList, OnSequenceFinished));
	FinishedPin->PinFriendlyName = LOCTEXT("FinishedPinName", "Then");
	UEdGraphPin* ReturnValuePin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, GetNodeStruct(), UEdGraphSchema_K2::PN_ReturnValue);
	ReturnValuePin->bAdvancedView = true;
}

FSlateIcon UBPNode_GameQuestSequenceList::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.Macro.DoN_16x");
	return Icon;
}

void UBPNode_GameQuestSequenceList::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	if (Elements.Num() == 0)
	{
		CompilerContext.MessageLog.Error(TEXT("@@Must have one or more elements"), this);
	}

	UEdGraphPin* ThenPin = FindPinChecked(GET_MEMBER_NAME_CHECKED(FGameQuestSequenceList, OnSequenceFinished));
	if (ThenPin->LinkedTo.Num() > 0)
	{
		UK2Node_Event* ActivateNextSequenceEvent = CompilerContext.SpawnIntermediateEventNode<UK2Node_Event>(this, nullptr, SourceGraph);
		ActivateNextSequenceEvent->bInternalEvent = true;
		ActivateNextSequenceEvent->CustomFunctionName = FGameQuestFinishEvent::MakeEventName(GetRefVarName(), GET_MEMBER_NAME_CHECKED(FGameQuestSequenceList, OnSequenceFinished));
		ActivateNextSequenceEvent->AllocateDefaultPins();

		CompilerContext.MovePinLinksToIntermediate(*ThenPin, *ActivateNextSequenceEvent->FindPinChecked(UEdGraphSchema_K2::PN_Then));
	}
}

void UBPNode_GameQuestSequenceList::CreateClassVariablesFromNode(FGameQuestGraphCompilerContext& CompilerContext)
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

void UBPNode_GameQuestSequenceList::CopyTermDefaultsToDefaultNode(FGameQuestGraphCompilerContext& CompilerContext, UObject* DefaultObject, UGameQuestGraphGeneratedClass* ObjectClass, FStructProperty* NodeProperty)
{
	Super::CopyTermDefaultsToDefaultNode(CompilerContext, DefaultObject, ObjectClass, NodeProperty);

	FGameQuestSequenceList* SequenceList = NodeProperty->ContainerPtrToValuePtr<FGameQuestSequenceList>(DefaultObject);
	SequenceList->Elements.Empty();
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
		SequenceList->Elements.Add(Element->NodeId);
		Element->CopyTermDefaultsToDefaultNode(CompilerContext, DefaultObject, ObjectClass, ElementProperty);
	}
}

UScriptStruct* UBPNode_GameQuestSequenceList::GetBaseNodeStruct() const
{
	return FGameQuestSequenceList::StaticStruct();
}

UScriptStruct* UBPNode_GameQuestSequenceList::GetNodeStruct() const
{
	return GetDefault<UGameQuestGraphEditorSettings>()->SequenceListType.Get();
}

void UBPNode_GameQuestSequenceBranch::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	using namespace GameQuestUtils;
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreatePin(EGPD_Input, Pin::ListPinCategory, Pin::ListPinName);
	CreateExposePins();
	UEdGraphPin* BranchPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, Pin::BranchPinSubCategory, Pin::BranchPinName);
	BranchPin->PinFriendlyName = LOCTEXT("BranchPinName", "BranchPin");
	UEdGraphPin* ReturnValuePin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, GetNodeStruct(), UEdGraphSchema_K2::PN_ReturnValue);
	ReturnValuePin->bAdvancedView = true;
}

FSlateIcon UBPNode_GameQuestSequenceBranch::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.Branch_16x");
	return Icon;
}

void UBPNode_GameQuestSequenceBranch::CreateClassVariablesFromNode(FGameQuestGraphCompilerContext& CompilerContext)
{
	using namespace GameQuestUtils;
	for (UBPNode_GameQuestElementBase* Element : Elements)
	{
		if (!ensure(Element))
		{
			continue;
		}
		Element->CreateClassVariablesFromNode(CompilerContext);
	}

	UEdGraphPin* BranchPin = FindPinChecked(Pin::BranchPinName);
	for (const UEdGraphPin* LinkedTo : BranchPin->LinkedTo)
	{
		UBPNode_GameQuestElementBase* BranchElement = Cast<UBPNode_GameQuestElementBase>(LinkedTo->GetOwningNode());
		if (!ensure(BranchElement))
		{
			continue;
		}
		BranchElement->CreateClassVariablesFromNode(CompilerContext);
	}

	Super::CreateClassVariablesFromNode(CompilerContext);
}

void UBPNode_GameQuestSequenceBranch::CopyTermDefaultsToDefaultNode(FGameQuestGraphCompilerContext& CompilerContext, UObject* DefaultObject, UGameQuestGraphGeneratedClass* ObjectClass, FStructProperty* NodeProperty)
{
	Super::CopyTermDefaultsToDefaultNode(CompilerContext, DefaultObject, ObjectClass, NodeProperty);

	using namespace GameQuestUtils;
	FGameQuestSequenceBranch* SequenceBranch = NodeProperty->ContainerPtrToValuePtr<FGameQuestSequenceBranch>(DefaultObject);
	SequenceBranch->Elements.Empty();
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
		SequenceBranch->Elements.Add(Element->NodeId);
		Element->CopyTermDefaultsToDefaultNode(CompilerContext, DefaultObject, ObjectClass, ElementProperty);
	}
	SequenceBranch->Branches.Empty();
	UEdGraphPin* BranchPin = FindPinChecked(Pin::BranchPinName);
	for (const UEdGraphPin* LinkedTo : BranchPin->LinkedTo)
	{
		UBPNode_GameQuestElementBase* BranchElement = Cast<UBPNode_GameQuestElementBase>(LinkedTo->GetOwningNode());
		if (!ensure(BranchElement))
		{
			continue;
		}
		FStructProperty* ElementProperty = FindFProperty<FStructProperty>(ObjectClass, BranchElement->GetRefVarName());
		if (!ensure(ElementProperty))
		{
			continue;
		}
		FGameQuestSequenceBranchElement& Branch = SequenceBranch->Branches.AddDefaulted_GetRef();
		Branch.Element = BranchElement->NodeId;
		Branch.bAutoDeactivateOtherBranch = BranchElement->bAutoDeactivateOtherBranch;
		BranchElement->CopyTermDefaultsToDefaultNode(CompilerContext, DefaultObject, ObjectClass, ElementProperty);
	}
}

UScriptStruct* UBPNode_GameQuestSequenceBranch::GetBaseNodeStruct() const
{
	return FGameQuestSequenceBranch::StaticStruct();
}

UScriptStruct* UBPNode_GameQuestSequenceBranch::GetNodeStruct() const
{
	return GetDefault<UGameQuestGraphEditorSettings>()->SequenceBranchType.Get();
}

void UBPNode_GameQuestSequenceSubQuest::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreateExposePins();

	if (const TSubclassOf<UGameQuestGraphBase> Class = SubQuestClass)
	{
		const TMap<FName, bool> OldSubQuestShowPins = SubQuestShowPins;
		SubQuestShowPins.Reset();
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		for (TFieldIterator<FProperty> It{ Class }; It; ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_ExposeOnSpawn) == false)
			{
				continue;
			}
			const FName PropertyName = It->GetFName();
			const bool bIsShowPin = OldSubQuestShowPins.FindRef(PropertyName);
			SubQuestShowPins.Add(PropertyName, bIsShowPin);
			if (bIsShowPin)
			{
				FEdGraphPinType PinType;
				if (Schema->ConvertPropertyToPinType(*It, PinType))
				{
					CreatePin(EGPD_Input, PinType, PropertyName);
				}
			}
		}

		for (const FName& FinishedTag : Class.GetDefaultObject()->GetFinishedTagNames())
		{
			CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, NAME_None, FinishedTag);
		}
	}
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, NAME_None, FGameQuestFinishedTag::FinishCompletedTagName);

	UEdGraphPin* ReturnValuePin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, GetNodeStruct(), UEdGraphSchema_K2::PN_ReturnValue);
	ReturnValuePin->bAdvancedView = true;
}

FSlateIcon UBPNode_GameQuestSequenceSubQuest::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("GameQuestGraphSlateStyle", "Graph.Quest");
	return Icon;
}

void UBPNode_GameQuestSequenceSubQuest::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, SubQuestClass))
	{
		if (FGameQuestSequenceSubQuest* SubQuestSequence = StructNodeInstance.GetMutablePtr<FGameQuestSequenceSubQuest>())
		{
			SubQuestSequence->SubQuestClass = SubQuestClass;
		};
		ReconstructNode();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ThisClass, SubQuestShowPins))
	{
		ReconstructNode();
	}
}

UObject* UBPNode_GameQuestSequenceSubQuest::GetJumpTargetForDoubleClick() const
{
	const FGameQuestSequenceSubQuest* SubQuestSequence = StructNodeInstance.GetPtr<FGameQuestSequenceSubQuest>();
	if (SubQuestSequence == nullptr)
	{
		return nullptr;
	}
	const TSubclassOf<UGameQuestGraphBase> Class = SubQuestClass;
	if (Class == nullptr || Class->ClassGeneratedBy == nullptr)
	{
		return nullptr;
	}
	return Class->ClassGeneratedBy;
}

void UBPNode_GameQuestSequenceSubQuest::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const FGameQuestSequenceSubQuest* SubQuestSequence = StructNodeInstance.GetPtr<FGameQuestSequenceSubQuest>();
	if (SubQuestSequence == nullptr)
	{
		return;
	}
	const TSubclassOf<UGameQuestGraphBase> Class = SubQuestClass;
	if (Class == nullptr)
	{
		CompilerContext.MessageLog.Error(TEXT("@@SubQuestClass is none"), this);
		return;
	}
	const FName RefVarName = GetRefVarName();
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin->Direction != EGPD_Output || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
		{
			continue;
		}
		UEdGraphPin* FinishPin = Pin;
		if (FinishPin->LinkedTo.Num() == 0)
		{
			continue;
		}
		UK2Node_Event* PostFinishTagEvent = CompilerContext.SpawnIntermediateEventNode<UK2Node_Event>(this, nullptr, SourceGraph);
		PostFinishTagEvent->bInternalEvent = true;
		PostFinishTagEvent->CustomFunctionName = FGameQuestFinishedTag::MakeEventName(RefVarName, Pin->GetFName());
		PostFinishTagEvent->AllocateDefaultPins();
		CompilerContext.MovePinLinksToIntermediate(*FinishPin, *PostFinishTagEvent->FindPinChecked(UEdGraphSchema_K2::PN_Then));
	}
}

bool UBPNode_GameQuestSequenceSubQuest::HasSubQuestExposePin() const
{
	for (const auto& [Name, IsExpose] : SubQuestShowPins)
	{
		if (IsExpose)
		{
			return true;
		}
	}
	return false;
}

bool UBPNode_GameQuestSequenceSubQuest::HasEvaluateActionParams() const
{
	return HasSubQuestExposePin() || Super::HasEvaluateActionParams();
}

void UBPNode_GameQuestSequenceSubQuest::ExpandNodeForEvaluateActionParams(UEdGraphPin*& AuthorityThenPin, UEdGraphPin*& ClientThenPin, FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNodeForEvaluateActionParams(AuthorityThenPin, ClientThenPin, CompilerContext, SourceGraph);
	if (HasSubQuestExposePin())
	{
		const FGameQuestSequenceSubQuest* SubQuestSequence = StructNodeInstance.GetPtr<FGameQuestSequenceSubQuest>();
		if (SubQuestSequence == nullptr)
		{
			return;
		}
		const TSubclassOf<UGameQuestGraphBase> Class = SubQuestClass;
		if (Class == nullptr)
		{
			return;
		}

		UK2Node_StructMemberGet* GetSubQuestNode = CompilerContext.SpawnIntermediateNode<UK2Node_StructMemberGet>(this, SourceGraph);
		GetSubQuestNode->VariableReference.SetSelfMember(GetRefVarName());
		GetSubQuestNode->StructType = GetNodeStruct();
		GetSubQuestNode->AllocatePinsForSingleMemberGet(GET_MEMBER_NAME_CHECKED(FGameQuestSequenceSubQuest, SubQuestInstance));
		UEdGraphPin* SubQuestPin = GetSubQuestNode->FindPinChecked(GET_MEMBER_NAME_CHECKED(FGameQuestSequenceSubQuest, SubQuestInstance));

		UK2Node_CallFunction* ValidObjectNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		ValidObjectNode->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, IsValid)));
		ValidObjectNode->AllocateDefaultPins();
		ValidObjectNode->FindPinChecked(TEXT("Object"))->MakeLinkTo(SubQuestPin);

		UK2Node_IfThenElse* AuthorityIfThenElseNode = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
		{
			AuthorityIfThenElseNode->AllocateDefaultPins();
			ValidObjectNode->GetReturnValuePin()->MakeLinkTo(AuthorityIfThenElseNode->GetConditionPin());
			AuthorityIfThenElseNode->GetExecPin()->MakeLinkTo(AuthorityThenPin);
			AuthorityThenPin = AuthorityIfThenElseNode->GetThenPin();
		}

		UK2Node_IfThenElse* ClientIfThenElseNode = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
		{
			ClientIfThenElseNode->AllocateDefaultPins();
			ValidObjectNode->GetReturnValuePin()->MakeLinkTo(ClientIfThenElseNode->GetConditionPin());
			ClientIfThenElseNode->GetExecPin()->MakeLinkTo(ClientThenPin);
			ClientThenPin = ClientIfThenElseNode->GetThenPin();
		}

		for (const auto& [PropertyName, IsExpose] : SubQuestShowPins)
		{
			if (IsExpose == false)
			{
				continue;
			}
			UEdGraphPin* OriginPin = FindPinChecked(PropertyName);
			{
				CreateAssignByNameFunction(CompilerContext, SourceGraph, OriginPin, SubQuestPin, AuthorityThenPin);
			}
			{
				const FProperty* PinProperty = Class->FindPropertyByName(PropertyName);
				if (PinProperty && PinProperty->HasAnyPropertyFlags(CPF_Net) == false)
				{
					CreateAssignByNameFunction(CompilerContext, SourceGraph, OriginPin, SubQuestPin, ClientThenPin);
				}
			}
			OriginPin->BreakAllPinLinks();
		}

		{
			UK2Node_Knot* KnotNode = CompilerContext.SpawnIntermediateNode<UK2Node_Knot>(this, SourceGraph);
			KnotNode->AllocateDefaultPins();
			AuthorityThenPin->MakeLinkTo(KnotNode->GetInputPin());
			AuthorityIfThenElseNode->GetElsePin()->MakeLinkTo(KnotNode->GetInputPin());
			AuthorityThenPin = KnotNode->GetOutputPin();
		}
		{
			UK2Node_Knot* KnotNode = CompilerContext.SpawnIntermediateNode<UK2Node_Knot>(this, SourceGraph);
			KnotNode->AllocateDefaultPins();
			ClientThenPin->MakeLinkTo(KnotNode->GetInputPin());
			ClientIfThenElseNode->GetElsePin()->MakeLinkTo(KnotNode->GetInputPin());
			ClientThenPin = KnotNode->GetOutputPin();
		}
	}
}

UScriptStruct* UBPNode_GameQuestSequenceSubQuest::GetBaseNodeStruct() const
{
	return FGameQuestSequenceSubQuest::StaticStruct();
}

UScriptStruct* UBPNode_GameQuestSequenceSubQuest::GetNodeStruct() const
{
	return GetDefault<UGameQuestGraphEditorSettings>()->SequenceSubQuestType.Get();
}

#undef LOCTEXT_NAMESPACE
