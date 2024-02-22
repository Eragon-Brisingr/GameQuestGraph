// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

class ISlateStyle;

namespace GameQuestGraphEditorStyle
{
	namespace ElementContentBorder
	{
		const FLinearColor Default(0.2f, 0.2f, 0.2f, 1.f);
	}

	namespace ElementBorder
	{
		const FLinearColor Default(0.2f, 0.2f, 0.2f, 0.2f);
		const FLinearColor Selected(1.00f, 0.08f, 0.08f);
	}

	namespace SequenceBranch
	{
		const FLinearColor Pin(0.4f, 0.2f, 0.2f);
	}

	namespace Debug
	{
		const FLinearColor Activated(0.1f, 0.9f, 0.1f, 1.f);
		const FLinearColor Deactivated(0.1f, 0.1f, 0.1f, 1.f);
		const FLinearColor Finished(0.1f, 0.1f, 0.9f, 1.f);
		const FLinearColor Interrupted(0.9f, 0.1f, 0.1f, 1.f);
	}
};

class FGameQuestGraphSlateStyle
{
public:
	static const ISlateStyle& Get();

	static void StartupModule();
private:
	FGameQuestGraphSlateStyle();
	~FGameQuestGraphSlateStyle();

	TSharedRef<FSlateStyleSet> Style;
};
