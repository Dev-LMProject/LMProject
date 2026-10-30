#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LMPlayerController.generated.h"

class UInputAction;
class ULMScanComponent;
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
	
protected:
	// UserHUD를 생성하고 화면에 등록한다.
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
	
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<ULMUserHUDWidget> UserHUDClass;

	UPROPERTY(Transient)
	TObjectPtr<ULMUserHUDWidget> UserHUD;

	// BP 디자이너에 배치한 자식 HUD에 입력을 전달한다.
	UPROPERTY(Transient)
	TObjectPtr<ULMInteractionHUDWidget> InteractionHUD;

	// BP의 Class Defaults에서 지정할 기본 키 배치.
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;
};