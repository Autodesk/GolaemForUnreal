/***************************************************************************
*                                                                          *
*  Copyright (C) Golaem S.A.  All Rights Reserved.                         *
*                                                                          *
***************************************************************************/

using System;
using System.IO;
using UnrealBuildTool;
using System.Collections.Generic;

namespace Golaem
{
    //////////////////////////////////////////
    // Build Parameters
    //////////////////////////////////////////
    public class GolaemParameters
    {
        // choose either GolaemPathPrefix to which will be added GolaemCrowd or GolaemCrowdDBG according to configuration (release or debug) or GolaemInstallPath which is the full install path
        // @Default
        public static string GolaemPathPrefix = "";
        public static string GolaemInstallPath = @"C:\Golaem\Golaem-X.Y.Z-MayaYYYY\";
    }

    //////////////////////////////////////////
    // Golaem Build Utils
    //////////////////////////////////////////
    public class GolaemMacros
    {
        // Returns the module path
        public static string GetModulePath(ModuleRules Module)
        {
            //Change this according to your module's relative location to your project file. If there is any better way to do this I'm interested!
            DirectoryInfo moduleDirectoryParent = Directory.GetParent(Module.ModuleDirectory);

            //Console.WriteLine("Module directory : " + moduleDirectoryParent.Parent.FullName.ToString());
            return moduleDirectoryParent.Parent.FullName.ToString();
        }

        public static void copyIfNewest(string sourceFilepath, string destFilePath)
        {
            if (!File.Exists(sourceFilepath))
            {
                Console.WriteLine(sourceFilepath + " not found, not copied.");
            }
            else
            {
                bool mustCopyFile = true;
                if (File.Exists(destFilePath))
                {
                    mustCopyFile = File.GetLastWriteTime(sourceFilepath) != File.GetLastWriteTime(destFilePath);
                }

                if (mustCopyFile)
                {
                    if (File.Exists(destFilePath))
                    {
                        Console.WriteLine("Copying " + sourceFilepath + " to " + destFilePath + " (was updated)");
                    }
                    else
                    {
                        Console.WriteLine("Copying " + sourceFilepath + " to " + destFilePath + " (creation)");
                    }
                    File.Copy(sourceFilepath, destFilePath, true);
                }
                else
                {
                    Console.WriteLine(destFilePath + " up to date. Not Copied");
                }
            }
        }

        // Copy a file to the project binary directory
        public static void CopyToBinariesDir(string Filepath, ModuleRules Module, ReadOnlyTargetRules Target)
        {
            string binariesDir = Path.Combine(GetModulePath(Module), "Binaries", Target.Platform.ToString());
            string filename = Path.GetFileName(Filepath);
            string destFilePath = Path.Combine(binariesDir, filename);

            if (!Directory.Exists(binariesDir))
                Directory.CreateDirectory(binariesDir);

            copyIfNewest(Filepath, destFilePath);
        }

        public static void CopyDir(string sourceDirectory, string targetDirectory, List<string> excludeFilters = null)
        {
            DirectoryInfo diSource = new DirectoryInfo(sourceDirectory);
            DirectoryInfo diTarget = new DirectoryInfo(targetDirectory);

            CopyAllFiles(diSource, diTarget, excludeFilters);
        }

        public static void CopyAllFiles(DirectoryInfo source, DirectoryInfo target, List<string> excludeFilters = null)
        {
            Directory.CreateDirectory(target.FullName);

            // Copy each file into the new directory.
            foreach (FileInfo fi in source.GetFiles())
            {
                if (excludeFilters == null || !excludeFilters.Contains(fi.Extension))
                {
                    Console.WriteLine(@"Copying {0}\{1}", target.FullName, fi.Name);
                    fi.CopyTo(Path.Combine(target.FullName, fi.Name), true);
                }
            }

            // Copy each subdirectory using recursion.
            foreach (DirectoryInfo diSourceSubDir in source.GetDirectories())
            {
                DirectoryInfo nextTargetSubDir = target.CreateSubdirectory(diSourceSubDir.Name);
                CopyAllFiles(diSourceSubDir, nextTargetSubDir);
            }
        }

