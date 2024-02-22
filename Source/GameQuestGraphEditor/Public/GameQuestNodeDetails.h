// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class UBPNode_GameQuestNodeBase;
class IDetailPropertyRow;
class IPropertyHandle;
struct FOptionalPinFromProperty;

class GAMEQUESTGRAPHEDITOR_API FGameQuestNodeDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance() { return MakeShared<FGameQuestNodeDetails>(); }
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};

class GAMEQUESTGRAPHEDITOR_API FGameQuestNodeScriptElementDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance() { return MakeShared<FGameQuestNodeScriptElementDetails>(); }
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
