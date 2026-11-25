#include "Animation/MovieScene2DTransformSection.h"
#include "Animation/MovieScene2DTransformTrack.h"
#include "Animation/WidgetAnimation.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateBorderBrush.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Factories/BlueprintFactory.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "IAssetTools.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "LMUI/LMSequencePanelActor.h"
#include "LMUI/LMSequencePanelWidget.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "MovieScene.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

namespace
{
DEFINE_LOG_CATEGORY_STATIC(LogLMSequencePanelAssets, Log, All);

constexpr TCHAR PanelFolder[] = TEXT("/Game/_Sequence");
constexpr TCHAR PanelWidgetName[] = TEXT("WBP_SequencePanel1");
constexpr TCHAR PanelActorName[] = TEXT("BP_SequencePanel1");
constexpr TCHAR PanelMaterialName[] = TEXT("M_LM_SequencePanelHologram");
constexpr TCHAR PanelInstanceName[] = TEXT("MI_LM_SequencePanelHologram");

struct FLMPanelKey
{
	float Time;
	float Opacity;
	float ScaleY = 1.0f;
	float MoveY = 0.0f;
};

// 패키지 경로가 이미 사용 중이면 기존 애셋을 읽고 새 생성 대상으로 취급하지 않는다.
UObject* FindPanelAsset(const TCHAR* AssetName, bool& bExists)
{
	const FString PackagePath = FString::Printf(TEXT("%s/%s"), PanelFolder, AssetName);
	const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackagePath, AssetName);
	if (UObject* LoadedAsset = FindObject<UObject>(nullptr, *ObjectPath))
	{
		bExists = true;
		return LoadedAsset;
	}

	bExists = FPackageName::DoesPackageExist(PackagePath) || FindPackage(nullptr, *PackagePath) != nullptr;
	return bExists ? LoadObject<UObject>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn) : nullptr;
}

// 새로 만들어진 패키지만 디스크에 기록한다.
bool SavePanelAsset(UObject& Asset)
{
	UPackage* AssetPackage = Asset.GetOutermost();
	const FString Filename = FPackageName::LongPackageNameToFilename(AssetPackage->GetName(), FPackageName::GetAssetPackageExtension());
	if (IFileManager::Get().FileExists(*Filename))
	{
		UE_LOG(LogLMSequencePanelAssets, Error, TEXT("File already exists: %s"), *Filename);
		return false;
	}

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	const bool bSaved = UPackage::SavePackage(AssetPackage, &Asset, *Filename, SaveArgs);
	UE_CLOG(!bSaved, LogLMSequencePanelAssets, Error, TEXT("Could not save %s"), *Filename);
	return bSaved;
}

// 위젯을 디자이너 트리에 만들고 블루프린트 변수 정보를 등록한다.
template<typename WidgetType>
WidgetType* AddPanelWidget(UWidgetBlueprint& PanelBlueprint, const FName WidgetName, bool bExpose = false)
{
	WidgetType* NewWidget = PanelBlueprint.WidgetTree->ConstructWidget<WidgetType>(WidgetType::StaticClass(), WidgetName);
	NewWidget->bIsVariable = bExpose;
	PanelBlueprint.OnVariableAdded(WidgetName);
	return NewWidget;
}

// Overlay의 자식에 정렬과 기준 해상도 여백을 적용한다.
void PlacePanelWidget(UOverlay& PanelOverlay, UWidget& ChildWidget, const FMargin& Gap, EHorizontalAlignment AlignX, EVerticalAlignment AlignY)
{
	UOverlaySlot* ChildSlot = PanelOverlay.AddChildToOverlay(&ChildWidget);
	ChildSlot->SetPadding(Gap);
	ChildSlot->SetHorizontalAlignment(AlignX);
	ChildSlot->SetVerticalAlignment(AlignY);
}

