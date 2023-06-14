// Fill out your copyright notice in the Description page of Project Settings.


#include "BPNode_GameQuestNodeBase.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "BPNode_GameQuestElementBase.h"
#include "BPNode_GameQuestSequenceBase.h"
#include "EdGraphSchemaGameQuest.h"
#include "Editor.h"
#include "FindInBlueprintManager.h"
#include "GameQuestElementBase.h"
#include "GameQuestGraphBase.h"
#include "GameQuestGraphBlueprint.h"
#include "GameQuestGraphCompilerContext.h"
#include "GameQuestNodeBase.h"
#include "GameQuestGraphEditorStyle.h"
#include "GameQuestType.h"
#include "GraphEditorSettings.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_Composite.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_EnumLiteral.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_StructMemberSet.h"
#include "K2Node_VariableGet.h"
#include "KismetCompiler.h"
#include "SGraphPin.h"
#include "SourceCodeNavigation.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

#define LOCTEXT_NAMESPACE "GameQuestGraphEditor"

const FGameQuestStructCollector& FGameQuestStructCollector::Get()
{
	static FGameQuestStructCollector Collector;
	return Collector;
}

TSubclassOf<UBPNode_GameQuestNodeBase> FGameQuestStructCollector::GetBPNodeClass(const UScriptStruct* Struct) const
{
	for (const UStruct* TestStruct = Struct; TestStruct; TestStruct = TestStruct->GetSuperStruct())
	{
		if (const TSubclassOf<UBPNode_GameQuestNodeBase> ElementNodeClass = CustomNodeMap.FindRef(TestStruct))
		{
			return ElementNodeClass;
		}
	}
	return nullptr;
}

FGameQuestStructCollector::FGameQuestStructCollector()
{
	TArray<UClass*> NodeClasses;
	GetDerivedClasses(UBPNode_GameQuestNodeBase::StaticClass(), NodeClasses);
	TSet<const UScriptStruct*> HasCustomNodeStructs;
	for (const UClass* NodeClass : NodeClasses)
	{
		if (NodeClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
		{
			continue;
		}
		if (const UScriptStruct* NodeStruct = NodeClass->GetDefaultObject<UBPNode_GameQuestNodeBase>()->GetNodeStruct())
		{
			HasCustomNodeStructs.Add(NodeStruct);
		}
	}
	for (TObjectIterator<UScriptStruct> It{ RF_ClassDefaultObject, false }; It; ++It)
	{
		UScriptStruct* Struct = *It;
		if (Struct->IsChildOf(FGameQuestNodeBase::StaticStruct()) == false)
		{
			continue;
		}
		if (Struct->RootPackageHasAnyFlags(PKG_EditorOnly))
		{
			continue;
		}
		if (HasCustomNodeStructs.Contains(Struct))
		{
			continue;
		}
		static FName MD_Hidden = TEXT("Hidden");
		if (Struct->HasMetaData(MD_Hidden))
		{
			continue;
		}
		ValidNodeStructMap.Add(Struct, FInstancedStruct{ Struct });
	}
	for (UClass* NodeNodeClass : NodeClasses)
	{
		if (NodeNodeClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
		{
			continue;
		}
		if (const UScriptStruct* SupportNodeStruct = NodeNodeClass->GetDefaultObject<UBPNode_GameQuestNodeBase>()->GetBaseNodeStruct())
		{
			CustomNodeMap.Add(SupportNodeStruct, NodeNodeClass);
		}
	}

	FCoreDelegates::OnPreExit.AddLambda([this]
	{
		ValidNodeStructMap.Empty();
		CustomNodeMap.Empty();
	});
}

FGameQuestClassCollector& FGameQuestClassCollector::Get()
{
	static FGameQuestClassCollector ClassCollector;
	return ClassCollector;
}

FGameQuestClassCollector::FGameQuestClassCollector()
{
	static auto RefreshClassAction = []
	{
		static FTSTicker::FDelegateHandle RefreshClassActionHandle;
		if (RefreshClassActionHandle.IsValid())
		{
			return;
		}
		RefreshClassActionHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			const FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			if (!AssetRegistryModule.Get().IsLoadingAssets())
			{
				if (FBlueprintActionDatabase* BAD = FBlueprintActionDatabase::TryGet())
				{
					RefreshClassActionHandle.Reset();
					BAD->RefreshClassActions(UBPNode_GameQuestElementScript::StaticClass());
					BAD->RefreshClassActions(UBPNode_GameQuestSequenceSingle::StaticClass());
					return false;
				}
			}
			return true;
		}));
	};

	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	AssetRegistryModule.Get().OnAssetAdded().AddLambda([](const FAssetData& AssetData)
	{
		if (AssetData.GetClass() == UBlueprint::StaticClass())
		{
			const FString NativeParentClassPath = AssetData.GetTagValueRef<FString>(FBlueprintTags::NativeParentClassPath);
			const UClass* NativeClass = FSoftClassPath{ NativeParentClassPath }.ResolveClass();
			if (NativeClass && NativeClass->IsChildOf(UGameQuestElementScriptable::StaticClass()))
			{
				RefreshClassAction();
			}
		}
	});
	AssetRegistryModule.Get().OnAssetRemoved().AddLambda([](const FAssetData& AssetData)
	{
		if (AssetData.GetClass() == UBlueprint::StaticClass())
		{
			const FString NativeParentClassPath = AssetData.GetTagValueRef<FString>(FBlueprintTags::NativeParentClassPath);
			const UClass* NativeClass = FSoftClassPath{ NativeParentClassPath }.ResolveClass();
			if (NativeClass && NativeClass->IsChildOf(UGameQuestElementScriptable::StaticClass()))
			{
				RefreshClassAction();
			}
		}
	});

	// Register to have Populate called when doing a Hot Reload.
	FCoreUObjectDelegates::ReloadCompleteDelegate.AddLambda([](EReloadCompleteReason Reason)
	{
		RefreshClassAction();
	});

	// Register to have Populate called when a Blueprint is compiled.
	GEditor->OnBlueprintCompiled().AddLambda([]
	{
		RefreshClassAction();
	});
	GEditor->OnClassPackageLoadedOrUnloaded().AddLambda([]
	{
		RefreshClassAction();
	});
}

