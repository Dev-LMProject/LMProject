#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/PlayerController.h"
#include "LMPlayerController.generated.h"

class UInputMappingContext;
class ULMInteractionHUDWidget;
class ULMUserHUDWidget;

// 로컬 플레이어의 입력 환경과 카메라 매니저를 연결한다.
UCLASS()
class LUMIA_API ALMPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	// 플레이어 카메라 매니저 클래스를 지정한다.
	ALMPlayerController();

	// 플레이어의 상호작용 요청 태그를 반환한다.
	UFUNCTION(BlueprintGetter, Category="Interaction")
	FGameplayTagContainer GetInteractionActionTags() const { return InteractionActionTags; }
	// 요청 태그를 변경하고 상호작용 HUD에 전달한다.
	UFUNCTION(BlueprintSetter, Category="Interaction")
	void SetInteractionActionTags(FGameplayTagContainer InTags);

protected:
	// UserHUD를 생성하고 요청 태그를 전달한 뒤 화면에 등록한다.
	virtual void BeginPlay() override;

	// 상호작용 키와 기본 입력 매핑을 등록하고 게임 입력 모드를 설정한다.
	virtual void SetupInputComponent() override;
	
	// UserHUD를 화면에서 제거하고 기본 입력 매핑을 해제한다.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
private:
	// HUD의 선택을 이전 상호작용 액터로 이동한다.
	void SelectPreviousInteractionTarget();
	// HUD의 선택을 다음 상호작용 액터로 이동한다.
	void SelectNextInteractionTarget();
	// HUD에서 선택한 액터의 상호작용을 요청하고 실패 결과를 기록한다.
	void InteractWithSelectedTarget();

	// 타겟의 HasTagContainer가 허용하는 요청 태그를 지정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintGetter=GetInteractionActionTags,
		BlueprintSetter=SetInteractionActionTags, Category="Interaction", meta=(AllowPrivateAccess="true"))
	FGameplayTagContainer InteractionActionTags;

	UPROPERTY(Transient)
	TObjectPtr<ULMUserHUDWidget> UserHUD;

	// 생성과 배치는 UserHUD가 담당하고 컨트롤러는 입력만 전달한다.
	UPROPERTY(Transient)
	TObjectPtr<ULMInteractionHUDWidget> InteractionHUD;

	// BP의 Class Defaults에서 지정할 기본 키 배치.
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;
};
