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

// 매 프레임 카메라 계산이 끝났다는 사실을 C++ Task에 알린다.
DECLARE_MULTICAST_DELEGATE(FLMFocusFrameDelegate);

// 선택 대상이 A에서 B 또는 nullptr로 바뀌었을 때 새 대상을 전달한다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLMFocusChangedDelegate, AActor*, FocusTarget);

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
	
	// 카메라를 먼저 계산하고 Task 검색을 실행한 뒤 HUD 표식 위치를 갱신한다.
	virtual void UpdateCameraManager(float DeltaSeconds) override;

	// 일시정지 성공 시 다음 카메라 갱신을 기다리지 않고 표식을 숨긴다.
	virtual bool SetPause(bool bPause, FCanUnpause CanUnpauseDelegate = FCanUnpause()) override;

	// Task가 UpdateFocus 함수를 연결할 C++ 알림이다.
	FLMFocusFrameDelegate OnFocusFrame;

	// Blueprint에서도 선택 대상 변경을 받을 수 있게 노출한다.
	UPROPERTY(BlueprintAssignable, Category = "LM|Focus")
	FLMFocusChangedDelegate OnFocusChanged;

	// 선택 Actor가 아직 존재하면 반환하고 파괴됐으면 nullptr을 반환한다.
	UFUNCTION(BlueprintPure, Category = "LM|Focus")
	AActor* GetFocusTarget() const;

	// 내부 약한 참조 배열에서 아직 유효한 Actor만 복사해 반환한다.
	UFUNCTION(BlueprintPure, Category = "LM|Focus")
	TArray<AActor*> GetFocusCandidates() const;

	// 해킹·일시정지·연출 중인지 검사해 지금 검색할 수 있는지 반환한다.
	bool CanUpdateFocus() const;

	// 후보 목록을 새 결과로 바꾸고 그 목록에 속한 대상만 현재 선택으로 저장한다.
	void SetFocusResult(const TArray<TWeakObjectPtr<AActor>>& _FocusCandidates, AActor* _FocusTarget);

	// 후보 배열을 비우고 선택을 nullptr로 만든 뒤 HUD 표식을 숨긴다.
	void ClearFocus();

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
	
	// 현재 프레임 검사에 통과한 Actor들만 보관한다.
	// 약한 참조이므로 대상이 삭제되면 Get()으로 유효한 Actor를 얻을 수 없게 된다.
	TArray<TWeakObjectPtr<AActor>> FocusCandidates;

	// 현재 후보 중 선택된 Actor 하나다. 선택이 없으면 비어 있다.
	TWeakObjectPtr<AActor> FocusTarget;
};
