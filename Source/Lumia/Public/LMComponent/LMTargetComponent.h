#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameplayTagContainer.h"
#include "LMTargetComponent.generated.h"

class USkeletalMeshComponent;

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

	// 마커 소켓의 위치를 읽을 스켈레탈 메시 컴포넌트다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="LM|Target")
	TObjectPtr<USkeletalMeshComponent> MarkerMesh = nullptr;

	// 마커 위치에 사용할 소켓 이름이다.
	UPROPERTY(EditAnywhere, Category="LM|Target")
	FName SocketName = NAME_None;

	// 플레이 시작 시 계산한 마커의 로컬 좌표다.
	UPROPERTY(Transient, BlueprintReadOnly, Category="LM|Target")
	FVector MarkerLocalPosition = FVector::ZeroVector;

protected:
	// BP에서 지정한 소켓의 로컬 좌표를 플레이 시작 시 저장한다.
	virtual void BeginPlay() override;
};