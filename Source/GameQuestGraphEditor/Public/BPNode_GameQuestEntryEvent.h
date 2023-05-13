// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "K2Node_Event.h"
#include "BPNode_GameQuestEntryEvent.generated.h"

/**
 *
 */
UCLASS()
class GAMEQUESTGRAPHEDITOR_API UBPNode_GameQuestEntryEvent : public UK2Node_Event
{
	GENERATED_BODY()
public:
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FText GetMenuCategory() const override;
	FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	bool CanUserDeleteNode() const override;
	bool CanDuplicateNode() const override;
	bool GetCanRenameNode() const override;
	TSharedPtr<INameValidatorInterface> MakeNameValidator() const override;
	void OnRenameNode(const FString& NewName) override;
	void PostPlacedNewNode() override;
	void AllocateDefaultPins() override;
	void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;

	bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;
	void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
};
