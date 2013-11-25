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

#include "Method.h"
#include "Configuration.h"
#include "Session.h"
#include "AuthenticatorApp.h"

#include <QtCore/QDebug>
#include <QtCore/QProcess>
#include <QtCore/QDir>

#ifndef USE_PAM
#include <crypt.h>
#include <shadow.h>
#else
#include "PAM.h"
#endif

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

namespace SDDM {
    Method::Method(QObject* parent)
    : QObject(parent) {

    }

    QString Method::name() const {
        if (m_started)
            return m_process->name();
        else
            return QString();
    }

    void Method::start(const QString& user, const QString& session, const QString& password, bool passwordless) {
        if (doStart(user, session, password, passwordless))
            emit loginSucceeded(user);
        else
            emit loginFailed();
    }

    void Method::buildSessionName(const QString& session) {
        if (session.endsWith(".desktop")) {
            // session directory
            QDir dir(Configuration::instance()->sessionsDir());

            // session file
            QFile file(dir.absoluteFilePath(session));

            // open file
            if (file.open(QIODevice::ReadOnly)) {

                // read line-by-line
                QTextStream in(&file);
                while (!in.atEnd()) {
                    QString line = in.readLine();

                    // line starting with Exec
                    if (line.startsWith("Exec="))
                        m_sessionCommand = line.mid(5);
                }

                // close file
                file.close();
            }

            // remove extension
            m_sessionName = QString(session.left(session.lastIndexOf(".")));
        } else {
            m_sessionCommand = QString(session);
            m_sessionName = QString(session);
        }
    }

    bool Method::authenticate() {
#ifdef USE_PAM
        if (m_pam)
            m_pam->deleteLater();

        m_pam = new PamService(m_user, m_password, m_passwordless);

        if (!m_pam)
            return false;

        QString display = AuthenticatorApp::instance()->requestDisplay();

        // set tty
        if (!m_pam->setItem(PAM_TTY, qPrintable(display)))
            return false;

        // set display name
        if (!m_pam->setItem(PAM_XDISPLAY, qPrintable(display)))
            return false;

        // set username
        if (!m_pam->setItem(PAM_USER, qPrintable(m_user)))
            return false;

        // authenticate the applicant
        if (!m_pam->authenticate())
            return false;

        // get mapped user name; PAM may have changed it
        const char *mapped = (const char *) m_pam->getItem(PAM_USER);
        if (mapped == NULL)
            return false;
        // TODO: Find out if PAM changing the name is a good or a bad thing
        //m_user = QString(mapped);

        if (!m_pam->acctMgmt())
            return false;
#else
        if (!m_passwordless) {
            // user name
            struct passwd *pw;
            if ((pw = getpwnam(qPrintable(m_user))) == nullptr) {
                // log error
                qCritical() << " AUTH: Failed to get user entry.";

                // return fail
                return false;
            }

            struct spwd *sp;
            if ((sp = getspnam(pw->pw_name)) == nullptr) {
                // log error
                qCritical() << " AUTH: Failed to get shadow entry.";

                // return fail
                return false;
            }

            // check if password is not empty
            if (sp->sp_pwdp && sp->sp_pwdp[0]) {

                // encrypt password
                char *encrypted = crypt(qPrintable(m_password), sp->sp_pwdp);

                if (strcmp(encrypted, sp->sp_pwdp))
                    return false;
            }
        }

        char *mapped = strdup(qPrintable(m_user));
        m_user = QString(mapped);
        free(mapped);
#endif
        return true;
    }

    bool Method::setupUser() {
        // user name
        struct passwd *pw;
        if ((pw = getpwnam(qPrintable(m_user))) == nullptr) {
            // log error
            qCritical() << " AUTH: Failed to get user name.";

            // return fail
            return false;
        }

        if (pw->pw_shell[0] == '\0') {
            setusershell();
            strcpy(pw->pw_shell, getusershell());
            endusershell();
        }

        // set session m_process params
        m_process->setUser(pw->pw_name);
        m_process->setDir(pw->pw_dir);
        m_process->setUid(pw->pw_uid);
        m_process->setGid(pw->pw_gid);
        // redirect error output to ~/.xession-errors
        m_process->setStandardErrorFile(QString("%1/.xsession-errors").arg(pw->pw_dir));
        if (!AuthenticatorApp::instance()->requestCookieTo(QString("%1/.Xauthority").arg(pw->pw_dir), m_user))
            return false;
        return true;
    }

    QProcessEnvironment Method::setupEnvironment() {        // set m_process environment
        QProcessEnvironment env = AuthenticatorApp::instance()->requestEnvironment(m_user);
#ifdef USE_PAM
        env.insert(m_pam->getEnv());
        m_pam->putEnv(env);
#endif
        env.insert("DESKTOP_SESSION", m_sessionName);
        env.insert("GDMSESSION", m_sessionName);
        return env;
    }

    bool Method::startSession() {
#ifdef USE_PAM
        // set credentials
        if (!m_pam->setCred(PAM_ESTABLISH_CRED))
            return false;

        // open session
        if (!m_pam->openSession())
            return false;

        // set credentials
        if (!m_pam->setCred(PAM_REINITIALIZE_CRED))
            return false;
#endif
        // start session
        m_process->start(Configuration::instance()->sessionCommand(), { m_sessionCommand });
        return true;
    }


    bool Method::doStart(const QString &user, const QString &session, const QString &password, bool passwordless) {
        m_user = user;
        m_password = password;
        m_passwordless = passwordless;
        qDebug() << " AUTH: Started for" << user << "with password" << password << "which will not be needed:" << passwordless << "to log into session" << session << ". Also, everybody will want to kill you if you don't put this message away for production.";
        // check flag
        if (m_started)
            return false;

        buildSessionName(session);

        if (m_sessionCommand.isEmpty()) {
            // log error
            qCritical() << " AUTH: Failed to find command for session:" << session;

            // return fail
            return false;
        }

        if (!authenticate())
            return false;

        qDebug() << " AUTH: Authenticated for user" << m_user;

        // create user session m_process
        m_process = new Session(QString("Session%1").arg(AuthenticatorApp::instance()->requestSessionId()), this);
        connect(m_process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(finished()));

        if (!setupUser())
            return false;

        m_process->setProcessEnvironment(setupEnvironment());

        if (!startSession())
            return false;

        qDebug() << " AUTH: User session" << m_sessionName << "set up.";

        // wait for started
        if (!m_process->waitForStarted()) {
            // log error
            qDebug() << " AUTH: Failed to start user session.";

            // return fail
            return false;
        }

        // log message
        qDebug() << " AUTH: User session started.";

        // register to the display manager FIXME
        // daemonApp->displayManager()->AddSession(m_process->name(), seat->name(), pw->pw_name);

        // set flag
        m_started = true;

        // return success
        return true;
    }

    void Method::stop() {
        // check flag
        if (!m_started)
            return;

        // log message
        qDebug() << " AUTH: User session stopping...";

        // terminate m_process
        m_process->terminate();

        // wait for finished
        if (!m_process->waitForFinished(5000))
            m_process->kill();
    }

    void Method::finished() {
        // check flag
        if (!m_started)
            return;

        // reset flag
        m_started = false;

        // log message
        qDebug() << " AUTH: User session ended.";

        // delete session process
        m_process->deleteLater();
        m_process = nullptr;

#ifdef USE_PAM
        delete m_pam;
        m_pam = nullptr;
#endif

        emit sessionTerminated();
    }

};

#include "Method.moc"
