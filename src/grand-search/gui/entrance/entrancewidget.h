/*
 * Copyright (C) 2021 Uniontech Software Technology Co., Ltd.
 *
 * Author:     wangchunlin<wangchunlin@uniontech.com>
 *
 * Maintainer: wangchunlin<wangchunlin@uniontech.com>
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
#ifndef ENTRANCEWIDGET_H
#define ENTRANCEWIDGET_H

#include <DBlurEffectWidget>

#include <QScopedPointer>

class EntranceWidgetPrivate;
class EntranceWidget : public Dtk::Widget::DBlurEffectWidget
{
    Q_OBJECT
public:
    explicit EntranceWidget(QWidget *parent = nullptr);
    ~EntranceWidget();

private:
    void initUI();
    void initConnections();

signals:
    void searchTextChanged(const QString &txt);

private:
    QScopedPointer<EntranceWidgetPrivate> d_p;
};

#endif // ENTRANCEWIDGET_H
