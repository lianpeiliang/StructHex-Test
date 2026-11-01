#ifndef WORKFLOWLAUNCHER_H
#define WORKFLOWLAUNCHER_H

#include <QDialog>
#include <QString>

class WorkflowLauncher : public QDialog
{
    Q_OBJECT
public:
    explicit WorkflowLauncher(const QString &defaultStructurePath, QWidget *parent = nullptr);

private:
    void openStructureWorkspace();
    void openBusinessDesigner();
    void openBusinessRunner();

    QString m_defaultStructurePath;
};

#endif
