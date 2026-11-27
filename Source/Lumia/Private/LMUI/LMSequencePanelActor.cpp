#include "LMUI/LMSequencePanelActor.h"

#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/CollisionProfile.h"
#include "LMUI/LMSequencePanelWidget.h"

// 월드 위젯을 배치하고 장면의 물리 및 입력과 분리한다.
ALMSequencePanelActor::ALMSequencePanelActor()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("DefaultSceneRoot")));

	// 컴포넌트가 인스턴스별 위젯을 소유하며 월드의 깊이 가림을 사용한다.
	Widget_SystemPanel = CreateDefaultSubobject<UWidgetComponent>(TEXT("Widget_SystemPanel"));
	Widget_SystemPanel->SetupAttachment(GetRootComponent());
	Widget_SystemPanel->SetWidgetSpace(EWidgetSpace::World);
	Widget_SystemPanel->SetDrawSize(FVector2D(960.0, 540.0));
	Widget_SystemPanel->SetDrawAtDesiredSize(false);
	Widget_SystemPanel->SetBlendMode(EWidgetBlendMode::Transparent);
	Widget_SystemPanel->SetManuallyRedraw(false);
	Widget_SystemPanel->SetRedrawTime(0.0f);
	Widget_SystemPanel->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Widget_SystemPanel->SetGenerateOverlapEvents(false);
	Widget_SystemPanel->SetWindowFocusable(false);
	Widget_SystemPanel->SetCastShadow(false);
	Widget_SystemPanel->SetRelativeScale3D(FVector(0.1));

	// 카메라 컷 직전에도 최신 위젯 렌더 타깃을 유지한다.
	Widget_SystemPanel->SetTickWhenOffscreen(true);
	Widget_SystemPanel->SetEditTimeUsable(true);
}

// 타이핑 트랙의 값이 적용되는 시점에 화면을 갱신한다.
void ALMSequencePanelActor::SetTypingAlpha(float _TypingAlpha)
{
	TypingAlpha = FMath::Clamp(_TypingAlpha, 0.0f, 1.0f);
	RefreshPanel();
}

// 다운로드 트랙의 값이 적용되는 시점에 화면을 갱신한다.
void ALMSequencePanelActor::SetDownloadAlpha(float _DownloadAlpha)
{
	DownloadAlpha = FMath::Clamp(_DownloadAlpha, 0.0f, 1.0f);
	RefreshPanel();
}

// 투명도 트랙의 값이 적용되는 시점에 화면을 갱신한다.
void ALMSequencePanelActor::SetPanelOpacity(float _PanelOpacity)
{
	PanelOpacity = FMath::Clamp(_PanelOpacity, 0.0f, 1.0f);
	RefreshPanel();
}

// 현재 프레임에서 재생할 위젯 애니메이션을 선택한다.
void ALMSequencePanelActor::SetWidgetAnimation(ELMSequencePanelAnimation _WidgetAnimation)
{
	WidgetAnimation = _WidgetAnimation;
	RefreshPanel();
}

// 시퀀서가 전달한 위치로 위젯 애니메이션을 이동시킨다.
void ALMSequencePanelActor::SetWidgetAnimationAlpha(float _WidgetAnimationAlpha)
{
	WidgetAnimationAlpha = FMath::Clamp(_WidgetAnimationAlpha, 0.0f, 1.0f);
	RefreshPanel();
}

// 컴포넌트가 소유한 위젯을 찾고 변경된 상태만 적용한다.
void ALMSequencePanelActor::RefreshPanel()
{
	if (IsTemplate() || !Widget_SystemPanel || !Widget_SystemPanel->IsRegistered())
	{
		return;
	}

	// 에디터에서 컴포넌트나 클래스가 바뀌면 참조를 다시 연결한다.
	if (!Widget_SystemPanel->GetUserWidgetObject())
		Widget_SystemPanel->InitWidget();
	ULMSequencePanelWidget* CurrentWidget = Cast<ULMSequencePanelWidget>(Widget_SystemPanel->GetUserWidgetObject());
	const bool bNewWidget = WidgetRef != CurrentWidget;
	WidgetRef = CurrentWidget;
	if (!IsValid(WidgetRef.Get())) return;

	const float NewTyping = FMath::Clamp(TypingAlpha, 0.0f, 1.0f);
	const float NewDownload = FMath::Clamp(DownloadAlpha, 0.0f, 1.0f);
	const float NewOpacity = FMath::Clamp(PanelOpacity, 0.0f, 1.0f);
	const bool bTypingChanged = bNewWidget || NewTyping != LastTypingAlpha || !TypingText.Equals(LastTypingText, ESearchCase::CaseSensitive);
	const bool bDownloadChanged = bNewWidget || NewDownload != LastDownloadAlpha;
	const bool bOpacityChanged = bNewWidget || NewOpacity != LastPanelOpacity;
	const float NewAnimationAlpha = FMath::Clamp(WidgetAnimationAlpha, 0.0f, 1.0f);
	const bool bAnimationChanged = bNewWidget || WidgetAnimation != LastWidgetAnimation || NewAnimationAlpha != LastWidgetAnimationAlpha;

	// 진행률이 같아도 문구를 바꾸면 새 문구의 현재 부분을 표시한다.
	if (bTypingChanged)
		WidgetRef->UpdateTypingFromText(TypingText, NewTyping);
	if (bDownloadChanged)
		WidgetRef->UpdateDownload(NewDownload);
	// 외곽 위젯에 전체 투명도를 적용해 내부 Overlay 애니메이션과 합성한다.
	if (bOpacityChanged)
		WidgetRef->SetRenderOpacity(NewOpacity);
	if (bAnimationChanged)
		WidgetRef->UpdateWidgetAnimation(WidgetAnimation, NewAnimationAlpha);

	LastTypingAlpha = NewTyping;
	if (bTypingChanged)
		LastTypingText = TypingText;
	LastDownloadAlpha = NewDownload;
	LastPanelOpacity = NewOpacity;
	LastWidgetAnimation = WidgetAnimation;
	LastWidgetAnimationAlpha = NewAnimationAlpha;
	if (bTypingChanged || bDownloadChanged || bOpacityChanged || bAnimationChanged)
		Widget_SystemPanel->RequestRenderUpdate();
}

// 레벨 에디터에서 등록된 컴포넌트에 표시할 값을 전달한다.
void ALMSequencePanelActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	RefreshPanel();
}

// 런타임 컴포넌트의 초기화가 끝나면 현재 상태를 적용한다.
void ALMSequencePanelActor::BeginPlay()
{
	Super::BeginPlay();
	RefreshPanel();
}

#if WITH_EDITOR
// Details 패널의 변경이나 에디터에서 컴포넌트를 다시 만들었을 때 값을 반영한다.
void ALMSequencePanelActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RefreshPanel();
}
#endif
