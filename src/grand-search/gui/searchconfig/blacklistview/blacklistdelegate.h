/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     zhaohanxi<zhaohanxi@uniontech.com>
 *
 * Maintainer: zhaohanxi<zhaohanxi@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef BLACKLISTDELEGATE_H
#define BLACKLISTDELEGATE_H

#include <DListView>
#include <DStyledItemDelegate>

namespace GrandSearch {
class BlackListDelegate : public Dtk::Widget::DStyledItemDelegate
{
    Q_OBJECT
public:
    explicit BlackListDelegate(QAbstractItemView *parent = Q_NULLPTR);
    ~BlackListDelegate() override;

protected:
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    void drawPathsText(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void drawItemBackground(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};
}

#endif // BLACKLISTDELEGATE_H
