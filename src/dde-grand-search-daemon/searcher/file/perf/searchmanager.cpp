#include "searchmanager.h"
#include "anythingsearcher.h"

#include <QFileInfo>
#include <QtConcurrent>
#include <QRegularExpression>
#include <QDebug>
#include <QThread>
#include <QElapsedTimer>
#include <QDir>

#include <lucene++/LuceneHeaders.h>
#include <unistd.h>
#include <filesystem>

using namespace Lucene;
namespace {

// 获取索引目录路径：/run/user/[当前用户id]/deepin-anything-server
static QString getIndexDirectory()
{
    QString indexDir = QString("/run/user/%1/deepin-anything-server").arg(getuid());
    return indexDir;
}

static QString getHomeDirectory()
{
    QString homeDir;

    if (QFileInfo::exists("/data/home")) {
        homeDir = "/data";
    } else if (QFileInfo::exists("/persistent/home")) {
        homeDir = "/persistent";
    }

    homeDir.append(QDir::homePath());

    return homeDir;
}

static QStringList search2(const QString &orginPath, const QString &key, bool nrt)
{
    QString keywords = key;
    QString path = orginPath;
    if (path.startsWith(QDir::homePath()))
        path.replace(0, QDir::homePath().length(), getHomeDirectory());

    if (keywords.isEmpty()) {
        return {};
    }

    // 原始词条
    String query_terms = StringUtils::toUnicode(keywords.toStdString());

    // 给普通 parser 用
    if (keywords.at(0) == QChar('*') || keywords.at(0) == QChar('?')) {
        keywords = keywords.mid(1);
    }

    try {
        int32_t max_results;

        // 获取索引目录
        QString indexDir = getIndexDirectory();
        qDebug() << "搜索索引目录:" << indexDir;

        // 打开索引目录
        FSDirectoryPtr directory = FSDirectory::open(StringUtils::toUnicode(indexDir.toStdString()));

        // 检查索引是否存在
        if (!IndexReader::indexExists(directory)) {
            qWarning() << "索引不存在:" << indexDir;
            return QStringList();
        }

        // 打开索引读取器
        IndexReaderPtr reader = IndexReader::open(directory, true);
        if (reader->numDocs() == 0) {
            qWarning() << "索引为空，没有文档";
            return QStringList();
        }

        // 创建搜索器
        SearcherPtr searcher = newLucene<IndexSearcher>(reader);

        if (reader->numDocs() == 0) {
            qWarning() << "索引为空，没有文档";
            return QStringList();
        }

        max_results = reader->numDocs();

        String queryString = L"*" + StringUtils::toLower(StringUtils::toUnicode(keywords.toStdString())) + L"*";
        TermPtr term = newLucene<Term>(L"file_name", queryString);
        QueryPtr query = newLucene<WildcardQuery>(term);

        auto search_results = searcher->search(query, max_results);

        QStringList results;
        results.reserve(search_results->scoreDocs.size());
        for (const auto &score_doc : search_results->scoreDocs) {
            DocumentPtr doc = searcher->doc(score_doc->doc);
            auto result = QString::fromStdWString(doc->get(L"full_path"));
            if (result.startsWith(path)) {
                results.append(std::move(result));
            }
        }

        return results;
    } catch (const LuceneException &e) {

        return {};
    }
}

static QStringList search(const QString &originPath, const QString &key)
{
    if (key.isEmpty()) {
        return QStringList();
    }

    QElapsedTimer timer;
    timer.start();

    try {
        // 获取索引目录
        QString indexDir = getIndexDirectory();
        qDebug() << "搜索索引目录:" << indexDir;

        // 打开索引目录
        FSDirectoryPtr directory = FSDirectory::open(StringUtils::toUnicode(indexDir.toStdString()));

        // 检查索引是否存在
        if (!IndexReader::indexExists(directory)) {
            qWarning() << "索引不存在:" << indexDir;
            return QStringList();
        }

        // 打开索引读取器
        IndexReaderPtr reader = IndexReader::open(directory, true);
        if (reader->numDocs() == 0) {
            qWarning() << "索引为空，没有文档";
            return QStringList();
        }

        // 创建搜索器
        SearcherPtr searcher = newLucene<IndexSearcher>(reader);

        // 创建多种查询以提高匹配率
        BooleanQueryPtr booleanQuery = newLucene<BooleanQuery>();

        // 1. 原始关键词处理
        String lowerKey = StringUtils::toLower(StringUtils::toUnicode(key.toStdString()));

        // 2. 使用通配符匹配
        TermPtr wildcardTerm = newLucene<Term>(L"file_name", L"*" + lowerKey + L"*");
        QueryPtr wildcardQuery = newLucene<WildcardQuery>(wildcardTerm);
        booleanQuery->add(wildcardQuery, BooleanClause::SHOULD);

        // 3. 使用前缀匹配
        TermPtr prefixTerm = newLucene<Term>(L"file_name", lowerKey);
        QueryPtr prefixQuery = newLucene<PrefixQuery>(prefixTerm);
        booleanQuery->add(prefixQuery, BooleanClause::SHOULD);

        // 4. 使用精确匹配
        TermPtr exactTerm = newLucene<Term>(L"file_name", lowerKey);
        QueryPtr termQuery = newLucene<TermQuery>(exactTerm);
        booleanQuery->add(termQuery, BooleanClause::SHOULD);

        // 5. 设置至少一个SHOULD子句必须匹配
        booleanQuery->setMinimumNumberShouldMatch(1);

        // 添加路径限制（如果提供）
        if (!originPath.isEmpty()) {
            BooleanQueryPtr combinedQuery = newLucene<BooleanQuery>();
            combinedQuery->add(booleanQuery, BooleanClause::MUST);

            // 路径前缀查询
            String pathPrefix = StringUtils::toUnicode(originPath.toStdString());
            QueryPtr pathQuery = newLucene<PrefixQuery>(newLucene<Term>(L"full_path", pathPrefix));
            combinedQuery->add(pathQuery, BooleanClause::MUST);

            booleanQuery = combinedQuery;
        }

        // 调试信息
        qDebug() << "搜索查询:" << QString::fromStdWString(booleanQuery->toString());
        qDebug() << "索引文档数:" << reader->numDocs();

        // 执行搜索
        int32_t maxResults = reader->numDocs();
        TopDocsPtr topDocs = searcher->search(booleanQuery, maxResults);

        qDebug() << "匹配文档数:" << topDocs->scoreDocs.size();

        // 处理结果
        QStringList results;
        results.reserve(topDocs->scoreDocs.size());

        for (int32_t i = 0; i < topDocs->scoreDocs.size(); ++i) {
            ScoreDocPtr scoreDoc = topDocs->scoreDocs[i];
            DocumentPtr doc = searcher->doc(scoreDoc->doc);

            // 获取全路径
            String fullPath = doc->get(L"full_path");
            if (!fullPath.empty()) {
                QString path = QString::fromStdWString(fullPath);

                // 检查文件是否存在
                if (QFileInfo::exists(path)) {
                    results.append(path);
                }
            }
        }

        // 如果没有结果，尝试检查索引中的字段
        if (results.isEmpty() && reader->numDocs() > 0) {
            qDebug() << "无搜索结果，尝试分析索引...";

            // 获取第一个文档的字段
            DocumentPtr sampleDoc = searcher->doc(0);
            auto fieldNames = sampleDoc->getFields();
            QString fields;
            for (auto &field : fieldNames) {
                fields += QString::fromStdWString(field->name()) + " ";
            }
            qDebug() << "索引文档字段:" << fields;

            // 尝试查询所有文档，看是否有问题
            qDebug() << "尝试使用MatchAllDocsQuery...";
            QueryPtr matchAllQuery = newLucene<MatchAllDocsQuery>();
            TopDocsPtr allDocs = searcher->search(matchAllQuery, reader->numDocs());
            qDebug() << "MatchAllDocsQuery结果数:" << allDocs->scoreDocs.size();

            // 如果有文档，检查第一个
            if (!allDocs->scoreDocs.empty()) {
                DocumentPtr firstDoc = searcher->doc(allDocs->scoreDocs[0]->doc);
                qDebug() << "第一个文档路径:" << QString::fromStdWString(firstDoc->get(L"full_path"));
                qDebug() << "文件名:" << QString::fromStdWString(firstDoc->get(L"file_name"));
            }
        }

        qDebug() << "搜索完成，耗时:" << timer.elapsed() << "ms，找到结果:" << results.size() << "个";
        return results;
    } catch (const LuceneException &e) {
        qWarning() << "Lucene搜索异常:" << QString::fromStdWString(e.getError());
        return QStringList();
    } catch (const std::exception &e) {
        qWarning() << "搜索过程中发生异常:" << e.what();
        return QStringList();
    } catch (...) {
        qWarning() << "搜索过程中发生未知异常";
        return QStringList();
    }
}

};

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

    QStringList results = ::search2(searchPath, searchText, true);

    // AnythingSearcher searcher;
    // // 执行同步搜索
    // QStringList results = searcher.searchSync(searchPath, searchText);

    // 缓存结果

    if (!results.isEmpty()) {
        QMutexLocker guard(&m_mutex);
        addToCache(searchText, results);
        m_lastSearchText = searchText;
    }

    return results;
}
