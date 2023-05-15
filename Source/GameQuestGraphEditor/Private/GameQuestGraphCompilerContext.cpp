// Fill out your copyright notice in the Description page of Project Settings.


#include "GameQuestGraphCompilerContext.h"

#include "BPNode_GameQuestElementBase.h"
#include "BPNode_GameQuestEntryEvent.h"
#include "BPNode_GameQuestRerouteTag.h"
#include "BPNode_GameQuestNodeBase.h"
#include "BPNode_GameQuestSequenceBase.h"
#include "GameQuestGraphBase.h"
#include "GameQuestGraphBlueprint.h"
#include "K2Node_BaseAsyncTask.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/KismetReinstanceUtilities.h"

struct FQuestNodeCollector
{
	FQuestNodeCollector(const UEdGraph* GameQuestGraph)
	{
		if (GameQuestGraph == nullptr)
		{
			return;
		}
		TArray<UBPNode_GameQuestEntryEvent*> EntryEvents;
		GameQuestGraph->GetNodesOfClass(EntryEvents);
		TSet<UEdGraphNode*> Visited;
		for (UBPNode_GameQuestEntryEvent* EntryEvent : EntryEvents)
		{
			DeepSearch(EntryEvent, EntryEvent, nullptr, Visited);
		}
	}

	TMap<UBPNode_GameQuestEntryEvent*, TArray<UBPNode_GameQuestNodeBase*, TInlineAllocator<1>>> StartEntryNodeMap;
	TMap<UBPNode_GameQuestNodeBase*, TArray<UBPNode_GameQuestNodeBase*, TInlineAllocator<1>>> SuccessorNodeMap;
	TMap<UBPNode_GameQuestNodeBase*, TArray<UBPNode_GameQuestNodeBase*, TInlineAllocator<1>>> PreviousNodeMap;
	TArray<UBPNode_GameQuestSequenceBase*> SequenceNodes;
	TArray<UBPNode_GameQuestRerouteTag*> RerouteTagNodes;
	struct FNodePin
	{
		UBPNode_GameQuestNodeBase* Node;
		FName EventName;
		friend bool operator==(const FNodePin& LHS, const FNodePin& RHS)
		{
			return LHS.Node == RHS.Node && LHS.EventName == RHS.EventName;
		}
	};
	TMap<FName, TArray<FNodePin, TInlineAllocator<1>>> RerouteTagPreNodesMap;
	TMap<UBPNode_GameQuestNodeBase*, TArray<FNodePin, TInlineAllocator<1>>> NodeFromEventPinMap;
	void DeepSearch(UEdGraphNode* Node, UEdGraphNode* FromNode, UEdGraphPin* FromPin, TSet<UEdGraphNode*>& Visited)
	{
		if (UBPNode_GameQuestRerouteTag* RerouteTagNode = Cast<UBPNode_GameQuestRerouteTag>(Node))
		{
			RerouteTagNodes.Add(RerouteTagNode);
			auto& PreNodes = RerouteTagPreNodesMap.FindOrAdd(RerouteTagNode->RerouteTag);
			if (UBPNode_GameQuestNodeBase* QuestNode = Cast<UBPNode_GameQuestNodeBase>(FromNode))
			{
				PreNodes.AddUnique({ QuestNode, FromPin ? FromPin->GetFName() : NAME_None });
			}
			return;
		}

		if (Visited.Contains(Node))
		{
			if (UBPNode_GameQuestNodeBase* QuestNode = Cast<UBPNode_GameQuestNodeBase>(Node))
			{
				if (UBPNode_GameQuestNodeBase* FromQuestNode = Cast<UBPNode_GameQuestNodeBase>(FromNode))
				{
					PreviousNodeMap.FindOrAdd(QuestNode).AddUnique(FromQuestNode);
				}
				if (FromPin)
				{
					NodeFromEventPinMap.FindOrAdd(QuestNode).AddUnique({ CastChecked<UBPNode_GameQuestNodeBase>(FromPin->GetOwningNode()), FromPin->GetFName() });
					FromPin = nullptr;
				}
			}
			return;
		}
		Visited.Add(Node);

		UBPNode_GameQuestNodeBase* QuestNode = Cast<UBPNode_GameQuestNodeBase>(Node);
		if (QuestNode)
		{
			if (UBPNode_GameQuestSequenceBase* SequenceNode = Cast<UBPNode_GameQuestSequenceBase>(QuestNode))
			{
				SequenceNodes.AddUnique(SequenceNode);
			}
			if (UBPNode_GameQuestNodeBase* FromQuestNode = Cast<UBPNode_GameQuestNodeBase>(FromNode))
			{
				SuccessorNodeMap.FindOrAdd(FromQuestNode).AddUnique(QuestNode);
				PreviousNodeMap.FindOrAdd(QuestNode).AddUnique(FromQuestNode);
			}
			else if (UBPNode_GameQuestEntryEvent* FromEventNode = Cast<UBPNode_GameQuestEntryEvent>(FromNode))
			{
				StartEntryNodeMap.FindOrAdd(FromEventNode).AddUnique(QuestNode);
			}
			if (FromPin)
			{
				NodeFromEventPinMap.FindOrAdd(QuestNode).AddUnique({ CastChecked<UBPNode_GameQuestNodeBase>(FromPin->GetOwningNode()), FromPin->GetFName() });
				FromPin = nullptr;
			}
		}
		if (const UBPNode_GameQuestSequenceSingle* SequenceSingle = Cast<UBPNode_GameQuestSequenceSingle>(QuestNode))
		{
			// SequenceSingleNode is combined node
			Node = SequenceSingle->Element;
		}
		const bool bIsRecordEventNode = Node->IsA(UBPNode_GameQuestElementBase::StaticClass()) || Node->IsA(UBPNode_GameQuestSequenceSubQuest::StaticClass());
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				for (UEdGraphPin* LinkToPin : Pin->LinkedTo)
				{
					UEdGraphNode* NextNode = TunnelPin::Redirect(LinkToPin)->GetOwningNode();
					if (QuestNode)
					{
						DeepSearch(NextNode, QuestNode, bIsRecordEventNode ? Pin : FromPin, Visited);
					}
					else
					{
						DeepSearch(NextNode, FromNode, FromPin, Visited);
					}
				}
			}
		}
	}
};

