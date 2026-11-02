#ifndef BUSINESSFORMPREVIEW_H
#define BUSINESSFORMPREVIEW_H

#include <QDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include "headerparser.h"
#include <QHash>

class BusinessFormPreviewDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BusinessFormPreviewDialog(const QJsonDocument &config, const StructInfo &structure,
                                       const QString &structureDefinitionPath, QWidget *parent = nullptr);

private:
    QWidget *createEditor(const QString &name, const QJsonObject &config);
    void addFieldInfo(const QVector<FieldInfo> &fields, const QString &parentPath);
    void generateHex();

    StructInfo m_structure;
    QString m_structureDefinitionPath;
    QHash<QString, QWidget *> m_fieldWidgets;
    QHash<QString, QJsonObject> m_fieldConfigs;
    QHash<QString, FieldInfo> m_fieldInfo;
};

#endif
