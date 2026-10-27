#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "LMBaseCharacter.generated.h"

/**
 * 프로젝트 캐릭터의 공통 부모.
 * 
 * 플레이어와 다른 캐릭터가 공유할 기능을 연결한다.
 * 현재는 공통 상속 기반만 준비한다.
 */
UCLASS(Abstract)
class LUMIA_API ALMBaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ALMBaseCharacter();
};
