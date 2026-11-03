#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "LMAbilitySystem/LMAbilitySystemComponent.h"
#include "LMPlayerController.generated.h"

class UInputAction;
class ULMScanComponent;
class UInputMappingContext;
class ULMInteractionHUDWidget;
class ULMUserHUDWidget;
class ULMAbilitySystemComponent;

/** 에디터에서 Input Action과 능력 입력 태그를 연결한다. */
USTRUCT(BlueprintType)
struct FLMAbilityInputBinding
{
	GENERATED_BODY()
	
	/** 눌림·해제 이벤트를 받을 입력 액션. 실제 키는 Input Mapping Context에서 지정한다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LM|Input")
	TObjectPtr<UInputAction> InputAction = nullptr;
	
	/** ASC에 전달할 입력 태그. AbilitySet에서 해당 능력에 지정한 InputTag와 일치시킨다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LM|Input", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

// 로컬 플레이어의 입력 환경과 카메라 매니저를 연결한다.
UCLASS()
class LUMIA_API ALMPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	// 플레이어 카메라 매니저 클래스를 지정한다.
	ALMPlayerController();
	
	/** UI 전환 등으로 중단된 능력 입력을 취소한다. */
	UFUNCTION(BlueprintCallable, Category="LM|Input")
	void CancelAbilityInput();
	
	/** 포커스 상실 등으로 키 상태가 초기화 되기 전에 능력 입력을 취소한다. */
	virtual void FlushPressedKeys() override;
	
protected:
	// UserHUD를 생성하고 화면에 등록한다.
	virtual void BeginPlay() override;

	// 상호작용 키와 기본 입력 매핑을 등록하고 게임 입력 모드를 설정한다.
	virtual void SetupInputComponent() override;
	
	// UserHUD를 화면에서 제거하고 기본 입력 매핑을 해제한다.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	/** 이번 프레임에 수집된 능력 입력을 ASC에서 처리한다. */
	virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;
	
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
	
	/** 현재 PlayerState가 소유한 프로젝트 ASC를 반환한다. */
	ULMAbilitySystemComponent* GetLMAbilitySystemComponent() const;
	
	/** 에디터에서 지정한 Input Action들을 공통 능력 입력 함수에 연결한다. */
	void BindAbilityInputs();
	
	/** 입력 시작을 해당 입력 태그와 함께 ASC에 전달한다. */
	void OnAbilityInputPressed(FGameplayTag InputTag);
	
	/** 정상적인 입력 해제를 ASC에 전달한다. */
	void OnAbilityInputReleased(FGameplayTag InputTag);
	
	/** 중단된 입력을 정상 해제와 구분하여 ASC에 전달한다. */
	void OnAbilityInputCanceled(FGameplayTag InputTag);
	
	/** 입력 액션과 GAS 입력 태그의 연결 목록. Controller BP의 Class Defaults에서 설정한다. */
	UPROPERTY(EditDefaultsOnly, Category = "LM|Input")
	TArray<FLMAbilityInputBinding> AbilityInputs;
};