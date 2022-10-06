// Fill out your copyright notice in the Description page of Project Settings.

using System;
using System.IO;
using System.Diagnostics;
using System.Text;
using UnrealBuildTool;

public class EngineSim : ModuleRules
{
    private string BoostVersion
    {
        get { return "1_70_0"; }
    }

    private string BoostPath
    {
        get { return "C:/local/boost_1_70_0"; }
    }

    private string ModulePath
	{
		get { return ModuleDirectory; }
	}

	private string ThirdPartyPath
	{
		get
		{
			return Path.GetFullPath(Path.Combine(ModulePath,"../"));
		}
	}

	public EngineSim(ReadOnlyTargetRules Target) : base(Target)
	{
        Type = ModuleType.External;

        const string buildType = "Release";
        var buildDirectory = "engine-sim/build/" + buildType;
        var buildPath = Path.Combine(ModulePath, buildDirectory);

        String EngineSimLibPath = Path.Combine(buildPath, buildType, "engine-sim.lib");
        String EngineSimScriptInterpreterLibPath = Path.Combine(buildPath, buildType, "engine-sim-script-interpreter.lib");
        String ConstraintResolverLibPath = Path.Combine(buildPath, "dependencies/submodules/simple-2d-constraint-solver", buildType, "simple-2d-constraint-solver.lib");
        String PiranhaLibPath = Path.Combine(buildPath, "dependencies/submodules/piranha", buildType, "piranha.lib");
        if (!File.Exists(EngineSimLibPath)
            || !File.Exists(EngineSimScriptInterpreterLibPath)
            || !File.Exists(ConstraintResolverLibPath)
            || !File.Exists(PiranhaLibPath))
        {
            var configureCommand = CreateCMakeBuildCommand(Target, buildPath, buildType);
            var configureCode = ExecuteCommandSync(configureCommand);
            if (configureCode != 0)
            {
                String error = "Cannot configure CMake project. Exited with code: "
                    + configureCode;
                System.Console.WriteLine(error);
                throw new BuildException(error);
            }

            var installCommand = CreateCMakeInstallCommand(buildPath, buildType);
            var buildCode = ExecuteCommandSync(installCommand);
            if (buildCode != 0)
            {
                String error = "Cannot build project. Exited with code: " + buildCode;
                System.Console.WriteLine(error);
                throw new BuildException(error);
            }
        }

        PublicDefinitions.Add("ATG_ENGINE_SIM_PIRANHA_ENABLED=1");

        PublicIncludePaths.Add(Path.Combine(ModulePath, "engine-sim/include"));
        PublicIncludePaths.Add(Path.Combine(ModulePath, "engine-sim/scripting/include"));
        PublicIncludePaths.Add(Path.Combine(ModulePath, "engine-sim/dependencies/submodules"));
        PublicAdditionalLibraries.Add(EngineSimLibPath);
        PublicAdditionalLibraries.Add(EngineSimScriptInterpreterLibPath);
        PublicAdditionalLibraries.Add(ConstraintResolverLibPath);
        PublicAdditionalLibraries.Add(PiranhaLibPath);
        LinkBoost(Target);
    }
	
	private string GetGeneratorName(WindowsCompiler compiler)
    {
        string generatorName="";

        switch(compiler)
        {
        case WindowsCompiler.Default:
        break;
        case WindowsCompiler.Clang:
            generatorName="NMake Makefiles";
        break;
        case WindowsCompiler.Intel:
            generatorName="NMake Makefiles";
        break;
        case WindowsCompiler.VisualStudio2019:
            generatorName="Visual Studio 16 2019";
        break;
        case WindowsCompiler.VisualStudio2022:
            generatorName="Visual Studio 17 2022";
        break;
        }

        return generatorName;
    }
	
