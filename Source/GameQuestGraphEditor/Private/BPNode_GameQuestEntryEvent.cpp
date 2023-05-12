// Fill out your copyright notice in the Description page of Project Settings.


#include "BPNode_GameQuestEntryEvent.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraphSchemaGameQuest.h"
#include "GameQuestGraphBase.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_IfThenElse.h"
#include "KismetCompiler.h"
#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "GameQuestGraphEditor"

FText UBPNode_GameQuestEntryEvent::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::MenuTitle)
	{
		return LOCTEXT("QuestEntryMenuTitle", "Add Custom Quest Entry");
	}
	if (bOverrideFunction)
	{
		FText FunctionName = FText::FromName(EventReference.GetMemberName());
		if (const UFunction* Function = EventReference.ResolveMember<UFunction>(GetBlueprintClassFromNode()))
		{
			FunctionName = UEdGraphSchema_K2::GetFriendlySignatureName(Function);
		}
		if (TitleType == ENodeTitleType::EditableTitle || TitleType == ENodeTitleType::ListView)
		{
			return FunctionName;
		}
		return FText::Format(LOCTEXT("QuestEntryEventName", "Event {0}\nQuest Entry Event"), FunctionName);
	}
	if (TitleType == ENodeTitleType::EditableTitle || TitleType == ENodeTitleType::ListView)
	{
		return FText::FromName(CustomFunctionName);
	}
	return FText::Format(LOCTEXT("QuestCustomEntryEventName", "Event {0}\nQuest Entry Event"), FText::FromName(CustomFunctionName));
}

FText UBPNode_GameQuestEntryEvent::GetMenuCategory() const
{
	return LOCTEXT("QuestEntryCategory", "GameQuest|FlowControl");
}

FSlateIcon UBPNode_GameQuestEntryEvent::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("GameQuestGraphSlateStyle", "Graph.Event.Start");
	return Icon;
}

bool UBPNode_GameQuestEntryEvent::CanUserDeleteNode() const
{
	return bOverrideFunction == false || EventReference.GetMemberName() != GET_FUNCTION_NAME_CHECKED(UGameQuestGraphBase, DefaultEntry);
}

bool UBPNode_GameQuestEntryEvent::CanDuplicateNode() const
{
	return bOverrideFunction == false || EventReference.GetMemberName() != GET_FUNCTION_NAME_CHECKED(UGameQuestGraphBase, DefaultEntry);
}

bool UBPNode_GameQuestEntryEvent::GetCanRenameNode() const
{
	return bOverrideFunction == false || EventReference.GetMemberName() != GET_FUNCTION_NAME_CHECKED(UGameQuestGraphBase, DefaultEntry);
}

TSharedPtr<INameValidatorInterface> UBPNode_GameQuestEntryEvent::MakeNameValidator() const
{
	return MakeShared<FKismetNameValidator>(GetBlueprint(), CustomFunctionName);
}

void UBPNode_GameQuestEntryEvent::OnRenameNode(const FString& NewName)
{
	CustomFunctionName = *NewName;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
}

void UBPNode_GameQuestEntryEvent::PostPlacedNewNode()
{
	Super::PostPlacedNewNode();
	CustomFunctionName = TEXT("CustomEntry");
}

void UBPNode_GameQuestEntryEvent::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	const UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

bool UBPNode_GameQuestEntryEvent::IsCompatibleWithGraph(const UEdGraph* TargetGraph) const
{
	const UBlueprint* Blueprint = TargetGraph->GetTypedOuter<UBlueprint>();
	if (Blueprint == nullptr)
	{
		return false;
	}
	return Blueprint->UbergraphPages.Contains(TargetGraph) && TargetGraph->Schema == UEdGraphSchemaGameQuest::StaticClass();
}

void UBPNode_GameQuestEntryEvent::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	UK2Node_CallFunction* TryActivateQuestNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	TryActivateQuestNode->SetFromFunction(UGameQuestGraphBase::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UGameQuestGraphBase, TryStartGameQuest)));
	TryActivateQuestNode->AllocateDefaultPins();

	UK2Node_IfThenElse* BranchNode = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
	BranchNode->AllocateDefaultPins();
	BranchNode->GetConditionPin()->MakeLinkTo(TryActivateQuestNode->GetReturnValuePin());
	TryActivateQuestNode->GetThenPin()->MakeLinkTo(BranchNode->GetExecPin());

	UEdGraphPin* ThenPin = FindPinChecked(UEdGraphSchema_K2::PN_Then);

	UK2Node_ExecutionSequence* SequenceNode = CompilerContext.SpawnIntermediateNode<UK2Node_ExecutionSequence>(this, SourceGraph);
	SequenceNode->AllocateDefaultPins();
	SequenceNode->GetExecPin()->MakeLinkTo(BranchNode->GetThenPin());

	CompilerContext.MovePinLinksToIntermediate(*ThenPin, *SequenceNode->GetThenPinGivenIndex(0));
	ThenPin->MakeLinkTo(TryActivateQuestNode->GetExecPin());

	UK2Node_CallFunction* PostExecuteEntryEventCheckNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	PostExecuteEntryEventCheckNode->SetFromFunction(UGameQuestGraphBase::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UGameQuestGraphBase, PostExecuteEntryEvent)));
	PostExecuteEntryEventCheckNode->AllocateDefaultPins();
	SequenceNode->GetThenPinGivenIndex(1)->MakeLinkTo(PostExecuteEntryEventCheckNode->GetExecPin());
}

#undef LOCTEXT_NAMESPACE
