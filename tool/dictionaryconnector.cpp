#include "dictionaryconnector.h"
#include "appconfig.h"

#include <QProcess>
#include <QDebug>

namespace {

// POSIX shell 单引号转义：把 word 包进单引号，内部的单引号写成 '\''
// 这样无论 word 含空格/引号/$/; 等都不会被 shell 解释，杜绝命令注入。
QString posixShellQuote(const QString& word)
{
    QString escaped = word;
    escaped.replace("'", "'\\''");
    return "'" + escaped + "'";
}

#ifdef Q_OS_WIN
// Windows cmd 简单转义：双引号包裹，内部双引号翻倍
QString winShellQuote(const QString& word)
{
    QString escaped = word;
    escaped.replace("\"", "\"\"");
    return "\"" + escaped + "\"";
}
#endif

} // namespace

DictionaryConnector::DictionaryConnector()
{
}

DictionaryConnector::~DictionaryConnector()
{
}

DictionaryConnector& DictionaryConnector::instance()
{
    static DictionaryConnector instance;
    return instance;
}

QString DictionaryConnector::buildCommand(const QString& commandTemplate, const QString& word)
{
    const QString trimmedWord = word.trimmed();

#ifdef Q_OS_WIN
    const QString quoted = winShellQuote(trimmedWord);
#else
    const QString quoted = posixShellQuote(trimmedWord);
#endif

    QString cmd = commandTemplate.trimmed();
    if (cmd.contains("{word}")) {
        cmd.replace("{word}", quoted);
    } else {
        // 模板未含占位符：把查询词追加到命令末尾
        cmd = cmd + " " + quoted;
    }
    return cmd;
}

bool DictionaryConnector::runCommand(const QString& commandTemplate,
                                     const QString& word,
                                     QString* errorMessage)
{
    if (commandTemplate.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("Dictionary command is not configured. Please set it in Settings.");
        }
        return false;
    }
    if (word.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("The query word is empty.");
        }
        return false;
    }

    const QString cmd = buildCommand(commandTemplate, word);

#ifdef Q_OS_WIN
    bool ok = QProcess::startDetached("cmd", QStringList() << "/c" << cmd);
#else
    bool ok = QProcess::startDetached("/bin/sh", QStringList() << "-c" << cmd);
#endif

    if (!ok && errorMessage) {
        *errorMessage = tr("Failed to start the external process. Please check the command:\n%1").arg(cmd);
    }
    return ok;
}

bool DictionaryConnector::lookup(const QString& word)
{
    const QString tmpl = AppConfig::instance().dictionaryCommand();

    if (tmpl.trimmed().isEmpty()) {
        emit lookupFailed(tr("Dictionary command is not configured. Please set the external dictionary command in Settings."));
        return false;
    }
    if (word.trimmed().isEmpty()) {
        emit lookupFailed(tr("The query word is empty."));
        return false;
    }

    QString error;
    bool ok = runCommand(tmpl, word, &error);

    if (ok) {
        qInfo() << "DictionaryConnector: launched dictionary for word:" << word;
        emit lookupStarted(word);
    } else {
        qWarning() << "DictionaryConnector:" << error;
        emit lookupFailed(error);
    }
    return ok;
}

bool DictionaryConnector::isConfigured() const
{
    return !AppConfig::instance().dictionaryCommand().trimmed().isEmpty();
}
