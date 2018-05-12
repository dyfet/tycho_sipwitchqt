/**
 ** Copyright (C) 2017 Tycho Softworks.
 **
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 3 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "desktop.hpp"
#include "ui_toolbar.h"

#include <QToolBar>

static Ui::Toolbar ui;

Toolbar *Toolbar::Instance = nullptr;
QLineEdit *Toolbar::Search = nullptr;
QPushButton *Toolbar::Close = nullptr;

Toolbar::Toolbar(QWidget *parent, QToolBar *bar, QMenuBar *mp) :
QWidget(parent)
{
    Q_ASSERT(Instance == nullptr);
    Instance = this;

    menu = mp;
    moving = false;

    bar->addWidget(this);
    bar->installEventFilter(this);

    ui.setupUi(this);
    ui.searchText->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui.searchButton->setVisible(false);
    Search = ui.searchText;
    Close = ui.closeButton;

    connect(ui.searchButton, &QPushButton::pressed, this, &Toolbar::showSearch);
}

void Toolbar::setTitle(const QString& text)
{
    ui.title->setText(text);
    setStyle("color: black;");
}

void Toolbar::setStyle(const QString& style)
{
    ui.title->setStyleSheet(style);
}

void Toolbar::clearSearch()
{
    ui.searchText->setText("");
    ui.searchText->setFocus();
}

void Toolbar::enableSearch()
{
    ui.searchText->setEnabled(true);
    clearSearch();
}

void Toolbar::disableSearch()
{
    ui.searchText->clear();
    ui.searchText->setEnabled(false);
}

void Toolbar::showSearch()
{
    ui.searchFrame->setVisible(true);
    ui.searchButton->setEnabled(false);
    ui.searchButton->setVisible(false);
}

void Toolbar::hideSearch()
{
    ui.searchFrame->setVisible(false);
    ui.searchButton->setVisible(true);
    ui.searchButton->setEnabled(true);
}

void Toolbar::noSearch()
{
    ui.searchFrame->setVisible(false);
    ui.searchButton->setVisible(false);
}

void Toolbar::noSession()
{
    ui.closeButton->setVisible(false);
}

void Toolbar::showSession()
{
    ui.closeButton->setVisible(true);
}

QString Toolbar::searching()
{
    return ui.searchText->text();
}

void Toolbar::onRefocus()
{
    if(ui.searchText->isEnabled())
        ui.searchText->setFocus();
}

bool Toolbar::eventFilter(QObject *object, QEvent *event)
{
    auto mouse = static_cast<QMouseEvent *>(event);
    auto window = static_cast<QWidget *>(Desktop::instance());
    QPoint pos;

    switch(event->type()) {
    case QEvent::MouseMove:
        if(!moving)
            break;

        pos = window->pos();
        pos.rx() += mouse->globalPos().x() - mpos.x();
        pos.ry() += mouse->globalPos().y() - mpos.y();
        window->move(pos);
        mpos = mouse->globalPos();
        event->accept();
        return true;

    case QEvent::MouseButtonPress:
        moving = false;
#if defined(Q_OS_WIN)
        if(menu && menu->isVisible())
            break;
#endif
#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
        if(mouse->button() != Qt::LeftButton)
            break;

        moving = true;
        mpos = mouse->globalPos();
        event->accept();
        return true;
#else
        break;      // use native window manager on linux, bsd, etc...
#endif
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseTrackingChange:
        if(!moving)
            break;

        moving = false;
        event->accept();
        return true;
    default:
        break;
    }
    return QObject::eventFilter(object, event);
}

