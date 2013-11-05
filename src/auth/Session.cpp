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

#include "Session.h"

#include "Configuration.h"

#include <QDebug>

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

namespace SDDM {
    Session::Session(const QString &name, QObject *parent) : QProcess(parent), m_name(name) {
    }

    const QString &Session::name() const {
        return m_name;
    }

    void Session::setUser(const QString &user) {
        m_user = user;
    }

    void Session::setDir(const QString &dir) {
        m_dir = dir;
    }

    void Session::setUid(int uid) {
        m_uid = uid;
    }

    void Session::setGid(int gid) {
        m_gid = gid;
    }

    void Session::setupChildProcess() {
        if (initgroups(qPrintable(m_user), m_gid)) {
            qCritical() << " AUTH: Failed to initialize user groups.";

            // emit signal
            emit finished(EXIT_FAILURE, QProcess::NormalExit);

            // exit
            exit(EXIT_FAILURE);
        }

        if (setsid() < 0) {
            qCritical() << " AUTH: Can't create a new session.";
            emit finished(EXIT_FAILURE, QProcess::NormalExit);
            exit(EXIT_FAILURE);
        }

        if (setgid(m_gid)) {
            qCritical() << " AUTH: Failed to set group id.";

            // emit signal
            emit finished(EXIT_FAILURE, QProcess::NormalExit);

            // exit
            exit(EXIT_FAILURE);
        }

        if (setuid(m_uid)) {
            qCritical() << " AUTH: Failed to set user id.";

            // emit signal
            emit finished(EXIT_FAILURE, QProcess::NormalExit);

            // exit
            exit(EXIT_FAILURE);

        }
/* FIXME
        // add cookie
        Display *display = qobject_cast<Display *>(authenticator->parent());
        display->addCookie(QString("%1/.Xauthority").arg(m_dir));
*/
        // change to user home dir
        if (chdir(qPrintable(m_dir))) {
            qCritical() << " AUTH: Failed to change dir to user home.";

            // emit signal
            emit finished(EXIT_FAILURE, QProcess::NormalExit);

            // exit
            exit(EXIT_FAILURE);
        }
    }
}
