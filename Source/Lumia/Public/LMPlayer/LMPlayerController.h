#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/PlayerController.h"
#include "LMPlayerController.generated.h"

class UInputMappingContext;
class ULMInteractionHUDWidget;

/** 로컬 플레이어의 입력 환경과 카메라 매니저를 연결한다. */
UCLASS()
class LUMIA_API ALMPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	/** 이 컨트롤러가 생성할 전용 카메라 매니저를 지정한다. */
	ALMPlayerController();

protected:
	/** 로컬 플레이어 화면에 감지 목록 HUD를 한 번 생성한다. */
	virtual void BeginPlay() override;

	virtual void SetupInputComponent() override;
	
	/** 종료할 때 사용한 입력 매핑을 정리한다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
private:
	/** 휠과 방향키 입력을 HUD의 선택 이동으로 전달한다. */
	void SelectPreviousInteractionTarget();
	void SelectNextInteractionTarget();
	/** HUD의 현재 선택만 실행 요청에 사용한다. */
	void InteractWithSelectedTarget();

	/** 타겟의 HasTagContainer가 허용하는 요청 태그를 지정한다. */
	UPROPERTY(EditDefaultsOnly, Category="Interaction")
	FGameplayTagContainer InteractionActionTags;

	UPROPERTY(Transient)
	TObjectPtr<ULMInteractionHUDWidget> InteractionHUD;

	/** BP의 Class Defaults에서 지정할 기본 키 배치. */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;
};
