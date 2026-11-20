using UnrealBuildTool;

public class LMHackingEditor : ModuleRules
{
    // 퍼즐 편집기와 PIE 테스트 도구의 의존성을 에디터 모듈에 연결한다.
    public LMHackingEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Lumia", "Slate", "SlateCore", "PropertyEditor", "UnrealEd", "GameplayAbilities", "GameplayTags" });
    }
}
