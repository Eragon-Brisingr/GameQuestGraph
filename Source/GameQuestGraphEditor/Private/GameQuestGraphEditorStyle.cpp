// Fill out your copyright notice in the Description page of Project Settings.


#include "GameQuestGraphEditorStyle.h"

#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

#define IMAGE_BRUSH(RelativePath, ...)	FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BOX_BRUSH(RelativePath, ...)	FSlateBoxBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BORDER_BRUSH(RelativePath, ...) FSlateBorderBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define TTF_FONT(RelativePath, ...)		FSlateFontInfo(Style->RootToContentDir(RelativePath, TEXT(".ttf")), __VA_ARGS__)
#define OTF_FONT(RelativePath, ...)		FSlateFontInfo(Style->RootToContentDir(RelativePath, TEXT(".otf")), __VA_ARGS__)

const ISlateStyle& FGameQuestGraphSlateStyle::Get()
{
	static FGameQuestGraphSlateStyle Style;
	return *Style.Style;
}

void FGameQuestGraphSlateStyle::StartupModule()
{
	Get();
}

FGameQuestGraphSlateStyle::FGameQuestGraphSlateStyle()
	: Style{ MakeShared<FSlateStyleSet>(TEXT("GameQuestGraphSlateStyle")) }
{
	Style->SetContentRoot(FPaths::ProjectPluginsDir() / "GameQuestGraph" / "Resources");

	FSlateBoxBrush* SlateBorderBrush = new BOX_BRUSH("ElementLogicAnd_16x", FMargin(0.45f, 0.45f));
	SlateBorderBrush->ImageSize = FVector2D(16.f, 16.f);
	Style->Set("ElementLogicAnd", SlateBorderBrush);
	SlateBorderBrush = new BOX_BRUSH("ElementLogicOr_16x", FMargin(0.45f, 0.45f));
	SlateBorderBrush->ImageSize = FVector2D(16.f, 16.f);
	Style->Set("ElementLogicOr", SlateBorderBrush);
	SlateBorderBrush = new BOX_BRUSH("ElementLogicTop_16x", FMargin(0.45f, 0.45f));
	SlateBorderBrush->ImageSize = FVector2D(16.f, 16.f);
	Style->Set("ElementLogicTop", SlateBorderBrush);
	SlateBorderBrush = new BOX_BRUSH("ElementLogicDown_16x", FMargin(0.45f, 0.45f));
	SlateBorderBrush->ImageSize = FVector2D(16.f, 16.f);
	Style->Set("ElementLogicDown", SlateBorderBrush);

	Style->Set("Border", new BOX_BRUSH("Border", FMargin(0.25f)));
	Style->Set("GroupBorder", new BOX_BRUSH("GroupBorder", FMargin(0.25f)));
	Style->Set("GroupBorderLight", new BOX_BRUSH("GroupBorderLight", FMargin(0.25f)));
	Style->Set("ThinLine.Horizontal", new IMAGE_BRUSH("ThinLine_Horizontal", FVector2D(11, 2), FLinearColor::White, ESlateBrushTileType::Horizontal));

	Style->Set("ClassThumbnail.GameQuestGraphBase", new IMAGE_BRUSH("GameQuestIcon_128x", FVector2D(128.f, 128.f)));
	Style->Set("ClassThumbnail.GameQuestElementScriptable", new IMAGE_BRUSH("GameQuestElementIcon_128x", FVector2D(128.f, 128.f)));

	Style->Set("Graph.Quest", new IMAGE_BRUSH("Quest_64x", FVector2D(16.f, 16.f)));
	Style->Set("Graph.Event.Start", new IMAGE_BRUSH("QuestStart_64x", FVector2D(16.f, 16.f)));
	Style->Set("Graph.Event.End", new IMAGE_BRUSH("QuestEnd_64x", FVector2D(16.f, 16.f)));
	Style->Set("Graph.Event.Interrupt", new IMAGE_BRUSH("QuestInterrupt_64x", FVector2D(16.f, 16.f)));

	FSlateStyleRegistry::RegisterSlateStyle(*Style);
}

FGameQuestGraphSlateStyle::~FGameQuestGraphSlateStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*Style);
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT
