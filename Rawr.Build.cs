// Fill out your copyright notice in the Description page of Project Settings.

using System.IO;
using UnrealBuildTool;

public class Rawr : ModuleRules
{
	string SourceDirectory { get { return ModuleDirectory; } }
	string BuildDirectory { get { return Path.Combine(SourceDirectory, "CMakeBuild"); } }
	string OutputDirectory { get { return Path.Combine(BuildDirectory, "Release"); } }

	public Rawr(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		const string buildType = "Release";
		PrintBanner();
		CMakeConfigure(buildType);
		CMakeBuild(buildType);

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Add the import library
			PublicAdditionalLibraries.Add(Path.Combine(OutputDirectory, "rawrDSO.lib"));

			// Delay-load the DLL, so we can load it from the right place first
			PublicDelayLoadDLLs.Add("rawrDSO.dll");

			// Ensure that the DLL is staged along with the executable
			RuntimeDependencies.Add(Path.Combine(OutputDirectory, "rawrDSO.dll"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PublicDelayLoadDLLs.Add(Path.Combine(ModuleDirectory, "Mac", "Release", "librawrDSO.dylib"));
            RuntimeDependencies.Add("$(PluginDir)/Source/ThirdParty/Test3PLibrary/Mac/Release/librawrDSO.dylib");
        }
	}

	void PrintBanner()
	{
		System.Console.WriteLine(@"__________    _____  __      ____________");
		System.Console.WriteLine(@"\______   \  /  _  \/  \    /  \______   \");
		System.Console.WriteLine(@" |       _/ /  /_\  \   \/\/   /|       _/");
		System.Console.WriteLine(@" |    |   \/    |    \        / |    |   \");
		System.Console.WriteLine(@" |____|_  /\____|__  /\__/\  /  |____|_  /");
		System.Console.WriteLine(@"        \/         \/      \/          \/ ");
		System.Console.WriteLine(@"RAWR: VoIP Utilities for TLM Partners Inc.");
		System.Console.WriteLine(@"");
		System.Console.WriteLine(@"Please be patient while CMake builds ... ");
		System.Console.WriteLine(@"");
	}

	void CMakeConfigure(string buildType)
	{
		if (!Directory.Exists(BuildDirectory)) Directory.CreateDirectory(BuildDirectory);
		string arguments = 	" -G \"Visual Studio 16 2019\"" +
							" -S " + SourceDirectory +
							" -B " + BuildDirectory +
							" -A x64 " +
							" -T host=x64" +
							" -DCMAKE_BUILD_TYPE=" + buildType;

		ExecuteCMakeCommand(arguments);
	}

	void CMakeBuild(string buildType)
	{
		ExecuteCMakeCommand("--build " + BuildDirectory + " --config " + buildType);
	}

	void ExecuteCMakeCommand(string args)
	{
		System.Console.WriteLine("cmake " + args);
        try
        {
			char[] buffer;
            System.Diagnostics.ProcessStartInfo procStartInfo = new System.Diagnostics.ProcessStartInfo("cmake", args);
            procStartInfo.RedirectStandardOutput = true;
            procStartInfo.UseShellExecute = false;
            procStartInfo.CreateNoWindow = true;
            System.Diagnostics.Process proc = new System.Diagnostics.Process();
            proc.StartInfo = procStartInfo;
            proc.Start();
			while (!proc.StandardOutput.EndOfStream)
			{
				System.Console.Write((char)proc.StandardOutput.Read());
			}
        }
        catch
        {
        }
    }
}
