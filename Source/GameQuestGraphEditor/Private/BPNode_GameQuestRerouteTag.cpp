// Fill out your copyright notice in the Description page of Project Settings.


#include "BPNode_GameQuestRerouteTag.h"

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

FText UBPNode_GameQuestRerouteTag::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::MenuTitle)
	{
		return LOCTEXT("QuestRerouteTagMenuTitle", "Add Quest Reroute Tag");
	}
	if (TitleType == ENodeTitleType::FullTitle)
	{
		return FText::Format(LOCTEXT("QuestRerouteTagFullTitle", "Reroute By [{0}] Tag\nQuest Reroute Tag"), FText::FromName(RerouteTag));
	}
	if (TitleType == ENodeTitleType::EditableTitle)
	{
		return FText::FromName(RerouteTag);
	}
	return LOCTEXT("QuestRerouteTagListTitle", "Quest Reroute Tag");
}

FText UBPNode_GameQuestRerouteTag::GetMenuCategory() const
{
	return LOCTEXT("QuestRerouteTagCategory", "GameQuest|FlowControl");
}

FSlateIcon UBPNode_GameQuestRerouteTag::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("GameQuestGraphSlateStyle", "Graph.Reroute");
	return Icon;
}

FLinearColor UBPNode_GameQuestRerouteTag::GetNodeTitleColor() const
{
	if (const UGameQuestGraphBase* Quest = Cast<UGameQuestGraphBase>(GetBlueprint()->GetObjectBeingDebugged()))
	{
		const FGameQuestRerouteTag* Tag = Quest->GetRerouteTag(RerouteTag);
		if (Tag && Tag->PreSequenceId != GameQuest::IdNone)
		{
			using namespace GameQuestGraphEditorStyle;
			return Debug::Finished;
		}
	}
	return FLinearColor::Red;
}

FLinearColor UBPNode_GameQuestRerouteTag::GetNodeBodyTintColor() const
{
	if (const UGameQuestGraphBase* Quest = Cast<UGameQuestGraphBase>(GetBlueprint()->GetObjectBeingDebugged()))
	{
		const FGameQuestRerouteTag* Tag = Quest->GetRerouteTag(RerouteTag);
		if (Tag && Tag->PreSequenceId != GameQuest::IdNone)
		{
			using namespace GameQuestGraphEditorStyle;
			return Debug::Finished;
		}
	}
	return Super::GetNodeBodyTintColor();
}

void UBPNode_GameQuestRerouteTag::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	const UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

bool UBPNode_GameQuestRerouteTag::IsCompatibleWithGraph(const UEdGraph* TargetGraph) const
{
	const UBlueprint* Blueprint = TargetGraph->GetTypedOuter<UBlueprint>();
	if (Blueprint == nullptr)
	{
		return false;
	}
	return Blueprint->UbergraphPages.Contains(TargetGraph) && TargetGraph->Schema == UEdGraphSchemaGameQuest::StaticClass();
}

void UBPNode_GameQuestRerouteTag::OnRenameNode(const FString& NewName)
{
	RerouteTag = *NewName;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
}

void UBPNode_GameQuestRerouteTag::PostPlacedNewNode()
{
	Super::PostPlacedNewNode();

	RerouteTag = TEXT("Default");
}

TSharedPtr<INameValidatorInterface> UBPNode_GameQuestRerouteTag::MakeNameValidator() const
{
	return MakeShared<FKismetNameValidator>(GetBlueprint(), RerouteTag);
}

void UBPNode_GameQuestRerouteTag::AddSearchMetaDataInfo(TArray<FSearchTagDataPair>& OutTaggedMetaData) const
{
	Super::AddSearchMetaDataInfo(OutTaggedMetaData);

	if (RerouteTag != NAME_None)
	{
		OutTaggedMetaData.Add(FSearchTagDataPair(LOCTEXT("RerouteTag", "RerouteTag"), FText::FromName(RerouteTag)));
	}
}

void UBPNode_GameQuestRerouteTag::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
}

void UBPNode_GameQuestRerouteTag::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	if (RerouteTag == NAME_None)
	{
		CompilerContext.MessageLog.Error(TEXT("@@RerouteTag must have value"), this);
		return;
	}

	UK2Node_CallFunction* CallTryActivateNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this);
	CallTryActivateNode->SetFromFunction(UGameQuestGraphBase::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UGameQuestGraphBase, ProcessRerouteTag)));
	CallTryActivateNode->AllocateDefaultPins();

	CallTryActivateNode->FindPinChecked(TEXT("RerouteTagName"))->DefaultValue = RerouteTag.ToString();

	const FName RerouteTagName = FGameQuestRerouteTag::MakeVariableName(RerouteTag);
	UK2Node_VariableGet* GetStructNode = CompilerContext.SpawnIntermediateNode<UK2Node_VariableGet>(this);
	GetStructNode->VariableReference.SetSelfMember(RerouteTagName);
	GetStructNode->AllocateDefaultPins();
	CallTryActivateNode->FindPinChecked(TEXT("RerouteTag"))->MakeLinkTo(GetStructNode->FindPinChecked(RerouteTagName));

	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *CallTryActivateNode->GetExecPin());
}

#undef LOCTEXT_NAMESPACE
