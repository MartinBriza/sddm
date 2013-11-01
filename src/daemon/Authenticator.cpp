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

#include "Authenticator.h"
#include "DaemonApp.h"
#include "Display.h"
#include "DisplayManager.h"
#include "SeatManager.h"
#include "Seat.h"
#include "Constants.h"

#include <QtCore/QDebug>
#include <QtCore/QFile>

#include <pwd.h>
#include <unistd.h>

namespace SDDM {
    Authenticator::Authenticator(Display *parent)
    : QProcess(parent)
    , m_display(parent) {
        connect(this, SIGNAL(finished(int)), this, SIGNAL(stopped()));
        setReadChannel(QProcess::StandardOutput);
        connect(this, SIGNAL(readyRead()), this, SLOT(readFromChild()));
        connect(this, SIGNAL(readyReadStandardError()), SLOT(forwardErrorOutput()));

        QProcess::start(QString("%1/sddm-auth").arg(BIN_INSTALL_DIR));
        if (!waitForStarted()) {
            qCritical() << " DAEMON: Failed to start authenticator process:" << errorString();
            return;
        }

        m_started = true;
        qDebug() << " DAEMON: Started the authenticator process.";
    }

    void Authenticator::forwardErrorOutput() {
        QFile err;
        err.open(fileno(stderr), QIODevice::WriteOnly | QIODevice::Unbuffered);
        err.write(this->readAllStandardError());
    }

    void Authenticator::readFromChild() {
        QDataStream stream(this);
        while (this->bytesAvailable()) {
            quint32 command;
            stream >> command;
            qDebug() << " DAEMON: Received message" << command << "from the authenticator";
            handleMessage(AuthMessages(command));
            if (stream.status() != QDataStream::Ok)
                break;
        }
    }

    void Authenticator::handleMessage(AuthMessages command) {
        QDataStream stream(this);
        switch (command) {
            case AuthMessages::RequestEnv: {
                QString user;
                stream >> user;
                QStringList env = m_display->sessionEnv(user).toStringList();
                stream << quint32(AuthMessages::Env) << env.count();
                foreach (const QString &s, env) {
                    stream << s;
                }
                break;
            }
            case AuthMessages::LoginFailed:
                emit loginFailed(m_parentSocket);
                break;
            case AuthMessages::LoginSucceeded: {
                QString user;
                stream >> m_name >> user;
                daemonApp->displayManager()->AddSession(m_name, qobject_cast<Seat*>(m_display->parent())->name(), user);
                emit loginSucceeded(m_parentSocket);
                break;
            }
            case AuthMessages::RequestSessionID:
                stream << quint32(AuthMessages::SessionID) << int(DaemonApp::instance()->newSessionId());
                break;
            case AuthMessages::RequestCookieLink: {
                QString path, user;
                struct passwd *pw;
                stream >> path >> user;
                qDebug() << "The path is" << path << "and the user" << user;
                m_display->addCookie(path);
                pw = getpwnam(qPrintable(user));
                if(pw)
                    chown(qPrintable(path), pw->pw_uid, pw->pw_gid);
                stream << quint32(AuthMessages::CookieLink);
                break;
            }
            default:
                qWarning() << " DAEMON: Child sent message type" << quint32(command) << "which cannot be handled.";
                break;
        }
    }

    void Authenticator::start(QLocalSocket *socket, const QString& user, const QString& session, const QString& password, bool passwordless) {
        if (!m_started)
            return;

        m_parentSocket = socket;

        QDataStream stream(this);
        stream << quint32(AuthMessages::Start) << user << session << password << passwordless;

        qDebug() << " DAEMON: Starting authentication for user" << user;
    }

    void Authenticator::stop() {
        if (!m_started)
            return;

        // unregister from the display manager
        daemonApp->displayManager()->RemoveSession(m_name);

        QDataStream stream(this);
        stream << quint32(AuthMessages::End);

        m_started = false;
        qDebug() << " DAEMON: Stopped the authenticator process";
    }
}