void FGameQuestClassCollector::ForEachDerivedClasses(const TSubclassOf<UGameQuestElementScriptable>& BaseClass,
			TFunctionRef<void(const TSubclassOf<UGameQuestElementScriptable>&, const TSoftClassPtr<UGameQuestGraphBase>&)> LoadedClassFunc,
			TFunctionRef<void(const TSoftClassPtr<UGameQuestElementScriptable>&, const TSoftClassPtr<UGameQuestGraphBase>&)> UnloadedClassFunc)
{
	const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	constexpr EClassFlags IgnoreClassFlags = CLASS_Abstract | CLASS_Deprecated;

	TArray<UClass*> DerivedClasses;
	GetDerivedClasses(BaseClass, DerivedClasses);
	DerivedClasses.Add(BaseClass);

	for (UClass* DerivedClass : DerivedClasses)
	{
		if (DerivedClass->HasAnyClassFlags(IgnoreClassFlags))
		{
			continue;
		}
		if (DerivedClass->RootPackageHasAnyFlags(PKG_EditorOnly))
		{
			continue;
		}
		if (DerivedClass->GetName().StartsWith(TEXT("SKEL_")) || DerivedClass->GetName().StartsWith(TEXT("REINST_")))
		{
			continue;
		}
		const TSoftClassPtr<UGameQuestGraphBase>& SupportInstanceSoftPath = DerivedClass->GetDefaultObject<UGameQuestElementScriptable>()->SupportType;
		LoadedClassFunc(DerivedClass, SupportInstanceSoftPath);
	}

	TArray<FAssetData> BlueprintList;
	AssetRegistry.GetAssetsByClass(FTopLevelAssetPath{ UBlueprint::StaticClass() }, BlueprintList);

	const FTopLevelAssetPath BaseClassPath{ BaseClass };
	for (const FAssetData& AssetData : BlueprintList)
	{
		FString ClassPath;
		FString NativeParentClassPath;
		uint32 ClassFlags;
		if (AssetData.GetTagValue(FBlueprintTags::GeneratedClassPath, ClassPath) && AssetData.GetTagValue(FBlueprintTags::NativeParentClassPath, NativeParentClassPath) && AssetData.GetTagValue(FBlueprintTags::ClassFlags, ClassFlags))
		{
			if ((ClassFlags & IgnoreClassFlags) != 0)
			{
				continue;
			}

			const UClass* NativeClass = FSoftClassPath{ NativeParentClassPath }.ResolveClass();
			if (NativeClass == nullptr || NativeClass->IsChildOf(UGameQuestElementScriptable::StaticClass()) == false)
			{
				continue;
			}

			const TSoftClassPtr<UGameQuestElementScriptable> ClassSoftPath{ FSoftObjectPath{ ClassPath } };
			if (DerivedClasses.ContainsByPredicate([&ClassSoftPath](const UClass* E)
			{
				return E == ClassSoftPath;
			}))
			{
				continue;
			}

			if (BaseClass != UGameQuestElementScriptable::StaticClass())
			{
				TArray<FTopLevelAssetPath> AncestorClassNames;
				AssetRegistry.GetAncestorClassNames(ClassSoftPath.GetUniqueID().GetAssetPath(), AncestorClassNames);
				if (AncestorClassNames.Contains(BaseClassPath) == false)
				{
					continue;
				}
			}

			TSoftClassPtr<UGameQuestGraphBase> SupportType;
			FString SupportTypePath;
			static const FName SupportTypeTagName = TEXT("SupportType");
			if (AssetData.GetTagValue(SupportTypeTagName, SupportTypePath))
			{
				SupportType = FSoftClassPath{ SupportTypePath };
			}
			else
			{
				SupportType = UGameQuestGraphBase::StaticClass();
			}

			UnloadedClassFunc(ClassSoftPath, SupportType);
		}
	}
}

UBPNode_GameQuestNodeBase::UBPNode_GameQuestNodeBase()
{
	AdvancedPinDisplay = ENodeAdvancedPins::Hidden;

	bCanRenameNode = true;
}

FText UBPNode_GameQuestNodeBase::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::EditableTitle || TitleType == ENodeTitleType::ListView)
	{
		return FText::FromString(GameQuestNodeName);
	}
	const UStruct* NodeType = GetNodeImplStruct();
	if (NodeType && TitleType == ENodeTitleType::MenuTitle)
	{
		return NodeType->GetDisplayNameText();
	}
	return FText::Format(LOCTEXT("GameQuestNodeFullTitle", "{0}\n{1}"), FText::FromString(GameQuestNodeName), NodeType ? NodeType->GetDisplayNameText() : LOCTEXT("MissingType", "Missing Type"));
}