	private string CreateCMakeBuildCommand(ReadOnlyTargetRules target, string buildDirectory, string buildType)
	{
		const string program = "cmake.exe";
		var rootDirectory = ModulePath;
		var installPath = Path.Combine(rootDirectory, "../../../Intermediate/Build/Win64/", "engine-sim");
		var sourceDir = Path.Combine(rootDirectory, "engine-sim");

		string generator = GetGeneratorName(target.WindowsPlatform.Compiler);

		var arguments = " -G \"" + generator + "\"" +
						" -S \"" + sourceDir + "\"" +
						" -B \"" + buildDirectory + "\"" +
                        " -A x64 " +
						" -T host=x64" +
                        " -Wno-dev " +
                        AddFlexDeps() +
                        AddBisonDeps() +
                        AddBoost(target) +
                        " -DDISCORD_ENABLED=OFF" +
                        " -DPIRANHA_ENABLED=ON" +
                        " -DBUILD_APP=OFF" +
                        " -DCMAKE_BUILD_TYPE=" + buildType +
						" -DCMAKE_INSTALL_PREFIX=\"" + installPath + "\"";

		return program + arguments;
	}

    private string AddFlexDeps()
    {
        var flexFolder = "win_flex_bison3-latest";
        var rootDirectory = ModulePath;
        return " -DFLEX_EXECUTABLE=\"" + Path.Combine(rootDirectory, "engine-sim/_deps", flexFolder, "win_flex.exe") + "\"";
    }

    private string AddBisonDeps()
    {
        var bisonFolder = "win_flex_bison3-latest";
        var rootDirectory = ModulePath;
        return " -DBISON_EXECUTABLE=\"" + Path.Combine(rootDirectory, "engine-sim/_deps", bisonFolder, "win_bison.exe") + "\"";
    }

    private string AddBoost(ReadOnlyTargetRules target)
    {
        String outString = "";

        string BoostVersionDir = "boost-" + BoostVersion;

        string BoostIncludePath = Path.Combine(BoostPath);
        outString += " -DBoost_INCLUDE_DIR=\"" + BoostIncludePath + "\"";

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string BoostToolsetVersion = "vc142";

            string BoostLibPath = Path.Combine(BoostPath, "lib64-msvc-14.2");

            outString += " -DBOOST_LIB_TOOLSET=\"" + BoostToolsetVersion + "\"";
            outString += " -DBOOST_ALL_NO_LIB=1";
            outString += " -DBOOST_LIBRARYDIR=\"" + BoostLibPath + "\"";
        }

        return outString;
    }

    private void LinkBoost(ReadOnlyTargetRules target)
    {
        string[] BoostLibraries = { "filesystem" };

        string BoostVersionDir = "boost-" + BoostVersion;

        string BoostIncludePath = Path.Combine(BoostPath);

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string BoostToolsetVersion = "vc142";

            string BoostLibPath = Path.Combine(BoostPath, "lib64-msvc-14.2");
            string BoostVersionShort = BoostVersion.Substring(BoostVersion.Length - 2) == "_0" ? BoostVersion.Substring(0, BoostVersion.Length - 2) : BoostVersion;

            foreach (string BoostLib in BoostLibraries)
            {
                string BoostLibName = "libboost_" + BoostLib + "-" + BoostToolsetVersion + "-mt-x64" + "-" + BoostVersionShort;
                PublicAdditionalLibraries.Add(Path.Combine(BoostLibPath, BoostLibName + ".lib"));
            }
        }
    }

    private string CreateCMakeInstallCommand(string buildDirectory, string buildType)
    {
        return "cmake.exe --build \"" + buildDirectory + "\"" +
               " --target install --config " + buildType;
    }
	
	private int ExecuteCommandSync(string command)
    {
        System.Console.WriteLine("Running: "+command);
        var processInfo = new ProcessStartInfo("cmd.exe", "/c "+command)
        {
            CreateNoWindow=true,
            UseShellExecute=false,
            RedirectStandardError=true,
            RedirectStandardOutput=true,
            WorkingDirectory=ModulePath
		};

        StringBuilder outputString = new StringBuilder();
        Process p = Process.Start(processInfo);

        p.OutputDataReceived+=(sender, args) => {outputString.Append(args.Data); System.Console.WriteLine(args.Data);};
        p.ErrorDataReceived+=(sender, args) => {outputString.Append(args.Data); System.Console.WriteLine(args.Data);};
        p.BeginOutputReadLine();
        p.BeginErrorReadLine();
        p.WaitForExit();

        if(p.ExitCode != 0)
        {
             Console.WriteLine(outputString);
        }
        return p.ExitCode;
    }
}
