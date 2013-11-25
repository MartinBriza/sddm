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

#include "Messages.h"
#include "SafeDataStream.h"

#include <QObject>
#include <QtCore/QProcess>

class QLocalSocket;
namespace SDDM {

    class Display;
    class Authenticator : public QProcess {
        Q_OBJECT
        Q_DISABLE_COPY(Authenticator)
    public:
        explicit Authenticator(Display *parent = nullptr);

    private slots:
        void readFromChild();
        void handleMessage(AuthMessages command, SafeDataStream &stream);
        void forwardErrorOutput();

    public slots:
        void start(QLocalSocket *socket, const QString& user, const QString& session, const QString& password, bool passwordless);
        void stop();

    signals:
        void loginFailed(QLocalSocket*);
        void loginSucceeded(QLocalSocket*);
        void stopped();

    private:
        QString m_name;
        Display *m_display { nullptr };
        QLocalSocket *m_parentSocket { nullptr }; // to be got rid of soon
        bool m_started { false };
    };
}

#endif // SDDM_AUTHENTICATOR_H