FName UBPNode_GameQuestNodeBase::GetCornerIcon() const
{
	static FName CornerIcon = TEXT("Graph.Latent.LatentIcon");
	return CornerIcon;
}

void UBPNode_GameQuestNodeBase::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	const UClass* ActionKey = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(ActionKey) == false)
	{
		return;
	}

	const UScriptStruct* BaseNodeStruct = GetBaseNodeStruct();
	if (BaseNodeStruct == nullptr)
	{
		return;
	}

	const UScriptStruct* ToCreateNodeStruct = GetClass()->GetDefaultObject<UBPNode_GameQuestNodeBase>()->GetNodeStruct();
	if (ToCreateNodeStruct == nullptr)
	{
		const FGameQuestStructCollector& Collector = FGameQuestStructCollector::Get();
		for (const auto& [NodeStruct, _] : Collector.ValidNodeStructMap)
		{
			const TSubclassOf<UBPNode_GameQuestNodeBase> NodeClass = Collector.GetBPNodeClass(NodeStruct);
			if (NodeClass != GetClass())
			{
				continue;
			}

			UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
			check(NodeSpawner != nullptr);

			NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateLambda([NodeStruct](UEdGraphNode* NewNode, bool bIsTemplateNode) mutable
			{
				UBPNode_GameQuestNodeBase* QuestNodeNode = CastChecked<UBPNode_GameQuestNodeBase>(NewNode);
				QuestNodeNode->StructNodeInstance.InitializeAs(NodeStruct);
			});

			ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
		}
	}
	else
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateLambda([ToCreateNodeStruct](UEdGraphNode* NewNode, bool bIsTemplateNode) mutable
		{
			UBPNode_GameQuestNodeBase* QuestNodeNode = CastChecked<UBPNode_GameQuestNodeBase>(NewNode);
			QuestNodeNode->StructNodeInstance.InitializeAs(ToCreateNodeStruct);
		});

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

bool UBPNode_GameQuestNodeBase::IsCompatibleWithGraph(const UEdGraph* TargetGraph) const
{
	return TargetGraph->Schema == UEdGraphSchemaGameQuest::StaticClass();
}

