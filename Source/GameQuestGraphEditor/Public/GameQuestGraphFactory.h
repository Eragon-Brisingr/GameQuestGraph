// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_Blueprint.h"
#include "Factories/Factory.h"
#include "GameQuestGraphFactory.generated.h"

class UGameQuestGraphBase;

UCLASS()
class GAMEQUESTGRAPHEDITOR_API UGameQuestGraphFactory : public UFactory
{
	GENERATED_BODY()
public:
	UGameQuestGraphFactory();

	UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext) override;
	UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;

	UPROPERTY(Transient)
	TSubclassOf<UGameQuestGraphBase> ToCreateGameQuestClass;

	bool ConfigureProperties() override;
};

class FGameQuestGraph_AssetTypeActions : public FAssetTypeActions_Blueprint
{
public:
	FGameQuestGraph_AssetTypeActions();

	// Inherited via FAssetTypeActions_Base
	FText GetName() const override;
	FText GetDisplayNameFromAssetData(const FAssetData& AssetData) const override;
	UClass* GetSupportedClass() const override;
	FColor GetTypeColor() const override;
	uint32 GetCategories() override;
	void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor) override;
};
