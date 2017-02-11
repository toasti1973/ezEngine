#include <Foundation/PCH.h>
#include <Foundation/IO/FileSystem/FileSystem.h>
#include <Foundation/Configuration/Startup.h>
#include <Foundation/Logging/Log.h>
#include <Foundation/IO/OSFile.h>

EZ_BEGIN_SUBSYSTEM_DECLARATION(Foundation, FileSystem)

ON_CORE_STARTUP
{
  ezFileSystem::Startup();
}

ON_CORE_SHUTDOWN
{
  ezFileSystem::Shutdown();
}

EZ_END_SUBSYSTEM_DECLARATION

ezFileSystem::FileSystemData* ezFileSystem::s_Data = nullptr;

ezMutex& ezFileSystem::GetFileSystemMutex()
{
  return s_Data->m_Mutex;
}


void ezFileSystem::RegisterDataDirectoryFactory(ezDataDirFactory Factory, float fPriority /*= 0*/)
{
  auto& data = s_Data->m_DataDirFactories.ExpandAndGetRef();
  data.m_Factory = Factory;
  data.m_fPriority = fPriority;
}

void ezFileSystem::RegisterEventHandler(ezEvent<const FileEvent&>::Handler handler)
{
  EZ_ASSERT_DEV(s_Data != nullptr, "FileSystem is not initialized.");

  EZ_LOCK(s_Data->m_Mutex);

  s_Data->m_Event.AddEventHandler(handler);
}

void ezFileSystem::UnregisterEventHandler(ezEvent<const FileEvent&>::Handler handler)
{
  EZ_ASSERT_DEV(s_Data != nullptr, "FileSystem is not initialized.");

  EZ_LOCK(s_Data->m_Mutex);

  s_Data->m_Event.RemoveEventHandler(handler);
}

ezResult ezFileSystem::AddDataDirectory(const char* szDataDirectory, const char* szGroup, const char* szRootName, DataDirUsage Usage)
{
  EZ_ASSERT_DEV(s_Data != nullptr, "FileSystem is not initialized.");
  EZ_ASSERT_DEV(Usage != AllowWrites || !ezStringUtils::IsNullOrEmpty(szRootName), "A data directory must have a non-empty, unique name to be mounted for write access");

  EZ_LOCK(s_Data->m_Mutex);

  ezStringBuilder sPath = szDataDirectory;
  sPath.MakeCleanPath();

  if (!sPath.IsEmpty() && !sPath.EndsWith("/"))
    sPath.Append("/");

  // this cleaning might actually make the root name empty
  // e.g. ":" becomes ""
  // which is intended to support passing through of absolute paths
  // ie. mounting the empty dir "" under the root ":" will allow to write directly to files using absolute paths
  ezStringBuilder sCleanRootName = szRootName;
  if (sCleanRootName.StartsWith(":"))
    sCleanRootName.Shrink(1, 0);
  if (sCleanRootName.EndsWith("/"))
    sCleanRootName.Shrink(0, 1);
  sCleanRootName.ToUpper();

  bool failed = false;
  if (!ezStringUtils::IsNullOrEmpty(szRootName))
  {
    for (const auto& dd : s_Data->m_DataDirectories)
    {
      if (dd.m_sRootName == sCleanRootName)
      {
        ezLog::Error("A data directory with root name '{0}' already exists.", sCleanRootName.GetData());
        failed = true;
        break;
      }
    }
  }

  if (!failed)
  {
    s_Data->m_DataDirFactories.Sort([](const auto& a, const auto& b) { return a.m_fPriority < b.m_fPriority; });

    // use the factory that was added last as the one with the highest priority -> allows to override already added factories
    for (ezInt32 i = s_Data->m_DataDirFactories.GetCount() - 1; i >= 0; --i)
    {
      ezDataDirectoryType* pDataDir = s_Data->m_DataDirFactories[i].m_Factory(sPath, szGroup, szRootName, Usage);

      if (pDataDir != nullptr)
      {
        DataDirectory dd;
        dd.m_Usage = Usage;
        dd.m_pDataDirectory = pDataDir;
        dd.m_sRootName = sCleanRootName;
        dd.m_sGroup = szGroup;

        s_Data->m_DataDirectories.PushBack(dd);

        {
          // Broadcast that a data directory was added
          FileEvent fe;
          fe.m_EventType = FileEventType::AddDataDirectorySucceeded;
          fe.m_szFileOrDirectory = sPath;
          fe.m_szOther = sCleanRootName;
          fe.m_pDataDir = pDataDir;
          s_Data->m_Event.Broadcast(fe);
        }

        return EZ_SUCCESS;
      }
    }
  }

  {
    // Broadcast that adding a data directory failed
    FileEvent fe;
    fe.m_EventType = FileEventType::AddDataDirectoryFailed;
    fe.m_szFileOrDirectory = sPath;
    fe.m_szOther = sCleanRootName;
    s_Data->m_Event.Broadcast(fe);
  }

  ezLog::Error("Adding Data Directory '{0}' failed.", szDataDirectory);
  return EZ_FAILURE;
}