void UBPNode_GameQuestNodeBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if ((PropertyName == GET_MEMBER_NAME_CHECKED(FOptionalPinFromProperty, bShowPin)))
	{
		GetSchema()->ReconstructNode(*this);
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

TSharedPtr<INameValidatorInterface> UBPNode_GameQuestNodeBase::MakeNameValidator() const
{
	class FNameValidatorInterface : public FKismetNameValidator
	{
		using Super = FKismetNameValidator;
	public:
		FNameValidatorInterface(const UBlueprint* Blueprint, const UBPNode_GameQuestNodeBase* TestNode)
			: Super(Blueprint)
			, TestNode(TestNode)
		{
		}

		const UBPNode_GameQuestNodeBase* TestNode;
		EValidatorResult IsValid(const FName& Name, bool bOriginal) override
		{
			const EValidatorResult Result = Super::IsValid(Name, bOriginal);
			if (Result != EValidatorResult::Ok)
			{
				return Result;
			}
			FText Reason;
			if (Name.IsValidObjectName(Reason) == false)
			{
				return EValidatorResult::ContainsInvalidCharacters;
			}
			const FName TestRefVarName = *(TestNode->GetRefVarPrefix() + Name.ToString());
			return IsUniqueRefVarName(TestNode->GetBlueprint(), TestRefVarName) ? EValidatorResult::Ok : EValidatorResult::ExistingName;
		}
		EValidatorResult IsValid(const FString& Name, bool bOriginal) override
		{
			return IsValid(FName(Name), bOriginal);
		}
	};
	return MakeShared<FNameValidatorInterface>(GetBlueprint(), this);
}

void UBPNode_GameQuestNodeBase::OnRenameNode(const FString& NewName)
{
	if (GameQuestNodeName == NewName)
	{
		return;
	}
	if (UBlueprint* Blueprint = GetBlueprint())
	{
		if (ensure(IsUniqueRefVarName(Blueprint, *NewName)))
		{
			const FName OldName = *GameQuestNodeName;
			GameQuestNodeName = *NewName;

			if (OldName != NAME_None)
			{
				FBlueprintEditorUtils::ReplaceVariableReferences(Blueprint, OldName, *NewName);
				RecompileSkeletalBlueprintDelayed();
			}
		}
	}
}

void UBPNode_GameQuestNodeBase::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const FGameQuestNodeBase* RuntimeNode = StructNodeInstance.GetMutablePtr<FGameQuestNodeBase>();
	if (RuntimeNode == nullptr)
	{
		CompilerContext.MessageLog.Error(TEXT("@@Runtime Instance Is Empty"), this);
		BreakAllNodeLinks();
		return;
	}

	if (HasEvaluateActionParams())
	{
		const FName RefVarName = GetRefVarName();
		UK2Node_CustomEvent* EvaluateActionParamsNode = CompilerContext.SpawnIntermediateEventNode<UK2Node_CustomEvent>(this, nullptr, SourceGraph);
		const TSharedRef<FUserPinInfo> IsAuthorityUserDefinedPin = MakeShared<FUserPinInfo>();
		IsAuthorityUserDefinedPin->PinName = TEXT("bHasAuthority");
		IsAuthorityUserDefinedPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		IsAuthorityUserDefinedPin->DesiredPinDirection = EGPD_Output;
		EvaluateActionParamsNode->UserDefinedPins.Add(IsAuthorityUserDefinedPin);
		EvaluateActionParamsNode->bInternalEvent = true;
		EvaluateActionParamsNode->CustomFunctionName = FGameQuestNodeBase::MakeEvaluateParamsFunctionName(RefVarName);
		EvaluateActionParamsNode->AllocateDefaultPins();

		UK2Node_IfThenElse* IfThenElseNode = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
		IfThenElseNode->AllocateDefaultPins();
		EvaluateActionParamsNode->FindPinChecked(IsAuthorityUserDefinedPin->PinName)->MakeLinkTo(IfThenElseNode->GetConditionPin());
		IfThenElseNode->GetExecPin()->MakeLinkTo(EvaluateActionParamsNode->FindPinChecked(UEdGraphSchema_K2::PN_Then));

		UEdGraphPin* AuthorityThenPin = IfThenElseNode->GetThenPin();
		UEdGraphPin* ClientThenPin = IfThenElseNode->GetElsePin();

		UScriptStruct* NodeStruct = GetNodeStruct();
		if (ShowPinForProperties.ContainsByPredicate([](const FOptionalPinFromProperty& E){ return E.bShowPin; }))
		{
			const FStructMemberSetGuard StructMemberSetGuard{ NodeStruct };

			UK2Node_StructMemberSet* AuthorityMemberSetNode = CompilerContext.SpawnIntermediateNode<UK2Node_StructMemberSet>(this);
			{
				AuthorityMemberSetNode->VariableReference.SetSelfMember(RefVarName);
				AuthorityMemberSetNode->StructType = NodeStruct;
				AuthorityMemberSetNode->ShowPinForProperties = ShowPinForProperties;
				AuthorityMemberSetNode->AllocateDefaultPins();
				AuthorityThenPin->MakeLinkTo(AuthorityMemberSetNode->GetExecPin());
				AuthorityThenPin = AuthorityMemberSetNode->FindPinChecked(UEdGraphSchema_K2::PN_Then);
			}

			UK2Node_StructMemberSet* ClientMemberSetNode = CompilerContext.SpawnIntermediateNode<UK2Node_StructMemberSet>(this);
			TArray<FName> ClientEvalPropertyNames;
			{
				ClientMemberSetNode->VariableReference.SetSelfMember(RefVarName);
				ClientMemberSetNode->StructType = NodeStruct;
				for (const FOptionalPinFromProperty& ShowPin : ShowPinForProperties)
				{
					if (ShowPin.bShowPin == false)
					{
						continue;
					}
					const FProperty* Property = NodeStruct->FindPropertyByName(ShowPin.PropertyName);
					if (Property == nullptr || Property->HasAnyPropertyFlags(CPF_RepSkip) == false)
					{
						continue;
					}
					ClientEvalPropertyNames.Add(ShowPin.PropertyName);
					ClientMemberSetNode->ShowPinForProperties.Add(ShowPin);
				}
				ClientMemberSetNode->AllocateDefaultPins();
				AuthorityThenPin->MakeLinkTo(ClientMemberSetNode->GetExecPin());
				AuthorityThenPin = ClientMemberSetNode->FindPinChecked(UEdGraphSchema_K2::PN_Then);
			}
			for (const FOptionalPinFromProperty& OptionalPin : ShowPinForProperties)
			{
				if (OptionalPin.bShowPin == false)
				{
					continue;
				}
				UEdGraphPin* OriginPin = FindPinChecked(OptionalPin.PropertyName);
				CompilerContext.CopyPinLinksToIntermediate(*OriginPin, *AuthorityMemberSetNode->FindPinChecked(OptionalPin.PropertyName));
				if (ClientEvalPropertyNames.Contains(OptionalPin.PropertyName))
				{
					CompilerContext.CopyPinLinksToIntermediate(*OriginPin, *ClientMemberSetNode->FindPinChecked(OptionalPin.PropertyName));
				}
				OriginPin->BreakAllPinLinks();
			}
		}
		ExpandNodeForEvaluateActionParams(AuthorityThenPin, ClientThenPin, CompilerContext, SourceGraph);
	}
}

void UBPNode_GameQuestNodeBase::PostPlacedNewNode()
{
	Super::PostPlacedNewNode();

	UGameQuestGraphBlueprint* Blueprint = Cast<UGameQuestGraphBlueprint>(GetBlueprint());
	const UStruct* Struct = GetNodeImplStruct();
	if (ensure(Blueprint && Struct))
	{
		Blueprint->NodeIdCounter += 1;
		NodeId = Blueprint->NodeIdCounter;
		GameQuestNodeName = *MakeUniqueRefVarName(Blueprint, Struct->GetDisplayNameText().ToString(), GetRefVarPrefix());
	}
	RecompileSkeletalBlueprintDelayed();
}

void UBPNode_GameQuestNodeBase::PostPasteNode()
{
	Super::PostPasteNode();
	UGameQuestGraphBlueprint* Blueprint = Cast<UGameQuestGraphBlueprint>(GetBlueprint());
	const UStruct* Struct = GetNodeImplStruct();
	if (ensure(Blueprint && Struct))
	{
		Blueprint->NodeIdCounter += 1;
		NodeId = Blueprint->NodeIdCounter;
		const UObject* ExistNode = FindNodeByRefVarName(Blueprint, GetRefVarName());
		if (ExistNode != nullptr && ExistNode != this)
		{
			OnRenameNode(MakeUniqueRefVarName(Blueprint, Struct->GetDisplayNameText().ToString(), GetRefVarPrefix()));
		}
	}
}

