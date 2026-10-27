#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LMTargetQueryLibrary.generated.h"

class APlayerController;

/** 상호작용과 전투에서 공유하는 상태 없는 카메라 화면 판정. */
UCLASS()
class LUMIA_API ULMTargetQueryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 현재 플레이어 화면 안의 점인지 반환한다. 좌표는 플레이어 뷰 좌상단 기준 픽셀이다. */
	UFUNCTION(BlueprintPure, Category = "LM|Camera")
	static bool IsPointInCameraView(APlayerController* PlayerController,
		FVector WorldPosition, FVector2D& OutScreenPosition);
};
