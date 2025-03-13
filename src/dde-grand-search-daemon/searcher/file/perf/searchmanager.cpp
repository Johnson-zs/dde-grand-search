#include "searchmanager.h"
#include "anythingsearcher.h"

#include <QFileInfo>
#include <QtConcurrent>
#include <QRegularExpression>
#include <QDebug>
#include <QThread>

SearchManager::SearchManager(QObject *parent)
    : QObject(parent)
{
}

SearchManager &SearchManager::instance()
{
    static SearchManager ins;
    return ins;
}

SearchManager::InputChangeType SearchManager::analyzeInputChange(const QString &oldText, const QString &newText)
{
    if (newText.length() > oldText.length()) {
        if (newText.startsWith(oldText)) {
            return InputChangeType::Addition;
        }
    } else if (newText.length() < oldText.length()) {
        if (oldText.startsWith(newText)) {
            return InputChangeType::Deletion;
        }
    }

    return InputChangeType::Replacement;
}

QString SearchManager::getFileName(const QString &filePath)
{

    // 缓存未命中，计算文件名
    int lastSeparator = filePath.lastIndexOf('/');
    QString fileName = (lastSeparator == -1) ? filePath : filePath.mid(lastSeparator + 1);

    return fileName;
}

QStringList SearchManager::filterLocalResults(const QStringList &sourceResults, const QString &query)
{
    QStringList filteredResults;
    filteredResults.reserve(sourceResults.size());

    const QString queryLower = query.toLower();

    for (const QString &filePath : sourceResults) {
        if (getFileName(filePath).toLower().contains(queryLower)) {
            filteredResults.append(filePath);
        }
    }

    return filteredResults;
}

void SearchManager::clearCache()
{
    QMutexLocker guard(&m_mutex);
    m_resultsCache.clear();
    m_cacheUsageOrder.clear();
    m_lastSearchText.clear();
}

bool SearchManager::tryGetFromCache(const QString &searchText, QStringList &results)
{
    // 首先检查是否直接有缓存
    if (m_resultsCache.contains(searchText)) {
        qDebug() << "===> search from direct cache: " << searchText;
        results = m_resultsCache[searchText];
        updateCacheUsage(searchText);   // 更新使用情况
        return true;
    }

    // 检查输入变化类型
    InputChangeType changeType = analyzeInputChange(m_lastSearchText, searchText);

    // 根据变化类型处理
    switch (changeType) {
    case InputChangeType::Addition:
        return handleIncrementalSearch(searchText, results);
    case InputChangeType::Deletion:
        return handleDeletionSearch(searchText, results);
    default:
        qDebug() << "==> cache break: replacement or unknown change";
        return false;
    }
}

bool SearchManager::handleIncrementalSearch(const QString &searchText, QStringList &results)
{
    // 如果新输入是旧输入的扩展，并且我们有旧缓存
    if (searchText.startsWith(m_lastSearchText) && m_resultsCache.contains(m_lastSearchText)) {
        qDebug() << "===> search from cache(add): " << searchText;
        results = filterLocalResults(m_resultsCache[m_lastSearchText], searchText);
        addToCache(searchText, results);
        updateCacheUsage(m_lastSearchText);   // 更新基础缓存的使用情况
        return true;
    }
    return false;
}

bool SearchManager::handleDeletionSearch(const QString &searchText, QStringList &results)
{
    // 查找最佳前缀匹配
    QString bestPrefix;

    for (auto it = m_resultsCache.begin(); it != m_resultsCache.end(); ++it) {
        // 找到所有是当前查询前缀的缓存项
        if (searchText.startsWith(it.key())) {
            // 选择最长的前缀（最接近当前查询的）
            if (bestPrefix.isEmpty() || it.key().length() > bestPrefix.length()) {
                bestPrefix = it.key();
            }
        }
    }

    if (!bestPrefix.isEmpty()) {
        qDebug() << "===> search from prefix cache(del): " << bestPrefix << " for " << searchText;
        results = filterLocalResults(m_resultsCache[bestPrefix], searchText);
        addToCache(searchText, results);
        updateCacheUsage(bestPrefix);   // 更新基础缓存的使用情况
        return true;
    }

    return false;
}

void SearchManager::addToCache(const QString &key, const QStringList &results)
{
    //   QWriteLocker writeLock(&m_cacheLock);

    // 如果缓存已满，移除最久未使用的项
    if (m_resultsCache.size() >= m_maxCacheSize && !m_resultsCache.contains(key)) {
        QString oldestKey = m_cacheUsageOrder.takeLast();
        m_resultsCache.remove(oldestKey);
    }

    // 添加新缓存项
    m_resultsCache[key] = results;
    updateCacheUsage(key);
}

void SearchManager::updateCacheUsage(const QString &key)
{
    // 注意：此方法应在持有m_cacheLock的写锁时调用
    m_cacheUsageOrder.removeAll(key);
    m_cacheUsageOrder.prepend(key);
}

QStringList SearchManager::searchSync(const QString &searchPath, const QString &searchText)
{
    // 如果输入为空，直接返回空结果
    if (searchText.isEmpty()) {
        return QStringList();
    }

    // 尝试从缓存获取结果
    {
        QMutexLocker guard(&m_mutex);
        QStringList cachedResults;
        if (!m_lastSearchText.isEmpty() && tryGetFromCache(searchText, cachedResults)) {
            qDebug() << "===> searchSync: using cache for " << searchText;
            return cachedResults;
        }
    }

    AnythingSearcher searcher;
    // 执行同步搜索
    QStringList results = searcher.searchSync(searchPath, searchText);

    // 缓存结果

    if (!results.isEmpty()) {
        QMutexLocker guard(&m_mutex);
        addToCache(searchText, results);
        m_lastSearchText = searchText;
    }

    return results;
}
