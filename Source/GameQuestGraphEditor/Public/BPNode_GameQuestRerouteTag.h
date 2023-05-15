// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "BPNode_GameQuestRerouteTag.generated.h"

UCLASS()
class GAMEQUESTGRAPHEDITOR_API UBPNode_GameQuestRerouteTag : public UK2Node
{
	GENERATED_BODY()
public:
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FText GetMenuCategory() const override;
	FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	FLinearColor GetNodeTitleColor() const override;
	FLinearColor GetNodeBodyTintColor() const override;
	void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;
	bool GetCanRenameNode() const override { return true; }
	void OnRenameNode(const FString& NewName) override;
	void PostPlacedNewNode() override;
	TSharedPtr<INameValidatorInterface> MakeNameValidator() const override;
	bool ShouldShowNodeProperties() const override { return true; }
	void AddSearchMetaDataInfo(TArray<FSearchTagDataPair>& OutTaggedMetaData) const override;
	void AllocateDefaultPins() override;
	void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;

	UPROPERTY()
	FName RerouteTag;
};
