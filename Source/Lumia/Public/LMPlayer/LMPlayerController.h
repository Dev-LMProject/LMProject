#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "LMAbilitySystem/LMAbilitySystemComponent.h"
#include "LMPlayerController.generated.h"

class ULMInputConfig;
class ULMScanComponent;
class UInputMappingContext;
class ULMInteractionHUDWidget;
class ULMUserHUDWidget;
class ULMHackingComponent;
class ULMHackingWidget;
class ULMAbilitySystemComponent;

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

	/** 상호작용 HUD의 선택 액터를 반환한다. HUD가 없거나 선택이 무효하면 nullptr. */
	AActor* GetSelectedInteractionActor() const;

	// 요청한 해킹 세션의 화면이 이 컨트롤러의 뷰포트에 등록되어 있는지 확인한다.
	bool IsHackingSessionActive(const ULMHackingComponent* InSession) const;
	
	// 닫기를 요청한 위젯이 지정한 세션의 현재 화면인지 확인한다.
	// 정리가 필요한 상황에서도 사용할 수 있도록 뷰포트 등록 여부는 요구하지 않는다.
	bool IsHackingWidgetSource(const ULMHackingComponent* InSession, const ULMHackingWidget* SourceWidget) const;
	
	// 지정한 해킹 컴포넌트의 화면을 생성하고 등록한다.
	// true는 화면 등록 성공이며, 해킹 성공을 의미하지 않는다.
	bool OpenHackingWidget(ULMHackingComponent* InSession);
	
	// 요청한 컴포넌트가 현재 화면의 세션일 때만 닫는다.
	// 생성 도중 호출되면 취소를 표시하고, 생성 함수에서 제거한다.
	void CloseHackingWidget(ULMHackingComponent* InSession);

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
	
	UPROPERTY(EditDefaultsOnly, Category="LM|UI")
	TSubclassOf<ULMUserHUDWidget> UserHUDClass;
	
	// 로컬 플레이어가 사용할 해킹 화면의 기본 클래스.
	UPROPERTY(EditDefaultsOnly, Category = "LM|UI|Hacking")
	TSubclassOf<ULMHackingWidget> HackingWidgetClass;

	// shortcut: 시작·종료 연결 전에는 두 참조가 비어 있으며, 세션 구현 시 함께 설정·해제한다.
	// 현재 해킹 화면은 컨트롤러가 보관하고 세션 종료 시 해제한다.
	UPROPERTY(Transient)
	TObjectPtr<ULMHackingWidget> ActiveHackingWidget = nullptr;

	// 화면을 요청한 해킹 컴포넌트를 식별하되 대상 액터의 수명은 유지하지 않는다.
	TWeakObjectPtr<ULMHackingComponent> ActiveHackingSession;
	
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
	
	/** InputConfig의 연결 목록을 읽어 공통 능력 입력 함수를 바인딩한다. */
	void BindAbilityInputs();
	
	/** 입력 시작을 해당 입력 태그와 함께 ASC에 전달한다. */
	void OnAbilityInputPressed(FGameplayTag InputTag);
	
	/** 정상적인 입력 해제를 ASC에 전달한다. */
	void OnAbilityInputReleased(FGameplayTag InputTag);
	
	/** 중단된 입력을 정상 해제와 구분하여 ASC에 전달한다. */
	void OnAbilityInputCanceled(FGameplayTag InputTag);
	
	/** 이 Controller가 사용할 능력 입력 연결 설정. */
	UPROPERTY(EditDefaultsOnly, Category = "LM|Input")
	TObjectPtr<ULMInputConfig> InputConfig = nullptr;
	
	// 생성·제거 중 발생하는 콜백에서 새 화면이 중복 생성되는 것을 막는다.
	bool bIsChangingHackingWidget = false;
	
	// 해킹 진입 중 저장한 상태를 복원해야 하는지 기록한다.
	bool bHasHackingInputState = false;

	// 해킹 진입에서 새로 일시정지를 요청했는지 기록한다.
	bool bPausedByHacking = false;

	// 해킹 진입 직전의 마우스 커서 표시 상태.
	bool bMouseCursorBeforeHacking = false;

	// 컨트롤러 종료 처리 중에는 새로운 해킹 화면을 열지 않는다.
	bool bIsEndingPlay = false;

	// 현재 해킹 화면을 정리한다. 생성 도중이면 취소만 기록한다.
	void ReleaseHackingWidget();

	// 저장한 커서 상태와 게임 입력을 복원하고, 필요한 경우 일시정지를 해제한다.
	void RestoreHackingInput();
};
