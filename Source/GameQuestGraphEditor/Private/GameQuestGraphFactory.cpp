// Fill out your copyright notice in the Description page of Project Settings.


#include "GameQuestGraphFactory.h"

#include "BPNode_GameQuestEntryEvent.h"
#include "ClassViewerFilter.h"
#include "ClassViewerModule.h"
#include "EdGraphSchemaGameQuest.h"
#include "GameQuestGraphBase.h"
#include "GameQuestGraphBlueprint.h"
#include "GameQuestGraphEditor.h"
#include "GameQuestGraphEditorSettings.h"
#include "ObjectEditorUtils.h"
#include "Engine/MemberReference.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/SClassPickerDialog.h"
#include "Settings/EditorStyleSettings.h"

#define LOCTEXT_NAMESPACE "GameQuestGraphEditor"

namespace GameQuestFactoryUtils
{
	bool SelectGameQuestType(TSubclassOf<UGameQuestGraphBase>& CreateGameQuestClass, const FText& TitleText)
	{
		class FGameQuestElementFilterViewer : public IClassViewerFilter
		{
		public:
			EClassFlags AllowedClassFlags = CLASS_Abstract;
			const UGameQuestGraphEditorSettings* EditorSettings = GetDefault<UGameQuestGraphEditorSettings>();

			bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef<class FClassViewerFilterFuncs> InFilterFuncs) override
			{
				if (InClass->HasAnyClassFlags(AllowedClassFlags) && InClass->IsChildOf<UGameQuestGraphBase>())
				{
					return EditorSettings->HiddenGameQuestGraphTypes.Contains(InClass) == false;
				}
				return false;
			}

			bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef<const class IUnloadedBlueprintData> InUnloadedClassData, TSharedRef<class FClassViewerFilterFuncs> InFilterFuncs) override
			{
				if (InUnloadedClassData->HasAnyClassFlags(AllowedClassFlags) && InUnloadedClassData->IsChildOf(UGameQuestGraphBase::StaticClass()))
				{
					return EditorSettings->HiddenGameQuestGraphTypes.ContainsByPredicate([&](const TSoftClassPtr<UGameQuestGraphBase>& E){ return E.ToSoftObjectPath() == FSoftObjectPath{ InUnloadedClassData->GetClassPathName() }; }) == false;
				}
				return false;
			}
		};

		CreateGameQuestClass = nullptr;

		FClassViewerInitializationOptions Options;
		Options.Mode = EClassViewerMode::ClassPicker;
		Options.ClassFilters.Add(MakeShared<FGameQuestElementFilterViewer>());
		Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::Dynamic;

		UClass* ChosenClass = nullptr;
		const bool bPressedOk = SClassPickerDialog::PickClass(TitleText, Options, ChosenClass, UGameQuestGraphBase::StaticClass());

		if (bPressedOk)
		{
			CreateGameQuestClass = ChosenClass ? ChosenClass : UGameQuestGraphBase::StaticClass();
		}
		return bPressedOk;
	}
}

