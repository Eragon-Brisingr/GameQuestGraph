// Fill out your copyright notice in the Description page of Project Settings.


#include "GameQuestBpNodeDetails.h"

#include "BPNode_GameQuestElementBase.h"
#include "BPNode_GameQuestNodeBase.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "GameQuestElementBase.h"
#include "InstancedStructDetails.h"
#include "ObjectEditorUtils.h"
#include "Widgets/Input/SCheckBox.h"

#define LOCTEXT_NAMESPACE "GameQuestGraphEditor"

void CreateExposePropertyWidget(FProperty* TargetProperty, TSharedRef<IPropertyHandle> TargetPropertyHandle, IDetailPropertyRow& PropertyRow, TSharedRef<IPropertyHandle> ShowHidePropertyHandle)
{
	TSharedPtr<SWidget> NameWidget;
	TSharedPtr<SWidget> ValueWidget;
	FDetailWidgetRow Row;
	static const FName MD_NoResetToDefault = TEXT("NoResetToDefault");
	TargetProperty->SetMetaData(MD_NoResetToDefault, TEXT("True"));
	PropertyRow.GetDefaultWidgets(NameWidget, ValueWidget, Row);
	TargetProperty->RemoveMetaData(MD_NoResetToDefault);
	if (TargetProperty->HasMetaData(TEXT("EditCondition")))
	{
		NameWidget = TargetPropertyHandle->CreatePropertyNameWidget();
	}

	ShowHidePropertyHandle->MarkHiddenByCustomization();

	auto GetVisibilityOfProperty = [Handle = ShowHidePropertyHandle]()
	{
		bool bShowAsPin;
		if (FPropertyAccess::Success == Handle->GetValue(/*out*/ bShowAsPin))
		{
			return bShowAsPin ? EVisibility::Hidden : EVisibility::Visible;
		}
		else
		{
			return EVisibility::Visible;
		}
	};

	ValueWidget->SetVisibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda(GetVisibilityOfProperty)));

	class SActionShowPinWidget : public SCompoundWidget
	{
		SLATE_BEGIN_ARGS(SActionShowPinWidget) {}

		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<IPropertyHandle>& InPropertyHandle)
		{
			PropertyHandle = InPropertyHandle;

			TSharedRef<SCheckBox> CheckBox = SNew(SCheckBox)
				.ToolTipText(LOCTEXT("AsPinEnableTooltip", "Show/hide the pins of this property on the node"))
				.IsChecked_Lambda([this]()
				{
					bool bValue;
					const FPropertyAccess::Result Result = PropertyHandle->GetValue(bValue);
					if(Result == FPropertyAccess::MultipleValues)
					{
						return ECheckBoxState::Undetermined;
					}
					return bValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([this](ECheckBoxState InCheckBoxState)
				{
					const bool bValue = InCheckBoxState == ECheckBoxState::Checked;
					PropertyHandle->SetValue(bValue);
				});

			ChildSlot
			[
				CheckBox
			];
		}

		TSharedPtr<IPropertyHandle> PropertyHandle;
	};

	// we only show children if visibility is one
	// whenever toggles, this gets called, so it will be refreshed
	const bool bShowChildren = GetVisibilityOfProperty() == EVisibility::Visible;
	PropertyRow.CustomWidget(bShowChildren)
	.NameContent()
	.MinDesiredWidth(Row.NameWidget.MinWidth)
	.MaxDesiredWidth(Row.NameWidget.MaxWidth)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SActionShowPinWidget, ShowHidePropertyHandle)
		]
		+ SHorizontalBox::Slot()
		.Padding(4.f, 0.f)
		.AutoWidth()
		[
			NameWidget.ToSharedRef()
		]
	]
	.ValueContent()
	.MinDesiredWidth(Row.ValueWidget.MinWidth)
	.MaxDesiredWidth(Row.ValueWidget.MaxWidth)
	[
		ValueWidget.ToSharedRef()
	];
}

void BuildPinOptionalDetails(IDetailLayoutBuilder& DetailBuilder, UStruct* Class, UClass* NodeClass, const TArray<FOptionalPinFromProperty>& ShowPinOptions, const FString& ShowPinPropertyName)
{
	for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		const FProperty* InstanceProperty = *It;

		if (InstanceProperty->HasAnyPropertyFlags(CPF_DisableEditOnInstance) || InstanceProperty->HasAllPropertyFlags(CPF_Edit) == false)
		{
			DetailBuilder.HideProperty(InstanceProperty->GetFName(), InstanceProperty->GetOwnerClass());
			continue;
		}

		const FName PropertyName = InstanceProperty->GetFName();
		const int32 CustomPinIndex = ShowPinOptions.IndexOfByPredicate([&](const FOptionalPinFromProperty& InOptionalPin)
		{
			return PropertyName == InOptionalPin.PropertyName;
		});
		if (CustomPinIndex == INDEX_NONE)
		{
			continue;
		}

		TSharedRef<IPropertyHandle> TargetPropertyHandle = DetailBuilder.GetProperty(InstanceProperty->GetFName(), InstanceProperty->GetOwnerStruct());
		FProperty* TargetProperty = TargetPropertyHandle->GetProperty();
		if (InstanceProperty != TargetProperty)
		{
			continue;
		}

		IDetailCategoryBuilder& CurrentCategory = DetailBuilder.EditCategory(FObjectEditorUtils::GetCategoryFName(TargetProperty));
		const FOptionalPinFromProperty& OptionalPin = ShowPinOptions[CustomPinIndex];

		if (OptionalPin.bCanToggleVisibility)
		{
			if (!TargetPropertyHandle->GetProperty())
			{
				continue;
			}

			if (TargetPropertyHandle->IsCustomized())
			{
				continue;
			}

			if (OptionalPin.bPropertyIsCustomized)
			{
				continue;
			}

			IDetailPropertyRow& PropertyRow = CurrentCategory.AddProperty(TargetPropertyHandle, TargetProperty->HasAnyPropertyFlags(CPF_AdvancedDisplay) ? EPropertyLocation::Advanced : EPropertyLocation::Default);

			const FName OptionalPinArrayEntryName(*FString::Printf(TEXT("%s[%d].bShowPin"), *ShowPinPropertyName, CustomPinIndex));
			const TSharedRef<IPropertyHandle> ShowHidePropertyHandle = DetailBuilder.GetProperty(OptionalPinArrayEntryName, NodeClass);
			CreateExposePropertyWidget(TargetProperty, TargetPropertyHandle, PropertyRow, ShowHidePropertyHandle);
		}
	}
}

