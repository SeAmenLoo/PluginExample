using UnrealBuildTool;
using System.IO;

public class UEIRInterchange : ModuleRules
{
	public UEIRInterchange(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.None;
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Json",
				"JsonUtilities"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",
				"AssetRegistry",
				"BlueprintGraph",
				"ContentBrowser",
				"DesktopPlatform",
				"Kismet",
				"KismetCompiler",
				"Projects",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"LevelEditor"
			}
		);

		bool bHasNiagara = false;
		try
		{
			bHasNiagara =
				Directory.Exists(Path.Combine(EngineDirectory, "Plugins", "FX", "Niagara")) ||
				Directory.Exists(Path.Combine(EngineDirectory, "Source", "Runtime", "Niagara")) ||
				Directory.Exists(Path.Combine(EngineDirectory, "Source", "Runtime", "Experimental", "Niagara"));
		}
		catch
		{
			bHasNiagara = false;
		}

		if (Target.bBuildEditor && bHasNiagara)
		{
			PrivateDependencyModuleNames.Add("Niagara");
			PublicDefinitions.Add("UEIR_WITH_NIAGARA=1");
		}
		else
		{
			PublicDefinitions.Add("UEIR_WITH_NIAGARA=0");
		}
	}
}
