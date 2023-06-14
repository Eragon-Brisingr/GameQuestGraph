// Fill out your copyright notice in the Description page of Project Settings.


#include "GameQuestFunctionLibrary.h"

#include "GameQuestGraphBase.h"

DEFINE_FUNCTION(UGameQuestFunctionLibrary::execQuestSequenceToPtr)
{
	Stack.StepCompiledIn<FProperty>(nullptr);
	void* SrcPropertyAddr = Stack.MostRecentPropertyAddress;
	FProperty* MostRecentProperty = Stack.MostRecentProperty;
	P_FINISH;

	P_NATIVE_BEGIN;
	const FStructProperty* StructProperty = CastField<FStructProperty>(MostRecentProperty);
	if (ensure(StructProperty && StructProperty->Struct->IsChildOf(FGameQuestSequenceBase::StaticStruct())))
	{
		*(FGameQuestSequencePtr*)RESULT_PARAM = FGameQuestSequencePtr{ *static_cast<FGameQuestSequenceBase*>(SrcPropertyAddr) };
	}
	P_NATIVE_END;
}

DEFINE_FUNCTION(UGameQuestFunctionLibrary::execQuestElementToPtr)
{
	Stack.StepCompiledIn<FProperty>(nullptr);
	void* SrcPropertyAddr = Stack.MostRecentPropertyAddress;
	FProperty* MostRecentProperty = Stack.MostRecentProperty;
	P_FINISH;

	P_NATIVE_BEGIN;
	const FStructProperty* StructProperty = CastField<FStructProperty>(MostRecentProperty);
	if (ensure(StructProperty && StructProperty->Struct->IsChildOf(FGameQuestElementBase::StaticStruct())))
	{
		*(FGameQuestElementPtr*)RESULT_PARAM = FGameQuestElementPtr{ *static_cast<FGameQuestElementBase*>(SrcPropertyAddr) };
	}
	P_NATIVE_END;
}

UGameQuestElementScriptable* UGameQuestFunctionLibrary::GetQuestElementScriptInstance(const FGameQuestElementPtr& Element)
{
	if (Element)
	{
		const FGameQuestElementScript* ElementScript = GameQuestCast<FGameQuestElementScript>(*Element);
		return ElementScript ? ElementScript->Instance : nullptr;
	}
	return nullptr;
}
