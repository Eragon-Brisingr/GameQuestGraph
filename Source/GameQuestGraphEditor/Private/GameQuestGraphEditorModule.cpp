#include "GameQuestGraphEditorModule.h"

#include "AssetToolsModule.h"
#include "GameQuestGraphFactory.h"
#include "GameQuestNodeDetails.h"
#include "GameQuestGraphBlueprint.h"
#include "GameQuestGraphCompilerContext.h"
#include "GameQuestGraphEditorStyle.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "GameQuestGraphEditor"

void FGameQuestGraphEditorModule::StartupModule()
{
	FKismetCompilerContext::RegisterCompilerForBP(UGameQuestGraphBlueprint::StaticClass(), [](UBlueprint* BP, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions)
	{
		return MakeShared<FGameQuestGraphCompilerContext>(CastChecked<UGameQuestGraphBlueprint>(BP), InMessageLog, InCompileOptions);
	});

	if (FAssetToolsModule::IsModuleLoaded())
	{
		IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
		GameQuestGraph_AssetTypeActions = MakeShared<FGameQuestGraph_AssetTypeActions>();
		AssetTools.RegisterAssetTypeActions(GameQuestGraph_AssetTypeActions.ToSharedRef());
	}
	if (FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
	{
		PropertyModule->RegisterCustomClassLayout(TEXT("BPNode_GameQuestNodeBase"), FOnGetDetailCustomizationInstance::CreateStatic(&FGameQuestNodeDetails::MakeInstance));
		PropertyModule->RegisterCustomClassLayout(TEXT("BPNode_GameQuestElementScript"), FOnGetDetailCustomizationInstance::CreateStatic(&FGameQuestNodeScriptElementDetails::MakeInstance));
	}

	FGameQuestGraphSlateStyle::StartupModule();
}

void FGameQuestGraphEditorModule::ShutdownModule()
{
	if (FAssetToolsModule::IsModuleLoaded())
	{
		IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
		AssetTools.UnregisterAssetTypeActions(GameQuestGraph_AssetTypeActions.ToSharedRef());
	}
	if (FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
	{
		PropertyModule->UnregisterCustomClassLayout(TEXT("BPNode_GameQuestNodeBase"));
		PropertyModule->UnregisterCustomClassLayout(TEXT("BPNode_GameQuestElementScript"));
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGameQuestGraphEditorModule, GameQuestGraphEditor)