void UBPNode_GameQuestNodeBase::DestroyNode()
{
	Super::DestroyNode();

	RecompileSkeletalBlueprintDelayed();
}

void UBPNode_GameQuestNodeBase::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);

	if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
	}
}

FString UBPNode_GameQuestNodeBase::GetFindReferenceSearchString() const
{
	return GetRefVarName().ToString();
}

void UBPNode_GameQuestNodeBase::AddSearchMetaDataInfo(TArray<FSearchTagDataPair>& OutTaggedMetaData) const
{
	Super::AddSearchMetaDataInfo(OutTaggedMetaData);
	OutTaggedMetaData.Add(FSearchTagDataPair(FFindInBlueprintSearchTags::FiB_Name, FText::FromName(GetRefVarName())));
	if (const UStruct* ImplStruct = GetNodeImplStruct())
	{
		OutTaggedMetaData.Add(FSearchTagDataPair(FFindInBlueprintSearchTags::FiB_NativeName, ImplStruct->GetDisplayNameText()));
	}
}

FText UBPNode_GameQuestNodeBase::GetTooltipText() const
{
	if (const UStruct* NodeType = GetNodeImplStruct())
	{
		return NodeType->GetToolTipText();
	}
	return Super::GetTooltipText();
}

void UBPNode_GameQuestNodeBase::JumpToDefinition() const
{
	if (const UObject* HyperlinkTarget = GetJumpTargetForDoubleClick())
	{
		FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(HyperlinkTarget);
	}
	else if (const UStruct* NodeType = GetNodeImplStruct())
	{
		if (const UClass* Class = Cast<UClass>(NodeType))
		{
			FSourceCodeNavigation::NavigateToClass(Class);
		}
		else
		{
			FSourceCodeNavigation::NavigateToStruct(NodeType);
		}
	}
}

bool UBPNode_GameQuestNodeBase::HasExternalDependencies(TArray<UStruct*>* OptionalOutput) const
{
	const UScriptStruct* ScriptStruct = StructNodeInstance.GetScriptStruct();
	if (OptionalOutput && ScriptStruct)
	{
		OptionalOutput->AddUnique(const_cast<UScriptStruct*>(ScriptStruct));
	}
	return ScriptStruct != nullptr;
}

bool UBPNode_GameQuestNodeBase::IsUniqueRefVarName(const UBlueprint* Blueprint, const FName& Name)
{
	return FindNodeByRefVarName(Blueprint, Name) == nullptr;
}

UObject* UBPNode_GameQuestNodeBase::FindNodeByRefVarName(const UBlueprint* Blueprint, const FName& Name)
{
	struct FLocal
	{
		static UBPNode_GameQuestNodeBase* ContainSameNameNode(const UEdGraph* InGraph, const FName& Name)
		{
			for (UEdGraphNode* EdNode : InGraph->Nodes)
			{
				if (UBPNode_GameQuestNodeBase* UtilityNode = Cast<UBPNode_GameQuestNodeBase>(EdNode))
				{
					if (UtilityNode->GetRefVarName() == Name)
					{
						return UtilityNode;
					}
				}
			}
			for (const UEdGraph* SubGraph : InGraph->SubGraphs)
			{
				if (UBPNode_GameQuestNodeBase* UtilityNode = ContainSameNameNode(SubGraph, Name))
				{
					return UtilityNode;
				}
			}
			return nullptr;
		}
	};
	for (const UEdGraph* EventGraph : Blueprint->UbergraphPages)
	{
		if (UBPNode_GameQuestNodeBase* UtilityNode = FLocal::ContainSameNameNode(EventGraph, Name))
		{
			return UtilityNode;
		}
	}
	return StaticFindObjectFast(nullptr, const_cast<UBlueprint*>(Blueprint), Name);
}

FString UBPNode_GameQuestNodeBase::MakeUniqueRefVarName(const UBlueprint* Blueprint, const FString& BaseName, const FString& VarPrefix)
{
	int32 UniqueNumber = 0;
	FString BaseNameNoSpace = BaseName;
	BaseNameNoSpace.RemoveSpacesInline();
	while (true)
	{
		const FString TestName = FString::Printf(TEXT("%s%s_%d"), *VarPrefix, *BaseNameNoSpace, UniqueNumber);
		if (IsUniqueRefVarName(Blueprint, *TestName))
		{
			return FString::Printf(TEXT("%s_%d"), *BaseNameNoSpace, UniqueNumber);
		}
		UniqueNumber += 1;
	}
	return FString();
}

bool UBPNode_GameQuestNodeBase::HasEvaluateActionParams() const
{
	return ShowPinForProperties.ContainsByPredicate([](const FOptionalPinFromProperty& E){ return E.bShowPin; });
}

