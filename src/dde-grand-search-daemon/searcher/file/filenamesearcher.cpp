// SPDX-FileCopyrightText: 2021 - 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filenamesearcher.h"
#include "global/builtinsearch.h"
#include "filenameworker.h"

#include <QDBusInterface>
#include <QDBusReply>

using namespace GrandSearch;

FileNameSearcher::FileNameSearcher(QObject *parent)
    : Searcher(parent)
{
}

QString FileNameSearcher::name() const
{
    return GRANDSEARCH_CLASS_FILE_DEEPIN;
}

bool FileNameSearcher::isActive() const
{
    QDBusInterface interface("com.deepin.anything",
                             "/com/deepin/anything",
                             "com.deepin.anything",
                             QDBusConnection::systemBus());
    interface.setTimeout(500);
    if (!interface.isValid()) {
        qWarning() << QDBusConnection::systemBus().lastError().message();
        return false;
    }

    return true;
}

bool FileNameSearcher::activate()
{
    return false;
}

ProxyWorker *FileNameSearcher::createWorker() const
{
    qDebug() << "===> create worker";
    auto worker = new FileNameWorker(name());
    return worker;
}

bool FileNameSearcher::action(const QString &action, const QString &item)
{
    Q_UNUSED(item)
    qWarning() << "no such action:" << action << ".";
    return false;
}
