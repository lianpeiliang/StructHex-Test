#ifndef BUSINESSFORMSERVICE_H
#define BUSINESSFORMSERVICE_H

#include "binarydecoder.h"
#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

// Editable business-form metadata. This model is independent of any control toolkit.
struct BusinessFieldConfig
{
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

// A display/input conversion result shared by every presentation layer.
struct BusinessValueResult
{
    bool success = false;
    QString value;
    QString error;
};

// Result of creating or editing business-form bytes. When successful, decoded
// contains the refreshed values and field tree for the presentation to render.
struct BusinessFormResult
{
    bool success = false;
    QByteArray bytes;
    DecodeResult decoded;
    QStringList diagnostics;
};

// Defaults, validation and JSON serialization for one business-field definition.
QString defaultBusinessWidget(FieldValueKind kind);
QJsonValue businessNumberOrString(const QString &text);
bool validateBusinessFieldConfig(const BusinessFieldConfig &field, QString &error);
QJsonObject businessFieldConfigObject(const BusinessFieldConfig &field);

// Convert a raw structure value into the value expected by a business editor.
// Numeric controls apply their configured scale; checkbox values are normalized.
BusinessValueResult businessRawToInputValue(const FieldInfo &field,
                                           const QJsonObject &fieldConfig,
                                           const QString &rawValue);

// Convert a normalized editor value back to the raw value used by the structure.
BusinessValueResult businessInputToRawValue(const FieldInfo &field,
                                           const QJsonObject &fieldConfig,
                                           const QString &inputValue);

// Convert values collected from presentation controls and encode the structure.
// A non-empty baseBytes preserves all untouched fields, arrays and padding.
// The result also includes a fresh little-endian decode for the UI to display.
BusinessFormResult encodeBusinessForm(const StructInfo &structure,
                                      const QJsonObject &fieldConfigs,
                                      const QHash<QString, QString> &inputValues,
                                      const QByteArray &baseBytes = QByteArray());

#endif
