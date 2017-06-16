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

#include <QVariant>
#include <QCoreApplication>
#include <QList>
#include <QSqlRecord>
#include <QSqlQuery>
#include <QTimer>

class Request : public QObject
{
	Q_OBJECT
	Q_DISABLE_COPY(Request)
public:
    typedef enum {
        Success = 0,
        Timeout,
        DbFailed,
        NotFound,
        Partial,
        Invalid,
    }   ErrorResult;

    Request(QObject *parent, const QVariantHash& args, int timeout);

	inline const QVariant operator[](const QString& id) const {
		return parms[id];
	}

	inline const QVariant value(const QString& id) const {
		return parms[id];
	}

    void setValue(const QString& id, const QVariant& value) {
        parms[id] = value;
    }

    void notifySuccess(QSqlQuery &results, ErrorResult error = Success);
    void notifyFailed(ErrorResult error = DbFailed);
	
private:
	QTimer *timer;
    ErrorResult status;
    QVariantHash parms;

    bool event(QEvent *evt) final;

signals:
    void results(ErrorResult, const QVariantHash&, const QList<QSqlRecord>&);

private slots:
	void timeout();
};
	
