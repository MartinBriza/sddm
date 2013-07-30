/***************************************************************************
* Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the
* Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
***************************************************************************/

#include "SeatManager.h"

#include "Seat.h"
#include <QDBusConnection>
#include <QDBusInterface>

#define LOGIN1_SERVICE                 QLatin1String("org.freedesktop.login1")
#define LOGIN1_MANAGER_PATH            QLatin1String("/org/freedesktop/login1")
#define LOGIN1_PATH_PREFIX_LEN         (sizeof("/org/freedesktop/login1"))
#define LOGIN1_MANAGER_INTERFACE       QLatin1String("org.freedesktop.login1.Manager")
#define LOGIN1_SEAT_GRAPHICAL_PROPERTY QLatin1String("CanGraphical")
#define DBUS_PROPERTY_INTERFACE        QLatin1String("org.freedesktop.DBus.Properties")
#define DBUS_PROPERTIES_CHANGED_SIGNAL QLatin1String("PropertiesChanged")

class Login1Seat;

class Login1Manager : public QDBusInterface {
    Q_OBJECT
public:
    explicit Login1Manager(QObject *parent = 0);
    virtual ~Login1Manager() {}
signals:
    void seatNew(QString name);
    void seatRemoved(QString name);
private slots:
    void dispatchChange(QString name, QDBusObjectPath path);
private:
    QMap<QDBusObjectPath, Login1Seat *> m_seats;
};

class Login1Seat : public QDBusInterface {
    Q_OBJECT
public:
    explicit Login1Seat(QDBusObjectPath path, QObject *parent = 0);
    virtual ~Login1Seat() {}
signals:
    void becameGraphical(QString name);
    void noMoreGraphical(QString name);
public slots:
    bool checkGraphical();
private slots:
    void propertiesChanged(QString interface, QMap<QString, QVariant> changedProperties, QList<QString> invalidatedProperties);
private:
    bool compareStates(bool currentState);
    QString getName();
    bool previousState { false };
};

Login1Manager::Login1Manager(QObject *parent)
: QDBusInterface(LOGIN1_SERVICE, LOGIN1_MANAGER_PATH, LOGIN1_MANAGER_INTERFACE, QDBusConnection::systemBus(), this)
{
    connect(this, SIGNAL(SeatNew(QString, QObjectPath)), SLOT(dispatchChange(QString, QDBusObjectPath)));
    connect(this, SIGNAL(SeatRemoved(QString, QObjectPath)), SLOT(dispatchChange(QString, QDBusObjectPath)));
}

void Login1Manager::dispatchChange(QString name, QDBusObjectPath path)
{
    if (m_seats.contains(path)) {
        if (!m_seats[path]->isValid()) {
            delete m_seats[path];
            m_seats.remove(path);
        }
        else {
            m_seats[path]->checkGraphical();
        }
    }
    else {
        m_seats[path] = new Login1Seat(path);
        connect(m_seats[path], SIGNAL(becameGraphical(QString)), this, SIGNAL(seatNew(QString)));
        connect(m_seats[path], SIGNAL(noMoreGraphical(QString)), this, SIGNAL(seatRemoved(QString)));
        m_seats[path]->checkGraphical();
    }
}

Login1Seat::Login1Seat(QDBusObjectPath path, QObject* parent)
: QDBusInterface(LOGIN1_SERVICE, path.path(), LOGIN1_MANAGER_INTERFACE, QDBusConnection::systemBus(), this)
{
    QDBusConnection::systemBus().connect(LOGIN1_SERVICE, path.path(), DBUS_PROPERTY_INTERFACE, DBUS_PROPERTIES_CHANGED_SIGNAL,
                             this, SLOT(propertiesChanged(QString, QMap<QString, QVariant>, QList<QString>)));
}

bool Login1Seat::compareStates(bool currentState)
{
    if (currentState != previousState) {
        if (currentState)
            emit becameGraphical(getName());
        else
            emit noMoreGraphical(getName());
        previousState = currentState;
    }
    return currentState;
}

QString Login1Seat::getName()
{
    return path().remove(0, LOGIN1_PATH_PREFIX_LEN);
}

bool Login1Seat::checkGraphical()
{
    bool currentState = (isValid() && property(LOGIN1_SEAT_GRAPHICAL_PROPERTY.latin1()).toBool());
    return compareStates(currentState);
}

void Login1Seat::propertiesChanged(QString interface, QMap< QString, QVariant > changedProperties, QList< QString > invalidatedProperties)
{
    bool currentState = changedProperties.contains(LOGIN1_SEAT_GRAPHICAL_PROPERTY) && changedProperties[LOGIN1_SEAT_GRAPHICAL_PROPERTY].toBool();
    compareStates(currentState);
}

namespace SDDM {
    SeatManager::SeatManager(QObject *parent) : QObject(parent) {
    }

    void SeatManager::createSeat(const QString &name) {
        // create a seat
        Seat *seat = new Seat(name, this);

        // add to the list
        m_seats.insert(name, seat);

        // emit signal
        emit seatCreated(name);
    }

    void SeatManager::removeSeat(const QString &name) {
        // check if seat exists
        if (!m_seats.contains(name))
            return;

        // remove from the list
        Seat *seat = m_seats.take(name);

        // delete seat
        seat->deleteLater();

        // emit signal
        emit seatRemoved(name);
    }

    void SeatManager::switchToGreeter(const QString &name) {
        // check if seat exists
        if (!m_seats.contains(name))
            return;

        // switch to greeter
        m_seats[name]->createDisplay();
    }
}

#include "SeatManager.moc"