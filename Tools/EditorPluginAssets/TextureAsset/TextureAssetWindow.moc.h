#pragma once

#include <Foundation/Basics.h>
#include <GuiFoundation/DocumentWindow/DocumentWindow.moc.h>

class QLabel;

class ezTextureAssetDocumentWindow : public ezDocumentWindow
{
  Q_OBJECT

public:
  ezTextureAssetDocumentWindow(ezDocumentBase* pDocument);
  ~ezTextureAssetDocumentWindow();

  virtual const char* GetGroupName() const { return "TextureAsset"; }

private slots:
  

private:
  void UpdatePreview();
  void PropertyEventHandler(const ezDocumentObjectTreePropertyEvent& e);

  QLabel* m_pImageLabel;
};