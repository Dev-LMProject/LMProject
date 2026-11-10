#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "LMBaseCharacter.generated.h"

class ULMAbilitySystemComponent;
class ULMAttackData;
class UGameplayEffect;

struct FGameplayTag;

/** ASC의 소유 위치와 캐릭터 종류에 관계없이 GAS 연결, 행동 제한, 공통 피해 접수를 관리한다. */
UCLASS(Abstract)
class LUMIA_API ALMBaseCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	/** 공통 GAS 연결을 관리할 캐릭터 기반을 생성한다. */
	ALMBaseCharacter();

	/** 캐릭터가 참조 중인 ASC를 GAS 인터페이스로 제공한다. */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	/**
	 * 공격자 정보를 보존해 이 피격자의 ASC에 피해를 적용하고 HP 계산은 AttributeSet에 맡긴다.
	 * 플레이어·일반 몬스터·보스가 공유하며, HP 소진 이후의 사망·무력화 정책은 각 캐릭터가 결정한다.
	 * @param DamageSource 공격한 캐릭터의 현재 Avatar다. 무기나 PlayerState가 아니며 기존 BP 핀 이름은 유지한다.
	 * @param AttackData 기본 피해를 읽을 공유 공격 데이터다. 이 함수에서 수정하지 않는다.
	 * @param HitResult 공격 판정에서 얻은 충돌 결과다. 액터 정보가 있으면 피해 Context에 보관한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "LM|Combat", meta = (AutoCreateRefTerm = "HitResult"))
	void ReceiveCombatDamage(AActor* DamageSource, const ULMAttackData* AttackData, const FHitResult& HitResult);

protected:
	/** 캐릭터 종료 시 남은 GAS 연결을 해제하고 부모 종료 처리를 수행한다. */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 착지 직전 속도를 보존하고 실제 착지 사건을 GAS에 한 번 전달한다. */
	virtual void Landed(const FHitResult& Hit) override;

	/** 파생 클래스가 선택한 소유자와 ASC를 현재 몸체에 연결한다. */
	void InitializeAbilitySystem(AActor* AbilityOwner, ULMAbilitySystemComponent* AbilitySystem);

	/** 자신이 등록한 구독과 현재 몸체의 실행만 정리한다. 능력 부여는 회수하지 않는다. */
	void DisconnectAbilitySystem();

	/** ActorInfo 준비 후 소유자별 초기화를 수행한다. 반복 호출될 수 있다. */
	virtual void OnAbilitySystemReady();

	/** 초기 연결·태그 변경·연결 해제 후 파생 클래스의 행동 정책을 갱신한다. */
	virtual void OnAbilitySystemTagsChanged();

	/** 이동 금지·사망·기절 태그에 의해 일반 이동이 제한되는지 반환한다. */
	bool IsMovementBlockedByAbilities() const;

	/** 능력이나 루트 모션이 일반 이동 대신 이동 제어를 맡고 있는지 반환한다. */
	bool IsMovementControlledByAbilities() const;
	
	/** 이 피격자의 IncomingDamage에 피해를 전달할 Instant GE다. HP 소진 이후의 반응은 결정하지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="LM|Combat")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

private:
	/** 현재 Avatar의 태그 변경만 파생 클래스의 행동 정책에 전달한다. */
	void HandleAbilityTagChanged(FGameplayTag Tag, int32 NewCount);

	// 실제 소유자는 PlayerState 또는 파생 캐릭터이며, Base는 수명을 연장하지 않는다.
	TWeakObjectPtr<ULMAbilitySystemComponent> CachedAbilitySystem;
	FDelegateHandle AbilityTagChangedHandle;

	bool bInitializingAbilitySystem = false;
	bool bDisconnectingAbilitySystem = false;
};
