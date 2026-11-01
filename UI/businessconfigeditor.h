#ifndef BUSINESSCONFIGEDITOR_H
#define BUSINESSCONFIGEDITOR_H

#include "headerparser.h"
#include <QDialog>
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
    explicit BusinessConfigEditorDialog(const StructInfo &structure, QWidget *parent = nullptr);

private:
    struct FieldConfig {
        QString path;
        QString label;
        QString widget;
        QString unit;
        QString minimum;
        QString maximum;
        QString scale;
        QString decimals;
        QString defaultValue;
        bool defaultIsString = false;
        QString options = QStringLiteral("[]");
    };

    void addFields(const QVector<FieldInfo> &fields, const QString &parentPath,
                   QTreeWidgetItem *parentItem);
    void selectField(int index);
    void saveField();
    void updatePropertyVisibility();
    void openConfig();
    void previewForm();
    void saveConfig();
    void saveConfigAs();
    void writeConfig(const QString &path);
    bool loadConfigFile(const QString &path, QString &error);
    void addOptionRow();
    void removeOptionRow();
    bool validateField(const FieldConfig &field, QString &error) const;
    QJsonDocument configDocument() const;

    StructInfo m_structure;
    QVector<FieldConfig> m_defaultFields;
    QVector<FieldConfig> m_fields;
    QString m_currentConfigPath;
    QTreeWidget *m_fieldTree = nullptr;
    QLabel *m_status = nullptr;
    QWidget *m_conversionGroup = nullptr;
    QLineEdit *m_title = nullptr;
    QLineEdit *m_label = nullptr;
    QComboBox *m_widget = nullptr;
    QWidget *m_unitLabel = nullptr;
    QLineEdit *m_unit = nullptr;
    QWidget *m_minimumLabel = nullptr;
    QLineEdit *m_minimum = nullptr;
    QWidget *m_maximumLabel = nullptr;
    QLineEdit *m_maximum = nullptr;
    QWidget *m_rangeHint = nullptr;
    QLineEdit *m_scale = nullptr;
    QWidget *m_decimalsLabel = nullptr;
    QLineEdit *m_decimals = nullptr;
    QWidget *m_defaultLabel = nullptr;
    QLineEdit *m_defaultValue = nullptr;
    QWidget *m_optionsLabel = nullptr;
    QWidget *m_optionsPanel = nullptr;
    QTableWidget *m_options = nullptr;
    QWidget *m_defaultHint = nullptr;
    int m_selectedIndex = -1;
};

#endif