FGameQuestGraphCompilerContext::FGameQuestGraphCompilerContext(UGameQuestGraphBlueprint* SourceSketch, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions)
	: Super(SourceSketch, InMessageLog, InCompilerOptions)
	, QuestNodeCollector(MakeShared<FQuestNodeCollector>(SourceSketch->GameQuestGraph))
{

}

void FGameQuestGraphCompilerContext::SpawnNewClass(const FString& NewClassName)
{
	// First, attempt to find the class, in case it hasn't been serialized in yet
	NewClass = FindObject<UGameQuestGraphGeneratedClass>(Blueprint->GetOutermost(), *NewClassName);
	if (NewClass == nullptr)
	{
		// If the class hasn't been found, then spawn a new one
		NewClass = NewObject<UGameQuestGraphGeneratedClass>(Blueprint->GetOutermost(), FName(*NewClassName), RF_Public | RF_Transactional);
	}
	else
	{
		// Already existed, but wasn't linked in the Blueprint yet due to load ordering issues
		NewClass->ClassGeneratedBy = Blueprint;
		FBlueprintCompileReinstancer::Create(NewClass);
	}
}

void FGameQuestGraphCompilerContext::PreCompile()
{
	Super::PreCompile();

	const UGameQuestGraphBlueprint* GameQuestBlueprint = CastChecked<UGameQuestGraphBlueprint>(Blueprint);
	const UEdGraph* GameQuestGraph = GameQuestBlueprint->GameQuestGraph;

	if (GameQuestGraph == nullptr)
	{
		return;
	}

	struct FLatentNodeValidChecker
	{
		FCompilerResultsLog& MessageLog;
		TSet<UEdGraphNode*> Visited;
		static bool IsGameQuestNode(const UEdGraphNode* Node) { return Node->IsA<UBPNode_GameQuestNodeBase>() || Node->IsA<UBPNode_GameQuestEntryEvent>() || Node->IsA<UBPNode_GameQuestRerouteTag>(); }
		bool Check(UEdGraphNode* Node)
		{
			if (Visited.Contains(Node))
			{
				return false;
			}
			Visited.Add(Node);

			bool bIsLinkGameQuestNode = false;
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin->Direction == EGPD_Output && (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec))
				{
					for (UEdGraphPin* LinkToPin : Pin->LinkedTo)
					{
						bIsLinkGameQuestNode |= Check(TunnelPin::Redirect(LinkToPin)->GetOwningNode());
					}
				}
			}
			if (IsGameQuestNode(Node))
			{
				return true;
			}
			bool bIsLatentNode = false;
			if (const UK2Node_CallFunction* FunctionNode = Cast<UK2Node_CallFunction>(Node))
			{
				bIsLatentNode = FunctionNode->IsLatentFunction();
			}
			else if (Node->IsA<UK2Node_BaseAsyncTask>())
			{
				bIsLatentNode = true;
			}
			else if (const UK2Node* K2Node = Cast<UK2Node_CallFunction>(Node))
			{
				static FName LatentIconName = TEXT("Graph.Latent.LatentIcon");
				bIsLatentNode = LatentIconName == K2Node->GetCornerIcon();
			}
			if (bIsLatentNode)
			{
				bool IsValidLatentNode = true;
				if (bIsLinkGameQuestNode)
				{
					IsValidLatentNode = false;
				}
				else
				{
					class FLatentNodeGraphValidChecker
					{
						static bool IsLinkedGameQuestNode(UEdGraphNode* FromNode, EEdGraphPinDirection Direct, TSet<UEdGraphNode*>& Visited)
						{
							if (Visited.Contains(FromNode))
							{
								return false;
							}
							Visited.Add(FromNode);

							for (UEdGraphPin* Pin : FromNode->Pins)
							{
								if (Pin->Direction == Direct && (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec))
								{
									for (UEdGraphPin* LinkToPin : Pin->LinkedTo)
									{
										LinkToPin = TunnelPin::Redirect(LinkToPin);
										if (IsGameQuestNode(LinkToPin->GetOwningNode()))
										{
											return true;
										}
										else
										{
											if (IsLinkedGameQuestNode(LinkToPin->GetOwningNode(), Direct, Visited))
											{
												return true;
											}
										}
									}
								}
							}
							return false;
						}
					public:
						static bool IsValid(UEdGraphNode* LatentNode)
						{
							TSet<UEdGraphNode*> Visited;
							if (IsLinkedGameQuestNode(LatentNode, EEdGraphPinDirection::EGPD_Input, Visited))
							{
								Visited.Empty();
								if (IsLinkedGameQuestNode(LatentNode, EEdGraphPinDirection::EGPD_Output, Visited))
								{
									return false;
								}
							}
							return true;
						}
					};
					IsValidLatentNode = FLatentNodeGraphValidChecker::IsValid(Node);
				}
				if (IsValidLatentNode == false)
				{
					MessageLog.Error(TEXT("Can not connect delay node between quest node @@"), Node);
				}
			}
			return bIsLinkGameQuestNode;
		}
		void Do(const UEdGraph* GameQuestGraph)
		{
			TArray<UBPNode_GameQuestEntryEvent*> EntryEvents;
			GameQuestGraph->GetNodesOfClass(EntryEvents);
			for (UBPNode_GameQuestEntryEvent* EntryEvent : EntryEvents)
			{
				Check(EntryEvent);
			}
		}
	};
	FLatentNodeValidChecker{ MessageLog }.Do(GameQuestGraph);
}

