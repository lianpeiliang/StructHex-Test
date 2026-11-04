#ifndef STRUCTUREDEFINITIONCREATOR_H
#define STRUCTUREDEFINITIONCREATOR_H

#include <QDialog>
#include <QString>

class QLabel;
class QComboBox;
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QPushButton;
struct FieldInfo;
struct StructInfo;

class StructureDefinitionCreatorDialog : public QDialog
{
    Q_OBJECT
public:
    explicit StructureDefinitionCreatorDialog(QWidget *parent = nullptr);
    StructureDefinitionCreatorDialog(const StructInfo &structure, const QString &filePath,
                                     QWidget *parent = nullptr);
    static bool supportsEditing(const StructInfo &structure, QString &reason);

signals:
    void definitionSaved(const QString &filePath);

private:
    void addField();
    void addNestedField();
    QTreeWidgetItem *appendScalarField(QTreeWidgetItem *parent, const QString &name,
                                       const QString &typeName);
    QTreeWidgetItem *appendRecordField(QTreeWidgetItem *parent, const QString &name,
                                       const QString &typeName);
    void appendExistingField(QTreeWidgetItem *parent, const FieldInfo &field);
    QTreeWidgetItem *currentContainer() const;
    void removeField();
    void moveField(int direction);
    void updatePreview();
    bool buildStructure(StructInfo &structure, QString &error) const;
    void saveDefinition();
    void saveDefinitionAs();
    void saveToPath(const QString &filePath);

    QString m_currentPath;
    QString m_originalFingerprint;
    QLineEdit *m_structName = nullptr;
    QComboBox *m_packAlignment = nullptr;
    QTreeWidget *m_fields = nullptr;
    QLabel *m_summary = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_saveAsButton = nullptr;
};

#endif
