﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameQuestType.h"
#include "Components/ActorComponent.h"
#include "GameQuestComponent.generated.h"

class UGameQuestGraphBase;

UCLASS(ClassGroup=(Game), meta=(BlueprintSpawnableComponent))
class GAMEQUESTGRAPH_API UGameQuestComponent : public UActorComponent
{
	GENERATED_BODY()

	friend UGameQuestGraphBase;
public:
	UGameQuestComponent();

	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	void Activate(bool bReset) override;
	void Deactivate() override;
	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

	TSet<TObjectPtr<UGameQuestGraphBase>> PreActivatedQuests;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameQuest", ReplicatedUsing = OnRep_ActivatedQuests)
	TArray<TObjectPtr<UGameQuestGraphBase>> ActivatedQuests;
	UFUNCTION()
	void OnRep_ActivatedQuests();

	TSet<TObjectPtr<UGameQuestGraphBase>> PreFinishedQuests;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameQuest", ReplicatedUsing = OnRep_FinishedQuests)
	TArray<TObjectPtr<UGameQuestGraphBase>> FinishedQuests;
	UFUNCTION()
	void OnRep_FinishedQuests();
private:
	void PostStartQuest(UGameQuestGraphBase* StartedQuest);
	void PostFinishQuest(UGameQuestGraphBase* FinishedQuest);
protected:
	virtual void WhenQuestStarted(UGameQuestGraphBase* FinishedQuest) {}
	virtual void WhenQuestFinished(UGameQuestGraphBase* FinishedQuest) {}

	virtual void WhenQuestActivated(UGameQuestGraphBase* Quest) { OnQuestActivated.Broadcast(this, Quest); }
	virtual void WhenQuestDeactivated(UGameQuestGraphBase* Quest) { OnQuestDeactivated.Broadcast(this, Quest); }
	virtual void WhenFinishedQuestAdded(UGameQuestGraphBase* Quest) { OnFinishedQuestAdded.Broadcast(this, Quest); }
	virtual void WhenFinishedQuestRemoved(UGameQuestGraphBase* Quest) { OnFinishedQuestRemoved.Broadcast(this, Quest); }

	virtual void WhenPreSequenceActivated(UGameQuestGraphBase* MainQuest, UGameQuestGraphBase* OwnerQuest, FGameQuestSequenceBase* Sequence) { OnPreSequenceActivated.Broadcast(this, MainQuest, OwnerQuest, FGameQuestSequencePtr{ *Sequence }); }
	virtual void WhenPostSequenceDeactivated(UGameQuestGraphBase* MainQuest, UGameQuestGraphBase* OwnerQuest, FGameQuestSequenceBase* Sequence) { OnPostSequenceDeactivated.Broadcast(this, MainQuest, OwnerQuest, FGameQuestSequencePtr{ *Sequence }); }
public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "GameQuest")
	void AddQuest(UGameQuestGraphBase* Quest, bool AutoActivate = true);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "GameQuest")
	void RemoveQuest(UGameQuestGraphBase* Quest);

	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FOnQuestActivated, UGameQuestComponent, OnQuestActivated, UGameQuestComponent*, QuestComponent, UGameQuestGraphBase*, Quest);
	UPROPERTY(BlueprintAssignable, Transient, Category = "GameQuest")
	FOnQuestActivated OnQuestActivated;
	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FOnQuestDeactivated, UGameQuestComponent, OnQuestDeactivated, UGameQuestComponent*, QuestComponent, UGameQuestGraphBase*, Quest);
	UPROPERTY(BlueprintAssignable, Transient, Category = "GameQuest")
	FOnQuestDeactivated OnQuestDeactivated;
	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FOnFinishedQuestAdded, UGameQuestComponent, OnFinishedQuestAdded, UGameQuestComponent*, QuestComponent, UGameQuestGraphBase*, Quest);
	UPROPERTY(BlueprintAssignable, Transient, Category = "GameQuest")
	FOnFinishedQuestAdded OnFinishedQuestAdded;
	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FOnFinishedQuestRemoved, UGameQuestComponent, OnFinishedQuestRemoved, UGameQuestComponent*, QuestComponent, UGameQuestGraphBase*, Quest);
	UPROPERTY(BlueprintAssignable, Transient, Category = "GameQuest")
	FOnFinishedQuestRemoved OnFinishedQuestRemoved;

	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_FourParams(FOnPreSequenceActivated, UGameQuestComponent, OnPreSequenceActivated, UGameQuestComponent*, QuestComponent, UGameQuestGraphBase*, MainQuest, UGameQuestGraphBase*, OwnerQuest, const FGameQuestSequencePtr&, Sequence);
	UPROPERTY(BlueprintAssignable, Transient, Category = "GameQuest")
	FOnPreSequenceActivated OnPreSequenceActivated;
	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_FourParams(FOnPostSequenceDeactivated, UGameQuestComponent, OnPostSequenceDeactivated, UGameQuestComponent*, QuestComponent, UGameQuestGraphBase*, MainQuest, UGameQuestGraphBase*, OwnerQuest, const FGameQuestSequencePtr&, Sequence);
	UPROPERTY(BlueprintAssignable, Transient, Category = "GameQuest")
	FOnPostSequenceDeactivated OnPostSequenceDeactivated;
};
