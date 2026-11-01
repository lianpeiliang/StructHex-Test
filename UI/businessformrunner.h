#ifndef BUSINESSFORMRUNNER_H
#define BUSINESSFORMRUNNER_H

#include <QDialog>

class QLabel;
class QLineEdit;

class BusinessFormRunnerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BusinessFormRunnerDialog(QWidget *parent = nullptr);

private:
    void chooseBusinessConfig();
    void chooseStructureDefinition();
    void runForm();

    QLineEdit *m_businessPath = nullptr;
    QLineEdit *m_structurePath = nullptr;
    QLabel *m_status = nullptr;
};

#endif
