// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintEditor.h"

class UGameQuestGraphBlueprint;

class GAMEQUESTGRAPHEDITOR_API FGameQuestGraphEditor : public FBlueprintEditor
{
	using Super = FBlueprintEditor;
public:
	FGraphAppearanceInfo GetGraphAppearance(UEdGraph* InGraph) const override;

	static void OpenEditor(const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost>& InToolkitHost, UGameQuestGraphBlueprint* GameQuestBlueprint);
protected:
	class FGameQuestGraphApplicationMode;
	class SGameQuestTreeViewerBase;
	class SGameQuestTreeViewerSubQuest;
	class SGameQuestTreeViewer;

	void InitGameQuestGraphEditor(const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost>& InToolkitHost, UGameQuestGraphBlueprint* InGameQuestBlueprint);

	TWeakObjectPtr<UGameQuestGraphBlueprint> GameQuestBlueprint;

	void Tick(float DeltaTime) override;
	void RegisterApplicationModes(const TArray<UBlueprint*>& InBlueprints, bool bShouldOpenInDefaultsMode, bool bNewlyCreated) override;
	void OnCreateGraphEditorCommands(TSharedPtr<FUICommandList> GraphEditorCommandsList) override;
	bool CanCollapseGameQuestEditorNodes() const;
};
