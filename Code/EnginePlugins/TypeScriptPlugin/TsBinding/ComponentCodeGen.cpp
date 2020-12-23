#include <TypeScriptPluginPCH.h>

#include <Foundation/IO/FileSystem/DeferredFileWriter.h>
#include <Foundation/IO/FileSystem/FileReader.h>
#include <Foundation/IO/FileSystem/FileWriter.h>
#include <Foundation/IO/OSFile.h>
#include <TypeScriptPlugin/Components/TypeScriptComponent.h>
#include <TypeScriptPlugin/TsBinding/TsBinding.h>

ezResult ezTypeScriptBinding::SetupProjectCode()
{
  ezStringBuilder sAbsPathToEzTemplate, sAbsPathToData;
  EZ_SUCCEED_OR_RETURN(ezFileSystem::ResolvePath(":plugins/TypeScript/ez-template.ts", &sAbsPathToEzTemplate, nullptr));

  sAbsPathToData = sAbsPathToEzTemplate;
  sAbsPathToData.PathParentDirectory();

  ezStringBuilder sEzFileContent;

  // read ez.ts
  {
    ezFileReader fileEz;
    EZ_SUCCEED_OR_RETURN(fileEz.Open(sAbsPathToEzTemplate));
    sEzFileContent.ReadAll(fileEz);
  }

  // Remove all files from the Project/TypeScript folder that are also in Plugins/TypeScript
#if EZ_ENABLED(EZ_SUPPORTS_FILE_ITERATORS)
  {
    ezStringBuilder sTargetPath;

    ezFileSystemIterator it;
    for (it.StartSearch(sAbsPathToData, ezFileSystemIteratorFlags::ReportFilesRecursive); it.IsValid(); it.Next())
    {
      it.GetStats().GetFullPath(sTargetPath);

      sTargetPath.MakeRelativeTo(sAbsPathToData).IgnoreResult();
      sTargetPath.Prepend(":project/TypeScript/");

      ezFileSystem::DeleteFile(sTargetPath);
    }
  }
#endif

  RemoveAutoGeneratedCode(sEzFileContent);

  GenerateComponentsFile(":project/TypeScript/ez/AllComponents.ts");
  InjectComponentImportExport(sEzFileContent, "./ez/AllComponents");

  GenerateMessagesFile(":project/TypeScript/ez/AllMessages.ts");
  InjectMessageImportExport(sEzFileContent, "./ez/AllMessages");

  GenerateEnumsFile(":project/TypeScript/ez/AllEnums.ts", s_RequiredEnums);
  GenerateEnumsFile(":project/TypeScript/ez/AllFlags.ts", s_RequiredFlags);

  InjectEnumImportExport(sEzFileContent, "./ez/AllEnums");
  InjectFlagsImportExport(sEzFileContent, "./ez/AllFlags");

  {
    ezDeferredFileWriter fileOut;
    fileOut.SetOutput(":project/TypeScript/ez.ts", true);
    EZ_SUCCEED_OR_RETURN(fileOut.WriteBytes(sEzFileContent.GetData(), sEzFileContent.GetElementCount()));
    EZ_SUCCEED_OR_RETURN(fileOut.Close());
  }

  return EZ_SUCCESS;
}

void ezTypeScriptBinding::GetTsName(const ezRTTI* pRtti, ezStringBuilder& out_sName)
{
  out_sName = pRtti->GetTypeName();
  out_sName.TrimWordStart("ez");
}

void ezTypeScriptBinding::GenerateComponentCode(ezStringBuilder& out_Code, const ezRTTI* pRtti)
{
  ezStringBuilder sComponentType, sParentType;
  GetTsName(pRtti, sComponentType);

  GetTsName(pRtti->GetParentType(), sParentType);

  out_Code.AppendFormat("export class {0} extends {1}\n", sComponentType, sParentType);
  out_Code.Append("{\n");
  out_Code.AppendFormat("  public static GetTypeNameHash(): number { return {}; }\n", ezHashingUtils::StringHashTo32(pRtti->GetTypeNameHash()));
  GenerateExposedFunctionsCode(out_Code, pRtti);
  GeneratePropertiesCode(out_Code, pRtti);
  out_Code.Append("}\n\n");
}

