#ifndef BUSINESSHEXDECODER_H
#define BUSINESSHEXDECODER_H

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTreeWidget;

class BusinessHexDecoderDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BusinessHexDecoderDialog(QWidget *parent = nullptr);

private:
    void chooseBusinessConfig();
    void chooseStructureDefinition();
    void chooseHexFile();
    void decode();
    void clearResult(const QString &message);

    QLineEdit *m_businessPath = nullptr;
    QLineEdit *m_structurePath = nullptr;
    QPlainTextEdit *m_hexInput = nullptr;
    QComboBox *m_byteOrder = nullptr;
    QLabel *m_status = nullptr;
    QTreeWidget *m_results = nullptr;
    bool m_legacyConfigAccepted = false;
};

#endif