void FGameQuestGraphCompilerContext::CreateClassVariablesFromBlueprint()
{
	Super::CreateClassVariablesFromBlueprint();

	RepFunctions.Empty();

	for (UBPNode_GameQuestSequenceBase* SequenceNode : QuestNodeCollector->SequenceNodes)
	{
		SequenceNode->CreateClassVariablesFromNode(*this);
	}

	TSet<FName> RerouteTags;
	for (const UBPNode_GameQuestRerouteTag* RerouteTagNode : QuestNodeCollector->RerouteTagNodes)
	{
		if (RerouteTagNode->RerouteTag == NAME_None)
		{
			continue;
		}
		RerouteTags.Add(RerouteTagNode->RerouteTag);
	}
	for (const FName& RerouteTag : RerouteTags)
	{
		const FName PropertyName = FGameQuestRerouteTag::MakeVariableName(RerouteTag);
		if (FStructProperty* Property = CastField<FStructProperty>(CreateVariable(PropertyName, FEdGraphPinType(UEdGraphSchema_K2::PC_Struct, NAME_None, FGameQuestRerouteTag::StaticStruct(), EPinContainerType::None, false, FEdGraphTerminalType()))))
		{
			Property->SetMetaData(TEXT("GameQuestRerouteTag"), TEXT("True"));
		}
		else
		{
			ensure(false);
		}
	}
}

