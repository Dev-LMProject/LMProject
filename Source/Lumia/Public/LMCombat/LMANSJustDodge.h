// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "LMANSJustDodge.generated.h"

/**
 * 
 */
UCLASS(meta = (DisplayName = "LM Just Dodge Window"))
class LUMIA_API ULMANSJustDodge : public UAnimNotifyState
{
	GENERATED_BODY()
public:
	// 몽타주 진행 중 재생 ID와 함께 정확한 구간 경계를 받도록 설정한다.
	ULMANSJustDodge();

	// 이번 회피에서만 저스트 회피가 될 수 있는 구간을 연다.
	virtual void BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload) override;

	// 해당 몽타주에서 열었던 저스트 회피 구간을 닫는다.
	virtual void BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload) override;

private:
	// 알림을 보낸 캐릭터의 활성 회피 GA에 경계를 전달한다.
	void UpdateDodgeWindow(const FBranchingPointNotifyPayload& Payload, bool bOpen) const;
};
