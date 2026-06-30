// LicenseSystem.Build.cs
using UnrealBuildTool;

public class LicenseSystem : ModuleRules
{
	public LicenseSystem(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new[]
		{
			"Core", "CoreUObject", "Engine",
			"HTTP",
			"Json",
			"JsonUtilities", "DeveloperSettings" // keep if you use FJsonObjectConverter etc.
		});

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Projects", "Sockets", "Networking"
		});

		// Editor-only deps must not be present for game/packaged targets
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new[]
			{
				"UnrealEd",
				"Slate", "SlateCore",  "DeveloperSettings"
			});
		}

	}
}