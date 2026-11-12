#include "LMCamera/LMTargetQueryLibrary.h"

#include "Components/SceneComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "LMCharacter/LMBaseCharacter.h"
#include "SceneView.h"
#include "UnrealClient.h"

// 월드 좌표를 화면으로 투영한 뒤 실제 화면 사각형 안에 들어오는지 검사한다.
bool ULMTargetQueryLibrary::IsPointInCameraView(APlayerController* PlayerController, FVector WorldPosition, FVector2D& OutScreenPosition)
{
	// 실패했을 때 호출자가 이전에 사용하던 좌표를 새 결과로 착각하지 않게 한다.
	OutScreenPosition = FVector2D::ZeroVector;

	if (!IsValid(PlayerController) || WorldPosition.ContainsNaN())
		return false;

	const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();

	// 뷰포트가 없으면 화면 크기와 카메라 투영 정보를 얻을 수 없다.
	if (!LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport)
		return false;

	FSceneViewProjectionData ProjectionData;
	if (!LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
		return false;

	// 검은 레터박스 여백을 제외한 실제 게임 화면의 사각형이다.
	const FIntRect ViewRect = ProjectionData.GetConstrainedViewRect();
	if (ViewRect.IsEmpty()) return false;

	FVector2D ScreenPosition;

	// 월드 위치를 현재 카메라로 봤을 때의 화면 픽셀 위치로 변환한다.
	// 투영 성공만으로 화면 안이라는 뜻은 아니므로 아래에서 좌표 범위도 검사한다.
	if (!FSceneView::ProjectWorldToScreen(WorldPosition, ViewRect, ProjectionData.ComputeViewProjectionMatrix(), ScreenPosition) || ScreenPosition.ContainsNaN())
		return false;

	// 이 기존 함수는 실제 표시 영역 좌상단 기준 좌표를 반환한다.
	OutScreenPosition = ScreenPosition - FVector2D(ViewRect.Min);

	return OutScreenPosition.X >= 0.0 && OutScreenPosition.X < ViewRect.Width() && OutScreenPosition.Y >= 0.0 && OutScreenPosition.Y < ViewRect.Height();
}

// 표식과 거리·가림 검사에 공통으로 사용할 대상의 월드 위치를 반환한다.
FVector ULMTargetQueryLibrary::GetFocusPoint(const AActor* Target)
{
	if (!IsValid(Target)) return FVector::ZeroVector;

	static const FName PointTag(TEXT("LM.Focus.Point"));

	// Actor가 직접 가지고 있는 SceneComponent들을 가져온다.
	// SceneComponent는 위치를 가지므로 메시가 없는 빈 컴포넌트도 표적점으로 사용할 수 있다.
	TInlineComponentArray<USceneComponent*> Components;
	Target->GetComponents(Components);

	for (const USceneComponent* Component : Components)
	{
		// Component Tags에 LM.Focus.Point가 있는 등록된 컴포넌트를 찾는다.
		// GetComponentLocation은 부모와의 상대 위치가 아니라 최종 월드 위치를 반환한다.
		if (IsValid(Component) && Component->IsRegistered() && Component->ComponentHasTag(PointTag))
			return Component->GetComponentLocation();
	}

	// 캐릭터의 전체 Bounds에는 무기나 다른 컴포넌트가 포함될 수 있다.
	// 별도 표적점이 없으면 캐릭터 Actor 원점을 사용해 그 영향을 피한다.
	if (Cast<ALMBaseCharacter>(Target)) return Target->GetActorLocation();

	FVector Origin;
	FVector Extent;

	// true이므로 충돌이 있는 컴포넌트들의 경계를 기준으로 중심을 계산한다.
	Target->GetActorBounds(true, Origin, Extent);

	// 사용할 충돌 경계가 없으면 Actor 원점으로 돌아간다.
	return Extent.IsNearlyZero() ? Target->GetActorLocation() : Origin;
}

// 주변 Actor를 찾고 태그·거리·화면·가림 검사를 통과한 대상 중 하나를 선택한다.
AActor* ULMTargetQueryLibrary::FindFocusTargets(APlayerController* PlayerController, float FocusRange, float SwitchMargin, AActor* CurrentTarget, TArray<TWeakObjectPtr<AActor>>& OutCandidates)
{
	// 이전 검색의 대상이 이번 결과에 남지 않도록 출력 배열부터 비운다.
	OutCandidates.Reset();

	if (!IsValid(PlayerController)) return nullptr;

	APawn* Pawn = PlayerController->GetPawn();
	if (!Pawn) return nullptr;

	UWorld* World = Pawn->GetWorld();
	const ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();

	if (!World || !LocalPlayer || !LocalPlayer->ViewportClient || !LocalPlayer->ViewportClient->Viewport)
		return nullptr;

	// 공간 조회에 사용할 반경은 유한한 양수여야 한다.
	if (!FMath::IsFinite(FocusRange) || FocusRange <= 0.0f)
		return nullptr;

	if (!FMath::IsFinite(SwitchMargin) || SwitchMargin < 0.0f)
		return nullptr;

	// 모든 후보가 같은 카메라 위치·방향·시야각으로 비교되도록 한 번만 가져온다.
	FSceneViewProjectionData ProjectionData;
	if (!LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
		return nullptr;

	const FIntRect ViewRect = ProjectionData.GetConstrainedViewRect();
	if (ViewRect.IsEmpty()) return nullptr;

	// 월드 위치를 화면으로 바꿀 때 사용할 카메라 투영 행렬이다.
	const FMatrix ViewMatrix = ProjectionData.ComputeViewProjectionMatrix();

	// 화면 전체가 아니라 실제 게임 표시 영역의 중심을 계산한다.
	const FVector2D ViewCenter = FVector2D(ViewRect.Min) + FVector2D(ViewRect.Size()) * 0.5;

	// 1920×1080 화면이면 1080이다.
	// 화면 중심에서의 픽셀 거리를 이 값으로 나누어 해상도에 따른 차이를 줄인다.
	const double ViewScale = FMath::Min(ViewRect.Width(), ViewRect.Height());

	const FVector PawnPosition = Pawn->GetActorLocation();

	// 거리 비교 때마다 제곱근을 계산할 필요가 없으므로 거리의 제곱끼리 비교한다.
	const double RangeSquared = FMath::Square(static_cast<double>(FocusRange));

	// 이 Object Type을 가진 충돌만 주변 검색에 포함한다.
	// Actor 태그 검사는 충돌로 Actor를 찾은 다음에 수행한다.
	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_Pawn);
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectTypes.AddObjectTypesToQuery(ECC_PhysicsBody);

	// false는 complex collision 대신 기본 충돌 형상을 사용한다는 뜻이다.
	// 마지막 Pawn은 검색과 가림 검사에서 무시할 플레이어 자신이다.
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LMFocusTargets), false, Pawn);

	// 플레이어에게 붙은 무기 Actor 등이 후보가 되거나 카메라 앞을 가리는 것을 막는다.
	TArray<AActor*> AttachedActors;
	Pawn->GetAttachedActors(AttachedActors, true, true);
	QueryParams.AddIgnoredActors(AttachedActors);

	// 플레이어 위치를 중심으로 FocusRange 반경의 구와 겹치는 충돌들을 얻는다.
	// 이 배열은 아직 태그·시야 검사를 하지 않은 임시 수집 결과다.
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, PawnPosition, FQuat::Identity, ObjectTypes, FCollisionShape::MakeSphere(FocusRange), QueryParams);

	static const FName TargetTag(TEXT("LM.Focus.Target"));
	static const FName DisabledTag(TEXT("LM.Focus.Disabled"));

	// 한 Actor의 Capsule과 Mesh가 모두 검색돼도 Actor 검사는 한 번만 한다.
	TSet<AActor*> SeenActors;

	AActor* BestTarget = nullptr;
	AActor* ValidCurrentTarget = nullptr;

	// 점수는 작을수록 화면 중앙에 가깝다.
	// 가장 큰 값으로 시작해야 첫 유효 후보가 비교를 통과한다.
	double BestScore = TNumericLimits<double>::Max();
	double CurrentScore = TNumericLimits<double>::Max();

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();

		// 파괴된 Actor, 플레이어 자신, 이미 검사한 Actor는 건너뛴다.
		if (!IsValid(Target) || Target == Pawn || Target->IsActorBeingDestroyed() || SeenActors.Contains(Target))
			continue;

		SeenActors.Add(Target);

		// Actor 전체가 숨겨졌거나 대상 태그가 없거나 비활성 태그가 있으면 제외한다.
		if (Target->IsHidden() || !Target->ActorHasTag(TargetTag) || Target->ActorHasTag(DisabledTag))
			continue;

		// 프로젝트 캐릭터는 기존 체력·사망 검사를 사용한다.
		// 일반 중요 오브젝트는 BaseCharacter가 아니므로 ASC가 없어도 다음 검사로 진행한다.
		const ALMBaseCharacter* Character = Cast<ALMBaseCharacter>(Target);
		if (Character && !Character->IsAliveForCombat())
			continue;

		const FVector Point = GetFocusPoint(Target);

		// Overlap은 충돌 부피가 구와 겹치는지만 판단한다.
		// 실제 표적점도 플레이어의 허용 거리 안에 있는지 별도로 확인한다.
		if (Point.ContainsNaN() || FVector::DistSquared(PawnPosition, Point) > RangeSquared)
			continue;

		FVector2D ScreenPosition;

		// 현재 카메라에서 표적점이 화면의 어느 픽셀에 놓이는지 계산한다.
		if (!FSceneView::ProjectWorldToScreen(Point, ViewRect, ViewMatrix, ScreenPosition) || ScreenPosition.ContainsNaN())
			continue;

		// 투영 좌표가 화면 사각형 밖이면 이번 후보에 넣지 않는다.
		// 기존 선택 대상이어도 이 검사에서 예외를 주지 않는다.
		if (ScreenPosition.X < ViewRect.Min.X || ScreenPosition.X >= ViewRect.Max.X || ScreenPosition.Y < ViewRect.Min.Y || ScreenPosition.Y >= ViewRect.Max.Y)
			continue;

		FHitResult Hit;

		// 카메라에서 표적점으로 선을 쏴 벽 같은 다른 Actor가 먼저 막는지 검사한다.
		// 대상 자신의 표면에 먼저 닿았다면 대상이 보이는 것으로 허용한다.
		if (World->LineTraceSingleByChannel(Hit, ProjectionData.ViewOrigin, Point, ECC_Visibility, QueryParams) && Hit.GetActor() != Target)
			continue;

		// 위 조건을 전부 통과한 뒤에야 Controller에 전달할 후보 배열에 넣는다.
		OutCandidates.Add(Target);

		// 예: 중심에서 108픽셀 떨어지고 화면 짧은 변이 1080이면 점수는 0.1이다.
		const double Score = (ScreenPosition - ViewCenter).Size() / ViewScale;

		if (Score < BestScore)
		{
			BestScore = Score;
			BestTarget = Target;
		}

		// 기존 선택도 모든 조건을 통과했을 때만 유지 비교에 참가시킨다.
		if (Target == CurrentTarget)
		{
			ValidCurrentTarget = Target;
			CurrentScore = Score;
		}
	}

	// 기존 0.20, 최선 0.19, 여유 0.03이면 기존 대상을 유지한다.
	// 기존 0.20, 최선 0.15라면 차이가 0.05이므로 최선 대상으로 전환한다.
	// 화면 밖으로 나간 기존 대상은 ValidCurrentTarget에 들어오지 못하므로 유지되지 않는다.
	if (ValidCurrentTarget && CurrentScore <= BestScore + SwitchMargin)
		return ValidCurrentTarget;

	// 유효 후보가 없었다면 BestTarget은 처음 값인 nullptr이다.
	return BestTarget;
}