// Fill out your copyright notice in the Description page of Project Settings.


#include "BPNode_GameQuestFinishedTag.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraphSchemaGameQuest.h"
#include "FindInBlueprintManager.h"
#include "GameQuestGraphBase.h"
#include "GameQuestGraphEditorStyle.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "KismetCompiler.h"
#include "Kismet2/BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "GameQuestGraphEditor"

FText UBPNode_GameQuestFinishedTag::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::MenuTitle)
	{
		return LOCTEXT("QuestFinishedTagMenuTitle", "Add Quest Finished Tag");
	}
	if (TitleType == ENodeTitleType::FullTitle)
	{
		return FText::Format(LOCTEXT("QuestFinishedTagFullTitle", "Finish By [{0}] Tag\nQuest Finished Tag"), FText::FromName(FinishedTag));
	}
	if (TitleType == ENodeTitleType::EditableTitle)
	{
		return FText::FromName(FinishedTag);
	}
	return LOCTEXT("QuestFinishedTagListTitle", "Quest Finished Tag");
}

FText UBPNode_GameQuestFinishedTag::GetMenuCategory() const
{
	return LOCTEXT("QuestFinishedTagCategory", "GameQuest|FlowControl");
}

FSlateIcon UBPNode_GameQuestFinishedTag::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("GameQuestGraphSlateStyle", "Graph.Event.End");
	return Icon;
}

FLinearColor UBPNode_GameQuestFinishedTag::GetNodeTitleColor() const
{
	if (const UGameQuestGraphBase* Quest = Cast<UGameQuestGraphBase>(GetBlueprint()->GetObjectBeingDebugged()))
	{
		const FGameQuestFinishedTag* Tag = Quest->GetFinishedTag(FinishedTag);
		if (Tag && Tag->PreSequenceId != GameQuest::IdNone)
		{
			using namespace GameQuestGraphEditorStyle;
			return Debug::Finished;
		}
	}
	return FLinearColor::Red;
}

FLinearColor UBPNode_GameQuestFinishedTag::GetNodeBodyTintColor() const
{
	if (const UGameQuestGraphBase* Quest = Cast<UGameQuestGraphBase>(GetBlueprint()->GetObjectBeingDebugged()))
	{
		const FGameQuestFinishedTag* Tag = Quest->GetFinishedTag(FinishedTag);
		if (Tag && Tag->PreSequenceId != GameQuest::IdNone)
		{
			using namespace GameQuestGraphEditorStyle;
			return Debug::Finished;
		}
	}
	return Super::GetNodeBodyTintColor();
}

void UBPNode_GameQuestFinishedTag::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	const UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

bool UBPNode_GameQuestFinishedTag::IsCompatibleWithGraph(const UEdGraph* TargetGraph) const
{
	const UBlueprint* Blueprint = TargetGraph->GetTypedOuter<UBlueprint>();
	if (Blueprint == nullptr)
	{
		return false;
	}
	return Blueprint->UbergraphPages.Contains(TargetGraph) && TargetGraph->Schema == UEdGraphSchemaGameQuest::StaticClass();
}

void UBPNode_GameQuestFinishedTag::OnRenameNode(const FString& NewName)
{
	FinishedTag = *NewName;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
}

void UBPNode_GameQuestFinishedTag::PostPlacedNewNode()
{
	Super::PostPlacedNewNode();

	FinishedTag = TEXT("Default");
}

TSharedPtr<INameValidatorInterface> UBPNode_GameQuestFinishedTag::MakeNameValidator() const
{
	return MakeShared<FKismetNameValidator>(GetBlueprint(), FinishedTag);
}

void UBPNode_GameQuestFinishedTag::AddSearchMetaDataInfo(TArray<FSearchTagDataPair>& OutTaggedMetaData) const
{
	Super::AddSearchMetaDataInfo(OutTaggedMetaData);

	if (FinishedTag != NAME_None)
	{
		OutTaggedMetaData.Add(FSearchTagDataPair(LOCTEXT("FinishedTag", "FinishedTag"), FText::FromName(FinishedTag)));
	}
}

void UBPNode_GameQuestFinishedTag::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
}

void UBPNode_GameQuestFinishedTag::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	if (FinishedTag == NAME_None)
	{
		CompilerContext.MessageLog.Error(TEXT("@@FinishedTag must have value"), this);
		return;
	}

	UK2Node_CallFunction* CallTryActivateNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this);
	CallTryActivateNode->SetFromFunction(UGameQuestGraphBase::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UGameQuestGraphBase, ProcessFinishedTag)));
	CallTryActivateNode->AllocateDefaultPins();

	CallTryActivateNode->FindPinChecked(TEXT("FinishedTagName"))->DefaultValue = FinishedTag.ToString();

	const FName FinishedTagName = FGameQuestFinishedTag::MakeVariableName(FinishedTag);
	UK2Node_VariableGet* GetStructNode = CompilerContext.SpawnIntermediateNode<UK2Node_VariableGet>(this);
	GetStructNode->VariableReference.SetSelfMember(FinishedTagName);
	GetStructNode->AllocateDefaultPins();
	CallTryActivateNode->FindPinChecked(TEXT("FinishedTag"))->MakeLinkTo(GetStructNode->FindPinChecked(FinishedTagName));

	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *CallTryActivateNode->GetExecPin());
}

#undef LOCTEXT_NAMESPACE
