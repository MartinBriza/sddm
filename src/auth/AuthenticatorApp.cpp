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

#include "AuthenticatorApp.h"
#include "Configuration.h"
#include "Constants.h"
#include "Method.h"
#include "PAM.h"
#include "Session.h"

#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/qtextstream.h>
#include <QtCore/QSocketNotifier>

#include <cstdio>
#include <iostream>

namespace SDDM {
    AuthenticatorApp *AuthenticatorApp::self = nullptr;

    AuthenticatorApp::AuthenticatorApp(int argc, char **argv)
    : QCoreApplication(argc, argv)
    , m_method(new Method(this))
    , m_notifier(new QSocketNotifier(fileno(stdin), QSocketNotifier::Read, this)) {
        if (!self)
            self = this;
        new Configuration(CONFIG_FILE, this);
        connect(this, SIGNAL(started(QString,QString,QString,bool)), m_method, SLOT(start(QString,QString,QString,bool)));
        connect(this, SIGNAL(stopped()), m_method, SLOT(stop()));
        connect(m_method, SIGNAL(loginFailed()), this, SLOT(slotLoginFailed()));
        connect(m_method, SIGNAL(loginSucceeded(QString)), this, SLOT(slotLoginSucceeded(QString)));
        connect(m_method, SIGNAL(sessionTerminated()), this, SLOT(quit()));
        connect(m_notifier, SIGNAL(activated(int)), this, SLOT(readFromParent(int)));
        m_notifier->setEnabled(true);

        m_input.open(fileno(stdin), QIODevice::ReadOnly | QIODevice::Unbuffered);
        m_output.open(fileno(stdout), QIODevice::WriteOnly | QIODevice::Unbuffered);

        qCritical() << " AUTH: Started";
    }

    AuthenticatorApp* AuthenticatorApp::instance() {
        return self;
    }

    void AuthenticatorApp::readFromParent(int fd) {
        qDebug() << " AUTH: Message received" << m_input.bytesAvailable();
        SafeDataStream inStream(&m_input);
        quint32 command = quint32(AuthMessages::AuthNone);
        inStream.receive();
        inStream >> command;
        qDebug() << "Command" << command;
        handleMessage(AuthMessages(command), inStream);
    }

    void AuthenticatorApp::handleMessage(AuthMessages command, SafeDataStream& inStream) {
        switch (command) {
            case AuthMessages::Start: {
                QString user, session, password;
                bool passwordless;
                inStream >> user >> session >> password >> passwordless;
                emit started(user, session, password, passwordless);
                break;
            }
            case AuthMessages::End:
                emit stopped();
                break;
            default:
                break;
        }
    }

    QProcessEnvironment AuthenticatorApp::requestEnvironment(const QString &user) {
        qDebug() << " AUTH: requestEnvironment start";
        SafeDataStream inStream(&m_input);
        SafeDataStream outStream(&m_output);
        quint32 command = quint32(AuthMessages::AuthNone);
        int count;
        QStringList envList;
        QProcessEnvironment env;

        qDebug() << "Requesting environment for user" << user;
        outStream << quint32(AuthMessages::RequestEnv) << user;
        outStream.send();

        inStream.receive();
        inStream >> command >> envList;
        if (command != quint32(AuthMessages::Env)) {
            qDebug() << " AUTH: Received out of order message" << command << "when waiting for Env";
            handleMessage(AuthMessages(command), inStream);
            return env;
        }
        qDebug() << " AUTH: Received environment:" << envList;

        foreach (const QString &s, envList)
            env.insert(s.left(s.indexOf("=")), s.mid(s.indexOf("=") + 1));

        return env;
    }

    int AuthenticatorApp::requestSessionId() {
        qDebug() << " AUTH: requestSessionId start";
        SafeDataStream inStream(&m_input);
        SafeDataStream outStream(&m_output);
        quint32 command = quint32(AuthMessages::AuthNone);
        int id;

        outStream << quint32(AuthMessages::RequestSessionID);
        outStream.send();

        inStream.receive();
        inStream >> command;
        if (command != quint32(AuthMessages::SessionID)) {
            qDebug() << " AUTH: Received out of order message" << command << "when waiting for SessionID";
            handleMessage(AuthMessages(command), inStream);
            return -1;
        }
        inStream >> id;

        qDebug() << " AUTH: requestSessionId end";
        return id;
    }

    bool AuthenticatorApp::requestCookieTo(const QString& path, const QString &user) {
        qDebug() << " AUTH: requestCookieTo start";
        SafeDataStream inStream(&m_input);
        SafeDataStream outStream(&m_output);
        quint32 command = quint32(AuthMessages::AuthNone);
        qDebug() << " AUTH: Requesting Cookie to path" << path << "for user" << user;

        outStream << quint32(AuthMessages::RequestCookieLink) << path << user;
        outStream.send();

        inStream.receive();
        inStream >> command;
        if (command != quint32(AuthMessages::CookieLink)) {
            qDebug() << " AUTH: Received out of order message" << command << "when waiting for CookieLink";
            handleMessage(AuthMessages(command), inStream);
            return false;
        }

        qDebug() << " AUTH: requestCookieTo end";
        return true;
    }

    QString AuthenticatorApp::requestDisplay() {
        qDebug() << " AUTH: requestDisplay start";
        SafeDataStream inStream(&m_input);
        SafeDataStream outStream(&m_output);
        quint32 command = quint32(AuthMessages::AuthNone);
        qDebug() << " AUTH: Requesting Display";
        QString display;

        outStream << quint32(AuthMessages::RequestDisplay);
        outStream.send();

        inStream.receive();
        inStream >> command;
        if (command != quint32(AuthMessages::Display)) {
            qDebug() << " AUTH: Received out of order message" << command << "when waiting for Display";
            handleMessage(AuthMessages(command), inStream);
            return display;
        }
        inStream >> display;

        qDebug() << " AUTH: requestDisplay end";
        return display;
    }

    void AuthenticatorApp::slotLoginFailed() {
        SafeDataStream outStream(&m_output);
        outStream << quint32(AuthMessages::LoginFailed);
        outStream.send();
    }

    void AuthenticatorApp::slotLoginSucceeded(QString user) {
        SafeDataStream outStream(&m_output);
        outStream << quint32(AuthMessages::LoginSucceeded) << m_method->name() << user;
        outStream.send();
    }
}

int main(int argc, char **argv) {
    SDDM::AuthenticatorApp app(argc, argv);
    return app.exec();
}

#include "AuthenticatorApp.moc"
