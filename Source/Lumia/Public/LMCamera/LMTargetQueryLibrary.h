#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LMTargetQueryLibrary.generated.h"

class AActor;
class APlayerController;

// 대상의 위치와 현재 카메라를 읽어 후보와 선택을 계산하는 함수 모음이다.
UCLASS()
class LUMIA_API ULMTargetQueryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 월드의 한 점이 플레이어의 실제 표시 영역 안에 있는지 검사한다.
	// OutScreenPosition은 해당 표시 영역의 좌상단을 0으로 하는 픽셀 좌표다.
	UFUNCTION(BlueprintPure, Category = "LM|Camera")
	static bool IsPointInCameraView(APlayerController* PlayerController, FVector WorldPosition, FVector2D& OutScreenPosition);

	// 대상의 ULMTargetComponent 월드 위치를 반환한다.
	// 컴포넌트가 없으면 기존 캐릭터 원점 또는 일반 Actor의 충돌 경계 중심을 반환한다.
	UFUNCTION(BlueprintPure, Category="LM|Focus")
	static FVector GetFocusPoint(const AActor* Target);

	// OutCandidates를 이번 검사에 통과한 Actor들로 채우고 선정된 Actor 하나를 반환한다.
	// 후보가 없거나 카메라 조회에 실패하면 빈 배열과 nullptr을 돌려준다.
	static AActor* FindFocusTargets(APlayerController* PlayerController, float FocusRange, float SwitchMargin, AActor* CurrentTarget, TArray<TWeakObjectPtr<AActor>>& OutCandidates);
};