UGameQuestGraphFactory::UGameQuestGraphFactory()
{
	SupportedClass = UGameQuestGraphBlueprint::StaticClass();
	ToCreateGameQuestClass = UGameQuestGraphBlueprint::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UGameQuestGraphFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
{
	UGameQuestGraphBlueprint* NewBP = CastChecked<UGameQuestGraphBlueprint>(FKismetEditorUtilities::CreateBlueprint(ToCreateGameQuestClass, InParent, Name, BPTYPE_Normal, UGameQuestGraphBlueprint::StaticClass(), UGameQuestGraphGeneratedClass::StaticClass(), CallingContext));

	UEdGraph* GameQuestGraph = FBlueprintEditorUtils::CreateNewGraph(NewBP, TEXT("Quest_Graph"), UEdGraph::StaticClass(), UEdGraphSchemaGameQuest::StaticClass());
	GameQuestGraph->bAllowDeletion = false;
	GameQuestGraph->bAllowRenaming = false;
	NewBP->GameQuestGraph = GameQuestGraph;
	FBlueprintEditorUtils::RemoveGraphs(NewBP, NewBP->UbergraphPages);
	FBlueprintEditorUtils::AddUbergraphPage(NewBP, GameQuestGraph);
	NewBP->LastEditedDocuments.Add(GameQuestGraph);

	struct FActionPerformanceEventUtil
	{
		static UBPNode_GameQuestEntryEvent* AddDefaultEventNode(UBlueprint* InBlueprint, UEdGraph* InGraph, FName InEventName, UClass* InEventClass, int32& InOutNodePosY)
		{
			UBPNode_GameQuestEntryEvent* NewEventNode = nullptr;

			FMemberReference EventReference;
			EventReference.SetExternalMember(InEventName, InEventClass);

			// Prevent events that are hidden in the Blueprint's class from being auto-generated.
			if (!FObjectEditorUtils::IsFunctionHiddenFromClass(EventReference.ResolveMember<UFunction>(InBlueprint), InBlueprint->ParentClass))
			{
				// Add the event
				NewEventNode = NewObject<UBPNode_GameQuestEntryEvent>(InGraph);
				NewEventNode->EventReference = EventReference;

				// Snap the new position to the grid
				if (const UEditorStyleSettings* StyleSettings = GetDefault<UEditorStyleSettings>())
				{
					const uint32 GridSnapSize = StyleSettings->GridSnapSize;
					InOutNodePosY = GridSnapSize * FMath::RoundFromZero(InOutNodePosY / (float)GridSnapSize);
				}

				// add update event graph
				NewEventNode->bOverrideFunction = true;
				NewEventNode->CreateNewGuid();
				NewEventNode->PostPlacedNewNode();
				NewEventNode->SetFlags(RF_Transactional);
				NewEventNode->AllocateDefaultPins();
				NewEventNode->bCommentBubblePinned = true;
				NewEventNode->bCommentBubbleVisible = true;
				NewEventNode->NodePosY = InOutNodePosY;
				UEdGraphSchema_K2::SetNodeMetaData(NewEventNode, FNodeMetadata::DefaultGraphNode);
				InOutNodePosY = NewEventNode->NodePosY + NewEventNode->NodeHeight + 200;

				InGraph->AddNode(NewEventNode);
				NewEventNode->bCommentBubbleVisible = false;
			}

			return NewEventNode;
		}

	};

	int32 NodePosY = 0;
	FActionPerformanceEventUtil::AddDefaultEventNode(NewBP, GameQuestGraph, GET_FUNCTION_NAME_CHECKED(UGameQuestGraphBase, DefaultEntry), ToCreateGameQuestClass, NodePosY);

	return NewBP;
}

UObject* UGameQuestGraphFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return FactoryCreateNew(InClass, InParent, InName, Flags, Context, Warn, NAME_None);
}

bool UGameQuestGraphFactory::ConfigureProperties()
{
	return GameQuestFactoryUtils::SelectGameQuestType(ToCreateGameQuestClass, LOCTEXT("SelectGameQuestTypeTitle", "Select Game Quest Type"));
}

FGameQuestGraph_AssetTypeActions::FGameQuestGraph_AssetTypeActions()
{
}

FText FGameQuestGraph_AssetTypeActions::GetName() const
{
	return LOCTEXT("GameQuestGraph", "GameQuestGraph");
}

FText FGameQuestGraph_AssetTypeActions::GetDisplayNameFromAssetData(const FAssetData& AssetData) const
{
	return LOCTEXT("GameQuestGraph", "GameQuestGraph");
}

UClass* FGameQuestGraph_AssetTypeActions::GetSupportedClass() const
{
	return UGameQuestGraphBlueprint::StaticClass();
}

FColor FGameQuestGraph_AssetTypeActions::GetTypeColor() const
{
	return FColor::Red;
}

uint32 FGameQuestGraph_AssetTypeActions::GetCategories()
{
	return EAssetTypeCategories::Gameplay;
}

void FGameQuestGraph_AssetTypeActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
	for (UObject* Object : InObjects)
	{
		if (UGameQuestGraphBlueprint* GameQuestBlueprint = Cast<UGameQuestGraphBlueprint>(Object))
		{
			bool bLetOpen = true;
			if (!GameQuestBlueprint->SkeletonGeneratedClass || !GameQuestBlueprint->GeneratedClass)
			{
				bLetOpen = EAppReturnType::Yes == FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("FailedToLoadBlueprintWithContinue", "Blueprint could not be loaded because it derives from an invalid class.  Check to make sure the parent class for this blueprint hasn't been removed! Do you want to continue (it can crash the editor)?"));
			}
			if (bLetOpen)
			{
				FGameQuestGraphEditor::OpenEditor(Mode, EditWithinLevelEditor, GameQuestBlueprint);
			}
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("FailedToLoadBlueprint", "Blueprint could not be loaded because it derives from an invalid class.  Check to make sure the parent class for this blueprint hasn't been removed!"));
		}
	}
}

#undef LOCTEXT_NAMESPACE
