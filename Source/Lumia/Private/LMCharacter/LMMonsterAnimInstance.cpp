#include "LMCharacter/LMMonsterAnimInstance.h"
#include "LMCharacter/LMFieldMonsterCharacter.h"

// 애니메이션 재초기화 시 이전 소유자와 표시 값을 비운다.
void ULMMonsterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// 이전 몬스터의 이동 판정이 새 실행에 남지 않도록 한다.
	CachedMonster.Reset();
	Locomotion = FLMMonsterLocomotionFrame();
}

// 실제 속도와 몬스터의 피격·사망 상태를 매 프레임 읽는다.
void ULMMonsterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	ALMFieldMonsterCharacter* Monster = Cast<ALMFieldMonsterCharacter>(TryGetPawnOwner());

	// 소유자가 없거나 파괴 중이면 이전 프레임의 표시 값을 남기지 않는다.
	if (!IsValid(Monster) || Monster->IsActorBeingDestroyed())
	{
		CachedMonster.Reset();
		Locomotion = FLMMonsterLocomotionFrame();
		return;
	}

	// 다른 몬스터를 표시하게 되면 이동 전환의 이전 상태부터 초기화한다.
	if (CachedMonster.Get() != Monster)
	{
		CachedMonster = Monster;
		Locomotion = FLMMonsterLocomotionFrame();
	}

	const FVector Velocity = Monster->GetVelocity();

	// 이동 허가나 목표 속도가 아닌 실제 수평 속도와 확정된 반응 상태를 읽는다.
	Locomotion.MoveSpeed = static_cast<float>(Velocity.Size2D());
	Locomotion.bHitReacting = Monster->IsHitReacting();
	Locomotion.bParryRetreating = Monster->IsParryRetreating();
	Locomotion.bGroggy = Monster->IsGroggy();
	Locomotion.bDead = Monster->IsDead();

	// 서로 다른 진입·종료 속도로 정지 부근의 반복 전환을 막는다.
	if (Locomotion.MoveSpeed >= 10.0f)
		Locomotion.bMoving = true;
	else if (Locomotion.MoveSpeed <= 3.0f)
		Locomotion.bMoving = false;

	// 거의 정지한 상태의 방향 노이즈는 표시하지 않는다.
	Locomotion.Direction = 0.0f;
	if (Locomotion.MoveSpeed > 3.0f)
	{
		// 메시 상대 회전이 아닌 몸체의 Yaw를 기준으로 이동 방향을 구한다.
		const FRotator BodyYaw(0.0f, Monster->GetActorRotation().Yaw, 0.0f);
		const FVector LocalVelocity = BodyYaw.UnrotateVector(Velocity);
		Locomotion.Direction = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, LocalVelocity.X)));
	}
}
