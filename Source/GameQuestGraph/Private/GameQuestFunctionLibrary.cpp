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

bool UGameQuestFunctionLibrary::IsQuestSequenceBranchesActivate(const FGameQuestSequencePtr& Sequence)
{
	if (const FGameQuestSequenceBranch* Branch = GameQuestCast<FGameQuestSequenceBranch>(*Sequence))
	{
		return Branch->bIsBranchesActivated;
	}
	return false;
}

bool UGameQuestFunctionLibrary::IsQuestSequenceBranchInterrupted(const FGameQuestSequencePtr& Sequence, int32 BranchIndex)
{
	if (const FGameQuestSequenceBranch* Branch = GameQuestCast<FGameQuestSequenceBranch>(*Sequence))
	{
		if (Branch->Branches.IsValidIndex(BranchIndex))
		{
			return Branch->Branches[BranchIndex].bInterrupted;
		}
	}
	return false;
}

TArray<FGameQuestElementPtr> UGameQuestFunctionLibrary::GetQuestSequenceListElements(const FGameQuestSequencePtr& Sequence, TArray<EGameQuestSequenceLogic>& Logics)
{
	if (!Sequence)
	{
		return {};
	}
	const UGameQuestGraphBase* Quest = Sequence->OwnerQuest;
	if (const FGameQuestSequenceSingle* Single = GameQuestCast<FGameQuestSequenceSingle>(*Sequence))
	{
		FGameQuestElementBase* Element = Quest->GetElementPtr(Single->Element);
		Logics.Empty();
		return { FGameQuestElementPtr{ *Element } };
	}
	if (const FGameQuestSequenceList* List = GameQuestCast<FGameQuestSequenceList>(*Sequence))
	{
		TArray<FGameQuestElementPtr> Res;
		Logics = Quest->GetLogicList(List);
		for (const uint16 ElementId : List->Elements)
		{
			FGameQuestElementBase* Element = Quest->GetElementPtr(ElementId);
			Res.Add(*Element);
		}
		return Res;
	}
	if (const FGameQuestSequenceBranch* Branch = GameQuestCast<FGameQuestSequenceBranch>(*Sequence))
	{
		TArray<FGameQuestElementPtr> Res;
		Logics = Quest->GetLogicList(Branch);
		for (const uint16 ElementId : Branch->Elements)
		{
			FGameQuestElementBase* Element = Quest->GetElementPtr(ElementId);
			Res.Add(*Element);
		}
		return Res;
	}
	return {};
}

TArray<FGameQuestElementPtr> UGameQuestFunctionLibrary::GetQuestSequenceBranchElements(const FGameQuestSequencePtr& Sequence)
{
	if (!Sequence)
	{
		return {};
	}
	if (const FGameQuestSequenceBranch* Branch = GameQuestCast<FGameQuestSequenceBranch>(*Sequence))
	{
		TArray<FGameQuestElementPtr> Res;
		const UGameQuestGraphBase* Quest = Sequence->OwnerQuest;
		for (const FGameQuestSequenceBranchElement& BranchElement : Branch->Branches)
		{
			FGameQuestElementBase* Element = Quest->GetElementPtr(BranchElement.Element);
			Res.Add(*Element);
		}
		return Res;
	}
	return {};
}

TArray<FGameQuestSequencePtr> UGameQuestFunctionLibrary::GetQuestSequenceNextSequence(const FGameQuestSequencePtr& Sequence)
{
	if (!Sequence)
	{
		return {};
	}
	const UGameQuestGraphBase* Quest = Sequence->OwnerQuest;
	if (const FGameQuestSequenceSingle* Single = GameQuestCast<FGameQuestSequenceSingle>(*Sequence))
	{
		TArray<FGameQuestSequencePtr> Res;
		for (const uint16 SequenceId : Single->NextSequences)
		{
			FGameQuestSequenceBase* NextSequence = Quest->GetSequencePtr(SequenceId);
			Res.Add(*NextSequence);
		}
		return Res;
	}
	if (const FGameQuestSequenceList* List = GameQuestCast<FGameQuestSequenceList>(*Sequence))
	{
		TArray<FGameQuestSequencePtr> Res;
		for (const uint16 SequenceId : List->NextSequences)
		{
			FGameQuestSequenceBase* NextSequence = Quest->GetSequencePtr(SequenceId);
			Res.Add(*NextSequence);
		}
		return Res;
	}
	if (const FGameQuestSequenceBranch* Branch = GameQuestCast<FGameQuestSequenceBranch>(*Sequence))
	{
		TArray<FGameQuestSequencePtr> Res;
		for (const FGameQuestSequenceBranchElement& BranchElement : Branch->Branches)
		{
			for (const uint16 SequenceId : BranchElement.NextSequences)
			{
				FGameQuestSequenceBase* NextSequence = Quest->GetSequencePtr(SequenceId);
				Res.Add(*NextSequence);
			}
		}
		return Res;
	}
	if (const FGameQuestSequenceSubQuest* SubQuest = GameQuestCast<FGameQuestSequenceSubQuest>(*Sequence))
	{
		TArray<FGameQuestSequencePtr> Res;
		const TArray<uint16> NextSequences{ SubQuest->GetNextSequences() };
		for (const uint16 SequenceId : NextSequences)
		{
			FGameQuestSequenceBase* NextSequence = Quest->GetSequencePtr(SequenceId);
			Res.Add(*NextSequence);
		}
		return Res;
	}
	return {};
}

TArray<FGameQuestSequencePtr> UGameQuestFunctionLibrary::GetQuestSequenceBranchNextSequence(const FGameQuestSequencePtr& Sequence, int32 BranchIndex)
{
	if (!Sequence)
	{
		return {};
	}
	if (const FGameQuestSequenceBranch* Branch = GameQuestCast<FGameQuestSequenceBranch>(*Sequence))
	{
		if (Branch->Branches.IsValidIndex(BranchIndex))
		{
			TArray<FGameQuestSequencePtr> Res;
			const UGameQuestGraphBase* Quest = Sequence->OwnerQuest;
			for (const uint16 SequenceId : Branch->Branches[BranchIndex].NextSequences)
			{
				FGameQuestSequenceBase* NextSequence = Quest->GetSequencePtr(SequenceId);
				Res.Add(*NextSequence);
			}
			return Res;
		}
	}
	return {};
}

TArray<FGameQuestElementPtr> UGameQuestFunctionLibrary::GetQuestElementBranchListElements(const FGameQuestElementPtr& Element, TArray<EGameQuestSequenceLogic>& Logics)
{
	if (!Element)
	{
		return {};
	}
	if (FGameQuestElementBranchList* BranchList = GameQuestCast<FGameQuestElementBranchList>(Element.ElementPtr))
	{
		TArray<FGameQuestElementPtr> Res;
		const UGameQuestGraphBase* Quest = Element->OwnerQuest;
		Logics = Quest->GetLogicList(BranchList);
		for (const uint16 ElementId : BranchList->Elements)
		{
			FGameQuestElementBase* ListElement = Quest->GetElementPtr(ElementId);
			Res.Add(*ListElement);
		}
		return Res;
	}
	return {};
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
