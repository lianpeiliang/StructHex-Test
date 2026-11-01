#include "businessconfigutils.h"

#include "definitionjson.h"

#include <QFile>
#include <QJsonObject>
#include <QSet>

namespace {

void appendFieldPaths(const QVector<FieldInfo> &fields, const QString &parentPath, QSet<QString> &paths)
{
    for (const FieldInfo &field : fields) {
        const QString path = parentPath.isEmpty() ? field.name : parentPath + QLatin1Char('.') + field.name;
        paths.insert(path);
        if (!field.children.isEmpty())
            appendFieldPaths(field.children, path, paths);
    }
}

} // namespace

bool loadBusinessConfig(const QString &path, QJsonDocument &document, QString &error)
{
    error.clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("无法读取业务配置：%1").arg(file.errorString());
        return false;
    }
    const int maximumBytes = 16 * 1024 * 1024;
    const QByteArray bytes = file.read(maximumBytes + 1);
    if (file.error() != QFileDevice::NoError) {
        error = QStringLiteral("读取业务配置失败：%1").arg(file.errorString());
        return false;
    }
    if (bytes.size() > maximumBytes) {
        error = QStringLiteral("业务配置 JSON 超过 16 MiB。");
        return false;
    }
    QJsonParseError parseError;
    document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        error = QStringLiteral("业务配置 JSON 格式错误：%1").arg(parseError.errorString());
        return false;
    }
    return true;
}

bool validateBusinessConfig(const QJsonDocument &document, const StructInfo &structure, QString &error)
{
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("struct")).toString() != structure.name) {
        error = QStringLiteral("业务配置对应“%1”，结构定义文件对应“%2”。")
            .arg(root.value(QStringLiteral("struct")).toString(), structure.name);
        return false;
    }
    if (!root.value(QStringLiteral("title")).isString()
        || root.value(QStringLiteral("title")).toString().trimmed().isEmpty()
        || root.value(QStringLiteral("layout")).toString() != QLatin1String("form")
        || !root.value(QStringLiteral("fields")).isObject()) {
        error = QStringLiteral("业务配置必须包含非空 title、layout=form 和 fields 对象。");
        return false;
    }
    if (root.contains(QStringLiteral("structureFingerprint"))
        && (!root.value(QStringLiteral("structureFingerprint")).isString()
            || root.value(QStringLiteral("structureFingerprint")).toString()
                != structureDefinitionFingerprint(structure))) {
        error = QStringLiteral("业务配置与所选结构定义的字段布局不一致。");
        return false;
    }

    QSet<QString> expectedPaths;
    appendFieldPaths(structure.fields, QString(), expectedPaths);
    const QJsonObject fields = root.value(QStringLiteral("fields")).toObject();
    if (fields.size() != expectedPaths.size()) {
        error = QStringLiteral("业务配置的字段数量与结构定义不一致。");
        return false;
    }
    const QStringList widgetTypes = QStringList() << QStringLiteral("spinbox")
        << QStringLiteral("doublespinbox") << QStringLiteral("combobox")
        << QStringLiteral("checkbox") << QStringLiteral("lineedit")
        << QStringLiteral("group") << QStringLiteral("readonly");
    for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) {
        if (!expectedPaths.contains(it.key()) || !it.value().isObject()) {
            error = QStringLiteral("业务配置包含无效字段：%1").arg(it.key());
            return false;
        }
        const QJsonObject field = it.value().toObject();
        const QString widget = field.value(QStringLiteral("widget")).toString();
        if (!field.value(QStringLiteral("label")).isString() || !widgetTypes.contains(widget)) {
            error = QStringLiteral("字段 %1 缺少有效的 label 或 widget。").arg(it.key());
            return false;
        }
        if (widget == QLatin1String("combobox") && !field.value(QStringLiteral("options")).isArray()) {
            error = QStringLiteral("字段 %1 是组合框，但没有 options 数组。").arg(it.key());
            return false;
        }
    }
    for (const QString &path : expectedPaths) {
        if (!fields.contains(path)) {
            error = QStringLiteral("业务配置缺少结构字段：%1").arg(path);
            return false;
        }
    }
    error.clear();
    return true;
}
