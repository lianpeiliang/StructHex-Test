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
        QString options = QStringLiteral("[]");
    };

    void addFields(const QVector<FieldInfo> &fields, const QString &parentPath,
                   QTreeWidgetItem *parentItem);
    void selectField(int index);
    void saveField();
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
    QLineEdit *m_title = nullptr;
    QLineEdit *m_label = nullptr;
    QComboBox *m_widget = nullptr;
    QLineEdit *m_unit = nullptr;
    QLineEdit *m_minimum = nullptr;
    QLineEdit *m_maximum = nullptr;
    QLineEdit *m_scale = nullptr;
    QLineEdit *m_decimals = nullptr;
    QLineEdit *m_defaultValue = nullptr;
    QTableWidget *m_options = nullptr;
    int m_selectedIndex = -1;
};

#endif
