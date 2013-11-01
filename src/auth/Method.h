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

#ifndef METHOD_H
#define METHOD_H

#include <QtCore/QObject>
#include <QtCore/QProcess>

namespace SDDM {
#ifdef USE_PAM
    class PamService;
#endif
    class Session;
    class Method : public QObject
    {
        Q_OBJECT
    public:
        explicit Method(QObject *parent = nullptr);
        QString name() const;

    protected:
        virtual bool authenticate();
        virtual bool setupUser();
        virtual QProcessEnvironment setupEnvironment();
        virtual bool startSession();

    signals:
        void loginSucceeded(QString);
        void loginFailed();
        void sessionTerminated();

    public slots:
        void start(const QString &user, const QString &session, const QString &password, bool passwordless);
        void stop();
        void finished();

    private:
        bool doStart(const QString &user, const QString &session, const QString &password, bool passwordless);
        void buildSessionName(const QString& session);

        bool m_passwordless { false };
        QString m_user { };
        QString m_password { };
        QString m_sessionName { };
        QString m_sessionCommand { };

        Session *m_process { nullptr };
        bool m_started { false };
#ifdef USE_PAM
        PamService *m_pam { nullptr };
#endif
    };
}

#endif // METHOD_H
