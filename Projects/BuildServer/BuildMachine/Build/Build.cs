﻿using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using BuildShared;

namespace BuildMachine
{
  public class ezBuild
  {
    /// <summary>
    /// This class takes care of building (i.e. compiling) in a build process.
    /// </summary>
    public ezBuild()
    {
    }

    public bool Init(BuildMachineSettings settings)
    {
      _Settings = settings;
      return true;
    }

    public BuildResult Run(ezCMake.CMakeResult cmakeResults)
    {
      _Result.Clean();

      Console.WriteLine("*** Starting Build ***");

      List<ezCMake.BuildTarget> targets = cmakeResults.BuildTargets;
      ezBuildTemplate buildTemplate = ezBuildTemplateFactory.Create(_Settings.Configuration);

      if (!cmakeResults.Success)
      {
        _Result.Error("Build failed: CMake step failed, aborting!");
        return _Result;
      }

      if (buildTemplate == null)
      {
        _Result.Error("Build failed: No build template available for the desired configuration ({0})!", _Settings.Configuration);
        return _Result;
      }

      Stopwatch sw = new Stopwatch();
      sw.Start();

      List<string> successfulTargets = new List<string>();
      int iFailedTargets = 0;
      foreach (ezCMake.BuildTarget target in targets)
      {
        Console.WriteLine("** Starting Build of target '{0}' **", target.Name);
        // Only build targets who's dependencies compiled successfully.
        bool bMeetDependencies = !target.Dependencies.Except(successfulTargets).Any();
        if (bMeetDependencies)
        {
          BuildTargetResult targetResult = buildTemplate.BuildTarget(target, _Settings.AbsCMakeWorkspace);
          targetResult.Duration = targetResult.ProcessRes.Duration;

          if (targetResult.ProcessRes.ExitCode == 0)
          {
            successfulTargets.Add(targetResult.Name);
          }
          else
          {
            if (!target.Experimental)
              iFailedTargets++;
          }
          _Result.BuildTargetResults.Add(targetResult);
        }
        else
        {
          // Dummy result for non-buildable targets.
          BuildTargetResult targetResult = new BuildTargetResult();
          targetResult.Name = target.Name;
          targetResult.Experimental = target.Experimental;
          targetResult.ProcessRes = null;

          _Result.BuildTargetResults.Add(targetResult);
        }
      }

      sw.Stop();
      _Result.Duration = sw.Elapsed.TotalSeconds;
      _Result.Success = iFailedTargets == 0;
      if (iFailedTargets > 0)
        _Result.Error("Build failed: {0} targets failed to build!", iFailedTargets);
      return _Result;
    }

    public BuildResult GetResult()
    {
      return _Result;
    }

    #region Private Functions

    #endregion Private Functions

    /// <summary>
    /// The results of the build step in the build process.
    /// </summary>
    public class BuildResult : BuildStepResults
    {
      public List<BuildTargetResult> BuildTargetResults { get; set; }

      public override void Clean()
      {
        base.Clean();
        BuildTargetResults = new List<BuildTargetResult>();
      }
    }

    /// <summary>
    /// The results of a build run for a single project.
    /// </summary>
    public class BuildTargetResult : BuildStepResults
    {
      public string Name { get; set; }
      public bool Experimental { get; set; }
      public ezProcessHelper.ProcessResult ProcessRes { get; set; }
    }

    #region Private Members

    BuildResult _Result = new BuildResult();
    BuildMachineSettings _Settings = null;

    #endregion Private Members
  }
}
