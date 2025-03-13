#ifndef SEARCHMANAGER_H
#define SEARCHMANAGER_H

#include "searcherinterface.h"

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QMap>
#include <QStringList>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QReadWriteLock>
#include <QThreadPool>

class SearchManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(SearchManager)
public:
    static SearchManager &instance();

    // 新增: 同步搜索方法
    QStringList searchSync(const QString &searchPath, const QString &searchText);

    // 清除缓存
    void clearCache();

private:
    explicit SearchManager(QObject *parent = nullptr);

    // 输入变化类型
    enum class InputChangeType { Addition,
                                 Deletion,
                                 Replacement,
                                 Unknown };

    // 分析输入变化
    InputChangeType analyzeInputChange(const QString &oldText, const QString &newText);

    // 在本地过滤结果
    QStringList filterLocalResults(const QStringList &sourceResults, const QString &query);

    QString getFileName(const QString &filePath);

    // 从缓存中查找结果
    bool tryGetFromCache(const QString &searchText, QStringList &results);

    // 处理增量搜索（当输入是添加字符时）
    bool handleIncrementalSearch(const QString &searchText, QStringList &results);

    // 处理删除操作搜索
    bool handleDeletionSearch(const QString &searchText, QStringList &results);

    // 添加到缓存
    void addToCache(const QString &key, const QStringList &results);

    // 更新缓存使用情况
    void updateCacheUsage(const QString &key);

    QString m_lastSearchText;

    // 缓存结构
    QMap<QString, QStringList> m_resultsCache;
    QList<QString> m_cacheUsageOrder;   // 用于LRU，使用QList替代QLinkedList
    int m_maxCacheSize = 50;   // 默认缓存大小

    QMutex m_mutex;
};

#endif   // SEARCHMANAGER_H
