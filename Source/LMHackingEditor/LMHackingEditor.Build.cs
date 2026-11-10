using UnrealBuildTool;

public class LMHackingEditor : ModuleRules
{
    // 퍼즐 편집기 의존성을 에디터 모듈에만 연결한다.
    public LMHackingEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Lumia", "Slate", "SlateCore", "PropertyEditor", "UnrealEd" });
    }
}
