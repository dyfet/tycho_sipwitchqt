/**
 ** Copyright 2017 Tycho Softworks.
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

#include "request.hpp"
#include "sqldriver.hpp"

#include <QObject>
#include <QString>
#include <QDebug>
#include <QSqlDatabase>

typedef QHash<QString,QSqlRecord> DbRecords;
typedef QHash<QString,DbRecords> DbResults;

class Database final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Database)

public:
    ~Database();

    static Database *instance() {
        return Instance;
    }

    static void init(unsigned order);

    static void queryExtensions();

    static DbRecords indexResults(const DbRecords& index, const QString& key);
    
    static DbRecords indexResults(const QList<QSqlRecord>& list, const QString& key);

private:
    static const int interval = 10000;

    QSqlDatabase db;
    QTimer timer;
    QString uuid;
    QString realm;
    QString driver;
    QString host;
    QString name;
    QString user;
    QString pass;
    int port;

    Database(unsigned order);

    bool event(QEvent *evt) final;

    DbRecords snapshotExtensions();    

    bool runQuery(const QString& string, QVariantList parms = QVariantList());
    int runQuery(const QStringList& list);
    QSqlRecord getRecord(const QString& request, QVariantList parms = QVariantList());
    bool reopen();
    bool create();
    void close();

    static Database *Instance;

signals:
    void snapshotResults(const DbResults& results);

private slots:
    void applyConfig(const QVariantHash& config);
    void onTimeout();
};

Q_DECLARE_METATYPE(DbRecords);
Q_DECLARE_METATYPE(DbResults);