        // Copy a file or directory to the project directory
        public static void CopyToModuleDir(string srcPath, ModuleRules Module, string[] dstDirs, List<string> excludeFilters = null)
        {
            List<string> fullDir = new List<string>();
            fullDir.Add(GetModulePath(Module));
            fullDir.AddRange(dstDirs);
            string contentDir = Path.Combine(fullDir.ToArray());
            if (!Directory.Exists(contentDir))
            {
                Directory.CreateDirectory(contentDir);
            }
            if (File.Exists(srcPath))
            {
                string filename = Path.GetFileName(srcPath);
                string fileExt = Path.GetExtension(srcPath);
                if (excludeFilters == null || !excludeFilters.Contains(fileExt))
                {
                    string destPath = Path.Combine(contentDir, filename);
                    copyIfNewest(srcPath, destPath);
                }
            }
            else if (Directory.Exists(srcPath))
            {
                DirectoryInfo diPath = new DirectoryInfo(srcPath);
                string dirname = diPath.Name;
                string destPath = Path.Combine(contentDir, dirname);
                Console.WriteLine("Copying " + srcPath + " to " + destPath);
                CopyDir(srcPath, destPath, excludeFilters);
            }
        }

        // Adds the Golaem SDK as a dependency
        public static void AddGolaemDependencies(ModuleRules Module, ReadOnlyTargetRules Target, bool bCopyDlls, bool bCopyPython)
        {
            var glmBinDlls = new List<string>();
            var glmLibs = new List<string>();
            var glmInstallPath = "";
            if (string.IsNullOrEmpty(GolaemParameters.GolaemInstallPath))
            {
                // if (Target.Configuration == UnrealTargetConfiguration.Debug || Target.Configuration == UnrealTargetConfiguration.DebugGame)
                // {
                // glmInstallPath = Path.Combine(GolaemParameters.GolaemPathPrefix, "GolaemCrowdDBG");
                // }
                // else
                {
                    glmInstallPath = Path.Combine(GolaemParameters.GolaemPathPrefix, "GolaemCrowd");
                }
            }
            else
            {
                glmInstallPath = GolaemParameters.GolaemInstallPath;
            }
            // if (Target.Configuration == UnrealTargetConfiguration.Debug || Target.Configuration == UnrealTargetConfiguration.DebugGame)
            // {
            // var glmDebugIncludePath = Path.Combine(glmInstallPath, "devkit", "include");

            // Module.PrivateIncludePaths.Add(glmDebugIncludePath);

            // var glmLibDebugPath = Path.Combine(glmInstallPath, "devkit", "lib");
            // // Module.PublicLibraryPaths.Add(glmLibDebugPath);
            // glmLibs.AddRange(Directory.EnumerateFiles(glmLibDebugPath, "glmCrowdIO*.lib"));
            // glmLibs.AddRange(Directory.EnumerateFiles(glmLibDebugPath, "glmCore*.lib"));
            // glmLibs.AddRange(Directory.EnumerateFiles(glmLibDebugPath, "glmSDK*.lib"));
            // glmLibs.AddRange(Directory.EnumerateFiles(glmLibDebugPath, "glmEngine*.lib"));
            // glmLibs.AddRange(Directory.EnumerateFiles(glmLibDebugPath, "glmVisualization*.lib"));
            // foreach (var glmLib in glmLibs)
            // {
            // Module.PublicAdditionalLibraries.Add(glmLib);
            // }

            // // for packaging / binaries testing 
            // var glmBinDebugPath = Path.Combine(glmInstallPath, "bin");
            // glmBinDlls.AddRange(Directory.EnumerateFiles(glmBinDebugPath, "glmCore*.dll"));
            // glmBinDlls.AddRange(Directory.EnumerateFiles(glmBinDebugPath, "glmCrowdIO*.dll"));
            // glmBinDlls.AddRange(Directory.EnumerateFiles(glmBinDebugPath, "glmSDK*.dll"));
            // glmBinDlls.AddRange(Directory.EnumerateFiles(glmBinDebugPath, "glmEngine*.dll"));
            // glmBinDlls.AddRange(Directory.EnumerateFiles(glmBinDebugPath, "glmVisualization*.dll"));
            // glmBinDlls.AddRange(Directory.EnumerateFiles(glmBinDebugPath, "ApexFramework*.dll"));
            // glmBinDlls.AddRange(Directory.EnumerateFiles(glmBinDebugPath, "APEX_*.dll"));
            // glmBinDlls.AddRange(Directory.EnumerateFiles(glmBinDebugPath, "glm_PhysX*.dll"));
            // glmBinDlls.AddRange(Directory.EnumerateFiles(glmBinDebugPath, "glm_Px*.dll"));
            // glmBinDlls.Add(Path.Combine(glmBinDebugPath, "nvToolsExt64_1.dll"));

            // if (bCopyDlls)
            // {
            // Console.WriteLine("Using Debug Golaem Library");
            // }				
            // }
            // else // UnrealTargetConfiguration.Development, Shipping
            {
                var glmReleaseIncludePath = Path.Combine(glmInstallPath, "devkit", "include");

                Module.PrivateIncludePaths.Add(glmReleaseIncludePath);

                var glmLibReleasePath = Path.Combine(glmInstallPath, "devkit", "lib");
                //Module.PublicLibraryPaths.Add(glmLibReleasePath);
                glmLibs.AddRange(Directory.EnumerateFiles(glmLibReleasePath, "glmCrowdIO*.lib"));
                glmLibs.AddRange(Directory.EnumerateFiles(glmLibReleasePath, "glmCore*.lib"));
                glmLibs.AddRange(Directory.EnumerateFiles(glmLibReleasePath, "glmSDK*.lib"));
                glmLibs.AddRange(Directory.EnumerateFiles(glmLibReleasePath, "glmEngine*.lib"));
                glmLibs.AddRange(Directory.EnumerateFiles(glmLibReleasePath, "glmVisualization*.lib"));
                foreach (var glmLib in glmLibs)
                {
                    Module.PublicAdditionalLibraries.Add(glmLib);
                }

                var glmBinReleasePath = Path.Combine(glmInstallPath, "devkit", "bin");
                glmBinDlls.AddRange(Directory.EnumerateFiles(glmBinReleasePath, "glmCore*.dll"));
                glmBinDlls.AddRange(Directory.EnumerateFiles(glmBinReleasePath, "Adp*"));
                glmBinDlls.AddRange(Directory.EnumerateFiles(glmBinReleasePath, "glmCrowdIO*.dll"));
                glmBinDlls.AddRange(Directory.EnumerateFiles(glmBinReleasePath, "glmSDK*.dll"));
                glmBinDlls.AddRange(Directory.EnumerateFiles(glmBinReleasePath, "glmEngine*.dll"));
                glmBinDlls.AddRange(Directory.EnumerateFiles(glmBinReleasePath, "glmVisualization*.dll"));
                glmBinDlls.AddRange(Directory.EnumerateFiles(glmBinReleasePath, "ApexFramework*.dll"));
                glmBinDlls.AddRange(Directory.EnumerateFiles(glmBinReleasePath, "APEX_*.dll"));
                glmBinDlls.AddRange(Directory.EnumerateFiles(glmBinReleasePath, "glmusd*.dll"));
                // glmBinDlls.AddRange(Directory.EnumerateFiles(glmBinReleasePath, "tbb*.dll")); // tbb is already provided with UE
                glmBinDlls.AddRange(Directory.EnumerateFiles(glmBinReleasePath, "glm_PhysX*.dll"));
                glmBinDlls.AddRange(Directory.EnumerateFiles(glmBinReleasePath, "glm_Px*.dll"));
                glmBinDlls.Add(Path.Combine(glmBinReleasePath, "nvToolsExt64_1.dll"));

                if (bCopyDlls)
                {
                    Console.WriteLine("Using Release Golaem Library");
                }
            }

            if (bCopyDlls)
            {
                foreach (var glmBinDll in glmBinDlls)
                {
                    CopyToBinariesDir(glmBinDll, Module, Target);
                }
            }

            // copy Python files
            if (bCopyPython)
            {
                List<string> pythonExcludeFilters = new List<string>();
                pythonExcludeFilters.Add(".git");
                pythonExcludeFilters.Add(".gitignore");

                string glmScriptsPath = Path.Combine(glmInstallPath, "scripts", "glm");
                List<string> pythonScripts = new List<string>();
                pythonScripts.Add(Path.Combine(glmScriptsPath, "__init__.py"));
                pythonScripts.Add(Path.Combine(glmScriptsPath, "_devkit.pyd"));
                pythonScripts.Add(Path.Combine(glmScriptsPath, "callbackUtils.py"));
                pythonScripts.Add(Path.Combine(glmScriptsPath, "devkit.py"));
                pythonScripts.Add(Path.Combine(glmScriptsPath, "golaemUtils.py"));
                pythonScripts.Add(Path.Combine(glmScriptsPath, "qtUtils.py"));
                pythonScripts.Add(Path.Combine(glmScriptsPath, "qtCoreUtils.py"));
                pythonScripts.Add(Path.Combine(glmScriptsPath, "jsonUtils.py"));
                pythonScripts.Add(Path.Combine(glmScriptsPath, "stringUtils.py"));
                pythonScripts.Add(Path.Combine(glmScriptsPath, "Qtpy"));
                pythonScripts.Add(Path.Combine(glmScriptsPath, "Nodz"));
                pythonScripts.Add(Path.Combine(glmScriptsPath, "layout"));
                pythonScripts.Add(Path.Combine(glmScriptsPath, "simCacheLib"));
                List<string> excludeFilters = new List<string>();
                excludeFilters.Add(".pyc");
                excludeFilters.Add(".git");
                excludeFilters.Add(".gitignore");
                excludeFilters.Add("simCacheLibWindowMayaWrapper.py");
                excludeFilters.Add("simCacheLibWindowStandAlone.py");
                string[] pythonGlmDirs = new string[] { "Content", "Python", "glm" };
                foreach (string scriptPath in pythonScripts)
                {
                    CopyToModuleDir(scriptPath, Module, pythonGlmDirs, excludeFilters);
                }

                // ui files
                string glmScriptsUiPath = Path.Combine(glmScriptsPath, "ui");
                List<string> pythonUiScripts = new List<string>();
                pythonUiScripts.Add(Path.Combine(glmScriptsUiPath, "windowWrapper.py"));
                pythonUiScripts.Add(Path.Combine(glmScriptsUiPath, "golaemAboutWindow.py"));

                // copy LICENSE file for about box from the root
                string RepoRootDir = Path.GetFullPath(Path.Combine(Module.PluginDirectory, "../../../../../"));
                string LicenseFile = Path.Combine(RepoRootDir, "LICENSE");
                pythonUiScripts.Add(LicenseFile);

                string[] pythonGlmUiDirs = new string[] { "Content", "Python", "glm", "ui" };

                foreach (string scriptPath in pythonUiScripts)
                {
                    CopyToModuleDir(scriptPath, Module, pythonGlmUiDirs);
                }

                // copy icons
                string glmIconsPath = Path.Combine(glmInstallPath, "icons");
                string[] iconDirs = new string[] { "Resources", "Icons" };
                string editorsIconPath = Path.Combine(glmIconsPath, "editors");
                string libraryToolIconPath = Path.Combine(glmIconsPath, "libraryTool");
                string layoutToolv7IconPath = Path.Combine(glmIconsPath, "layoutToolv7");
                string buttonSimulationCacheLibraryIconPath = Path.Combine(glmIconsPath, "buttonSimulationCacheLibrary.png");
                string glmAboutIconPath = Path.Combine(glmIconsPath, "glmAbout.png");
                string glmDefaultIconPath = Path.Combine(glmIconsPath, "glmDefaultIcon.png");

                CopyToModuleDir(editorsIconPath, Module, iconDirs);
                CopyToModuleDir(libraryToolIconPath, Module, iconDirs);
                CopyToModuleDir(layoutToolv7IconPath, Module, iconDirs);
                CopyToModuleDir(buttonSimulationCacheLibraryIconPath, Module, iconDirs);
                CopyToModuleDir(glmAboutIconPath, Module, iconDirs);
                CopyToModuleDir(glmDefaultIconPath, Module, iconDirs);
            }

            string[] resourcesDir = new string[] { "Resources" };

            // copy EULA files
            string glmEULAFilesPath = Path.Combine(glmInstallPath, "EULA");
            CopyToModuleDir(glmEULAFilesPath, Module, resourcesDir);

            // make runtime depend of the copied libraries , which are in the project thus can be exported
            string binariesDir = Path.Combine(GetModulePath(Module), "Binaries", Target.Platform.ToString());
            foreach (var glmBinDll in glmBinDlls)
            {
                var glmBinDllFileName = Path.GetFileName(glmBinDll);
                Module.RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)", glmBinDllFileName), Path.Combine(binariesDir, glmBinDllFileName));
            }
        }
    }
}