UBPNode_GameQuestNodeBase* GetFirstNode(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> SelectedObjectsList;
	DetailBuilder.GetObjectsBeingCustomized(SelectedObjectsList);

	// get first Quest nodes
	UBPNode_GameQuestNodeBase* QuestBPNode = Cast<UBPNode_GameQuestNodeBase>(SelectedObjectsList[0].Get());
	if (QuestBPNode == nullptr)
	{
		return nullptr;
	}

	// make sure type matches with all the nodes.
	UBPNode_GameQuestNodeBase* FirstNodeType = QuestBPNode;
	for (int32 Index = 1; Index < SelectedObjectsList.Num(); ++Index)
	{
		UBPNode_GameQuestNodeBase* CurrentNode = Cast<UBPNode_GameQuestNodeBase>(SelectedObjectsList[Index].Get());
		if (!CurrentNode || CurrentNode->GetClass() != FirstNodeType->GetClass())
		{
			// if type mismatches, multi selection doesn't work, just return
			return nullptr;
		}
	}

	return FirstNodeType;
}

void FGameQuestBpNodeDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Hide the pin options property; it's represented inline per-property instead
	DetailBuilder.HideCategory(TEXT("PinOptions"));

	UBPNode_GameQuestNodeBase* QuestBPNode = GetFirstNode(DetailBuilder);
	if (QuestBPNode == nullptr)
	{
		return;
	}

	if (QuestBPNode->StructNodeInstance.IsValid())
	{
		TSharedRef<IPropertyHandle> QuestStructProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBPNode_GameQuestNodeBase, StructNodeInstance), UBPNode_GameQuestNodeBase::StaticClass());
		DetailBuilder.HideProperty(QuestStructProperty);
		IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory(FObjectEditorUtils::GetCategoryFName(QuestStructProperty->GetProperty()));

		class FQuestStructInstanceDetails : public FInstancedStructDataDetails
		{
			using Super = FInstancedStructDataDetails;
		public:
			FQuestStructInstanceDetails(TSharedPtr<IPropertyHandle> InStructProperty, UBPNode_GameQuestNodeBase* QuestBPNode, IDetailLayoutBuilder& DetailBuilder)
				: Super(InStructProperty)
				, QuestBPNode(QuestBPNode)
				, DetailBuilder(DetailBuilder)
			{
			}

			void OnChildRowAdded(IDetailPropertyRow& ChildRow) override
			{
				const TSharedRef<IPropertyHandle> PropertyHandle = ChildRow.GetPropertyHandle().ToSharedRef();
				FProperty* Property = PropertyHandle->GetProperty();
				const int32 CustomPinIndex = QuestBPNode->ShowPinForProperties.IndexOfByPredicate([&](const FOptionalPinFromProperty& InOptionalPin)
				{
					return Property->GetFName() == InOptionalPin.PropertyName;
				});
				if (CustomPinIndex == INDEX_NONE)
				{
					return;
				}
				const FName OptionalPinArrayEntryName(*FString::Printf(TEXT("ShowPinForProperties[%d].bShowPin"), CustomPinIndex));
				const TSharedRef<IPropertyHandle> ShowHidePropertyHandle = DetailBuilder.GetProperty(OptionalPinArrayEntryName, UBPNode_GameQuestNodeBase::StaticClass());
				CreateExposePropertyWidget(Property, PropertyHandle, ChildRow, ShowHidePropertyHandle);
			}

			UBPNode_GameQuestNodeBase* QuestBPNode;
			IDetailLayoutBuilder& DetailBuilder;
		};
		CategoryBuilder.AddCustomBuilder(MakeShared<FQuestStructInstanceDetails>(QuestStructProperty, QuestBPNode, DetailBuilder));
	}
}

void FGameQuestBpNodeScriptElementDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	if (DetailBuilder.HasClassDefaultObject())
	{
		DetailBuilder.HideCategory(TEXT("Settings"));
	}

	const UBPNode_GameQuestElementScript* ElementScriptNode = Cast<UBPNode_GameQuestElementScript>(GetFirstNode(DetailBuilder));
	if (ElementScriptNode == nullptr || ElementScriptNode->ScriptInstance == nullptr)
	{
		return;
	}
	BuildPinOptionalDetails(DetailBuilder, ElementScriptNode->ScriptInstance->GetClass(), UBPNode_GameQuestElementScript::StaticClass(), ElementScriptNode->ShowPinForScript, GET_MEMBER_NAME_STRING_CHECKED(UBPNode_GameQuestElementScript, ShowPinForScript));
}

#undef LOCTEXT_NAMESPACE