void UBPNode_GameQuestNodeBase::CreateClassVariablesFromNode(FGameQuestGraphCompilerContext& CompilerContext)
{
	UScriptStruct* NodeStruct = GetNodeStruct();
	if (ensure(NodeStruct))
	{
		const FName RefVarName = GetRefVarName();
		if (FStructProperty* Property = CastField<FStructProperty>(CompilerContext.CreateVariable(RefVarName, FEdGraphPinType(UEdGraphSchema_K2::PC_Struct, NAME_None, NodeStruct, EPinContainerType::None, false, FEdGraphTerminalType()))))
		{
			Property->SetPropertyFlags(CPF_Edit | CPF_BlueprintVisible | CPF_SaveGame);
			Property->SetMetaData(TEXT("Category"), TEXT("Quest"));
			Property->SetMetaData(TEXT("GameQuestNode"), TEXT("True"));

			Property->SetPropertyFlags(CPF_Net);
			CompilerContext.NewClass->NumReplicatedProperties += 1;
			CompilerContext.CreateRepFunction(Property, RefVarName);
		}
		else
		{
			ensure(false);
		}
	}
}

void UBPNode_GameQuestNodeBase::CopyTermDefaultsToDefaultNode(FGameQuestGraphCompilerContext& CompilerContext, UObject* DefaultObject, UGameQuestGraphGeneratedClass* ObjectClass, FStructProperty* NodeProperty)
{
	ObjectClass->NodeNameIdMap.Add(NodeProperty->GetFName(), NodeId);
	const FGameQuestNodeBase* RuntimeNode = StructNodeInstance.GetMutablePtr<FGameQuestNodeBase>();
	const UScriptStruct* NodeStruct = GetNodeStruct();
	if (NodeStruct != StructNodeInstance.GetScriptStruct())
	{
		const UScriptStruct* OldStruct = StructNodeInstance.GetScriptStruct();
		if (NodeStruct->IsChildOf(OldStruct))
		{
			const FInstancedStruct OldInstance{ MoveTemp(StructNodeInstance) };
			StructNodeInstance.InitializeAs(NodeStruct);
			OldStruct->CopyScriptStruct(StructNodeInstance.GetMutableMemory(), OldInstance.GetMemory());
			RuntimeNode = StructNodeInstance.GetMutablePtr<FGameQuestNodeBase>();
		}
		else if (OldStruct->IsChildOf(NodeStruct))
		{
			const FInstancedStruct OldInstance{ MoveTemp(StructNodeInstance) };
			StructNodeInstance.InitializeAs(NodeStruct);
			NodeStruct->CopyScriptStruct(StructNodeInstance.GetMutableMemory(), OldInstance.GetMemory());
			RuntimeNode = StructNodeInstance.GetMutablePtr<FGameQuestNodeBase>();
		}
		else
		{
			ensure(false);
			StructNodeInstance.InitializeAs(NodeStruct);
			RuntimeNode = StructNodeInstance.GetMutablePtr<FGameQuestNodeBase>();
		}
	}
	if (ensure(RuntimeNode))
	{
		NodeProperty->CopySingleValue(NodeProperty->ContainerPtrToValuePtr<FGameQuestNodeBase>(DefaultObject), RuntimeNode);
	}
}

void UBPNode_GameQuestNodeBase::CreateAssignByNameFunction(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, UEdGraphPin* OrgPin, UEdGraphPin* SetObjectPin, UEdGraphPin*& LastThen)
{
	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	if (const UFunction* SetByNameFunction = Schema->FindSetVariableByNameFunction(OrgPin->PinType))
	{
		static const FName ObjectParamName(TEXT("Object"));
		static const FName ValueParamName(TEXT("Value"));
		static const FName PropertyNameParamName(TEXT("PropertyName"));

		UK2Node_CallFunction* SetVarNode;
		if (OrgPin->PinType.IsArray())
		{
			SetVarNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallArrayFunction>(this, SourceGraph);
		}
		else
		{
			SetVarNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		}
		SetVarNode->SetFromFunction(SetByNameFunction);
		SetVarNode->AllocateDefaultPins();

		// Connect this node into the exec chain
		Schema->TryCreateConnection(LastThen, SetVarNode->GetExecPin());
		LastThen = SetVarNode->GetThenPin();

		// Connect the new object to the 'object' pin
		UEdGraphPin* ObjectPin = SetVarNode->FindPinChecked(ObjectParamName);
		SetObjectPin->MakeLinkTo(ObjectPin);

		// Fill in literal for 'property name' pin - name of pin is property name
		UEdGraphPin* PropertyNamePin = SetVarNode->FindPinChecked(PropertyNameParamName);
		PropertyNamePin->DefaultValue = OrgPin->PinName.ToString();

		UEdGraphPin* ValuePin = SetVarNode->FindPinChecked(ValueParamName);
		if (OrgPin->LinkedTo.Num() == 0 &&
			OrgPin->DefaultValue != FString() &&
			OrgPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Byte &&
			OrgPin->PinType.PinSubCategoryObject.IsValid() &&
			OrgPin->PinType.PinSubCategoryObject->IsA<UEnum>())
		{
			// Pin is an enum, we need to alias the enum value to an int:
			UK2Node_EnumLiteral* EnumLiteralNode = CompilerContext.SpawnIntermediateNode<UK2Node_EnumLiteral>(this, SourceGraph);
			EnumLiteralNode->Enum = CastChecked<UEnum>(OrgPin->PinType.PinSubCategoryObject.Get());
			EnumLiteralNode->AllocateDefaultPins();
			EnumLiteralNode->FindPinChecked(UEdGraphSchema_K2::PN_ReturnValue)->MakeLinkTo(ValuePin);

			UEdGraphPin* InPin = EnumLiteralNode->FindPinChecked(UK2Node_EnumLiteral::GetEnumInputPinName());
			check( InPin );
			InPin->DefaultValue = OrgPin->DefaultValue;
		}
		else
		{
			// For non-array struct pins that are not linked, transfer the pin type so that the node will expand an auto-ref that will assign the value by-ref.
			if (OrgPin->PinType.IsArray() == false && OrgPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct && OrgPin->LinkedTo.Num() == 0)
			{
				ValuePin->PinType.PinCategory = OrgPin->PinType.PinCategory;
				ValuePin->PinType.PinSubCategory = OrgPin->PinType.PinSubCategory;
				ValuePin->PinType.PinSubCategoryObject = OrgPin->PinType.PinSubCategoryObject;
				CompilerContext.CopyPinLinksToIntermediate(*OrgPin, *ValuePin);
			}
			else
			{
				// For interface pins we need to copy over the subcategory
				if (OrgPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Interface)
				{
					ValuePin->PinType.PinSubCategoryObject = OrgPin->PinType.PinSubCategoryObject;
				}

				CompilerContext.CopyPinLinksToIntermediate(*OrgPin, *ValuePin);
				SetVarNode->PinConnectionListChanged(ValuePin);
			}
		}
	}
}

