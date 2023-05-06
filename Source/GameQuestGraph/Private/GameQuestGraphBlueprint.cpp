// Fill out your copyright notice in the Description page of Project Settings.


#include "GameQuestGraphBlueprint.h"

#include "GameQuestGraphBase.h"
#include "GameQuestNodeBase.h"

#if WITH_EDITOR
UClass* UGameQuestGraphBlueprint::GetBlueprintClass() const
{
	return UGameQuestGraphGeneratedClass::StaticClass();
}

void UGameQuestGraphBlueprint::GetReparentingRules(TSet<const UClass*>& AllowedChildrenOfClasses, TSet<const UClass*>& DisallowedChildrenOfClasses) const
{
	Super::GetReparentingRules(AllowedChildrenOfClasses, DisallowedChildrenOfClasses);

	AllowedChildrenOfClasses.Add(UGameQuestGraphBase::StaticClass());
}

void UGameQuestGraphBlueprint::GatherDependencies(TSet<TWeakObjectPtr<UBlueprint>>& InDependencies) const
{
	Super::GatherDependencies(InDependencies);

	const UGameQuestGraphGeneratedClass* Class = Cast<UGameQuestGraphGeneratedClass>(GeneratedClass);
	if (Class == nullptr)
	{
		return;
	}
	const UGameQuestGraphBase* CDO = Class->GetDefaultObject<UGameQuestGraphBase>();
	for (const auto& [_, Property] : Class->NodeIdPropertyMap)
	{
		const FGameQuestNodeBase* QuestNode = Property->ContainerPtrToValuePtr<FGameQuestNodeBase>(CDO);
		QuestNode->GatherDependencies(InDependencies);
	}
}
#endif

void UGameQuestGraphGeneratedClass::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << NodeIdLogicsMap;
	Ar << NodeToSuccessorMap;
	Ar << NodeIdEventNameMap;
}

void UGameQuestGraphGeneratedClass::PostQuestCDOInitProperties()
{
	NodeIdPropertyMap.Empty();
	FinishedTags.Empty();
	for (TFieldIterator<FStructProperty> It{ this }; It; ++It)
	{
		if (It->Struct->IsChildOf(FGameQuestNodeBase::StaticStruct()))
		{
			const uint16* NodeIndex = NodeNameIdMap.Find(It->GetFName());
			if (NodeIndex == nullptr)
			{
				continue;
			}
			NodeIdPropertyMap.Add(*NodeIndex, *It);
		}
		else if (It->Struct->IsChildOf(FGameQuestFinishedTag::StaticStruct()))
		{
			FString FinishedTagName = It->GetName();
			if (ensure(FinishedTagName.RemoveFromStart(TEXT("__GQFT_"))))
			{
				FinishedTags.Add(*FinishedTagName, *It);
			}
		}
	}
	NodeToPredecessorMap.Empty();
	for (const auto& [FromNode, ToNodes] : NodeToSuccessorMap)
	{
		for (uint16 ToNode : ToNodes)
		{
			NodeToPredecessorMap.FindOrAdd(ToNode).Add(FromNode);
		}
	}
}
