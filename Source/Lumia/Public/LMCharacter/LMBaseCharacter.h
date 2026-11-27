#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "LMBaseCharacter.generated.h"

class UAnimMontage;
class ULMAbilitySystemComponent;
class ULMAttackData;
class UGameplayEffect;

struct FGameplayTag;
struct FGameplayEffectSpec;

/** 전투 진영을 구분하며 중립 대상은 자동 적대 관계에서 제외한다. */
UENUM(BlueprintType)
enum class ELMCombatTeam : uint8
{
	Neutral,
	Player,
	Enemy
};

/** ASC의 소유 위치와 캐릭터 종류에 관계없이 GAS 연결, 행동 제한, 공통 피해 접수를 관리한다. */
UCLASS(Abstract)
class LUMIA_API ALMBaseCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	/** 파생 클래스가 기본 이동 컴포넌트 클래스를 지정할 수 있도록 생성 정보를 받는다. */
	ALMBaseCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
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

	// 공격자의 현재 전투 정책에 맞는 DA 기본 피해를 읽는다.
	virtual float GetAttackDamage(const ULMAttackData* AttackData) const;

	// 유효한 원래 피해를 캐릭터별 규칙에 맞게 바꾸며 기본 캐릭터는 그대로 받는다.
	virtual float ChangeIncomingDamage(const FGameplayEffectSpec& DamageSpec, float RequestedDamage, float OldHealth, float MaximumHealth, bool& bAllowCritical);

	// 피해 계산이 끝난 뒤 예약된 반응을 처리하며 실제 HP 감소가 0인 보호도 포함한다.
	virtual void FinishCombatDamage();
	
	/** 두 캐릭터가 전투상 적대 관계인지 확인한다. */
	bool IsHostileTo(const ALMBaseCharacter* Other) const;
	
	/** 접근 대상으로 선택할 수 있는 살아 있는 적인지 확인한다. */
	bool CanBeAttackTargetFor(const ALMBaseCharacter* Attacker) const;
	
	/** 피해를 받을 수 있는 살아 있는 적인지 확인한다. */
	bool CanReceiveAttackFrom(const ALMBaseCharacter* Attacker) const;
	
	/** 현재 몸체의 ASC·체력·사망 상태를 확인한다. */
	UFUNCTION(BlueprintPure, Category = "LM|Combat")
	bool IsAliveForCombat() const;

	// 이 캐릭터의 회피 GA가 재생할 몽타주다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|Dodge")
	TObjectPtr<UAnimMontage> DodgeMontage;

protected:
	// 종류별 BP에서 설정하며 ASC 재연결 때 다시 채우지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|Combat", meta = (ClampMin = "1", ClampMax = "100000"))
	int32 StartHealth = 100;

	UPROPERTY(EditDefaultsOnly, Category = "LM|Critical", meta = (ClampMin = "0", ClampMax = "100"))
	int32 CriticalChance = 10;

	UPROPERTY(EditDefaultsOnly, Category = "LM|Critical", meta = (ClampMin = "0", ClampMax = "100"))
	int32 CriticalStep = 5;

	UPROPERTY(EditDefaultsOnly, Category = "LM|Critical", meta = (ClampMin = "1", ClampMax = "100"))
	int32 CriticalLimit = 10;

	UPROPERTY(EditDefaultsOnly, Category = "LM|Critical", meta = (ClampMin = "100", ClampMax = "300"))
	int32 CriticalDamagePercent = 150;

	// 검증된 일반 타격 또는 지정 시연 Spec을 기존 Instant GE로 적용한다.
	bool ApplyCombatDamage(ALMBaseCharacter* DamageSource, float RequestedDamage, const ULMAttackData* AttackData, const FHitResult& HitResult, FGameplayTag SceneTag);

	// 이 캐릭터의 공격이 패링됐을 때 캐릭터별 반응을 처리한다.
	virtual void ReceiveParryReaction(ALMBaseCharacter* Parrier);

	// BP 기본값이 적용된 뒤 전투 진영에 맞는 몸체 채널을 한 번 설정한다.
	virtual void PostInitializeComponents() override;

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
	
	/** 플레이어·일반 몬스터·보스가 같은 진영 판정 규칙을 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "LM|Combat")
	ELMCombatTeam CombatTeam = ELMCombatTeam::Neutral;
	
	/** 무력화나 전투 종료 시 접근 대상에서 제외할 수 있다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LM|Combat")
	bool bCanBeAttackTarget = true;

	/** 무적 상태에서 접근은 허용하면서 피해만 막을 수 있다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LM|Combat")
	bool bCanReceiveCombatDamage = true;

private:
#if WITH_DEV_AUTOMATION_TESTS
	// 실제 피해 경로 검사의 ASC 연결과 GE 설정만 허용한다.
	friend class FLMEliteCombatDamageTest;
	friend class FLMCombatDamagePathTest;
#endif

	/** 현재 Avatar의 태그 변경만 파생 클래스의 행동 정책에 전달한다. */
	void HandleAbilityTagChanged(FGameplayTag Tag, int32 NewCount);

	// 실제 소유자는 PlayerState 또는 파생 캐릭터이며, Base는 수명을 연장하지 않는다.
	TWeakObjectPtr<ULMAbilitySystemComponent> CachedAbilitySystem;
	FDelegateHandle AbilityTagChangedHandle;

	bool bInitializingAbilitySystem = false;
	bool bDisconnectingAbilitySystem = false;
	bool bCombatStatsInitialized = false;
};
