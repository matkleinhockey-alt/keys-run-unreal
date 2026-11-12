using UnrealBuildTool;

public class KeysRunTarget : TargetRules
{
	public KeysRunTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("KeysRun");
	}
}
