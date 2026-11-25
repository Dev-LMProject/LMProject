#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/Interface.h"
#include "LMBreakable.generated.h"

class AActor;
class UPrimitiveComponent;

// 에디터에서 블루프린트에 추가할 수 있는 인터페이스 이름을 등록한다.
UINTERFACE(Blueprintable, meta = (DisplayName = "LM Breakable"))
class LUMIA_API ULMBreakable : public UInterface
{
	GENERATED_BODY()
};

class LUMIA_API ILMBreakable
{
	GENERATED_BODY()

public:
	// 이 액터에서 공격을 받을 충돌 상자 하나를 반환한다. 여기서는 타수나 상태를 바꾸지 않는다.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "LM|Break")
	UPrimitiveComponent* GetBreakHitBody() const;

	// 유효한 한 타의 출발점과 접점을 월드 좌표로 받아 물체 고유의 반응을 실행한다.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "LM|Break")
	void ReceiveBreakHit(const FVector& SourcePoint, const FVector& HitPoint);

	// 검과 착지가 같은 기준으로 실제 맞은 부품을 검사한다.
	static bool IsHitBody(const AActor* HitActor, const UPrimitiveComponent* HitBody, ECollisionChannel HitChannel);

	// 전방 구체 조회에서 첫 장애물인 파괴 물체를 찾는다. 거리와 반경은 cm이며 앞에 캐릭터가 있으면 반환하지 않는다.
	static AActor* FindHitActorAhead(const AActor* SourceActor, float CheckDistance, float CheckRadius, ECollisionChannel HitChannel);
};
