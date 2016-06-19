#pragma once

#include <ToolsFoundation/Basics.h>
#include <Foundation/Communication/Event.h>
#include <ToolsFoundation/Basics/Status.h>
#include <Foundation/Configuration/Singleton.h>

class ezToolsProject;

struct ezToolsProjectEvent
{
  enum class Type
  {
    ProjectCreated,
    ProjectOpened,
    ProjectClosing,
    ProjectClosed,
    ProjectConfigChanged, ///< Sent when global project configuration data was changed and thus certain menus would need to update their content (or just deselect any item, forcing the user to reselect and thus update state)
  };

  ezToolsProject* m_pProject;
  Type m_Type;
};

struct ezToolsProjectRequest
{
  enum class Type
  {
    CanProjectClose,
  };

  Type m_Type;
  bool m_bProjectCanClose; // when the event 'CanProjectClose' is sent, interested code can set this to false to prevent project closing
};

class EZ_TOOLSFOUNDATION_DLL ezToolsProject
{
  EZ_DECLARE_SINGLETON(ezToolsProject);

public:

  static ezEvent<const ezToolsProjectEvent&> s_Events;
  static ezEvent<ezToolsProjectRequest&> s_Requests;

public:

  static bool IsProjectOpen() { return GetSingleton() != nullptr; }
  static bool IsProjectClosing() { return (GetSingleton() != nullptr && GetSingleton()->m_bIsClosing); }
  static void CloseProject();
  static bool CanCloseProject();
  static ezStatus OpenProject(const char* szProjectPath);
  static ezStatus CreateProject(const char* szProjectPath);

  /// \brief Sent when global project configuration data was changed and thus certain menus would need to update their content (or just deselect any item, forcing the user to reselect and thus update state)
  static void BroadcastConfigChanged();

  /// \brief Returns the path to the 'ezProject' file
  const ezString& GetProjectFile() const { return m_sProjectPath; }

  /// \brief Returns the path in which the 'ezProject' file is stored
  ezString GetProjectDirectory() const;

  /// \brief Returns the directory path in which project settings etc. should be stored
  ezString GetProjectDataFolder() const;

  /// \brief Starts at the  given document and then searches the tree upwards until it finds an ezProject file.
  static ezString FindProjectDirectoryForDocument(const char* szDocumentPath);

  bool IsDocumentInAllowedRoot(const char* szDocumentPath, ezString* out_RelativePath = nullptr) const;

  void AddAllowedDocumentRoot(const char* szPath);

  /// \brief Makes sure the given sub-folder exists inside the project directory
  void CreateSubFolder(const char* szFolder) const;

private:

  static ezStatus CreateOrOpenProject(const char* szProjectPath, bool bCreate);

private:
  ezToolsProject(const char* szProjectPath);
  ~ezToolsProject();

  ezStatus Create();
  ezStatus Open();

private:
  bool m_bIsClosing;
  ezString m_sProjectPath;
  ezHybridArray<ezString, 4> m_AllowedDocumentRoots;
};



