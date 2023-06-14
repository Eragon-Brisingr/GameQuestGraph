// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class UBPNode_GameQuestNodeBase;
class IDetailPropertyRow;
class IPropertyHandle;
struct FOptionalPinFromProperty;

class GAMEQUESTGRAPHEDITOR_API FGameQuestBpNodeDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance() { return MakeShared<FGameQuestBpNodeDetails>(); }
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};

class GAMEQUESTGRAPHEDITOR_API FGameQuestBpNodeScriptElementDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance() { return MakeShared<FGameQuestBpNodeScriptElementDetails>(); }
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
