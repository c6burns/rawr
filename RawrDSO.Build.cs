// Fill out your copyright notice in the Description page of Project Settings.

using System;
using System.IO;
using UnrealBuildTool;

public class RawrDSO : ModuleRules
{
    string CMakeBuildType { get { return "Debug"; } }
	string SourceDirectory { get { return ModuleDirectory; } }
	string BuildDirectory { get { return Path.Combine(SourceDirectory, "CMakeBuild"); } }
	string OutputDirectory { get { return Path.Combine(BuildDirectory, CMakeBuildType); } }

    public RawrDSO(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        PrintBanner();
        CMakeConfigure(CMakeBuildType);
        CMakeBuild(CMakeBuildType);

        PublicDefinitions.Add("RAWR_BUILD_TYPE=\"" + CMakeBuildType + "\"");
        if (CMakeBuildType == "Debug")
        {
            PublicDefinitions.Add("RAWR_DEBUG=1");
        }

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
            RuntimeDependencies.Add("$(PluginDir)/Source/ThirdParty/rawrDSO/Mac/Release/librawrDSO.dylib");
        }
	}

	void PrintBanner()
	{
		Console.WriteLine(@"__________    _____  __      ____________");
		Console.WriteLine(@"\______   \  /  _  \/  \    /  \______   \");
		Console.WriteLine(@" |       _/ /  /_\  \   \/\/   /|       _/");
		Console.WriteLine(@" |    |   \/    |    \        / |    |   \");
		Console.WriteLine(@" |____|_  /\____|__  /\__/\  /  |____|_  /");
		Console.WriteLine(@"        \/         \/      \/          \/ ");
		Console.WriteLine(@"RAWR: VoIP Utilities for TLM Partners Inc.");
		Console.WriteLine(@"");
		Console.WriteLine(@"Please be patient while CMake builds ... ");
		Console.WriteLine(@"");
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
		Console.WriteLine("cmake " + args);
        try
        {
            System.Diagnostics.ProcessStartInfo procStartInfo = new System.Diagnostics.ProcessStartInfo("cmake", args);
            procStartInfo.RedirectStandardOutput = true;
            procStartInfo.UseShellExecute = false;
            procStartInfo.CreateNoWindow = true;
            System.Diagnostics.Process proc = new System.Diagnostics.Process();
            proc.StartInfo = procStartInfo;
            proc.Start();
			while (!proc.StandardOutput.EndOfStream)
			{
				Console.Write((char)proc.StandardOutput.Read());
			}
        }
        catch
        {
        }
    }
}
