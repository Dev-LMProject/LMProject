#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "LMQTETypes.generated.h"

class ULMQTEWidget;

// 입력 성공과 시간 실패, 외부 중단을 구분한다.
UENUM(BlueprintType)
enum class ELMQTEResult : uint8
{
    Success,
    Failed,
    Cancelled
};

// 공유 설정만 보관하며 진행 시간과 입력 횟수는 컴포넌트가 관리한다.
USTRUCT(BlueprintType)
struct LUMIA_API FLMQTEConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LM|QTE", meta = (Categories = "LM.QTE.Interaction"))
    FGameplayTag InteractionTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LM|QTE", meta = (Categories = "LM.InputTag"))
    FGameplayTag InputTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LM|QTE", meta = (ClampMin = "0.01", Units = "s"))
    float TimeLimit = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LM|QTE", meta = (ClampMin = "1"))
    int32 TargetPresses = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LM|QTE")
    FVector2D DragStart = FVector2D(0.2, 0.5);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LM|QTE")
    FVector2D DragGoal = FVector2D(0.8, 0.5);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LM|QTE", meta = (ClampMin = "0.001", ClampMax = "1.0"))
    float DragRadius = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LM|QTE")
    TSubclassOf<ULMQTEWidget> WidgetClass;
};
