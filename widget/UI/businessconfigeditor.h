#ifndef BUSINESSCONFIGEDITOR_H
#define BUSINESSCONFIGEDITOR_H

#include "business/businessformservice.h"
#include "headerparser.h"
#include <QDialog>
#include <QHash>
#include <QJsonDocument>
#include <QVector>

class QComboBox;
class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QTableWidget;

// Lets users describe how structure fields should appear in a business form.
// It edits UI metadata only; no business field values are collected here.
class BusinessConfigEditorDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BusinessConfigEditorDialog(const StructInfo &structure,
                                        const QString &structureDefinitionPath = QString(),
                                        QWidget *parent = nullptr);

private:
    typedef BusinessFieldConfig FieldConfig;

    void addFields(const QVector<FieldInfo> &fields, const QString &parentPath,
                   QTreeWidgetItem *parentItem);
    void selectField(int index);
    void saveField();
    void updatePropertyVisibility();
    void openConfig();
    void chooseStructureDefinition();
    void previewForm();
    void saveConfig();
    void saveConfigAs();
    void writeConfig(const QString &path);
    bool loadConfigFile(const QString &path, QString &error);
    void addOptionRow();
    void removeOptionRow();
    QJsonDocument configDocument() const;

    StructInfo m_structure;
    QString m_structureDefinitionPath;
    QVector<FieldConfig> m_defaultFields;
    QVector<FieldConfig> m_fields;
    QHash<QString, QWidget *> m_propertyRows;
    QString m_currentConfigPath;
    QTreeWidget *m_fieldTree = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_structureStatus = nullptr;
    QWidget *m_conversionGroup = nullptr;
    QLineEdit *m_title = nullptr;
    QLineEdit *m_label = nullptr;
    QComboBox *m_widget = nullptr;
    QLineEdit *m_unit = nullptr;
    QLineEdit *m_minimum = nullptr;
    QLineEdit *m_maximum = nullptr;
    QWidget *m_rangeHint = nullptr;
    QLineEdit *m_scale = nullptr;
    QLineEdit *m_decimals = nullptr;
    QLineEdit *m_defaultValue = nullptr;
    QWidget *m_optionsPanel = nullptr;
    QTableWidget *m_options = nullptr;
    QWidget *m_defaultHint = nullptr;
    int m_selectedIndex = -1;
};

#endif
