using UnrealBuildTool;

public class KeysRun : ModuleRules
{
	public KeysRun(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseUnity = false;   // each .cpp keeps its own file-local helpers (Clamp, Lerp, ...)
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "ProceduralMeshComponent", "AudioMixer", "SignalProcessing", "WebSockets", "Json", "MeshDescription", "StaticMeshDescription" });
	}
}
