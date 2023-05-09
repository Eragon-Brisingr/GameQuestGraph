// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InstancedStruct.h"
#include "K2Node.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "KismetNodes/SGraphNodeK2Default.h"
#include "BPNode_GameQuestNodeBase.generated.h"

namespace GameQuestUtils
{
	namespace Pin
	{
		const FName BranchPinSubCategory = TEXT("GameQuestSequenceBranch");
		const FName BranchPinName = TEXT("GameQuestSequenceBranch");
		const FName ListPinCategory = TEXT("GameQuestSequenceList");
		const FName ListPinName = TEXT("GameQuestSequenceList");
	}
}

struct FGameQuestNodeBase;
class UBPNode_GameQuestNodeBase;
class FGameQuestGraphCompilerContext;
class UGameQuestGraphGeneratedClass;

struct FGameQuestStructCollector
{
	static const FGameQuestStructCollector& Get();
	TSubclassOf<UBPNode_GameQuestNodeBase> GetBPNodeClass(const UScriptStruct* Struct) const;

	TMap<UScriptStruct*, FInstancedStruct> ValidNodeStructMap;
	TMap<const UStruct*, TSubclassOf<UBPNode_GameQuestNodeBase>> CustomNodeMap;
private:
	FGameQuestStructCollector();
};

class UGameQuestElementScriptable;
class UGameQuestGraphBase;

struct FGameQuestClassCollector
{
	static FGameQuestClassCollector& Get();

	FGameQuestClassCollector();

	void ForEachDerivedClasses(const TSubclassOf<UGameQuestElementScriptable>& BaseClass,
			TFunctionRef<void(const TSubclassOf<UGameQuestElementScriptable>&, const TSoftClassPtr<UGameQuestGraphBase>&)> LoadedClassFunc,
			TFunctionRef<void(const TSoftClassPtr<UGameQuestElementScriptable>&, const TSoftClassPtr<UGameQuestGraphBase>&)> UnloadedClassFunc);
};

UCLASS(Abstract)
class GAMEQUESTGRAPHEDITOR_API UBPNode_GameQuestNodeBase : public UK2Node
{
	GENERATED_BODY()
public:
	UBPNode_GameQuestNodeBase();

	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FName GetCornerIcon() const override;
	void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	TSharedPtr<INameValidatorInterface> MakeNameValidator() const override;
	void OnRenameNode(const FString& NewName) override;
	void ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	void PostPlacedNewNode() override;
	void PostPasteNode() override;
	void DestroyNode() override;
	bool ShouldShowNodeProperties() const override { return true; }
	void PinConnectionListChanged(UEdGraphPin* Pin) override;
	FString GetFindReferenceSearchString() const override;
	void AddSearchMetaDataInfo(TArray<FSearchTagDataPair>& OutTaggedMetaData) const override;
	FText GetTooltipText() const override;
	bool CanJumpToDefinition() const override { return true; }
	void JumpToDefinition() const override;
	// TODO：增加复制的支持
	bool CanDuplicateNode() const override { return false; }

	UPROPERTY(EditAnywhere, Category=PinOptions, EditFixedSize)
	TArray<FOptionalPinFromProperty> ShowPinForProperties;

	UPROPERTY()
	FString GameQuestNodeName;
	UPROPERTY(VisibleAnywhere, Category = "Quest")
	uint16 NodeId = 0;
	UPROPERTY(VisibleAnywhere, Category = "Quest", meta = (BaseStruct = "/Script/GameQuestGraph.GameQuestNodeBase", StructTypeConst = true))
	FInstancedStruct StructNodeInstance;

	FORCEINLINE FName GetRefVarName() const { return *(GetRefVarPrefix() + GameQuestNodeName); }
	static bool IsUniqueRefVarName(const UBlueprint* Blueprint, const FName& Name);
	static UObject* FindNodeByRefVarName(const UBlueprint* Blueprint, const FName& Name);
	static FString MakeUniqueRefVarName(const UBlueprint* Blueprint, const FString& BaseName, const FString& VarPrefix);

	virtual FString GetRefVarPrefix() const { return {}; }
	virtual UScriptStruct* GetBaseNodeStruct() const { return nullptr; }
	virtual UScriptStruct* GetNodeStruct() const { return const_cast<UScriptStruct*>(StructNodeInstance.GetScriptStruct()); }
	virtual UStruct* GetNodeImplStruct() const { return GetNodeStruct(); }

	virtual bool HasEvaluateActionParams() const;
	virtual void ExpandNodeForEvaluateActionParams(UEdGraphPin*& AuthorityThenPin, UEdGraphPin*& ClientThenPin, FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) {}
	virtual void CreateClassVariablesFromNode(FGameQuestGraphCompilerContext& CompilerContext);
	virtual void CopyTermDefaultsToDefaultNode(FGameQuestGraphCompilerContext& CompilerContext, UObject* DefaultObject, UGameQuestGraphGeneratedClass* ObjectClass, FStructProperty* NodeProperty);

	void CreateAssignByNameFunction(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UEdGraphPin* OrgPin, UEdGraphPin* SetObjectPin, UEdGraphPin*& LastThen);
public:
	enum class EDebugState
	{
		None,
		Activated,
		Deactivated,
		Finished,
		Interrupted
	};
	EDebugState DebugState = EDebugState::None;

	FLinearColor GetNodeBodyTintColor() const override;
	FLinearColor GetNodeTitleColor() const override;

	FLinearColor GetDebugStateColor() const;
protected:
	static FTimerHandle SkeletalRecompileChildrenHandle;
	void RecompileSkeletalBlueprintDelayed();

	struct FQuestNodeOptionalPinManager : FOptionalPinManager
	{
		using Super = FOptionalPinManager;
		void GetRecordDefaults(FProperty* TestProperty, FOptionalPinFromProperty& Record) const override;
		bool CanTreatPropertyAsOptional(FProperty* TestProperty) const override;
	};

	void CreateExposePins();
	void CreateFinishEventPins();
	void ExpandNodeForFinishEvents(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph);

	struct GAMEQUESTGRAPHEDITOR_API FStructMemberSetGuard
	{
		FStructMemberSetGuard(UStruct* Struct);
		~FStructMemberSetGuard();

		UStruct* Struct;
		TArray<bool, TInlineAllocator<4>> HasHiddenMetaList;
	};
};

class GAMEQUESTGRAPHEDITOR_API SNode_GameQuestNodeBase : public SGraphNodeK2Default
{
	using Super = SGraphNodeK2Default;
public:
	void CreatePinWidgets() override;
	void CreateStandardPinWidget(UEdGraphPin* Pin) override;
	void GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const override final;
};

class UBPNode_GameQuestElementBase;
class FGameQuestElementDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FGameQuestElementDragDropOp, FDecoratedDragDropOp)

	TWeakObjectPtr<UBPNode_GameQuestElementBase> ElementNode;

	static TSharedRef<FGameQuestElementDragDropOp> New(TWeakObjectPtr<UBPNode_GameQuestElementBase> ElementNode);
	void SetAllowedTooltip();
	void SetErrorTooltip();
};

namespace TunnelPin
{
	UEdGraphPin* Redirect(UEdGraphPin* Pin);
};