void FGameQuestGraphCompilerContext::CreateFunctionList()
{
	Super::CreateFunctionList();

	for (const FRepFunction& RepFunction : RepFunctions)
	{
		const FStructProperty* Property = RepFunction.Property;
		const FName& FunctionName = RepFunction.FunctionName;
		UEdGraph* FunctionGraph = SpawnIntermediateFunctionGraph(FunctionName.ToString());
		const UEdGraphSchema_K2* K2Schema = Cast<const UEdGraphSchema_K2>(FunctionGraph->GetSchema());

		Schema->CreateDefaultNodesForGraph(*FunctionGraph);
		K2Schema->MarkFunctionEntryAsEditable(FunctionGraph, true);
		K2Schema->AddExtraFunctionFlags(FunctionGraph, FUNC_Private | FUNC_BlueprintCallable);

		static const FName PreValuePinName = TEXT("PreValue");
		UK2Node_FunctionEntry* EntryNode = CastChecked<UK2Node_FunctionEntry>(FunctionGraph->Nodes[0]);
		EntryNode->CustomGeneratedFunctionName = FunctionName;
		const TSharedPtr<FUserPinInfo> InputPin = MakeShareable(new FUserPinInfo());
		InputPin->PinName = PreValuePinName;
		InputPin->PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
		InputPin->PinType.PinSubCategoryObject = Property->Struct;
		InputPin->DesiredPinDirection = EGPD_Output;
		EntryNode->CreatePinFromUserDefinition(InputPin);

		FGraphNodeCreator<UK2Node_CallFunction> CallNodeRepFunctionCreator(*FunctionGraph);
		UK2Node_CallFunction* CallNodeRepNode = CallNodeRepFunctionCreator.CreateNode();
		static UFunction* CallNodeRepFunction = UGameQuestGraphBase::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UGameQuestGraphBase, CallNodeRepFunction));
		CallNodeRepNode->SetFromFunction(CallNodeRepFunction);
		CallNodeRepFunctionCreator.Finalize();

		FGraphNodeCreator<UK2Node_VariableGet> VariableGetCreator(*FunctionGraph);
		UK2Node_VariableGet* VariableGet = VariableGetCreator.CreateNode();
		VariableGet->VariableReference.SetFromField<FObjectProperty>(Property, Blueprint->SkeletonGeneratedClass);
		VariableGetCreator.Finalize();

		EntryNode->FindPinChecked(UEdGraphSchema_K2::PN_Then)->MakeLinkTo(CallNodeRepNode->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
		Schema->TryCreateConnection(VariableGet->FindPinChecked(Property->GetFName()), CallNodeRepNode->FindPinChecked(TEXT("Node")));
		Schema->TryCreateConnection(EntryNode->FindPinChecked(PreValuePinName), CallNodeRepNode->FindPinChecked(PreValuePinName));

		ProcessOneFunctionGraph(FunctionGraph, false);
	};
}