FLinearColor UBPNode_GameQuestNodeBase::GetNodeBodyTintColor() const
{
	if (GetBlueprint()->GetObjectBeingDebugged() != nullptr)
	{
		if (DebugState != EDebugState::None)
		{
			return GetDebugStateColor();
		}
	}
	return Super::GetNodeBodyTintColor();
}

FLinearColor UBPNode_GameQuestNodeBase::GetNodeTitleColor() const
{
	if (GetBlueprint()->GetObjectBeingDebugged() != nullptr)
	{
		if (DebugState != EDebugState::None)
		{
			return GetDebugStateColor();
		}
	}
	return Super::GetNodeTitleColor();
}

FLinearColor UBPNode_GameQuestNodeBase::GetDebugStateColor() const
{
	using namespace GameQuestGraphEditorStyle;
	switch (DebugState)
	{
	case EDebugState::Activated:
		return Debug::Activated;
	case EDebugState::Deactivated:
		return Debug::Deactivated;
	case EDebugState::Finished:
		return Debug::Finished;
	case EDebugState::Interrupted:
		return Debug::Interrupted;
	default: ;
	}
	return FLinearColor::White;
}

FTimerHandle UBPNode_GameQuestNodeBase::SkeletalRecompileChildrenHandle;

void UBPNode_GameQuestNodeBase::RecompileSkeletalBlueprintDelayed()
{
	if (GEditor == nullptr)
	{
		return;
	}
	UBlueprint* Blueprint = GetBlueprint();
	if (Blueprint == nullptr)
	{
		return;
	}
	GEditor->GetTimerManager()->SetTimer(SkeletalRecompileChildrenHandle, FTimerDelegate::CreateWeakLambda(Blueprint, [Blueprint]()
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}), MIN_flt, false);
}

void UBPNode_GameQuestNodeBase::FQuestNodeOptionalPinManager::GetRecordDefaults(FProperty* TestProperty, FOptionalPinFromProperty& Record) const
{
	Super::GetRecordDefaults(TestProperty, Record);

	static FName MD_PinShownByDefault = TEXT("PinShownByDefault");
	Record.bShowPin = TestProperty->GetBoolMetaData(MD_PinShownByDefault);
}

bool UBPNode_GameQuestNodeBase::FQuestNodeOptionalPinManager::CanTreatPropertyAsOptional(FProperty* TestProperty) const
{
	return TestProperty->HasAllPropertyFlags(CPF_Edit | CPF_ExposeOnSpawn) && TestProperty->HasAnyPropertyFlags(CPF_EditConst) == false;
}

void UBPNode_GameQuestNodeBase::CreateExposePins()
{
	UStruct* NodeStruct = GetNodeStruct();
	if (NodeStruct == nullptr)
	{
		return;
	}

	FQuestNodeOptionalPinManager QuestNodeOptionalPinManager;
	QuestNodeOptionalPinManager.RebuildPropertyList(ShowPinForProperties, NodeStruct);
	QuestNodeOptionalPinManager.CreateVisiblePins(ShowPinForProperties, NodeStruct, EGPD_Input, this);
}

void UBPNode_GameQuestNodeBase::CreateFinishEventPins()
{
	UStruct* ImplStruct = GetNodeImplStruct();
	if (ImplStruct == nullptr)
	{
		return;
	}

	for (TFieldIterator<FStructProperty> It{ ImplStruct }; It; ++It)
	{
		if (It->Struct->IsChildOf(FGameQuestFinishEvent::StaticStruct()) == false)
		{
			continue;
		}
		UEdGraphPin* Pin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, It->GetFName());
		Pin->PinFriendlyName = It->GetDisplayNameText();
		Pin->PinToolTip = It->GetToolTipText().ToString();
	}
}

