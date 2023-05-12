// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class UGameQuestGraphBase;
struct FGameQuestSequenceBase;

struct GAMEQUESTGRAPH_API FGameQuestPrivateVisitor
{
	static bool TryStartGameQuest(UGameQuestGraphBase& Quest);
	static void PostExecuteEntryEvent(UGameQuestGraphBase& Quest);
	static void TryActivateSequence(FGameQuestSequenceBase& Sequence);
};
