// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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
	void StartQuest(UGameQuestGraphBase* StartedQuest);
	void PostFinishQuest(UGameQuestGraphBase* FinishedQuest);
protected:
	virtual void WhenQuestStarted(UGameQuestGraphBase* FinishedQuest) {}
	virtual void WhenQuestFinished(UGameQuestGraphBase* FinishedQuest) {}

	virtual void WhenQuestActivated(UGameQuestGraphBase* Quest) {}
	virtual void WhenQuestDeactivated(UGameQuestGraphBase* Quest) {}
	virtual void WhenFinishedQuestAdded(UGameQuestGraphBase* Quest) {}
	virtual void WhenFinishedQuestRemoved(UGameQuestGraphBase* Quest) {}
public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void AddQuest(UGameQuestGraphBase* Quest, bool AutoActivate = true);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void RemoveQuest(UGameQuestGraphBase* Quest);
};