// 텍스트 블록이 에디터에서 교체할 수 있는 글꼴과 색을 가진 채 트리에 들어가게 한다.
UTextBlock* AddPanelText(UWidgetBlueprint& PanelBlueprint, UOverlay& PanelOverlay, const FName WidgetName, UObject& FontAsset, float FontSize, FLinearColor TextColor, const FText& PreviewText, const FMargin& Gap, EHorizontalAlignment AlignX, EVerticalAlignment AlignY, bool bExpose)
{
	UTextBlock* Label = AddPanelWidget<UTextBlock>(PanelBlueprint, WidgetName, bExpose);
	Label->SetFont(FSlateFontInfo(&FontAsset, FontSize));
	Label->SetColorAndOpacity(FSlateColor(TextColor));
	Label->SetText(PreviewText);
	PlacePanelWidget(PanelOverlay, *Label, Gap, AlignX, AlignY);
	return Label;
}

// 960×540 전체에 배경, 외곽선, 진행 문구와 게이지를 배치한다.
UOverlay* BuildPanelTree(UWidgetBlueprint& PanelBlueprint, UObject& FontAsset, UTexture2D& FrameTexture)
{
	UCanvasPanel* Canvas = Cast<UCanvasPanel>(PanelBlueprint.WidgetTree->RootWidget);
	if (!Canvas)
	{
		Canvas = AddPanelWidget<UCanvasPanel>(PanelBlueprint, TEXT("Canvas_Root"));
		PanelBlueprint.WidgetTree->RootWidget = Canvas;
	}
	Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);

	// 네 변의 앵커를 사용해 위젯 컴포넌트의 고정 해상도를 다 채운다.
	UOverlay* PanelOverlay = AddPanelWidget<UOverlay>(PanelBlueprint, TEXT("Overlay_Root"));
	UCanvasPanelSlot* OverlaySlot = Canvas->AddChildToCanvas(PanelOverlay);
	OverlaySlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	OverlaySlot->SetOffsets(FMargin(0.0f));

	UImage* BackgroundImage = AddPanelWidget<UImage>(PanelBlueprint, TEXT("IMG_Background"));
	BackgroundImage->SetBrush(FSlateColorBrush(FLinearColor(0.001f, 0.003f, 0.008f, 0.76f)));
	PlacePanelWidget(*PanelOverlay, *BackgroundImage, FMargin(0.0f), HAlign_Fill, VAlign_Fill);

	// 테두리에도 상위 위젯의 투명도가 적용되도록 색을 브러시 틴트에 넣는다.
	const float LineX = 2.0f / FMath::Max(8, FrameTexture.GetSizeX());
	const float LineY = 2.0f / FMath::Max(8, FrameTexture.GetSizeY());
	const FLinearColor LineColor(0.13f, 0.62f, 0.86f, 0.90f);
	UImage* FrameImage = AddPanelWidget<UImage>(PanelBlueprint, TEXT("IMG_Frame"));
	FrameImage->SetBrush(FSlateBorderBrush(&FrameTexture, FMargin(LineX, LineY), FSlateColor(LineColor)));
	PlacePanelWidget(*PanelOverlay, *FrameImage, FMargin(2.0f), HAlign_Fill, VAlign_Fill);

	const FLinearColor WhiteBlue(0.79f, 0.94f, 1.0f, 1.0f);
	AddPanelText(PanelBlueprint, *PanelOverlay, TEXT("TXT_Title"), FontAsset, 40.0f, WhiteBlue, NSLOCTEXT("LMSequencePanel", "Title", "SYSTEM STATUS"), FMargin(64.0f, 53.0f, 64.0f, 0.0f), HAlign_Left, VAlign_Top, false);
	UTextBlock* TypingLabel = AddPanelText(PanelBlueprint, *PanelOverlay, TEXT("TXT_Typing"), FontAsset, 30.0f, WhiteBlue, NSLOCTEXT("LMSequencePanel", "TypingPreview", "INITIALIZING SYSTEM..."), FMargin(64.0f, 155.0f, 64.0f, 177.0f), HAlign_Fill, VAlign_Fill, true);
	TypingLabel->SetAutoWrapText(true);
	TypingLabel->SetLineHeightPercentage(1.20f);
	TypingLabel->SetClipping(EWidgetClipping::ClipToBoundsAlways);

	const FLinearColor Cyan(0.24f, 0.86f, 1.0f, 1.0f);
	AddPanelText(PanelBlueprint, *PanelOverlay, TEXT("TXT_Percent"), FontAsset, 32.0f, WhiteBlue, FText::FromString(TEXT("0%")), FMargin(64.0f, 417.0f, 64.0f, 0.0f), HAlign_Right, VAlign_Top, true);
	UProgressBar* DownloadBar = AddPanelWidget<UProgressBar>(PanelBlueprint, TEXT("PB_Download"), true);
	FProgressBarStyle BarStyle;
	BarStyle.SetBackgroundImage(FSlateColorBrush(FLinearColor(0.004f, 0.013f, 0.022f, 0.80f)));
	BarStyle.SetFillImage(FSlateColorBrush(FLinearColor::White));
	DownloadBar->SetWidgetStyle(BarStyle);
	DownloadBar->SetFillColorAndOpacity(Cyan);
	DownloadBar->SetBarFillType(EProgressBarFillType::LeftToRight);
	DownloadBar->SetPercent(0.0f);
	PlacePanelWidget(*PanelOverlay, *DownloadBar, FMargin(64.0f, 380.0f, 64.0f, 140.0f), HAlign_Fill, VAlign_Fill);
	AddPanelText(PanelBlueprint, *PanelOverlay, TEXT("TXT_Status"), FontAsset, 28.0f, Cyan, NSLOCTEXT("LMSequencePanel", "InProgress", "IN PROGRESS"), FMargin(64.0f, 422.0f, 64.0f, 0.0f), HAlign_Left, VAlign_Top, true);
	return PanelOverlay;
}

