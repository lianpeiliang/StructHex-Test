#ifndef BUSINESSHEXDECODER_H
#define BUSINESSHEXDECODER_H

#include "binarydecoder.h"

#include <QDialog>
#include <QByteArray>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTreeWidget;
class QWidget;

class BusinessHexDecoderDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BusinessHexDecoderDialog(QWidget *parent = nullptr);

private:
    void chooseBusinessConfig();
    void chooseStructureDefinition();
    void chooseHexFile();
    void initializeDefaultsIfReady();
    void decode();
    void clearResult(const QString &message);
    void rebuildBusinessForm(const QJsonObject &fields, const StructInfo &structure);
    void populateBusinessForm(const DecodeResult &decoded);
    void businessFormEdited();
    void renderResults(const DecodeResult &decoded, const StructInfo &structure,
                       const QJsonObject &config);
    bool collectBusinessValues(const QString &fieldPath, QHash<QString, QString> &values,
                               QString &error) const;

    QLineEdit *m_businessPath = nullptr;
    QLineEdit *m_structurePath = nullptr;
    QPlainTextEdit *m_hexInput = nullptr;
    QComboBox *m_byteOrder = nullptr;
    QLabel *m_status = nullptr;
    QTreeWidget *m_results = nullptr;
    QWidget *m_businessForm = nullptr;
    QWidget *m_formContent = nullptr;
    QPushButton *m_copyHexButton = nullptr;
    QHash<QString, QWidget *> m_fieldWidgets;
    QJsonObject m_fieldConfigs;
    QHash<QString, FieldInfo> m_fieldInfo;
    StructInfo m_activeStructure;
    QJsonDocument m_activeConfig;
    QByteArray m_currentBytes;
    bool m_populatingForm = false;
    bool m_hexGeneratedByForm = false;
    bool m_legacyConfigAccepted = false;
};

#endif
