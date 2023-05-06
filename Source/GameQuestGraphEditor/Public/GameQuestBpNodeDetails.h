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
protected:
	UBPNode_GameQuestNodeBase* GetFirstNode(IDetailLayoutBuilder& DetailBuilder) const;

	static void BuildPinOptionalDetails(IDetailLayoutBuilder& DetailBuilder, UStruct* Class, UClass* NodeClass, const TArray<FOptionalPinFromProperty>& ShowPinOptions, const FString& ShowPinPropertyName);
	static void CreateExposePropertyWidget(FProperty* TargetProperty, TSharedRef<IPropertyHandle> TargetPropertyHandle, IDetailPropertyRow& PropertyRow, TSharedRef<IPropertyHandle> ShowHidePropertyHandle);
};

class GAMEQUESTGRAPHEDITOR_API FGameQuestBpNodeScriptElementDetails : public FGameQuestBpNodeDetails
{
	using Super = FGameQuestBpNodeDetails;
public:
	static TSharedRef<IDetailCustomization> MakeInstance() { return MakeShared<FGameQuestBpNodeScriptElementDetails>(); }
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
