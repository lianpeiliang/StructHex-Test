#ifndef DEFINITIONFILEBAR_H
#define DEFINITIONFILEBAR_H

#include <QWidget>

class QLineEdit;
class QPushButton;

// 定义文件操作栏：管理待加载路径，将打开、回车和重载统一转换为加载请求。
class DefinitionFileBar : public QWidget
{
    Q_OBJECT
public:
    explicit DefinitionFileBar(const QString &filePath, QWidget *parent = nullptr);
    void setExportEnabled(bool enabled); // 有可用定义时允许导出，与字段值是否合法无关。
    void setEditEnabled(bool enabled);
    void setLoadedPath(const QString &filePath);

signals:
    void createRequested(); // 从界面新建结构定义。
    void editRequested(); // 修改当前加载的结构定义 JSON。
    void loadRequested(const QString &filePath); // 只表达用户意图，不校验或解析文件。
    void exportRequested(); // 请求导出当前已加载定义，不读取正在编辑的路径。

private:
    QLineEdit *m_filePath; // 编辑中的路径与当前会话的已加载路径相互独立。
    QPushButton *m_editButton;
    QPushButton *m_exportButton;
};

#endif
