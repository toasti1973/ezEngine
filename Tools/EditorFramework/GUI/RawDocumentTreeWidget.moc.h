#pragma once

#include <EditorFramework/GUI/RawDocumentTreeModel.moc.h>
#include <ToolsFoundation/Selection/SelectionManager.h>
#include <QTreeView>

class EZ_EDITORFRAMEWORK_DLL ezRawDocumentTreeWidget : public QTreeView
{
  Q_OBJECT

public:

  ezRawDocumentTreeWidget(QWidget* pParent, const ezDocumentBase* pDocument);
  ~ezRawDocumentTreeWidget();

  void EnsureLastSelectedItemVisible();

protected:
  virtual void keyPressEvent(QKeyEvent* e) override;

private slots:
  void on_selectionChanged_triggered(const QItemSelection& selected, const QItemSelection& deselected);

private:
  void SelectionEventHandler(const ezSelectionManager::Event& e);

private:
  ezRawDocumentTreeModel m_Model;
  const ezDocumentBase* m_pDocument;
  bool m_bBlockSelectionSignal;
};

