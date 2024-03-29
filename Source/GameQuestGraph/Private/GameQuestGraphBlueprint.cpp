﻿// Fill out your copyright notice in the Description page of Project Settings.


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
#endif

void UGameQuestGraphGeneratedClass::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << NodeIdLogicsMap;
	Ar << NodeToSuccessorMap;
	Ar << NodeIdEventNameMap;
	Ar << RerouteTagPreNodesMap;
}

void UGameQuestGraphGeneratedClass::PostQuestCDOInitProperties()
{
	NodeIdPropertyMap.Empty();
	RerouteTags.Empty();
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
		else if (It->Struct->IsChildOf(FGameQuestRerouteTag::StaticStruct()))
		{
			FString RerouteTagName = It->GetName();
			if (ensure(RerouteTagName.RemoveFromStart(TEXT("__GQRT_"))))
			{
				RerouteTags.Add(*RerouteTagName, *It);
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