ezUInt32 ezFileSystem::RemoveDataDirectoryGroup(const char* szGroup)
{
  EZ_ASSERT_DEV(s_Data != nullptr, "FileSystem is not initialized.");

  EZ_LOCK(s_Data->m_Mutex);

  ezUInt32 uiRemoved = 0;

  for (ezUInt32 i = 0; i < s_Data->m_DataDirectories.GetCount(); )
  {
    if (s_Data->m_DataDirectories[i].m_sGroup == szGroup)
    {
      {
        // Broadcast that a data directory is about to be removed
        FileEvent fe;
        fe.m_EventType = FileEventType::RemoveDataDirectory;
        fe.m_szFileOrDirectory = s_Data->m_DataDirectories[i].m_pDataDirectory->GetDataDirectoryPath();
        fe.m_szOther = s_Data->m_DataDirectories[i].m_sRootName;
        fe.m_pDataDir = s_Data->m_DataDirectories[i].m_pDataDirectory;
        s_Data->m_Event.Broadcast(fe);
      }

      ++uiRemoved;

      s_Data->m_DataDirectories[i].m_pDataDirectory->RemoveDataDirectory();
      s_Data->m_DataDirectories.RemoveAt(i);
    }
    else
      ++i;
  }

  return uiRemoved;
}

void ezFileSystem::ClearAllDataDirectories()
{
  EZ_ASSERT_DEV(s_Data != nullptr, "FileSystem is not initialized.");

  EZ_LOCK(s_Data->m_Mutex);

  for (ezInt32 i = s_Data->m_DataDirectories.GetCount() - 1; i >= 0; --i)
  {
    {
      // Broadcast that a data directory is about to be removed
      FileEvent fe;
      fe.m_EventType = FileEventType::RemoveDataDirectory;
      fe.m_szFileOrDirectory = s_Data->m_DataDirectories[i].m_pDataDirectory->GetDataDirectoryPath();
      fe.m_szOther = s_Data->m_DataDirectories[i].m_sRootName;
      fe.m_pDataDir = s_Data->m_DataDirectories[i].m_pDataDirectory;
      s_Data->m_Event.Broadcast(fe);
    }

    s_Data->m_DataDirectories[i].m_pDataDirectory->RemoveDataDirectory();
  }

  s_Data->m_DataDirectories.Clear();
}

ezUInt32 ezFileSystem::GetNumDataDirectories()
{
  EZ_ASSERT_DEV(s_Data != nullptr, "FileSystem is not initialized.");

  return s_Data->m_DataDirectories.GetCount();
}

ezDataDirectoryType* ezFileSystem::GetDataDirectory(ezUInt32 uiDataDirIndex)
{
  EZ_ASSERT_DEV(s_Data != nullptr, "FileSystem is not initialized.");

  return s_Data->m_DataDirectories[uiDataDirIndex].m_pDataDirectory;
}

const char* ezFileSystem::GetDataDirRelativePath(const char* szPath, ezUInt32 uiDataDir)
{
  // if an absolute path is given, this will check whether the absolute path would fall into this data directory
  // if yes, the prefix path is removed and then only the relative path is given to the data directory type
  // otherwise the data directory would prepend its own path and thus create an invalid path to work with
  const ezString128& sDirPath = s_Data->m_DataDirectories[uiDataDir].m_pDataDirectory->GetDataDirectoryPath();

  // If the data dir is empty we return the paths as is or the code below would remove the '/' in front of an
  // absolute path.
  if (sDirPath.IsEmpty())
    return szPath;

  const char* szRelPath = szPath;

  if (ezStringUtils::StartsWith_NoCase(szPath, sDirPath))
    szRelPath = &szPath[sDirPath.GetElementCount()];

  // if the relative path still starts with a path-separator, skip it
  if (ezPathUtils::IsPathSeparator(*szRelPath))
    ++szRelPath;

  return szRelPath;
}


