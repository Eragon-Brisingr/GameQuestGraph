// Fill out your copyright notice in the Description page of Project Settings.


#include "EdGraphSchemaGameQuest.h"

#include "BPNode_GameQuestNodeBase.h"
#include "BlueprintConnectionDrawingPolicy.h"
#include "BPNode_GameQuestElementBase.h"
#include "BPNode_GameQuestRerouteTag.h"
#include "BPNode_GameQuestSequenceBase.h"
#include "GameQuestElementBase.h"
#include "GameQuestGraphBase.h"
#include "GameQuestGraphBlueprint.h"
#include "GameQuestSequenceBase.h"
#include "GameQuestGraphEditorStyle.h"

#define LOCTEXT_NAMESPACE "GameQuestGraphEditor"

class FGameQuestConnectionDrawingPolicy : public FKismetConnectionDrawingPolicy
{
	using Super = FKismetConnectionDrawingPolicy;
public:
	FGameQuestConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj)
		: Super(InBackLayerID, InFrontLayerID, ZoomFactor, InClippingRect, InDrawElements, InGraphObj)
	{}

	void Draw(TMap<TSharedRef<SWidget>, FArrangedWidget>& InPinGeometries, FArrangedChildren& ArrangedNodes) override
	{
		Super::Draw(InPinGeometries, ArrangedNodes);
		const UGameQuestGraphBlueprint* Blueprint = GraphObj->GetTypedOuter<UGameQuestGraphBlueprint>();
		if (Blueprint == nullptr)
		{
			return;
		}
		struct FNode
		{
			UBPNode_GameQuestNodeBase* QuestNode;
			FArrangedWidget& Widget;
		};
		TMap<FName, FNode> QuestNodeMap;
		struct FTagNode
		{
			UBPNode_GameQuestRerouteTag* Node;
			FArrangedWidget& Widget;
		};
		TArray<FTagNode> RerouteTagNodes;
		TMap<FName, TArray<FTagNode>> RerouteTagNodeMap;
		for (int32 NodeIndex = 0; NodeIndex < ArrangedNodes.Num(); ++NodeIndex)
		{
			FArrangedWidget& CurWidget = ArrangedNodes[NodeIndex];
			const TSharedRef<SGraphNode> ChildNode = StaticCastSharedRef<SGraphNode>(CurWidget.Widget);
			if (UBPNode_GameQuestNodeBase* QuestNode = Cast<UBPNode_GameQuestNodeBase>(ChildNode->GetNodeObj()))
			{
				QuestNodeMap.Add(QuestNode->GetRefVarName(), { QuestNode, CurWidget });
			}
			else if (UBPNode_GameQuestRerouteTag* RerouteTagNode = Cast<UBPNode_GameQuestRerouteTag>(ChildNode->GetNodeObj()))
			{
				RerouteTagNodes.Add({ RerouteTagNode, CurWidget });
				RerouteTagNodeMap.FindOrAdd(RerouteTagNode->RerouteTag).Add({ RerouteTagNode, CurWidget });
			}
		}

		TGuardValue<bool> DirectDrawGuard{ bDirectDraw, true };

		if (UGameQuestGraphGeneratedClass* Class = Cast<UGameQuestGraphGeneratedClass>(Blueprint->GeneratedClass))
		{
			struct FPinWidget
			{
				const FArrangedWidget& Widget;
			};
			TMap<const UEdGraphPin*, FPinWidget> EventPinMap;
			for (const auto& [Widget, ArrangedWidget] : InPinGeometries)
			{
				const TSharedRef<SGraphPin> Pin = StaticCastSharedRef<SGraphPin>(Widget);
				const UEdGraphPin* PinObj = Pin->GetPinObj();
				if (const UBPNode_GameQuestElementBase* ElementNode = Cast<UBPNode_GameQuestElementBase>(PinObj->GetOwningNode()))
				{
					if (ElementNode->IsListMode())
					{
						continue;
					}
					EventPinMap.Add(PinObj, FPinWidget{ ArrangedWidget });
				}
				else if (const UBPNode_GameQuestSequenceSubQuest* SequenceSubQuest = Cast<UBPNode_GameQuestSequenceSubQuest>(PinObj->GetOwningNode()))
				{
					EventPinMap.Add(PinObj, FPinWidget{ ArrangedWidget });
				}
			}
			FConnectionParams Params;
			Params.StartDirection = EGPD_Output;
			Params.EndDirection = EGPD_Input;
			Params.WireColor = FColor::Purple;
			Params.WireThickness = 1.f;
			const float Padding = ZoomFactor * 65.f;
			for (const auto& [NodeId, NextNodes] : Class->NodeToSuccessorMap)
			{
				const FNode* Node = QuestNodeMap.Find(Class->NodeIdPropertyMap[NodeId]->GetFName());
				if (Node == nullptr)
				{
					continue;
				}
				const UBPNode_GameQuestNodeBase* QuestNode = Node->QuestNode;
				if (const UBPNode_GameQuestSequenceSingle* SingleNode = Cast<UBPNode_GameQuestSequenceSingle>(Node->QuestNode))
				{
					QuestNode = SingleNode->Element;
				}
				using FEventNameToNextNode = UGameQuestGraphGeneratedClass::FEventNameNodeId;
				const auto EventNames = Class->NodeIdEventNameMap.Find(QuestNode->NodeId);
				for (const uint16 NextNodeId : NextNodes)
				{
					const FNode* NextNode = QuestNodeMap.Find(Class->NodeIdPropertyMap[NextNodeId]->GetFName());
					if (NextNode == nullptr)
					{
						continue;
					}
					const FVector2D EndLinkPoint = NextNode->Widget.Geometry.GetAbsolutePosition() + FVector2D{ 0.f, Padding };

					const FEventNameToNextNode* EventName = EventNames ? EventNames->FindByPredicate([NextNodeId](const FEventNameToNextNode& E){ return E.NodeId == NextNodeId; }) : nullptr;
					if (EventName != nullptr)
					{
						const UEdGraphPin* Pin = QuestNode->FindPin(EventName->EventName);
						if (Pin == nullptr)
						{
							continue;
						}
						if (const FPinWidget* PinWidget = EventPinMap.Find(Pin))
						{
							const FVector2D PinSize = PinWidget->Widget.Geometry.GetAbsoluteSize();
							const FVector2D StartLinkPoint = PinWidget->Widget.Geometry.GetAbsolutePosition() + FVector2D{ PinSize.X, PinSize.Y / 2.f + 6.f * ZoomFactor };
							DrawConnection(WireLayerID, StartLinkPoint, EndLinkPoint, Params);
						}
					}
					else if (const FNode* StartNode = QuestNodeMap.Find(Class->NodeIdPropertyMap[QuestNode->NodeId]->GetFName()))
					{
						const FVector2D StartLinkPoint = StartNode->Widget.Geometry.GetAbsolutePosition() + FVector2D{ StartNode->Widget.Geometry.GetAbsoluteSize().X, Padding };
						DrawConnection(WireLayerID, StartLinkPoint, EndLinkPoint, Params);
					}
				}
			}

			for (const auto& [TagName, PreNodes] : Class->RerouteTagPreNodesMap)
			{
				for (const auto& PreNode : PreNodes)
				{
					const FNode* Node = QuestNodeMap.Find(Class->NodeIdPropertyMap[PreNode.NodeId]->GetFName());
					if (Node == nullptr)
					{
						continue;
					}
					const auto TagNodesPtr = RerouteTagNodeMap.Find(TagName);
					if (TagNodesPtr == nullptr)
					{
						continue;
					}
					const FTagNode* NearestTagNode = nullptr;
					float TestDistanceSquared = FLT_MAX;
					for (const auto& TagNode : *TagNodesPtr)
					{
						const float DistanceSquared = (Node->Widget.Geometry.GetAbsolutePosition() - TagNode.Widget.Geometry.GetAbsolutePosition()).SizeSquared();
						if (DistanceSquared < TestDistanceSquared)
						{
							NearestTagNode = &TagNode;
						}
					}
					if (NearestTagNode)
					{
						const FVector2D EndLinkPoint = NearestTagNode->Widget.Geometry.GetAbsolutePosition() + FVector2D{ 0.f, Padding };
						const UBPNode_GameQuestNodeBase* QuestNode = Node->QuestNode;
						if (const UBPNode_GameQuestSequenceSingle* SingleNode = Cast<UBPNode_GameQuestSequenceSingle>(Node->QuestNode))
						{
							QuestNode = SingleNode->Element;
						}
						const UEdGraphPin* EventPin = QuestNode->FindPin(PreNode.EventName);
						if (EventPin != nullptr)
						{
							if (const FPinWidget* PinWidget = EventPinMap.Find(EventPin))
							{
								const FVector2D PinSize = PinWidget->Widget.Geometry.GetAbsoluteSize();
								const FVector2D StartLinkPoint = PinWidget->Widget.Geometry.GetAbsolutePosition() + FVector2D{ PinSize.X, PinSize.Y / 2.f + 6.f * ZoomFactor };
								DrawConnection(WireLayerID, StartLinkPoint, EndLinkPoint, Params);
							}
						}
						else
						{
							const FVector2D StartLinkPoint = Node->Widget.Geometry.GetAbsolutePosition() + FVector2D{ Node->Widget.Geometry.GetAbsoluteSize().X, Padding };
							DrawConnection(WireLayerID, StartLinkPoint, EndLinkPoint, Params);
						}
					}
				}
			}
		}

		if (const UGameQuestGraphBase* Quest = Cast<UGameQuestGraphBase>(Blueprint->GetObjectBeingDebugged()))
		{
			auto DrawCustomConnection = [&, LinkArrowImage = FAppStyle::GetBrush(TEXT("Graph.Arrow"))](const FGeometry& StartGeom, const FGeometry& EndGeom)
			{
				const float Padding = ZoomFactor * 70.f;
				const float ArrowPadding = ZoomFactor * 4.f;
				const FVector2D StartAnchorPoint = StartGeom.GetAbsolutePosition() + FVector2D{ StartGeom.GetAbsoluteSize().X, Padding };
				const FVector2D EndAnchorPoint = EndGeom.GetAbsolutePosition() + FVector2D{ (ArrowPadding - LinkArrowImage->ImageSize.X) * ZoomFactor, Padding };

				FConnectionParams Params;
				Params.StartDirection = EGPD_Output;
				Params.EndDirection = EGPD_Input;
				Params.WireColor = FColor::Orange;
				Params.WireThickness = 1.f;
				DrawConnection(WireLayerID, StartAnchorPoint, EndAnchorPoint, Params);
				if (LinkArrowImage != nullptr)
				{
					const FVector2D ArrowPoint = EndAnchorPoint - FVector2D{ ArrowPadding, LinkArrowImage->ImageSize.Y * ZoomFactor / 2.f };
					FSlateDrawElement::MakeBox(
						DrawElementsList,
						WireLayerID,
						FPaintGeometry(ArrowPoint, LinkArrowImage->ImageSize * ZoomFactor, ZoomFactor),
						LinkArrowImage,
						ESlateDrawEffect::None,
						Params.WireColor);
				}
			};
			auto DrawQuestConnection = [&](const FNode* StartQuestNode, const FNode* EndQuestNode)
			{
				if (StartQuestNode == nullptr || EndQuestNode == nullptr)
				{
					return;
				}
				DrawCustomConnection(StartQuestNode->Widget.Geometry, EndQuestNode->Widget.Geometry);
			};

			WireLayerID += 1;
			for (const uint16 StartSequenceId : Quest->GetStartSequencesIds())
			{
				struct FNextActivatedSequenceDrawer
				{
					const UGameQuestGraphBase* Quest;
					decltype(DrawQuestConnection)& DrawConnection;
					TMap<FName, FNode>& QuestNodeMap;

					TSet<uint16> Visited;
					void Draw(const uint16 SequenceId, const FGameQuestSequenceBase* PreSequence)
					{
						if (Visited.Contains(SequenceId))
						{
							return;
						}
						Visited.Add(SequenceId);

						const FGameQuestSequenceBase* Sequence = Quest->GetSequencePtr(SequenceId);
						if (PreSequence)
						{
							const FNode* QuestNode = QuestNodeMap.Find(Sequence->GetNodeName());
							const FNode* PreQuestNode;
							if (const FGameQuestSequenceSingle* SequenceSingle = GameQuestCast<FGameQuestSequenceSingle>(PreSequence))
							{
								PreQuestNode = QuestNodeMap.Find(Quest->GetElementPtr(SequenceSingle->Element)->GetNodeName());
							}
							else if (const FGameQuestSequenceBranch* SequenceBranch = GameQuestCast<FGameQuestSequenceBranch>(PreSequence))
							{
								const FGameQuestSequenceBranchElement* BranchElement = SequenceBranch->Branches.FindByPredicate([SequenceId = Quest->GetSequenceId(Sequence)](const FGameQuestSequenceBranchElement& E){ return E.NextSequences.Contains(SequenceId); });
								PreQuestNode = QuestNodeMap.Find(Quest->GetElementPtr(BranchElement->Element)->GetNodeName());
								DrawConnection(QuestNodeMap.Find(SequenceBranch->GetNodeName()), PreQuestNode);
							}
							else
							{
								PreQuestNode = QuestNodeMap.Find(PreSequence->GetNodeName());
							}
							DrawConnection(PreQuestNode, QuestNode);
						}
						TArray<uint16> NextSequences = Sequence->GetNextSequences();
						if (NextSequences.Num() == 0)
						{
							if (const FGameQuestSequenceBranch* SequenceBranch = GameQuestCast<FGameQuestSequenceBranch>(Sequence))
							{
								for (const FGameQuestSequenceBranchElement& Branch : SequenceBranch->Branches)
								{
									const FGameQuestElementBase* Element = Quest->GetElementPtr(Branch.Element);
									if (Element->bIsFinished == false)
									{
										continue;
									}
									DrawConnection(QuestNodeMap.Find(SequenceBranch->GetNodeName()), QuestNodeMap.Find(Element->GetNodeName()));
								}
							}
						}
						for (const uint16 NextSequenceId : NextSequences)
						{
							Draw(NextSequenceId, Sequence);
						}
					}
				};
				FNextActivatedSequenceDrawer{ Quest, DrawQuestConnection, QuestNodeMap }.Draw(StartSequenceId, nullptr);
			}
			for (const auto& [RerouteTagNode, Widget] : RerouteTagNodes)
			{
				const FGameQuestRerouteTag* RerouteTag = Quest->GetRerouteTag(RerouteTagNode->RerouteTag);
				if (RerouteTag == nullptr || RerouteTag->PreSequenceId == GameQuest::IdNone)
				{
					continue;
				}
				const FGameQuestSequenceBase* Sequence = Quest->GetSequencePtr(RerouteTag->PreSequenceId);
				const FNode* SequenceNode = QuestNodeMap.Find(Sequence->GetNodeName());
				if (SequenceNode == nullptr)
				{
					continue;
				}
				if (const FGameQuestSequenceBranch* SequenceBranch = GameQuestCast<FGameQuestSequenceBranch>(Sequence))
				{
					for (const FGameQuestSequenceBranchElement& Branch : SequenceBranch->Branches)
					{
						if (Branch.Element != RerouteTag->PreBranchId)
						{
							continue;
						}
						const FGameQuestElementBase* Element = Quest->GetElementPtr(Branch.Element);
						const FNode* BranchNode = QuestNodeMap.Find(Element->GetNodeName());
						if (BranchNode == nullptr)
						{
							continue;
						}
						DrawCustomConnection(BranchNode->Widget.Geometry, Widget.Geometry);
					}
				}
				else
				{
					DrawCustomConnection(SequenceNode->Widget.Geometry, Widget.Geometry);
				}
			}
		}
	}

	void DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params) override
	{
		Super::DetermineWiringStyle(OutputPin, InputPin, Params);
		if (InputPin && InputPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && InputPin->PinType.PinSubCategory == GameQuestUtils::Pin::BranchPinSubCategory)
		{
			Params.bUserFlag2 = true;
			Params.WireColor = GameQuestGraphEditorStyle::SequenceBranch::Pin;
		}
	}

	void DrawSplineWithArrow(const FVector2D& StartAnchorPoint, const FVector2D& EndAnchorPoint, const FConnectionParams& Params) override
	{
		if (Params.bUserFlag2)
		{
			DrawBranchConnection(StartAnchorPoint, EndAnchorPoint, Params);
		}
		else
		{
			Super::DrawSplineWithArrow(StartAnchorPoint, EndAnchorPoint, Params);
		}
	}

	void DrawPreviewConnector(const FGeometry& PinGeometry, const FVector2D& StartPoint, const FVector2D& EndPoint, UEdGraphPin* Pin) override
	{
		if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && Pin->PinType.PinSubCategory == GameQuestUtils::Pin::BranchPinSubCategory)
		{
			FConnectionParams Params;
			DetermineWiringStyle(Pin, nullptr, /*inout*/ Params);
			DrawBranchConnection(StartPoint, EndPoint, Params);
		}
		else
		{
			Super::DrawPreviewConnector(PinGeometry, StartPoint, EndPoint, Pin);
		}
	}

	FVector2D ComputeSplineTangent(const FVector2D& Start, const FVector2D& End) const override
	{
		if (bDirectDraw)
		{
			const FVector2D Delta = End - Start;
			const FVector2D NormDelta = Delta.GetSafeNormal();
			return NormDelta;
		}
		return Super::ComputeSplineTangent(Start, End);
	}
