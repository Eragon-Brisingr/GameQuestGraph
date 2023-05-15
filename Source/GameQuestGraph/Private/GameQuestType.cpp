// Fill out your copyright notice in the Description page of Project Settings.


#include "GameQuestType.h"

#include "GameQuestElementBase.h"
#include "GameQuestGraphBase.h"
#include "GameQuestSequenceBase.h"

DEFINE_LOG_CATEGORY(LogGameQuest);

const FName FGameQuestRerouteTag::FinishCompletedTagName = TEXT("FinishCompleted");

FGameQuestSequencePtr::FGameQuestSequencePtr(FGameQuestSequenceBase& Sequence)
	: SequencePtr(&Sequence)
{
	check(Sequence.OwnerQuest->GetSequencePtr(Sequence.OwnerQuest->GetSequenceId(SequencePtr)) == SequencePtr);
	Owner = Sequence.OwnerQuest;
}

FGameQuestElementPtr::FGameQuestElementPtr(FGameQuestElementBase& Element)
	: ElementPtr(&Element)
{
	check(Element.OwnerQuest->GetElementPtr(Element.OwnerQuest->GetElementId(ElementPtr)) == ElementPtr);
	Owner = Element.OwnerQuest;
}
