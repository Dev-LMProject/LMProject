using UnrealBuildTool;

public class Lumia : ModuleRules
{
	public Lumia(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// 공개 헤더에서 사용하는 기본 타입과 엔진 클래스.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			 "UMG",
			 "GameplayTags",
		});

		// 입력, 카메라 시퀀스, 이동 애니메이션 계산에서 사용하는 모듈.
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InputCore",
			"EnhancedInput",
			"LevelSequence",
			"MovieScene",
			"AnimationLocomotionLibraryRuntime",
			"MotionWarping"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "MovieSceneTracks", "RenderCore" });
		}
	}
}
