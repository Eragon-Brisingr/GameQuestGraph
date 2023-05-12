// Fill out your copyright notice in the Description page of Project Settings.


#include "GameQuestPrivateVisitor.h"

#include "GameQuestGraphBase.h"
#include "GameQuestSequenceBase.h"

bool FGameQuestPrivateVisitor::TryStartGameQuest(UGameQuestGraphBase& Quest)
{
	return Quest.TryStartGameQuest();
}

void FGameQuestPrivateVisitor::PostExecuteEntryEvent(UGameQuestGraphBase& Quest)
{
	Quest.PostExecuteEntryEvent();
}

void FGameQuestPrivateVisitor::TryActivateSequence(FGameQuestSequenceBase& Sequence)
{
	Sequence.TryActivateSequence();
}
