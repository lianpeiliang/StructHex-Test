#ifndef BUSINESSCONFIGUTILS_H
#define BUSINESSCONFIGUTILS_H

#include "headerparser.h"

#include <QJsonDocument>

// Shared loading and structure matching for business configuration JSON files.
bool loadBusinessConfig(const QString &path, QJsonDocument &document, QString &error);
bool validateBusinessConfig(const QJsonDocument &document, const StructInfo &structure, QString &error);

#endif
