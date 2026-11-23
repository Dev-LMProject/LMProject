// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "LMANSDodgeParry.generated.h"

// 몽타주의 회피 또는 패링 구간 경계를 해당 재생을 맡은 GA에 전달한다.
UCLASS(meta = (DisplayName = "LM Dodge Parry Window"))
class LUMIA_API ULMANSDodgeParry : public UAnimNotifyState
{
	GENERATED_BODY()
public:
	// 몽타주 진행 중 재생 ID와 함께 정확한 구간 경계를 받도록 설정한다.
	ULMANSDodgeParry();

	// 이 몽타주 배치의 판정 구간 시작을 전달한다.
	virtual void BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload) override;

	// 이 몽타주 배치의 판정 구간 끝을 전달한다.
	virtual void BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload) override;

private:
	// 알림을 보낸 몸체의 회피 또는 공격 GA로 경계를 보낸다.
	void UpdateWindow(const FBranchingPointNotifyPayload& Payload, bool bOpen) const;
};
