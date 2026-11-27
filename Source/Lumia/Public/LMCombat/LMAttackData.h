#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "LMAttackData.generated.h"

/** 공격 종류별 기본 피해와 검 판정 설정을 보관한다. 실행 중 결과는 저장하지 않는다.*/
UCLASS(BlueprintType)
class LUMIA_API ULMAttackData : public UDataAsset
{
	GENERATED_BODY()
	
public:
	/** 추가 계산 전의 기본 피해 입력이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Damage", meta = (ClampMin = "0.0", ClampMax = "100000.0"))
	float BaseDamage = 10.0f;

	// 각성 이벤트 엘리트만 사용하는 기본 피해다. 0이면 일반 피해를 유지한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Damage", meta = (ClampMin = "0.0", ClampMax = "100000.0"))
	float EventDamage = 0.0f;

	// 장면 전용 공격 등은 누적 확률에 참여하지 않도록 끌 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Damage")
	bool bAllowCritical = true;

	// 미들 주먹만 기존 타격창 시작 시 공용 공격 효과를 재생한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Effects")
	bool bPlayStrikeEffect = false;

	// 실제 유효 피해가 발생한 타격의 검 게이지 보상이며 피해량과 독립적이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Special", meta = (ClampMin = "0.0"))
	float SpecialChargeGain = 0.0f;
	
	/** 플레이어 몸체 Mesh에 만든 검 시작점 소켓이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LM|Trace")
	FName TraceStartSocket = TEXT("LMTraceStart");
	
	/** 플레이어 몸체 Mesh에 만든 검 끝점 소켓이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Trace")
	FName TraceEndSocket = TEXT("LMTraceEnd");
	
	/** 검 길이를 덮는 구체 판정의 반경이며 단위는 cm이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Trace", meta = (ClampMin = "1.0"))
	float TraceRadius = 12.0f;
	
	/** 에디터에서 LMCombatHit Trace Channel을 선택한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;
	
	/** 검 위치와 판정 범위를 확인할 때 사용하는 임시 표시다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Trace")
	bool bDrawDebug = false;
};
