// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BPNode_GameQuestNodeBase.h"
#include "GameQuestType.h"
#include "BPNode_GameQuestElementBase.generated.h"

struct FGameQuestElementBase;

UCLASS(Abstract)
class GAMEQUESTGRAPHEDITOR_API UBPNode_GameQuestElementBase : public UBPNode_GameQuestNodeBase
{
	GENERATED_BODY()
public:
	UBPNode_GameQuestElementBase()
		: bIsOptional(false)
		, bAutoDeactivateOtherBranch(true)
		, bListMode(false)
	{}

	void AllocateDefaultPins() override;
	TSharedPtr<SGraphNode> CreateVisualWidget() override;
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FText GetMenuCategory() const override;
	FName GetCornerIcon() const override;
	FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	void PinConnectionListChanged(UEdGraphPin* Pin) override;
	bool IsNodePure() const override { return !!bListMode; }
	void DestroyNode() override;
	bool CanPlaceBreakpoints() const override { return false; }
	void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	bool CanPasteHere(const UEdGraph* TargetGraph) const override;
	void PostPasteNode() override;
	bool CanEditChange(const FProperty* InProperty) const override;
	void GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;

	FGameQuestElementBase* GetElementNode() const;
	void CopyTermDefaultsToDefaultNode(FGameQuestGraphCompilerContext& CompilerContext, UObject* DefaultObject, UGameQuestGraphGeneratedClass* ObjectClass, FStructProperty* NodeProperty) override;
	FString GetRefVarPrefix() const override;

	virtual TSubclassOf<UGameQuestGraphBase> GetSupportQuest() const;

	FVector2D NodeSize;

	UPROPERTY(EditAnywhere, Category = "Quest")
	uint8 bIsOptional : 1;
	UPROPERTY(EditAnywhere, Category = "Quest")
	uint8 bAutoDeactivateOtherBranch : 1;
	UPROPERTY()
	uint8 bListMode : 1;
	UPROPERTY()
	TObjectPtr<UBPNode_GameQuestNodeBase> OwnerNode = nullptr;
	bool IsListMode() const { return bListMode; }
};

UCLASS()
class GAMEQUESTGRAPHEDITOR_API UBPNode_GameQuestElementBranchList : public UBPNode_GameQuestElementBase
{
	GENERATED_BODY()
public:
	void AllocateDefaultPins() override;
	TSharedPtr<SGraphNode> CreateVisualWidget() override;
	void DestroyNode() override;
	void GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;
	void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	void PostPasteNode() override;

	void CreateClassVariablesFromNode(FGameQuestGraphCompilerContext& CompilerContext) override;
	void CopyTermDefaultsToDefaultNode(FGameQuestGraphCompilerContext& CompilerContext, UObject* DefaultObject, UGameQuestGraphGeneratedClass* ObjectClass, FStructProperty* NodeProperty) override;

	UPROPERTY()
	TArray<TObjectPtr<UBPNode_GameQuestElementBase>> Elements;
	UPROPERTY()
	TArray<EGameQuestSequenceLogic> ElementLogics;

	UScriptStruct* GetBaseNodeStruct() const override;
	UScriptStruct* GetNodeStruct() const override;

	void AddElement(UBPNode_GameQuestElementBase* Element);
	void InsertElementNode(UBPNode_GameQuestElementBase* Element, int32 Idx);
	void RemoveElement(UBPNode_GameQuestElementBase* Element);
	void MoveElementNode(UBPNode_GameQuestElementBase* Element, int32 Idx);
};

UCLASS()
class GAMEQUESTGRAPHEDITOR_API UBPNode_GameQuestElementStruct : public UBPNode_GameQuestElementBase
{
	GENERATED_BODY()
public:
	UScriptStruct* GetBaseNodeStruct() const override;

	bool IsActionFilteredOut(FBlueprintActionFilter const& Filter) override;
};

UCLASS()
class GAMEQUESTGRAPHEDITOR_API UBPNode_GameQuestElementScript : public UBPNode_GameQuestElementBase
{
	GENERATED_BODY()
public:
	void AllocateDefaultPins() override;
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FText GetMenuCategory() const override;
	UScriptStruct* GetBaseNodeStruct() const override;
	UScriptStruct* GetNodeStruct() const override;
	UStruct* GetNodeImplStruct() const override;
	void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	UObject* GetJumpTargetForDoubleClick() const override;
	bool IsActionFilteredOut(FBlueprintActionFilter const& Filter) override;
	bool HasExternalDependencies(TArray<UStruct*>* OptionalOutput) const override;

	bool HasEvaluateActionParams() const override;
	void ExpandNodeForEvaluateActionParams(UEdGraphPin*& AuthorityThenPin, UEdGraphPin*& ClientThenPin, FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	void CopyTermDefaultsToDefaultNode(FGameQuestGraphCompilerContext& CompilerContext, UObject* DefaultObject, UGameQuestGraphGeneratedClass* ObjectClass, FStructProperty* NodeProperty) override;
	TSubclassOf<UGameQuestGraphBase> GetSupportQuest() const override;

	struct FUnloadNodeData
	{
		TSoftClassPtr<UGameQuestElementScriptable> Class;
		TSoftClassPtr<UGameQuestGraphBase> SupportType;
	};
	TOptional<FUnloadNodeData> UnloadNodeData;

	void InitialByClass(const TSubclassOf<UGameQuestElementScriptable>& Class);

	UPROPERTY(VisibleAnywhere, Instanced)
	TObjectPtr<UGameQuestElementScriptable> ScriptInstance;
	UPROPERTY(EditAnywhere, Category=PinOptions, EditFixedSize)
	TArray<FOptionalPinFromProperty> ShowPinForScript;
};
