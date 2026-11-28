#include "LMPuzzle/LMTerrariumPuzzle.h"

#include "Components/BoxComponent.h"
#include "LMComponent/LMInteractionComponent.h"
#include "LMGameplayTags/LMGameplayTags.h"
#include "TimerManager.h"

// 감지 크기와 콘솔 외형은 방의 자식 BP에서 맞춘다.
ALMTerrariumPuzzle::ALMTerrariumPuzzle()
{
	UseZone = CreateDefaultSubobject<UBoxComponent>(TEXT("UseZone"));
	SetRootComponent(UseZone);
	Interaction = CreateDefaultSubobject<ULMInteractionComponent>(TEXT("Interaction"));
	Interaction->InteractionName = NSLOCTEXT("LMTerrarium", "CheckConsole", "기동 점검");
}

// 잘못 연결된 장치는 닫아 두고 두 밸브가 준비된 경우에만 조작을 연다.
void ALMTerrariumPuzzle::BeginPlay()
{
	Super::BeginPlay();
	Interaction->SetInteractionTags({});
	Interaction->SetDetectionCollision(UseZone);
	ShowPanel(ELMTerrariumPanel::BrokenFilter);

	// 두 장치가 모두 유효할 때만 E 입력을 열어 일부만 작동하는 배치를 피한다.
	for (AActor* Valve : {FilterValve.Get(), ReturnValve.Get()})
	{
		if (!IsValid(Valve) || Valve == this || FilterValve == ReturnValve || Valve->GetLevel() != GetLevel() || !Valve->Implements<ULMInteractable>() || !Valve->FindComponentByClass<ULMInteractionComponent>())
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: assign two distinct interactable valves from this level."), *GetPathName());
			return;
		}
	}

	for (AActor* Valve : {FilterValve.Get(), ReturnValve.Get()})
	{
		// 밸브 BP는 자신의 소유 콘솔로 요청을 전달한다.
		Valve->SetOwner(this);
		Valve->FindComponentByClass<ULMInteractionComponent>()->SetInteractionTags(FGameplayTagContainer(LMGameplayTags::Interaction_Actor_ActiveTarget));
	}

	// 방 인스턴스 안의 서로 다른 세 판만 같은 콘솔에 연결한다.
	bool bValidOrder = PlateOrder.Num() == 3;
	for (int32 Index = 0; bValidOrder && Index < PlateOrder.Num(); ++Index)
	{
		AActor* Plate = PlateOrder[Index];
		bValidOrder = IsValid(Plate) && Plate != this && Plate != FilterValve && Plate != ReturnValve && Plate->GetLevel() == GetLevel() && PlateOrder.Find(Plate) == Index;
	}
	if (!bValidOrder)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: assign three distinct pressure plates in playback order."), *GetPathName());
		PlateOrder.Reset();
		return;
	}
	for (AActor* Plate : PlateOrder)
		Plate->SetOwner(this);
}

// 마지막으로 선택한 배관 조합을 한 번 판정하고 정상 순환일 때만 조작 대상을 바꾼다.
bool ALMTerrariumPuzzle::TryTurnValve(AActor* Valve, bool& bTurned)
{
	if (!IsValid(FilterValve) || !IsValid(ReturnValve) || (bUseSpareFilter && bUseReturnPipe))
	{
		return false;
	}
	if (Valve != FilterValve && Valve != ReturnValve) return false;

	bool& bSelectedPath = Valve == FilterValve ? bUseSpareFilter : bUseReturnPipe;
	bSelectedPath = !bSelectedPath;
	bTurned = bSelectedPath;

	// 성공을 확정한 뒤 HUD 후보를 바꿔 재입력으로 순환이 끊기지 않게 한다.
	if (bUseSpareFilter && bUseReturnPipe)
	{
		for (AActor* LockedValve : {FilterValve.Get(), ReturnValve.Get()})
		{
			if (ULMInteractionComponent* ValveInput = LockedValve->FindComponentByClass<ULMInteractionComponent>())
				ValveInput->SetInteractionTags({});
		}
		Interaction->SetInteractionTags(FGameplayTagContainer(LMGameplayTags::Interaction_Actor_ActiveTarget));
		ShowPanel(ELMTerrariumPanel::Ready);
	}
	else
	{
		ShowPanel(bUseSpareFilter ? ELMTerrariumPanel::NoReturnPressure : ELMTerrariumPanel::BrokenFilter);
	}
	return true;
}

// E로 처음부터 다시 볼 때 이전에 밟은 순서도 비운다.
bool ALMTerrariumPuzzle::Interact_Implementation(AActor* Interactor)
{
	if (!bUseSpareFilter || !bUseReturnPipe) return false;
	if (PlatePhase == EPlatePhase::Done || PlatePhase == EPlatePhase::Showing) return false;

	ShowPanel(ELMTerrariumPanel::Confirmed);
	if (PlateOrder.IsEmpty())
	{
		// 감압판이 없는 기존 배치에서는 준비 확인까지만 제공한다.
		Interaction->SetIsBusy(false);
		return true;
	}
	PressedPlates.Reset();
	PlatePhase = EPlatePhase::Showing;
	LightStep = 0;
	PlayNextLight();
	return true;
}

// 각 판의 점등과 소등 사이에 간격을 두어 순서를 구분할 수 있게 한다.
void ALMTerrariumPuzzle::PlayNextLight()
{
	const int32 PlateIndex = LightStep / 2;
	const bool bOn = LightStep % 2 == 0;
	SetPlateLight(PlateOrder[PlateIndex], bOn);
	++LightStep;
	if (LightStep == PlateOrder.Num() * 2)
	{
		PlatePhase = EPlatePhase::Input;
		Interaction->SetIsBusy(false);
		return;
	}
	constexpr float LightSeconds = 0.75f;
	constexpr float GapSeconds = 0.3f;
	GetWorldTimerManager().SetTimer(LightTimer, this, &ALMTerrariumPuzzle::PlayNextLight, bOn ? LightSeconds : GapSeconds, false);
}

// 세 번을 모두 밟은 뒤에만 판정하여 중간에 정답을 드러내지 않는다.
void ALMTerrariumPuzzle::PressPlate(AActor* Plate)
{
	if (PlatePhase != EPlatePhase::Input || !PlateOrder.Contains(Plate)) return;
	PressedPlates.Add(Plate);
	if (PressedPlates.Num() < PlateOrder.Num()) return;

	if (PressedPlates == PlateOrder)
	{
		PlatePhase = EPlatePhase::Done;
		Interaction->SetInteractionTags({});
		ShowPanel(ELMTerrariumPanel::Complete);
	}
	else
	{
		// 밸브가 고정된 상태에서 바로 다시 밟거나 E로 순서를 다시 볼 수 있다.
		ShowPanel(ELMTerrariumPanel::Ready);
	}
	PressedPlates.Reset();
}