ezFileSystem::DataDirectory* ezFileSystem::GetDataDirForRoot(const ezString& sRoot)
{
  for (ezUInt32 i = 0; i < s_Data->m_DataDirectories.GetCount(); ++i)
  {
    if (s_Data->m_DataDirectories[i].m_sRootName == sRoot)
      return &s_Data->m_DataDirectories[i];
  }

  return nullptr;
}

void ezFileSystem::DeleteFile(const char* szFile)
{
  EZ_ASSERT_DEV(s_Data != nullptr, "FileSystem is not initialized.");

  EZ_LOCK(s_Data->m_Mutex);

  if (ezPathUtils::IsAbsolutePath(szFile))
  {
    ezOSFile::DeleteFile(szFile);
    return;
  }

  ezString sRootName;
  szFile = ExtractRootName(szFile, sRootName);

  EZ_ASSERT_DEV(!sRootName.IsEmpty(), "Files can only be deleted with a rooted path name.");

  if (sRootName.IsEmpty())
    return;

  for (ezUInt32 i = 0; i < s_Data->m_DataDirectories.GetCount(); ++i)
  {
    // do not delete data from directories that are mounted as read only
    if (s_Data->m_DataDirectories[i].m_Usage != AllowWrites)
      continue;

    if (s_Data->m_DataDirectories[i].m_sRootName != sRootName)
      continue;

    const char* szRelPath = GetDataDirRelativePath(szFile, i);

    {
      // Broadcast that a file is about to be deleted
      // This can be used to check out files or mark them as deleted in a revision control system
      FileEvent fe;
      fe.m_EventType = FileEventType::DeleteFile;
      fe.m_szFileOrDirectory = szRelPath;
      fe.m_pDataDir = s_Data->m_DataDirectories[i].m_pDataDirectory;
      fe.m_szOther = sRootName;
      s_Data->m_Event.Broadcast(fe);
    }

    s_Data->m_DataDirectories[i].m_pDataDirectory->DeleteFile(szRelPath);
  }
}

bool ezFileSystem::ExistsFile(const char* szFile)
{
  EZ_ASSERT_DEV(s_Data != nullptr, "FileSystem is not initialized.");

  EZ_LOCK(s_Data->m_Mutex);

  ezString sRootName;
  szFile = ExtractRootName(szFile, sRootName);

  for (ezUInt32 i = 0; i < s_Data->m_DataDirectories.GetCount(); ++i)
  {
    if (!sRootName.IsEmpty() && s_Data->m_DataDirectories[i].m_sRootName != sRootName)
      continue;

    const char* szRelPath = GetDataDirRelativePath(szFile, i);

    if (s_Data->m_DataDirectories[i].m_pDataDirectory->ExistsFile(szRelPath))
      return true;
  }

  return false;
}

const char* ezFileSystem::ExtractRootName(const char* szPath, ezString& rootName)
{
  rootName.Clear();

  if (!ezStringUtils::StartsWith(szPath, ":"))
    return szPath;

  ezStringBuilder sCur;
  ezStringView it(szPath);
  ++it;

  while (!it.IsEmpty() && (it.GetCharacter() != '/'))
  {
    sCur.Append(it.GetCharacter());

    ++it;
  }

  EZ_ASSERT_DEV(!it.IsEmpty(), "Cannot parse the path \"{0}\". The data-dir root name starts with a ':' but does not end with '/'.", szPath);

  sCur.ToUpper();
  rootName = sCur;
  ++it;

  return it.GetData(); // return the string after the data-dir filter declaration
}

