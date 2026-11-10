#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Hacking/LMHackingDefinition.h"
#include "LMHackingPuzzleData.generated.h"

// 여러 액터가 공유하는 퍼즐 원본이며 플레이 중인 값은 저장하지 않는다.
UCLASS(BlueprintType)
class LUMIA_API ULMHackingPuzzleData : public UDataAsset
{
    GENERATED_BODY()

public:
    // 크기·초기 숫자·선택 목록·목표·횟수를 작성하는 설정이다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Hacking")
    FLMHexPuzzleDefinition Definition;
};