// UMG 디자이너에서 편집할 수 있는 키를 내부 Overlay에 기록한다.
void AddPanelAnimation(UWidgetBlueprint& PanelBlueprint, const UOverlay& PanelOverlay, FName ClipName, TConstArrayView<FLMPanelKey> Keys, bool bAnimateTransform)
{
	UWidgetAnimation* Clip = NewObject<UWidgetAnimation>(&PanelBlueprint, ClipName, RF_Transactional);
	Clip->SetDisplayLabel(ClipName.ToString());
	UMovieScene* ClipScene = NewObject<UMovieScene>(Clip, ClipName, RF_Transactional);
	Clip->MovieScene = ClipScene;
	const FFrameRate ClipRate(1000, 1);
	ClipScene->SetTickResolutionDirectly(ClipRate);
	ClipScene->SetDisplayRate(FFrameRate(100, 1));
	const FFrameNumber ClipEnd = ClipRate.AsFrameTime(Keys.Last().Time).RoundToFrame();
	const TRange<FFrameNumber> ClipRange(FFrameNumber(0), ClipEnd + 1);
	ClipScene->SetPlaybackRange(ClipRange);
	ClipScene->GetEditorData().WorkStart = 0.0;
	ClipScene->GetEditorData().WorkEnd = Keys.Last().Time;

	// 재생 시 각 인스턴스의 같은 이름을 가진 Overlay를 찾는다.
	const FGuid OverlayId = ClipScene->AddPossessable(PanelOverlay.GetName(), PanelOverlay.GetClass());
	FWidgetAnimationBinding& ClipBinding = Clip->AnimationBindings.AddDefaulted_GetRef();
	ClipBinding.WidgetName = PanelOverlay.GetFName();
	ClipBinding.AnimationGuid = OverlayId;

	UMovieSceneFloatTrack* OpacityTrack = ClipScene->AddTrack<UMovieSceneFloatTrack>(OverlayId);
	OpacityTrack->SetPropertyNameAndPath(TEXT("RenderOpacity"), TEXT("RenderOpacity"));
	UMovieSceneFloatSection* OpacitySection = CastChecked<UMovieSceneFloatSection>(OpacityTrack->CreateNewSection());
	OpacityTrack->AddSection(*OpacitySection);
	OpacitySection->SetRange(ClipRange);

	UMovieScene2DTransformSection* MoveSection = nullptr;
	if (bAnimateTransform)
	{
		UMovieScene2DTransformTrack* MoveTrack = ClipScene->AddTrack<UMovieScene2DTransformTrack>(OverlayId);
		MoveTrack->SetPropertyNameAndPath(TEXT("RenderTransform"), TEXT("RenderTransform"));
		MoveSection = CastChecked<UMovieScene2DTransformSection>(MoveTrack->CreateNewSection());
		MoveTrack->AddSection(*MoveSection);
		MoveSection->SetRange(ClipRange);
		MoveSection->SetMask(EMovieScene2DTransformChannel::Scale | EMovieScene2DTransformChannel::TranslationY);
	}

	for (const FLMPanelKey& Key : Keys)
	{
		const FFrameNumber KeyFrame = ClipRate.AsFrameTime(Key.Time).RoundToFrame();
		OpacitySection->GetChannel().AddLinearKey(KeyFrame, Key.Opacity);
		if (MoveSection)
		{
			MoveSection->Scale[0].AddLinearKey(KeyFrame, 1.0f);
			MoveSection->Scale[1].AddLinearKey(KeyFrame, Key.ScaleY);
			MoveSection->Translation[1].AddLinearKey(KeyFrame, Key.MoveY);
		}
	}

	PanelBlueprint.Animations.Add(Clip);
	PanelBlueprint.OnVariableAdded(ClipName);
}

