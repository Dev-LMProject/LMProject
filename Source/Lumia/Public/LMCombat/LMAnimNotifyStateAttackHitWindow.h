// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "LMAnimNotifyStateAttackHitWindow.generated.h"

class ULMAttackData;

/** 공격 몽타주의 타격 시작·종료를 알리며 실행 중 판정 정보는 Task가 관리한다. */
UCLASS(meta = (DisplayName = "LM Attack Hit Window"))
class LUMIA_API ULMAnimNotifyStateAttackHitWindow : public UAnimNotifyState
{
	GENERATED_BODY()
	
public:
	/** 몽타주 실행 정보를 받을 수 있도록 Branching Point 방식으로 설정한다. */
	ULMAnimNotifyStateAttackHitWindow();
	
	/** 타격 허용 구간이 시작되면 공격자에게 시작 이벤트를 보낸다. */
	virtual void BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload) override;
	
	/** 정상 종료와 중단을 구분하여 공격자에게 종료 이벤트를 보낸다. */
	virtual void BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload) override;
	
	/** 이 타격 구간만 다른 공격 데이터를 사용할 때 지정한다. 비워 두면 공격 GA의 기본 데이터를 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Attack")
	TObjectPtr<ULMAttackData> AttackDataOverride;
	
private:
	/** 몽타주와 이번 재생의 식별 정보를 GameplayEvent에 담아 전달한다. */
	void SendHitWindowEvent(const FBranchingPointNotifyPayload& BranchingPointPayload, FGameplayTag EventTag) const;
};