ezDataDirectoryReader* ezFileSystem::GetFileReader(const char* szFile, bool bAllowFileEvents)
{
  EZ_ASSERT_DEV(s_Data != nullptr, "FileSystem is not initialized.");

  EZ_LOCK(s_Data->m_Mutex);

  ezString sRootName;
  szFile = ExtractRootName(szFile, sRootName);

  // clean up the path to get rid of ".." etc.
  ezStringBuilder sPath = szFile;
  sPath.MakeCleanPath();

  const bool bOneSpecificDataDir = !sRootName.IsEmpty();

  // the last added data directory has the highest priority
  for (ezInt32 i = (ezInt32)s_Data->m_DataDirectories.GetCount() - 1; i >= 0; --i)
  {
    // if a root is used, ignore all directories that do not have the same root name
    if (bOneSpecificDataDir && s_Data->m_DataDirectories[i].m_sRootName != sRootName)
      continue;

    const char* szRelPath = GetDataDirRelativePath(sPath, i);

    if (bAllowFileEvents)
    {
      // Broadcast that we now try to open this file
      // Could be useful to check this file out before it is accessed
      FileEvent fe;
      fe.m_EventType = FileEventType::OpenFileAttempt;
      fe.m_szFileOrDirectory = szRelPath;
      fe.m_szOther = sRootName;
      fe.m_pDataDir = s_Data->m_DataDirectories[i].m_pDataDirectory;
      s_Data->m_Event.Broadcast(fe);
    }

    // Let the data directory try to open the file.
    ezDataDirectoryReader* pReader = s_Data->m_DataDirectories[i].m_pDataDirectory->OpenFileToRead(szRelPath, bOneSpecificDataDir);

    if (bAllowFileEvents && pReader != nullptr)
    {
      // Broadcast that this file has been opened.
      FileEvent fe;
      fe.m_EventType = FileEventType::OpenFileSucceeded;
      fe.m_szFileOrDirectory = szRelPath;
      fe.m_szOther = sRootName;
      fe.m_pDataDir = s_Data->m_DataDirectories[i].m_pDataDirectory;
      s_Data->m_Event.Broadcast(fe);

      return pReader;
    }
  }

  if (bAllowFileEvents)
  {
    // Broadcast that opening this file failed.
    FileEvent fe;
    fe.m_EventType = FileEventType::OpenFileFailed;
    fe.m_szFileOrDirectory = sPath;
    s_Data->m_Event.Broadcast(fe);
  }

  return nullptr;
}

ezDataDirectoryWriter* ezFileSystem::GetFileWriter(const char* szFile, bool bAllowFileEvents)
{
  EZ_ASSERT_DEV(s_Data != nullptr, "FileSystem is not initialized.");

  EZ_LOCK(s_Data->m_Mutex);

  ezString sRootName;

  if (!ezPathUtils::IsAbsolutePath(szFile))
  {
    EZ_ASSERT_DEV(ezStringUtils::StartsWith(szFile, ":"), "Only native absolute paths or rooted paths (starting with a colon and then the data dir root name) are allowed for writing to files. This path is neither: '{0}'", szFile);
    szFile = ExtractRootName(szFile, sRootName);
  }

  // clean up the path to get rid of ".." etc.
  ezStringBuilder sPath = szFile;
  sPath.MakeCleanPath();

  // the last added data directory has the highest priority
  for (ezInt32 i = (ezInt32)s_Data->m_DataDirectories.GetCount() - 1; i >= 0; --i)
  {
    if (s_Data->m_DataDirectories[i].m_Usage != AllowWrites)
      continue;

    // ignore all directories that have not the category that is currently requested
    if (s_Data->m_DataDirectories[i].m_sRootName != sRootName)
      continue;

    const char* szRelPath = GetDataDirRelativePath(szFile, i);

    if (bAllowFileEvents)
    {
      // Broadcast that we now try to open this file
      // Could be useful to check this file out before it is accessed
      FileEvent fe;
      fe.m_EventType = FileEventType::CreateFileAttempt;
      fe.m_szFileOrDirectory = szRelPath;
      fe.m_szOther = sRootName;
      fe.m_pDataDir = s_Data->m_DataDirectories[i].m_pDataDirectory;
      s_Data->m_Event.Broadcast(fe);
    }

    ezDataDirectoryWriter* pWriter = s_Data->m_DataDirectories[i].m_pDataDirectory->OpenFileToWrite(szRelPath);

    if (bAllowFileEvents && pWriter != nullptr)
    {
      // Broadcast that this file has been created.
      FileEvent fe;
      fe.m_EventType = FileEventType::CreateFileSucceeded;
      fe.m_szFileOrDirectory = szRelPath;
      fe.m_szOther = sRootName;
      fe.m_pDataDir = s_Data->m_DataDirectories[i].m_pDataDirectory;
      s_Data->m_Event.Broadcast(fe);

      return pWriter;
    }
  }

  if (bAllowFileEvents)
  {
    // Broadcast that creating this file failed.
    FileEvent fe;
    fe.m_EventType = FileEventType::CreateFileFailed;
    fe.m_szFileOrDirectory = sPath;
    s_Data->m_Event.Broadcast(fe);
  }

  return nullptr;
}

