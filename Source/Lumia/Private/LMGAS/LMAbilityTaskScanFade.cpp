// Fill out your copyright notice in the Description page of Project Settings.


#include "LMGAS/LMAbilityTaskScanFade.h"

ULMAbilityTaskScanFade::ULMAbilityTaskScanFade(const FObjectInitializer& ObjectInitializer)
{
	bTickingTask=true;
}

ULMAbilityTaskScanFade* ULMAbilityTaskScanFade::CreateScanFade(UGameplayAbility* OwningAbility, float Duration)
{
	ULMAbilityTaskScanFade* Task =NewAbilityTask<ULMAbilityTaskScanFade>(OwningAbility);

	Task->DurationSec = Duration;
	return Task;
}

void ULMAbilityTaskScanFade::Activate()
{
	Super::Activate();
	if (!FMath::IsFinite(DurationSec)|| DurationSec <= 0.f)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnFinished.Broadcast(0.f);
		}
		EndTask();
		return;
	}
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnUpdate.Broadcast(1.f);
	}
}

void ULMAbilityTaskScanFade::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);
	if (!ShouldBroadcastAbilityTaskDelegates())
	{
		EndTask();
		return;
	}
	ElapsedSec+=DeltaTime;
	
	const float Progress=FMath::Clamp(ElapsedSec/DurationSec,0.f,1.f);
	const float Alpha=1.f-Progress;
	OnUpdate.Broadcast(Alpha);
	
	if (IsFinished())
	{
		return;
	}
	if (ElapsedSec>=DurationSec)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnFinished.Broadcast(0.f);
		}
		EndTask();
	}
}
