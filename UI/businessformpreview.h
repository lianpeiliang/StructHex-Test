#ifndef BUSINESSFORMPREVIEW_H
#define BUSINESSFORMPREVIEW_H

#include <QDialog>
#include <QJsonDocument>
#include <QJsonObject>

class BusinessFormPreviewDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BusinessFormPreviewDialog(const QJsonDocument &config, QWidget *parent = nullptr);

private:
    QWidget *createEditor(const QString &name, const QJsonObject &config);
};

#endif
