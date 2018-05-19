/*
 * Copyright (C) 2017 Tycho Softworks.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TOOLBAR_HPP_
#define TOOLBAR_HPP_

#include <QObject>
#include <QToolBar>
#include <QMenu>
#include <QMenuBar>
#include <QLineEdit>
#include <QPushButton>

#include "../Common/compiler.hpp"

class Toolbar final : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY(Toolbar)

public:
    Toolbar(QWidget *parent, QToolBar *bar, QMenuBar *mp = nullptr);

    QString topic() const {
        return currentTopic;
    }

    void clearSearch();
    void enableSearch();
    void disableSearch();
    void disableTopics();
    void enableTopics();
    void hideSearch();
    void noSearch();
    void showSession();
    void noSession();
    void setOperators();
    void setTopics(const QString& topic, const QStringList& topics);
    void newTopics(const QString& topic, const QStringList& topics);
    QString searching();

    static inline Toolbar *instance() {
        return Instance;
    }

    static inline QLineEdit *search() {
        return Search;
    }

    static void setTitle(const QString& text);
    static void setStyle(const QString& text);

private:
    QPoint mpos;
    bool moving;
    QMenuBar *menu;
    QString currentTopic;

    static Toolbar *Instance;
    static QLineEdit *Search;

    bool eventFilter(QObject *obj, QEvent *evt) final;

signals:
    void closeSession();
    void changeTopic(const QString& text);

public slots:
    void onRefocus();
    void showSearch();
};

/*!
 * Toolbar manager.  The Toolbar is at the top of the main window.  This is
 * used both for the toolbar application buttons such as session, contacts,
 * and to manage the global search input box.
 * \file toolbar.hpp
 */

#endif
