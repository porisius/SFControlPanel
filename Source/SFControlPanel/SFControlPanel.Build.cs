using UnrealBuildTool;
using System.IO;
using System;

public class SFControlPanel : ModuleRules
{
	
	private string ModulePath
	{
		get { return ModuleDirectory; }
	}
	
	private string ModPath
	{
		get { return Path.GetFullPath(Path.Combine(ModulePath, "../../")); }
	}


	private string ThirdPartyPath
	{
		get { return Path.GetFullPath(Path.Combine(ModulePath, "../ThirdParty/")); }
	}
	
	private void CopyToBinaries(string Filepath, ReadOnlyTargetRules Target)
	{
		string binariesDir = Path.Combine(ModPath, "Binaries", Target.Platform.ToString());
		string filename = Path.GetFileName(Filepath);

		if (!Directory.Exists(binariesDir))
			Directory.CreateDirectory(binariesDir);

		if (!File.Exists(Path.Combine(binariesDir, filename)))
			File.Copy(Filepath, Path.Combine(binariesDir, filename), true);
	}
	
	public SFControlPanel(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;

		// FactoryGame transitive dependencies
		// Not all of these are required, but including the extra ones saves you from having to add them later.
		// Some entries are commented out to avoid compile-time warnings about depending on a module that you don't explicitly depend on.
		// You can uncomment these as necessary when your code actually needs to use them.
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject",
			"Engine",
			"DeveloperSettings",
			"PhysicsCore",
			"InputCore",
			//"OnlineSubsystem", "OnlineSubsystemUtils", "OnlineSubsystemNull",
			//"SignificanceManager",
			"GeometryCollectionEngine",
			//"ChaosVehiclesCore", "ChaosVehicles", "ChaosSolverEngine",
			"AnimGraphRuntime",
			//"AkAudio",
			"AssetRegistry",
			"NavigationSystem",
			//"ReplicationGraph",
			"AIModule",
			"GameplayTasks",
			"SlateCore", "Slate", "UMG",
			//"InstancedSplines",
			"RenderCore",
			"CinematicCamera",
			"Foliage",
			//"Niagara",
			//"EnhancedInput",
			//"GameplayCameras",
			//"TemplateSequence",
			"NetCore",
			"GameplayTags",
			"Json", "JsonUtilities"
		});

		// FactoryGame plugins
		PublicDependencyModuleNames.AddRange(new string[] {
			//"AbstractInstance",
			//"InstancedSplinesComponent",
			//"SignificanceISPC"
		});

		// Header stubs
		PublicDependencyModuleNames.AddRange(new string[] {
			"DummyHeaders",
		});

		if (Target.Type == TargetRules.TargetType.Editor) {
			PublicDependencyModuleNames.AddRange(new string[] {/*"OnlineBlueprintSupport",*/ "AnimGraph"});
		}
		PublicDependencyModuleNames.AddRange(new string[] {"FactoryGame", "SML"});
		
		PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "uWebSockets"));
		PrivateIncludePaths.Add(Path.Combine(ThirdPartyPath, "uWebSockets"));
		
		PublicDefinitions.Add("UWS_STATICLIB");
		
		// Add uWebSockets
		LoaduWebSockets(Target);
		
		// Enable exception handling
		bEnableExceptions = true;
		
		PublicIncludePaths.AddRange(new string[] {
			// ... add public include paths required here ...
		});
		
		PrivateIncludePaths.AddRange(new string[] {
			// ... add private include paths required here ...
		});
		
		PublicDependencyModuleNames.AddRange(new string[] {
			// ... add public dependencies that you statically link with here ...
		});
		
		PrivateDependencyModuleNames.AddRange(new string[] {
			// ... add private dependencies that you statically link with here ...	
		});
		
		DynamicallyLoadedModuleNames.AddRange(new string[] {
			// ... add any modules that your module loads dynamically here ...
		});
	}

	public bool LoaduWebSockets(ReadOnlyTargetRules Target)
	{
		bool isLibrarySupported = false;
		string LibrariesPath = Path.Combine(ThirdPartyPath, "uWebSockets", "lib");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			isLibrarySupported = true;

			PublicAdditionalLibraries.AddRange(new string[] {
				Path.Combine(LibrariesPath, "uSockets.lib"),
				Path.Combine(LibrariesPath, "uv.lib"),
				Path.Combine(LibrariesPath, "zlib.lib")
			});
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			isLibrarySupported = true;

			PublicAdditionalLibraries.AddRange(new string[] {
				Path.Combine(LibrariesPath, "libuSockets.a"),
				Path.Combine(LibrariesPath, "libuv.a"),
				Path.Combine(LibrariesPath, "libz.a")
			});
		}

		RuntimeDependencies.Add("$(BinaryOutputDir)/uv.dll", Path.Combine(LibrariesPath, "uv.dll"));
		RuntimeDependencies.Add("$(BinaryOutputDir)/zlib1.dll", Path.Combine(LibrariesPath, "zlib1.dll"));

		CopyToBinaries(Path.Combine(LibrariesPath, "zlib1.dll"), Target);
		CopyToBinaries(Path.Combine(LibrariesPath, "uv.dll"), Target);

		PublicDefinitions.Add(string.Format("WITH_UWEBSOCKETS_BINDING={0}", isLibrarySupported ? 1 : 0));

		return isLibrarySupported;
	}
}
