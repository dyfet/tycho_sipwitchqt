/*
 * Copyright 2017 Tycho Softworks.
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

/*!
 * Local IPC control support of a service daemon.
 * \file control.hpp
 */

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSettings>
#include <QLocalServer>
#include <QVariant>
#include <QHash>
#include <QCommandLineParser>

/*!
 * \brief Server IPC control class.
 * This offers the skeletal management of the fifo and is meant to be
 * derived into an application specific control class, in particular to
 * execute command operations.  A few specific commands, like reload, are
 * built-in to the base class execute handler.  The fifo input is converted
 * to a QStringList for execute, and is managed thru QLocalSocket.
 * \author David Sugar <tychosoft@gmail.com>
 */
class Control : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Control)

public:
    virtual void options(QCommandLineParser& args);

    inline static Control *instance() {
	return Instance;
    }

    static void reset(const char *settingsPath);

    static int command(const QString& value, bool verbose = false, int timeout = 1000);

    static int request(const QStringList& args, bool verbose = true, int timeout = 1000);

protected:
    QSettings settings;

    Control(const char *path);
    Control();
    ~Control();

    virtual const QString execute(const QStringList& args);

    bool setValue(const QString& key, const QVariant& value);
    bool setServer(const QString& id, const QVariant& value, bool override = true);

private:
    QLocalServer *localServer;

    static Control *Instance;

    void init();

signals:
    void changeValue(const QString& id, const QVariant& value);
    void publishKeys(const QVariantHash& list);

public slots:
    void storeValue(const QString& id, const QVariant& value);
    void requestKeys(const QString& key);
    void updateKeys(const QString& key, const QVariantHash& list);

protected slots:
    virtual void onStartup() = 0;
    virtual void onShutdown() = 0;

private slots:
    void acceptConnection();
    void processConfig();
};
