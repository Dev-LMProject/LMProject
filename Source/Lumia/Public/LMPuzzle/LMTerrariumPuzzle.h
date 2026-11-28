#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LMInterface/LMInteractable.h"
#include "LMTerrariumPuzzle.generated.h"

class UBoxComponent;
class ULMInteractionComponent;

// 콘솔 BP가 배관 상태에 맞는 문구를 고른다.
UENUM(BlueprintType)
enum class ELMTerrariumPanel : uint8 { BrokenFilter, NoReturnPressure, Ready, Confirmed, Complete };

UCLASS()
class LUMIA_API ALMTerrariumPuzzle : public AActor, public ILMInteractable
{
	GENERATED_BODY()

public:
	// 기존 E 상호작용을 받을 콘솔 감지 영역을 만든다.
	ALMTerrariumPuzzle();

	// 연결된 밸브 하나를 전환하고 손잡이에 적용할 방향을 돌려준다.
	UFUNCTION(BlueprintCallable, Category="Terrarium", meta=(DefaultToSelf="Valve"))
	bool TryTurnValve(AActor* Valve, bool& bTurned);

	// 감압판 BP가 실제로 밟힌 순간을 콘솔에 전달한다.
	UFUNCTION(BlueprintCallable, Category="Terrarium")
	void PressPlate(AActor* Plate);

	// 정상 순환이 잡힌 콘솔에서 불빛 순서를 재생한다.
	virtual bool Interact_Implementation(AActor* Interactor) override;

protected:
	// 같은 방의 두 밸브를 연결하고 초기 계기 표시를 맞춘다.
	virtual void BeginPlay() override;

	// 배관 판정에 해당하는 계기 문구를 콘솔 BP에 표시한다.
	UFUNCTION(BlueprintImplementableEvent, Category="Terrarium")
	void ShowPanel(ELMTerrariumPanel Panel);

	// 콘솔 BP가 지정한 판의 불빛을 켜거나 끈다.
	UFUNCTION(BlueprintImplementableEvent, Category="Terrarium")
	void SetPlateLight(AActor* Plate, bool bOn);

private:
	enum class EPlatePhase : uint8 { Ready, Showing, Input, Done };

	// 타이머가 순서대로 한 판씩 점등하고 마지막 소등 뒤 입력을 연다.
	void PlayNextLight();

	UPROPERTY(EditInstanceOnly, Category="Terrarium")
	TArray<TObjectPtr<AActor>> PlateOrder;

	TArray<TObjectPtr<AActor>> PressedPlates;
	FTimerHandle LightTimer;
	EPlatePhase PlatePhase = EPlatePhase::Ready;
	int32 LightStep = 0;

	UPROPERTY(VisibleAnywhere, Category="Interaction")
	TObjectPtr<UBoxComponent> UseZone;

	UPROPERTY(EditInstanceOnly, Category="Terrarium", meta=(MustImplement="/Script/Lumia.LMInteractable"))
	TObjectPtr<AActor> FilterValve;

	UPROPERTY(EditInstanceOnly, Category="Terrarium", meta=(MustImplement="/Script/Lumia.LMInteractable"))
	TObjectPtr<AActor> ReturnValve;

	UPROPERTY(VisibleAnywhere, Category="Interaction")
	TObjectPtr<ULMInteractionComponent> Interaction;

	bool bUseSpareFilter = false;
	bool bUseReturnPipe = false;
};
