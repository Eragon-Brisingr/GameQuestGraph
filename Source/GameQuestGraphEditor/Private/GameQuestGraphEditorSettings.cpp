// Fill out your copyright notice in the Description page of Project Settings.


#include "GameQuestGraphEditorSettings.h"

#include "GameQuestElementBase.h"
#include "GameQuestSequenceBase.h"

UGameQuestGraphEditorSettings::UGameQuestGraphEditorSettings()
{
	SequenceSingleType = FGameQuestSequenceSingle::StaticStruct();
	SequenceListType = FGameQuestSequenceList::StaticStruct();
	SequenceBranchType = FGameQuestSequenceBranch::StaticStruct();
	SequenceSubQuestType = FGameQuestSequenceSubQuest::StaticStruct();
	ElementScriptType = FGameQuestElementScript::StaticStruct();
}
