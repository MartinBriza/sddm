/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2013  Martin Bříza <email>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * 
 */

#ifndef AUTHENTICATORAPP_H
#define AUTHENTICATORAPP_H

#include "Messages.h"

#include <QtCore/QObject>
#include <QtCore/QCoreApplication>
#include <QtCore/QProcess>
#include <QtCore/QSocketNotifier>
#include <QtCore/QFile>

namespace SDDM {

    class Method;
    class AuthenticatorApp : public QCoreApplication
    {
        Q_OBJECT
    public:
        explicit AuthenticatorApp(int argc, char **argv);
        static AuthenticatorApp *instance();

        QProcessEnvironment requestEnvironment(const QString &user);
        int requestSessionId();
        bool requestCookieTo(const QString &path, const QString &user);

    signals:
        void started(const QString &user, const QString &session, const QString &password, bool passwordless);
        void stopped();

    public slots:
        void slotLoginSucceeded(QString user);
        void slotLoginFailed();

    private slots:
        void readFromParent(int fd);
        void handleMessage(AuthMessages command);

    private:
        static AuthenticatorApp *self;

        Method *m_method { nullptr };
        QSocketNotifier *m_notifier { nullptr };
        QFile m_input { };
        QFile m_output { };
    };
}

#endif // AUTHENTICATORAPP_H
