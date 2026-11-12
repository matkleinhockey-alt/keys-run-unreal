using UnrealBuildTool;

public class KeysRunEditorTarget : TargetRules
{
	public KeysRunEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("KeysRun");
	}
}
