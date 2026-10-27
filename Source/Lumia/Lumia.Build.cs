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
			"Engine"
		});

		// 입력과 카메라 시퀀스 재생의 상세 구현은 cpp에서 사용한다.
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InputCore",
			"EnhancedInput",
			"LevelSequence",
			"MovieScene"
		});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "MovieSceneTracks", "RenderCore" });
		}
	}
}
