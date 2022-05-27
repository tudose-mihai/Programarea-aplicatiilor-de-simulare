// Some copyright should be here...

using UnrealBuildTool;

public class Blt : ModuleRules
{
	public Blt(ReadOnlyTargetRules target) : base(target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Json"
		});
	}
}