ezResult ezFileSystem::ResolvePath(const char* szPath, ezStringBuilder* out_sAbsolutePath, ezStringBuilder* out_sDataDirRelativePath)
{
  EZ_ASSERT_DEV(s_Data != nullptr, "FileSystem is not initialized.");

  EZ_LOCK(s_Data->m_Mutex);

  ezStringBuilder absPath, relPath;

  if (ezStringUtils::StartsWith(szPath, ":"))
  {
    // writing is only allowed using rooted paths
    ezString sRootName;
    ExtractRootName(szPath, sRootName);

    DataDirectory* pDataDir = GetDataDirForRoot(sRootName);

    if (pDataDir == nullptr)
      return EZ_FAILURE;

    relPath = &szPath[sRootName.GetElementCount() + 2];

    absPath = pDataDir->m_pDataDirectory->GetDataDirectoryPath();
    absPath.AppendPath(relPath);
  }
  else
  {
    // try to get a reader -> if we get one, the file does indeed exist
    ezDataDirectoryReader* pReader = ezFileSystem::GetFileReader(szPath, true);

    if (!pReader)
      return EZ_FAILURE;

    relPath = pReader->GetFilePath();

    absPath = pReader->GetDataDirectory()->GetDataDirectoryPath();
    absPath.AppendPath(relPath);

    pReader->Close();
  }

  if (out_sAbsolutePath)
    *out_sAbsolutePath = absPath;

  if (out_sDataDirRelativePath)
    *out_sDataDirRelativePath = relPath;

  return EZ_SUCCESS;
}


ezResult ezFileSystem::FindFolderWithSubPath(const char* szStartDirectory, const char* szSubPath, ezStringBuilder& result)
{
  ezStringBuilder sStartDirAbs = szStartDirectory;
  sStartDirAbs.MakeCleanPath();

  // in this case the given path and the absolute path are different
  // but we want to return the same path format as is given
  // ie. if we get ":MyRoot\Bla" with "MyRoot" pointing to "C:\Game", then the result should be
  // ":MyRoot\blub", rather than "C:\Game\blub"
  if (sStartDirAbs.StartsWith(":"))
  {
    ezStringBuilder abs;
    if (ResolvePath(sStartDirAbs, &abs, nullptr).Failed())
    {
      result.Clear();
      return EZ_FAILURE;
    }

    sStartDirAbs = abs;
  }


  result = szStartDirectory;
  result.MakeCleanPath();

  ezStringBuilder FullPath;

  while (!result.IsEmpty())
  {
    FullPath = sStartDirAbs;
    FullPath.AppendPath(szSubPath);
    FullPath.MakeCleanPath();

    if (ezOSFile::ExistsDirectory(FullPath) || ezOSFile::ExistsFile(FullPath))
      return EZ_SUCCESS;

    result.PathParentDirectory();
    sStartDirAbs.PathParentDirectory();
  }

  return EZ_FAILURE;
}

void ezFileSystem::ReloadAllExternalDataDirectoryConfigs()
{
  EZ_LOG_BLOCK("ReloadAllExternalDataDirectoryConfigs");

  for (auto& dd : s_Data->m_DataDirectories)
  {
    dd.m_pDataDirectory->ReloadExternalConfigs();
  }
}

void ezFileSystem::Startup()
{
  s_Data = EZ_DEFAULT_NEW(FileSystemData);
}

void ezFileSystem::Shutdown()
{
  s_Data->m_DataDirFactories.Clear();

  ClearAllDataDirectories();

  EZ_DEFAULT_DELETE(s_Data);
}


EZ_STATICLINK_FILE(Foundation, Foundation_IO_FileSystem_Implementation_FileSystem);