// 기존 위젯 블루프린트를 재사용하거나 디자이너 트리를 가진 새 위젯을 생성한다.
UWidgetBlueprint* MakePanelWidget(IAssetTools& AssetTools)
{
	bool bExists = false;
	UObject* FoundAsset = FindPanelAsset(PanelWidgetName, bExists);
	if (bExists)
	{
		UWidgetBlueprint* FoundWidget = Cast<UWidgetBlueprint>(FoundAsset);
		if (FoundWidget && FoundWidget->ParentClass == ULMSequencePanelWidget::StaticClass() && FoundWidget->GeneratedClass && FoundWidget->Status != BS_Error)
			return FoundWidget;
		UE_LOG(LogLMSequencePanelAssets, Error, TEXT("Existing %s cannot be used as the panel widget."), PanelWidgetName);
		return nullptr;
	}

	UFont* MonoFont = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/DroidSansMono.DroidSansMono"));
	UTexture2D* FrameTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
	if (!MonoFont || !FrameTexture)
	{
		UE_LOG(LogLMSequencePanelAssets, Error, TEXT("Engine font DroidSansMono or brush texture WhiteSquareTexture was not found."));
		return nullptr;
	}

	UWidgetBlueprintFactory* PanelFactory = NewObject<UWidgetBlueprintFactory>();
	PanelFactory->ParentClass = ULMSequencePanelWidget::StaticClass();
	UWidgetBlueprint* NewPanel = Cast<UWidgetBlueprint>(AssetTools.CreateAsset(PanelWidgetName, PanelFolder, UWidgetBlueprint::StaticClass(), PanelFactory));
	if (!NewPanel) return nullptr;

	UOverlay* PanelOverlay = BuildPanelTree(*NewPanel, *MonoFont, *FrameTexture);
	const FLMPanelKey AppearKeys[] = { {0.0f, 0.0f, 0.05f, 20.0f}, {0.05f, 0.8f, 0.1f, 15.0f}, {0.1f, 0.3f, 0.6f, 10.0f}, {0.2f, 1.0f, 1.03f, 0.0f}, {0.3f, 1.0f, 1.0f, 0.0f} };
	const FLMPanelKey DisappearKeys[] = { {0.0f, 1.0f}, {0.08f, 0.72f, 0.78f, 3.0f}, {0.15f, 0.24f, 0.35f, 11.0f}, {0.22f, 0.0f, 0.05f, 20.0f} };
	const FLMPanelKey FlickerKeys[] = { {0.0f, 1.0f}, {0.04f, 0.45f}, {0.08f, 0.95f}, {0.12f, 0.72f}, {0.18f, 1.0f} };
	AddPanelAnimation(*NewPanel, *PanelOverlay, TEXT("ANIM_Appear"), AppearKeys, true);
	AddPanelAnimation(*NewPanel, *PanelOverlay, TEXT("ANIM_Disappear"), DisappearKeys, true);
	AddPanelAnimation(*NewPanel, *PanelOverlay, TEXT("ANIM_Flicker"), FlickerKeys, false);

	// 네이티브 BindWidget을 연결한 뒤 디자이너의 기본 치수를 저장한다.
	FKismetEditorUtilities::CompileBlueprint(NewPanel);
	if (!NewPanel->GeneratedClass || NewPanel->Status == BS_Error) return nullptr;
	UUserWidget* PanelDefaults = NewPanel->GeneratedClass->GetDefaultObject<UUserWidget>();
	PanelDefaults->DesignSizeMode = EDesignPreviewSizeMode::Custom;
	PanelDefaults->DesignTimeSize = FVector2D(960.0, 540.0);
	NewPanel->MarkPackageDirty();
	return SavePanelAsset(*NewPanel) ? NewPanel : nullptr;
}