static void CreateComponentTypeList(ezSet<const ezRTTI*>& found, ezDynamicArray<const ezRTTI*>& sorted, const ezRTTI* pRtti)
{
  if (found.Contains(pRtti))
    return;

  if (!pRtti->IsDerivedFrom<ezComponent>())
    return;

  if (pRtti == ezGetStaticRTTI<ezComponent>() || pRtti == ezGetStaticRTTI<ezTypeScriptComponent>())
    return;

  found.Insert(pRtti);
  CreateComponentTypeList(found, sorted, pRtti->GetParentType());

  sorted.PushBack(pRtti);
}

void ezTypeScriptBinding::GenerateAllComponentsCode(ezStringBuilder& out_Code)
{
  ezSet<const ezRTTI*> found;
  ezDynamicArray<const ezRTTI*> sorted;
  sorted.Reserve(100);

  ezHybridArray<const ezRTTI*, 64> alphabetical;
  for (auto pRtti : ezRTTI::GetAllTypesDerivedFrom(ezGetStaticRTTI<ezComponent>(), alphabetical, true))
  {
    if (pRtti == ezGetStaticRTTI<ezComponent>() || pRtti == ezGetStaticRTTI<ezTypeScriptComponent>())
      continue;

    CreateComponentTypeList(found, sorted, pRtti);
  }

  for (auto pRtti : sorted)
  {
    GenerateComponentCode(out_Code, pRtti);
  }
}

void ezTypeScriptBinding::GenerateComponentsFile(const char* szFile)
{
  ezStringBuilder sFileContent;

  sFileContent =
    R"(
import __GameObject = require("TypeScript/ez/GameObject")
export import GameObject = __GameObject.GameObject;

import __Component = require("TypeScript/ez/Component")
export import Component = __Component.Component;

import __Vec2 = require("TypeScript/ez/Vec2")
export import Vec2 = __Vec2.Vec2;

import __Vec3 = require("TypeScript/ez/Vec3")
export import Vec3 = __Vec3.Vec3;

import __Mat3 = require("TypeScript/ez/Mat3")
export import Mat3 = __Mat3.Mat3;

import __Mat4 = require("TypeScript/ez/Mat4")
export import Mat4 = __Mat4.Mat4;

import __Quat = require("TypeScript/ez/Quat")
export import Quat = __Quat.Quat;

import __Transform = require("TypeScript/ez/Transform")
export import Transform = __Transform.Transform;

import __Color = require("TypeScript/ez/Color")
export import Color = __Color.Color;

import __Time = require("TypeScript/ez/Time")
export import Time = __Time.Time;

import __Angle = require("TypeScript/ez/Angle")
export import Angle = __Angle.Angle;

import Enum = require("./AllEnums")
import Flags = require("./AllFlags")

declare function __CPP_ComponentProperty_get(component: Component, id: number);
declare function __CPP_ComponentProperty_set(component: Component, id: number, value);
declare function __CPP_ComponentFunction_Call(component: Component, id: number, ...args);

)";

  GenerateAllComponentsCode(sFileContent);

  ezDeferredFileWriter file;
  file.SetOutput(szFile, true);

  file.WriteBytes(sFileContent.GetData(), sFileContent.GetElementCount()).IgnoreResult();

  if (file.Close().Failed())
  {
    ezLog::Error("Failed to write file '{}'", szFile);
    return;
  }
}

void ezTypeScriptBinding::InjectComponentImportExport(ezStringBuilder& content, const char* szComponentFile)
{
  ezSet<const ezRTTI*> found;
  ezDynamicArray<const ezRTTI*> sorted;
  sorted.Reserve(100);

  ezHybridArray<const ezRTTI*, 64> alphabetical;
  for (auto pRtti : ezRTTI::GetAllTypesDerivedFrom(ezGetStaticRTTI<ezComponent>(), alphabetical, true))
  {
    CreateComponentTypeList(found, sorted, pRtti);
  }

  ezStringBuilder sImportExport, sTypeName;

  sImportExport.Format(R"(import __AllComponents = require("{}")
)",
    szComponentFile);

  for (const ezRTTI* pRtti : sorted)
  {
    GetTsName(pRtti, sTypeName);
    sImportExport.AppendFormat("export import {0} = __AllComponents.{0};\n", sTypeName);
  }

  AppendToTextFile(content, sImportExport);
}
