#ifndef STRUCTUREDEFINITIONCREATOR_H
#define STRUCTUREDEFINITIONCREATOR_H

#include <QDialog>
#include <QString>

class QLabel;
class QComboBox;
class QTableWidget;
class QLineEdit;
struct StructInfo;

class StructureDefinitionCreatorDialog : public QDialog
{
    Q_OBJECT
public:
    explicit StructureDefinitionCreatorDialog(QWidget *parent = nullptr);

signals:
    void definitionSaved(const QString &filePath);

private:
    void addField();
    void removeField();
    void moveField(int direction);
    void updatePreview();
    bool buildStructure(StructInfo &structure, QString &error) const;
    void saveDefinition();

    QLineEdit *m_structName = nullptr;
    QComboBox *m_packAlignment = nullptr;
    QTableWidget *m_fields = nullptr;
    QLabel *m_summary = nullptr;
    QLabel *m_status = nullptr;
};

#endif
