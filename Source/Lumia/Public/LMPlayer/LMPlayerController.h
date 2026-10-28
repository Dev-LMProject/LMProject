#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LMPlayerController.generated.h"

class UInputMappingContext;

/** 로컬 플레이어의 입력 환경과 카메라 매니저를 연결한다. */
UCLASS()
class LUMIA_API ALMPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	/** 이 컨트롤러가 생성할 전용 카메라 매니저를 지정한다. */
	ALMPlayerController();

protected:
	virtual void SetupInputComponent() override;
	
	/** 종료할 때 사용한 입력 매핑을 정리한다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
private:
	/** BP의 Class Defaults에서 지정할 기본 키 배치. */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;
};
