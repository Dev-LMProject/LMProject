#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameplayTagContainer.h"
#include "LMTargetComponent.generated.h"

// 이 컴포넌트의 위치를 표적점으로 사용하고, TargetTags로 대상 사용 여부를 제공한다.
UCLASS(ClassGroup=(LM), meta=(BlueprintSpawnableComponent))
class LUMIA_API ULMTargetComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	// 검색은 플레이어의 GA Task가 담당하므로 대상마다 실행되는 Tick을 끈다.
	ULMTargetComponent();

	// 컴포넌트가 월드에 등록되어 있고 허용 태그가 있으며 제외 태그가 없는지 검사한다.
	UFUNCTION(BlueprintPure, Category="LM|Target")
	bool CanFocus() const;

	// Actor Tags와 별개의 Gameplay Tag 저장소다. 에디터에서 사용할 대상마다 태그를 지정한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LM|Target", meta=(Categories="LM.Focus"))
	FGameplayTagContainer TargetTags;
};