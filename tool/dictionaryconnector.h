#ifndef DICTIONARYCONNECTOR_H
#define DICTIONARYCONNECTOR_H

#include <QObject>
#include <QString>

/**
 * @brief 词典连接器 - 调用外部词典程序
 *
 * 全局单例，与 OCR 查词配合使用。调用命令来自 AppConfig 的
 * dictionaryCommand 模板：用 {word} 占位查询词，经系统 shell 执行。
 */
class DictionaryConnector : public QObject
{
    Q_OBJECT

public:
    static DictionaryConnector& instance();

    /**
     * @brief 查词：用配置的命令模板调用外部词典
     * @param word 要查的单词
     * @return 是否成功启动外部进程
     */
    bool lookup(const QString& word);

    /**
     * @brief 是否已配置词典命令
     */
    bool isConfigured() const;

    /**
     * @brief 用给定命令模板与查询词执行一次（供设置里的"测试"使用）
     * @param commandTemplate 命令模板（含 {word} 占位，缺省则把词追加到末尾）
     * @param word 查询词
     * @param errorMessage 失败原因（可选）
     * @return 是否成功启动外部进程
     */
    static bool runCommand(const QString& commandTemplate,
                           const QString& word,
                           QString* errorMessage = nullptr);

    /**
     * @brief 把命令模板与查询词组装成最终 shell 命令（word 已做 shell 转义）
     */
    static QString buildCommand(const QString& commandTemplate, const QString& word);

signals:
    void lookupStarted(const QString& word);
    void lookupFailed(const QString& error);

private:
    DictionaryConnector();
    ~DictionaryConnector();
    DictionaryConnector(const DictionaryConnector&) = delete;
    DictionaryConnector& operator=(const DictionaryConnector&) = delete;
};

#endif // DICTIONARYCONNECTOR_H
