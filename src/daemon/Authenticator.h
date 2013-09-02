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

#ifndef SDDM_AUTHENTICATOR_H
#define SDDM_AUTHENTICATOR_H

#include <QObject>

namespace SDDM {
#ifdef USE_PAM
    class PamService;
#endif
    class Session;

    class AuthenticatorPrivate;
    class Authenticator : public QObject {
        Q_OBJECT
        Q_DISABLE_COPY(Authenticator)
    public:
        Authenticator(QObject *parent = 0);
        ~Authenticator();

    public slots:
        bool start(const QString &user, const QString &session);
        bool start(const QString &user, const QString &password, const QString &session);

        void stop();
        void finished();

    signals:
        void stopped();

    private:
        bool doStart(const QString &user, const QString &password, const QString &session, bool passwordless);

        bool m_started { false };

#ifdef USE_PAM
        PamService *m_pam { nullptr };
#endif

        Session *process { nullptr };
    };
}

#endif // SDDM_AUTHENTICATOR_H
