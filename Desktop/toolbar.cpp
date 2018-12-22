/*
 * Copyright (C) 2017-2018 Tycho Softworks.
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

#include "desktop.hpp"
#include "ui_toolbar.h"

#include <QToolBar>
#include <QValidator>

namespace {

class ValidateTopic final : public QValidator
{
    Q_DISABLE_COPY(ValidateTopic)

public:
    explicit ValidateTopic(QObject *parent) : QValidator(parent) {}

    State validate(QString& input, int& pos) const final {
        if(input.length() < 1)
            return Intermediate;

        if(pos < 0)
            pos = 0;

        if(pos > input.length() + 1)
            return Invalid;

        QChar first = input[0], last = input[pos];
        auto desktop = Desktop::instance();
        if(pos > 0)
            last = input[pos - 1];

        if(!first.isLetterOrNumber()) {
            desktop->warningMessage(tr("Invalid start to topic"), 2000);
            return Invalid;
        }

        if(last.isLetterOrNumber()) {
            desktop->clearMessage();
            return Acceptable;
        }

        if(last.isSpace()) {
            desktop->clearMessage();
            return Acceptable;
        }

        desktop->warningMessage(tr("Invalid text in topic"), 2000);
        return Invalid;
    }
};

Ui::Toolbar ui;
};

Toolbar *Toolbar::Instance = nullptr;
QLineEdit *Toolbar::Search = nullptr;

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

    auto topic_validator = new ValidateTopic(this);
    ui.topicList->setValidator(topic_validator);

    connect(ui.topicList, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::currentIndexChanged), this, [this](const QString& text) {
        if(ui.topicList->isEnabled() && ui.topicList->isVisible() && !text.isEmpty()) {
            emit changeTopic(text);
            ui.topicList->setEnabled(false);
        }
    });

    connect(ui.searchButton, &QPushButton::pressed, this, [this] {
        QTimer::singleShot(CONST_CLICKTIME, this, [this] {
            showSearch();
        });
    });

    connect(ui.topicButton, &QPushButton::pressed, this, [this] {
        QTimer::singleShot(CONST_CLICKTIME, this, [this] {
            hideSearch();
        });
    });

    connect(ui.closeButton, &QPushButton::pressed, this, [this] {
        QTimer::singleShot(CONST_CLICKTIME, this, [this] {
            emit closeSession();
        });
    });
}

void Toolbar::setOperators()
{
    ui.topicList->setEnabled(false);
    ui.topicList->clear();
    ui.topicList->setVisible(false);
    showSession();
    hideSearch();
}

void Toolbar::setTopics(const QString& topic, const QStringList& topics)
{
    ui.topicList->setEnabled(false);
    ui.topicList->clear();
    ui.topicList->addItems(topics);
    int pos = topics.indexOf(topic);
    if(pos > -1)
        ui.topicList->setCurrentIndex(pos);
    else
        ui.topicList->setCurrentText(topic);
    currentTopic = topic;

    ui.topicList->setVisible(true);
    ui.topicList->setEnabled(true);
    showSession();
    hideSearch();
}

void Toolbar::disableTopics()
{
    if(ui.topicList->isVisible())
        ui.topicList->setEnabled(false);
}

void Toolbar::enableTopics()
{
    if(ui.topicList->isVisible())
        ui.topicList->setEnabled(true);
}

void Toolbar::newTopics(const QString& topic, const QStringList& topics)
{
    if(topic.isEmpty())
        return;

    if(topic == currentTopic)
        return;

    setTopics(topic, topics);
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
    ui.topicButton->setVisible(true);
    ui.topicList->setVisible(false);
}

void Toolbar::hideSearch()
{
    ui.searchFrame->setVisible(false);
    ui.searchButton->setVisible(true);
    ui.searchButton->setEnabled(true);
    ui.topicButton->setVisible(false);
    if(ui.topicList->count() > 0)
        ui.topicList->setVisible(true);
}

void Toolbar::noSearch()
{
    ui.searchFrame->setVisible(false);
    ui.searchButton->setVisible(false);
}

void Toolbar::noSession()
{
    ui.sessionFrame->setVisible(false);
}

void Toolbar::showSession()
{
    ui.sessionFrame->setVisible(true);
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
    auto mouse = dynamic_cast<QMouseEvent *>(event);
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