// 프로젝트에 복제한 머티리얼의 기존 발광 결과에 세기 파라미터를 곱한다.
UMaterial* MakePanelMaterial()
{
	bool bExists = false;
	UObject* FoundAsset = FindPanelAsset(PanelMaterialName, bExists);
	if (bExists)
	{
		UMaterial* FoundMaterial = Cast<UMaterial>(FoundAsset);
		float Intensity = 0.0f;
		if (FoundMaterial && !FoundMaterial->bDisableDepthTest && FoundMaterial->GetScalarParameterValue(FMaterialParameterInfo(TEXT("EmissiveIntensity")), Intensity))
			return FoundMaterial;
		return nullptr;
	}

	UMaterialInterface* EnginePreset = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent_OneSided.Widget3DPassThrough_Translucent_OneSided"));
	if (!EnginePreset) return nullptr;
	UMaterial* EngineMaterial = EnginePreset->GetMaterial();
	if (!EngineMaterial || !EngineMaterial->HasEmissiveColorConnected()) return nullptr;

	// 수정 전 머티리얼이 디스크에 저장되지 않도록 새 패키지에 먼저 복제한다.
	UPackage* MaterialPackage = CreatePackage(*FString::Printf(TEXT("%s/%s"), PanelFolder, PanelMaterialName));
	UMaterial* NewMaterial = DuplicateObject<UMaterial>(EngineMaterial, MaterialPackage, FName(PanelMaterialName));
	if (!NewMaterial) return nullptr;
	NewMaterial->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
	NewMaterial->PreEditChange(nullptr);
	NewMaterial->BlendMode = EnginePreset->GetBlendMode();
	NewMaterial->TwoSided = EnginePreset->IsTwoSided();
	NewMaterial->bDisableDepthTest = false;

	UMaterialExpressionScalarParameter* IntensityNode = CastChecked<UMaterialExpressionScalarParameter>(UMaterialEditingLibrary::CreateMaterialExpression(NewMaterial, UMaterialExpressionScalarParameter::StaticClass(), -530, 260));
	IntensityNode->ParameterName = TEXT("EmissiveIntensity");
	IntensityNode->DefaultValue = 3.0f;

	// SlateUI와 Opacity 경로를 그대로 두고 Emissive 입력만 바꾼다.
	UMaterialExpressionMultiply* GlowNode = CastChecked<UMaterialExpressionMultiply>(UMaterialEditingLibrary::CreateMaterialExpression(NewMaterial, UMaterialExpressionMultiply::StaticClass(), -240, -85));
	FColorMaterialInput& GlowInput = NewMaterial->GetEditorOnlyData()->EmissiveColor;
	GlowNode->A = GlowInput;
	GlowNode->B.Connect(0, IntensityNode);
	GlowInput.Connect(0, GlowNode);

	NewMaterial->PostEditChange();
	NewMaterial->MarkPackageDirty();
	if (!SavePanelAsset(*NewMaterial)) return nullptr;
	FAssetRegistryModule::AssetCreated(NewMaterial);
	return NewMaterial;
}