void FGameQuestGraphCompilerContext::CopyTermDefaultsToDefaultObject(UObject* DefaultObject)
{
	Super::CopyTermDefaultsToDefaultObject(DefaultObject);

	UGameQuestGraphGeneratedClass* Class = CastChecked<UGameQuestGraphGeneratedClass>(DefaultObject->GetClass());
	Class->NodeNameIdMap.Empty();
	Class->NodeIdLogicsMap.Empty();
	Class->NodeToSuccessorMap.Empty();
	Class->NodeToPredecessorMap.Empty();
	Class->NodeIdEventNameMap.Empty();
	Class->RerouteTagPreNodesMap.Empty();

	for (UBPNode_GameQuestSequenceBase* SequenceNode : QuestNodeCollector->SequenceNodes)
	{
		FStructProperty* SequenceProperty = FindFProperty<FStructProperty>(Class, SequenceNode->GetRefVarName());
		if (!ensure(SequenceProperty))
		{
			continue;
		}
		SequenceNode->CopyTermDefaultsToDefaultNode(*this, DefaultObject, Class, SequenceProperty);
	}
	for (const auto& [FromNode, ToNodes] : QuestNodeCollector->SuccessorNodeMap)
	{
		auto& Indexes = Class->NodeToSuccessorMap.Add(FromNode->NodeId);
		for (const UBPNode_GameQuestNodeBase* NextNode : ToNodes)
		{
			Indexes.Add(NextNode->NodeId);
		}
	}
	for (const auto& [Node, NodePins] : QuestNodeCollector->NodeFromEventPinMap)
	{
		for (const auto& NodePin : NodePins)
		{
			const UBPNode_GameQuestNodeBase* QuestNode = NodePin.Node;
			Class->NodeIdEventNameMap.FindOrAdd(QuestNode->NodeId).Add({ NodePin.EventName, Node->NodeId });
		}
	}
	for (const auto& [RerouteTag, PreNodes] : QuestNodeCollector->RerouteTagPreNodesMap)
	{
		if (PreNodes.Num() == 0)
		{
			continue;
		}
		auto& Ids = Class->RerouteTagPreNodesMap.Add(RerouteTag);
		for (const auto& PreNode : PreNodes)
		{
			Ids.Add({ PreNode.EventName, PreNode.Node->NodeId });
		}
	}

	Class->PostQuestCDOInitProperties();
}

void FGameQuestGraphCompilerContext::ValidatePin(const UEdGraphPin* Pin) const
{
	if (Pin->PinType.PinCategory == GameQuestUtils::Pin::ListPinCategory || Pin->PinType.PinSubCategory == GameQuestUtils::Pin::BranchPinSubCategory)
	{
		return;
	}
	Super::ValidatePin(Pin);
}

void FGameQuestGraphCompilerContext::CreateRepFunction(FStructProperty* Property, const FName& RefVarName)
{
	UBlueprintGeneratedClass* SkeletonClass = NewClass;
	const FName RepFunctionName = *FString::Printf(TEXT("OnRep_%s"), *RefVarName.ToString());
	Property->RepNotifyFunc = RepFunctionName;
	Property->SetBlueprintReplicationCondition(COND_None);
	// reference FBlueprintCompilationManagerImpl::FastGenerateSkeletonClass
	UFunction* NewFunction = NewObject<UFunction>(SkeletonClass, RepFunctionName, RF_Public | RF_Transient);
	NewFunction->StaticLink(true);
	NewFunction->FunctionFlags = FUNC_Private;
	NewFunction->SetMetaData(FBlueprintMetadata::MD_BlueprintInternalUseOnly, TEXT("true"));

	const FEdGraphPinType ParamType { UEdGraphSchema_K2::PC_Object, NAME_None, Property->Struct, EPinContainerType::None, false, FEdGraphTerminalType() };
	FProperty* Param = FKismetCompilerUtilities::CreatePropertyOnScope(NewFunction, Property->GetFName(), ParamType, SkeletonClass, CPF_BlueprintVisible | CPF_BlueprintReadOnly, Schema, MessageLog);
	Param->SetFlags(RF_Transient);
	Param->PropertyFlags |= CPF_Parm;
	NewFunction->ChildProperties = Param;

	// reference FAnimBlueprintCompilerContext::SetCalculatedMetaDataAndFlags
	NewFunction->NumParms = 1;
	NewFunction->ParmsSize = Property->GetSize();

	SkeletonClass->AddFunctionToFunctionMap(NewFunction, NewFunction->GetFName());

	RepFunctions.Add({ Property, RepFunctionName });
}
