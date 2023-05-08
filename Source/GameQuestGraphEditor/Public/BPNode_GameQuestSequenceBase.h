// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BPNode_GameQuestNodeBase.h"
#include "GameQuestType.h"
#include "BPNode_GameQuestSequenceBase.generated.h"

UCLASS(Abstract)
class GAMEQUESTGRAPHEDITOR_API UBPNode_GameQuestSequenceBase : public UBPNode_GameQuestNodeBase
{
	GENERATED_BODY()
public:
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FText GetMenuCategory() const override;
	void GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;
	void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
};

UCLASS()
class GAMEQUESTGRAPHEDITOR_API UBPNode_GameQuestSequenceSingle final : public UBPNode_GameQuestSequenceBase
{
	GENERATED_BODY()
public:
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FText GetMenuCategory() const override;
	FName GetCornerIcon() const override { return NAME_None; }
	void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	void AllocateDefaultPins() override;
	TSharedPtr<SGraphNode> CreateVisualWidget() override;
	FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	void PostPlacedNewNode() override;
	void DestroyNode() override;
	bool IsActionFilteredOut(FBlueprintActionFilter const& Filter) override;

	void CreateClassVariablesFromNode(FGameQuestGraphCompilerContext& CompilerContext);
	void CopyTermDefaultsToDefaultNode(FGameQuestGraphCompilerContext& CompilerContext, UObject* DefaultObject, UGameQuestGraphGeneratedClass* ObjectClass, FStructProperty* NodeProperty);

	UPROPERTY()
	TObjectPtr<UBPNode_GameQuestElementBase> Element;

	UScriptStruct* GetBaseNodeStruct() const override;
	UScriptStruct* GetNodeStruct() const override;
	UScriptStruct* ElementNodeStruct;

	struct FScriptNodeData
	{
		TSoftClassPtr<UGameQuestElementScriptable> Class;
		TSoftClassPtr<UGameQuestGraphBase> SupportType;
	};
	TOptional<FScriptNodeData> ScriptNodeData;

	FVector2D NodeSize;
};

class UBPNode_GameQuestElementBase;

UCLASS()
class GAMEQUESTGRAPHEDITOR_API UBPNode_GameQuestSequenceListBase : public UBPNode_GameQuestSequenceBase
{
	GENERATED_BODY()
public:
	TSharedPtr<SGraphNode> CreateVisualWidget() override;
	void GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;
	void DestroyNode() override;
	void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;

	void CopyTermDefaultsToDefaultNode(FGameQuestGraphCompilerContext& CompilerContext, UObject* DefaultObject, UGameQuestGraphGeneratedClass* ObjectClass, FStructProperty* NodeProperty);

	UPROPERTY()
	TArray<TObjectPtr<UBPNode_GameQuestElementBase>> Elements;
	UPROPERTY()
	TArray<EGameQuestSequenceLogic> ElementLogics;

	void AddElement(UBPNode_GameQuestElementBase* Element);
	void InsertElementNode(UBPNode_GameQuestElementBase* Element, int32 Idx);
	void RemoveElement(UBPNode_GameQuestElementBase* Element);
	void MoveElementNode(UBPNode_GameQuestElementBase* Element, int32 Idx);
};

UCLASS()
class GAMEQUESTGRAPHEDITOR_API UBPNode_GameQuestSequenceList final : public UBPNode_GameQuestSequenceListBase
{
	GENERATED_BODY()
public:
	void AllocateDefaultPins() override;
	FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;

	void CreateClassVariablesFromNode(FGameQuestGraphCompilerContext& CompilerContext);
	void CopyTermDefaultsToDefaultNode(FGameQuestGraphCompilerContext& CompilerContext, UObject* DefaultObject, UGameQuestGraphGeneratedClass* ObjectClass, FStructProperty* NodeProperty);

	UScriptStruct* GetBaseNodeStruct() const override;
	UScriptStruct* GetNodeStruct() const override;
};

UCLASS()
class GAMEQUESTGRAPHEDITOR_API UBPNode_GameQuestSequenceBranch final : public UBPNode_GameQuestSequenceListBase
{
	GENERATED_BODY()
public:
	void AllocateDefaultPins() override;
	FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;

	void CreateClassVariablesFromNode(FGameQuestGraphCompilerContext& CompilerContext);
	void CopyTermDefaultsToDefaultNode(FGameQuestGraphCompilerContext& CompilerContext, UObject* DefaultObject, UGameQuestGraphGeneratedClass* ObjectClass, FStructProperty* NodeProperty);

	UScriptStruct* GetBaseNodeStruct() const override;
	UScriptStruct* GetNodeStruct() const override;
};

class UGameQuestGraphBase;

UCLASS()
class GAMEQUESTGRAPHEDITOR_API UBPNode_GameQuestSequenceSubQuest final : public UBPNode_GameQuestSequenceBase
{
	GENERATED_BODY()
public:
	void AllocateDefaultPins() override;
	FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	UObject* GetJumpTargetForDoubleClick() const override;
	void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;

	bool HasSubQuestExposePin() const;
	bool HasEvaluateActionParams() const override;
	void ExpandNodeForEvaluateActionParams(UEdGraphPin*& AuthorityThenPin, UEdGraphPin*& ClientThenPin, FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	UScriptStruct* GetBaseNodeStruct() const override;
	UScriptStruct* GetNodeStruct() const override;

	UPROPERTY(EditAnywhere, Category = "Quest", meta = (BlueprintBaseOnly))
	TSubclassOf<UGameQuestGraphBase> SubQuestClass;
	UPROPERTY(EditAnywhere, Category = "Quest", EditFixedSize, meta = (ReadOnlyKeys, DisplayName = "Expose Pins"))
	TMap<FName, bool> SubQuestShowPins;
};