// 위젯 컴포넌트에 적용할 발광 머티리얼 인스턴스를 생성한다.
UMaterialInstanceConstant* MakePanelMaterialInstance(IAssetTools& AssetTools)
{
	UMaterial* GlowMaterial = MakePanelMaterial();
	if (!GlowMaterial) return nullptr;

	bool bExists = false;
	UObject* FoundAsset = FindPanelAsset(PanelInstanceName, bExists);
	if (bExists)
	{
		UMaterialInstanceConstant* FoundInstance = Cast<UMaterialInstanceConstant>(FoundAsset);
		return FoundInstance && FoundInstance->GetMaterial() == GlowMaterial ? FoundInstance : nullptr;
	}

	UMaterialInstanceConstantFactoryNew* InstanceFactory = NewObject<UMaterialInstanceConstantFactoryNew>();
	InstanceFactory->InitialParent = GlowMaterial;
	UMaterialInstanceConstant* GlowInstance = Cast<UMaterialInstanceConstant>(AssetTools.CreateAsset(PanelInstanceName, PanelFolder, UMaterialInstanceConstant::StaticClass(), InstanceFactory));
	return GlowInstance && SavePanelAsset(*GlowInstance) ? GlowInstance : nullptr;
}

// 부모의 위젯 컴포넌트 기본값에 생성한 위젯과 발광 머티리얼을 연결한다.
bool MakePanelActor(IAssetTools& AssetTools, UClass& PanelWidgetClass, UMaterialInterface* GlowMaterial)
{
	bool bExists = false;
	UObject* FoundAsset = FindPanelAsset(PanelActorName, bExists);
	UBlueprint* ActorBlueprint = Cast<UBlueprint>(FoundAsset);
	if (!bExists)
	{
		UBlueprintFactory* ActorFactory = NewObject<UBlueprintFactory>();
		ActorFactory->ParentClass = ALMSequencePanelActor::StaticClass();
		ActorBlueprint = Cast<UBlueprint>(AssetTools.CreateAsset(PanelActorName, PanelFolder, UBlueprint::StaticClass(), ActorFactory));
		if (ActorBlueprint)
			FKismetEditorUtilities::CompileBlueprint(ActorBlueprint);
	}

	if (!ActorBlueprint || ActorBlueprint->ParentClass != ALMSequencePanelActor::StaticClass() || !ActorBlueprint->GeneratedClass || ActorBlueprint->Status == BS_Error)
	{
		UE_LOG(LogLMSequencePanelAssets, Error, TEXT("Could not use %s as the panel actor."), PanelActorName);
		return false;
	}

	UObject* ActorDefaults = ActorBlueprint->GeneratedClass->GetDefaultObject();
	UWidgetComponent* PanelComponent = Cast<UWidgetComponent>(ActorDefaults->GetDefaultSubobjectByName(TEXT("Widget_SystemPanel")));
	if (!PanelComponent) return false;
	if (bExists)
	{
		const bool bLinked = PanelComponent->GetWidgetClass() == &PanelWidgetClass;
		UE_CLOG(!bLinked, LogLMSequencePanelAssets, Error, TEXT("Existing %s uses a different widget class."), PanelActorName);
		return bLinked;
	}

	PanelComponent->SetWidgetClass(&PanelWidgetClass);
	if (GlowMaterial)
		PanelComponent->SetMaterial(0, GlowMaterial);
	ActorBlueprint->MarkPackageDirty();
	return SavePanelAsset(*ActorBlueprint);
}

// 패널 애셋 생성을 명시적으로 실행하고 저장된 결과를 에디터 로그에 남긴다.
void CreateSequencePanelAssets()
{
	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
	UWidgetBlueprint* PanelBlueprint = MakePanelWidget(AssetTools);
	if (!PanelBlueprint) return;

	UMaterialInterface* GlowMaterial = MakePanelMaterialInstance(AssetTools);
	UE_CLOG(!GlowMaterial, LogLMSequencePanelAssets, Warning, TEXT("Using the default translucent widget material; hologram material was unavailable."));
	if (MakePanelActor(AssetTools, *PanelBlueprint->GeneratedClass.Get(), GlowMaterial))
		UE_LOG(LogLMSequencePanelAssets, Display, TEXT("Panel assets ready: %s/%s, %s/%s"), PanelFolder, PanelWidgetName, PanelFolder, PanelActorName);
}

FAutoConsoleCommand CreateSequencePanelAssetsCommand(TEXT("LM.SequencePanel.CreateAssets"), TEXT("Create or find the cinematic sequence panel assets in /Game/_Sequence."), FConsoleCommandDelegate::CreateStatic(&CreateSequencePanelAssets));
}
