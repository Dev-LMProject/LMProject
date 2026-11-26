#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "LMAttackData.generated.h"

USTRUCT(BlueprintType)
struct LUMIA_API FLMTraceSocketPair
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Trace")
	FName StartSocket = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Trace")
	FName EndSocket = NAME_None;
};

// 공격 종류별 기본 피해와 타격 판정 설정을 보관한다.
UCLASS(BlueprintType)
class LUMIA_API ULMAttackData : public UDataAsset
{
	GENERATED_BODY()
	
public:
	// 기존 공격 데이터의 단일 소켓 쌍을 배열의 첫 줄로 옮긴다.
	virtual void PostLoad() override;

	/** 추가 계산 전의 기본 피해 입력이다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Damage", meta = (ClampMin = "0.0"))
	float BaseDamage = 10.0f;

	// 실제 유효 피해가 발생한 타격의 검 게이지 보상이며 피해량과 독립적이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Special", meta = (ClampMin = "0.0"))
	float SpecialChargeGain = 0.0f;
	
	// 한 타격창에서 검사할 시작점과 끝점의 목록이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Trace", meta = (TitleProperty = "EndSocket", ToolTip = "비워 두면 기존에 저장된 시작·끝 소켓을 사용합니다. 새 DA의 기본 소켓은 LMTraceStart와 LMTraceEnd입니다. 에셋을 로드하면 사용되는 쌍이 첫 줄에 표시됩니다."))
	TArray<FLMTraceSocketPair> TraceSocketPairs;

	// 기존 애셋에 저장된 소켓 이름을 읽기 위해 두 필드를 유지한다.
	UPROPERTY(BlueprintReadOnly, Category = "LM|Trace")
	FName TraceStartSocket = TEXT("LMTraceStart");
	
	UPROPERTY(BlueprintReadOnly, Category = "LM|Trace")
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
