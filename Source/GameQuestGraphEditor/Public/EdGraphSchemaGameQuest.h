// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphSchemaGameQuest.generated.h"

UCLASS()
class GAMEQUESTGRAPHEDITOR_API UEdGraphSchemaGameQuest : public UEdGraphSchema_K2
{
	GENERATED_BODY()
public:
	FConnectionDrawingPolicy* CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const override;
	void OnPinConnectionDoubleCicked(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2D& GraphPosition) const override;
	const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
	FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const;

	EGraphType GetGraphType(const UEdGraph* TestEdGraph) const override;
};
