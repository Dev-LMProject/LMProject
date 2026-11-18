#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "LMInputConfig.generated.h"

class UInputAction;

/** 하나의 Input Action을 ASC에 전달할 입력 태그와 연결한다. */
USTRUCT(BlueprintType)
struct LUMIA_API FLMAbilityInputBinding
{
	GENERATED_BODY()

	/** 시작·해제·취소 이벤트를 받을 입력 액션. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Input")
	TObjectPtr<UInputAction> InputAction = nullptr;

	/** AbilitySet에서 해당 능력에 지정한 InputTag와 일치시킨다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Input", meta = (Categories = "LM.InputTag"))
	FGameplayTag InputTag;
};

/** Controller가 사용할 능력 입력 연결 목록을 보관한다.  */
UCLASS(BlueprintType)
class LUMIA_API ULMInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 공유 설정 데이터이며, 입력의 눌림 여부 같은 실행 상태는 저장하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|Input", meta = (TitleProperty = "InputTag"))
	TArray<FLMAbilityInputBinding> AbilityInputs;
};