private:
	bool bDirectDraw = false;
	void DrawBranchConnection(const FVector2D& StartPoint, const FVector2D& EndPoint, const FConnectionParams& Params)
	{
		TGuardValue<bool> DirectDrawGuard{ bDirectDraw, true };

		if (FMath::IsNearlyEqual(StartPoint.Y, EndPoint.Y))
		{
			DrawConnection(WireLayerID, StartPoint, EndPoint, Params);
		}
		else
		{
			const FVector2D CenterPoint = (StartPoint + EndPoint) / 2.f;
			const float Padding = (StartPoint.X < EndPoint.X ? Params.WireThickness : -Params.WireThickness) / 2.f;

			DrawConnection(WireLayerID, StartPoint, { CenterPoint.X + Padding, StartPoint.Y }, Params);
			DrawConnection(WireLayerID, { CenterPoint.X, StartPoint.Y }, { CenterPoint.X, EndPoint.Y }, Params);
			DrawConnection(WireLayerID, { CenterPoint.X - Padding, EndPoint.Y }, EndPoint, Params);
		}
	}
};

FConnectionDrawingPolicy* UEdGraphSchemaGameQuest::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const
{
	return new FGameQuestConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}

void UEdGraphSchemaGameQuest::OnPinConnectionDoubleCicked(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2D& GraphPosition) const
{
	if (PinA && PinB && PinA->PinType.PinSubCategory == GameQuestUtils::Pin::BranchPinSubCategory && PinB->PinType.PinSubCategory == GameQuestUtils::Pin::BranchPinSubCategory)
	{
		// Pass
	}
	else
	{
		Super::OnPinConnectionDoubleCicked(PinA, PinB, GraphPosition);
	}
}

const FPinConnectionResponse UEdGraphSchemaGameQuest::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
	using namespace GameQuestUtils;

	if (A != B)
	{
		if (A->PinType.PinSubCategory == Pin::BranchPinSubCategory && B->PinType.PinSubCategory == Pin::BranchPinSubCategory)
		{
			const UEdGraphPin* InputPin = A->Direction == EGPD_Input ? A : B;

			if (InputPin->LinkedTo.Num() == 0)
			{
				return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT(""));
			}
			else
			{
				return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Branch Pin can not multi connected"));
			}
		}
	}
	return Super::CanCreateConnection(A, B);
}

FLinearColor UEdGraphSchemaGameQuest::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	using namespace GameQuestUtils;
	if (PinType.PinSubCategory == Pin::BranchPinSubCategory)
	{
		return GameQuestGraphEditorStyle::SequenceBranch::Pin;
	}
	return Super::GetPinTypeColor(PinType);
}

EGraphType UEdGraphSchemaGameQuest::GetGraphType(const UEdGraph* TestEdGraph) const
{
	return EGraphType::GT_Ubergraph;
}

#undef LOCTEXT_NAMESPACE