void UBPNode_GameQuestNodeBase::ExpandNodeForFinishEvents(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	UStruct* ImplStruct = GetNodeImplStruct();
	if (ImplStruct == nullptr)
	{
		return;
	}

	for (TFieldIterator<FStructProperty> It{ ImplStruct }; It; ++It)
	{
		if (It->Struct->IsChildOf(FGameQuestFinishEvent::StaticStruct()) == false)
		{
			continue;
		}

		UEdGraphPin* FinishPin = FindPinChecked(It->GetFName());
		if (FinishPin->LinkedTo.Num() > 0)
		{
			UK2Node_Event* PostFinishEvent = CompilerContext.SpawnIntermediateEventNode<UK2Node_Event>(this, nullptr, SourceGraph);
			PostFinishEvent->bInternalEvent = true;
			PostFinishEvent->CustomFunctionName = FGameQuestFinishEvent::MakeEventName(GetRefVarName(), It->GetFName());
			PostFinishEvent->AllocateDefaultPins();

			CompilerContext.MovePinLinksToIntermediate(*FinishPin, *PostFinishEvent->FindPinChecked(UEdGraphSchema_K2::PN_Then));
		}
	}
}

const static FName MD_HiddenByDefault{ TEXT("HiddenByDefault") };
UBPNode_GameQuestNodeBase::FStructMemberSetGuard::FStructMemberSetGuard(UStruct* Struct)
	: Struct(Struct)
{
	for (UStruct* TestStruct = Struct; TestStruct; TestStruct = TestStruct->GetSuperStruct())
	{
		const bool HasHiddenMeta = TestStruct->HasMetaData(MD_HiddenByDefault);
		HasHiddenMetaList.Add(HasHiddenMeta);
		if (HasHiddenMeta == false)
		{
			TestStruct->SetMetaData(MD_HiddenByDefault, TEXT("True"));
		}
	}
}

UBPNode_GameQuestNodeBase::FStructMemberSetGuard::~FStructMemberSetGuard()
{
	int32 Idx = 0;
	for (UStruct* TestStruct = Struct; TestStruct; TestStruct = TestStruct->GetSuperStruct())
	{
		if (HasHiddenMetaList[Idx] == false)
		{
			TestStruct->RemoveMetaData(MD_HiddenByDefault);
		}
		Idx += 1;
	}
}

void SNode_GameQuestNodeBase::CreatePinWidgets()
{
	for (UEdGraphPin* CurPin : GraphNode->Pins)
	{
		if (!ensure(CurPin->GetOuter() == GraphNode))
		{
			continue;
		}

		if (CurPin->PinType.PinCategory == GameQuestUtils::Pin::ListPinCategory)
		{
			continue;
		}
		CreateStandardPinWidget(CurPin);
	}
}

void SNode_GameQuestNodeBase::CreateStandardPinWidget(UEdGraphPin* Pin)
{
	if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->PinType.PinSubCategory == GameQuestUtils::Pin::BranchPinSubCategory)
	{
		class SGraphPin_BranchPin : public SGraphPin
		{
		public:
			void Construct(const FArguments& InArgs, UEdGraphPin* InPin)
			{
				SGraphPin::Construct(InArgs, InPin);

				bShowLabel = false;
			}
		protected:
			FSlateColor GetPinColor() const override
			{
				return GameQuestGraphEditorStyle::SequenceBranch::Pin;
			}
			TSharedRef<SWidget>	GetDefaultValueWidget() override
			{
				return SNullWidget::NullWidget;
			}
			const FSlateBrush* GetPinIcon() const override
			{
				return IsConnected() ? CachedImg_DelegatePin_Connected : CachedImg_DelegatePin_Disconnected;
			}
		};

		AddPin(SNew(SGraphPin_BranchPin, Pin));
	}
	else
	{
		Super::CreateStandardPinWidget(Pin);
	}
}

void SNode_GameQuestNodeBase::GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const
{
	Super::GetNodeInfoPopups(Context, Popups);
}

TSharedRef<FGameQuestElementDragDropOp> FGameQuestElementDragDropOp::New(TWeakObjectPtr<UBPNode_GameQuestElementBase> ElementNode)
{
	TSharedRef<FGameQuestElementDragDropOp> Operation = MakeShareable(new FGameQuestElementDragDropOp);
	Operation->ElementNode = ElementNode;
	Operation->Construct();
	Operation->SetErrorTooltip();
	return Operation;
}

void FGameQuestElementDragDropOp::SetAllowedTooltip()
{
	SetToolTip(LOCTEXT("InsertElementHere", "InsertElementHere"), FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK")));
}

void FGameQuestElementDragDropOp::SetErrorTooltip()
{
	SetToolTip(LOCTEXT("SelectInsertElementPosition", "SelectInsertElementPosition"), FAppStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error")));
}

UEdGraphPin* TunnelPin::Redirect(UEdGraphPin* Pin)
{
	if (const UK2Node_Composite* CompositeNode = Cast<UK2Node_Composite>(Pin->GetOwningNode()))
	{
		UEdGraphPin* TunnelPin = CompositeNode->GetEntryNode()->FindPin(Pin->PinName, EGPD_Output);
		if (TunnelPin && TunnelPin->LinkedTo.Num() == 1)
		{
			return Redirect(TunnelPin->LinkedTo[0]);
		}
	}
	else if (const UK2Node_Tunnel* ExitNode = Cast<UK2Node_Tunnel>(Pin->GetOwningNode()))
	{
		if (const UK2Node_Composite* OwningCompositeNode = ExitNode->GetTypedOuter<UK2Node_Composite>())
		{
			UEdGraphPin* TunnelPin = OwningCompositeNode->FindPin(Pin->PinName, EGPD_Output);
			if (TunnelPin && TunnelPin->LinkedTo.Num() == 1)
			{
				return Redirect(TunnelPin->LinkedTo[0]);
			}
		}
	}
	return Pin;
}

#undef LOCTEXT_NAMESPACE
