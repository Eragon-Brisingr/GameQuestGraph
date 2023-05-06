#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FGameQuestGraphEditorModule : public IModuleInterface
{
public:
    void StartupModule() override;
    void ShutdownModule() override;

    TSharedPtr<class FGameQuestGraph_AssetTypeActions> GameQuestGraph_AssetTypeActions;
};
