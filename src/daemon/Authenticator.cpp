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

#include "Configuration.h"
#include "DaemonApp.h"
#include "Display.h"
#include "DisplayManager.h"
#include "PAM.h"
#include "Seat.h"
#include "Session.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTextStream>

#ifndef USE_PAM
#include <crypt.h>
#include <shadow.h>
#endif

#include <grp.h>
#include <pwd.h>
#include <unistd.h>

namespace SDDM {

    Authenticator::Authenticator(QObject *parent) : QObject(parent) {
    }

    Authenticator::~Authenticator() {
        stop();
    }

    bool Authenticator::start(const QString &user, const QString &session) {
        return doStart(user, QString(), session, true);
    }

    bool Authenticator::start(const QString &user, const QString &password, const QString &session) {
        return doStart(user, password, session, false);
    }

    bool Authenticator::doStart(const QString &user, const QString &password, const QString &session, bool passwordless) {
        // check flag
        if (m_started)
            return false;

        // convert session to command
        QString sessionName = "";
        QString command = "";

        if (session.endsWith(".desktop")) {
            // session directory
            QDir dir(daemonApp->configuration()->sessionsDir());

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
                        command = line.mid(5);
                }

                // close file
                file.close();
            }

            // remove extension
            sessionName = session.left(session.lastIndexOf("."));
        } else {
            command = session;
            sessionName = session;
        }

        if (command.isEmpty()) {
            // log error
            qCritical() << " DAEMON: Failed to find command for session:" << session;

            // return fail
            return false;
        }

        // get display and display
        Display *display = qobject_cast<Display *>(parent());
        Seat *seat = qobject_cast<Seat *>(display->parent());

#ifdef USE_PAM
        if (m_pam)
            m_pam->deleteLater();

        m_pam = new PamService("sddm", user, password, passwordless);

        connect(display, SIGNAL(finished()), m_pam, SLOT(deleteLater()));

        if (!m_pam)
            return false;

        // set tty
        if (!m_pam->setItem(PAM_TTY, qPrintable(display->name())))
            return false;

        // set display name
        if (!m_pam->setItem(PAM_XDISPLAY, qPrintable(display->name())))
            return false;

        // set username
        if (!m_pam->setItem(PAM_USER, qPrintable(user)))
            return false;

        if (!passwordless) {
            // authenticate the applicant
            if (!m_pam->authenticate())
                return false;

            if (!m_pam->acctMgmt())
                return false;
        }

        // set credentials
        if (!m_pam->setCred(PAM_ESTABLISH_CRED))
            return false;

        // open session
        if (!m_pam->openSession())
            return false;

        // get mapped user name; PAM may have changed it
        const char *mapped = (const char *) m_pam->getItem(PAM_USER);
        if (mapped == NULL)
            return false;
#else
        if (!passwordless) {
            // user name
            struct passwd *pw;
            if ((pw = getpwnam(qPrintable(user))) == nullptr) {
                // log error
                qCritical() << " DAEMON: Failed to get user entry.";

                // return fail
                return false;
            }

            struct spwd *sp;
            if ((sp = getspnam(pw->pw_name)) == nullptr) {
                // log error
                qCritical() << " DAEMON: Failed to get shadow entry.";

                // return fail
                return false;
            }

            // check if password is not empty
            if (sp->sp_pwdp && sp->sp_pwdp[0]) {

                // encrypt password
                char *encrypted = crypt(qPrintable(password), sp->sp_pwdp);

                if (strcmp(encrypted, sp->sp_pwdp))
                    return false;
            }
        }

        char *mapped = strdup(qPrintable(user));
#endif

        // user name
        struct passwd *pw;
        if ((pw = getpwnam(mapped)) == nullptr) {
            // log error
            qCritical() << " DAEMON: Failed to get user name.";

            // return fail
            return false;
        }

        if (pw->pw_shell[0] == '\0') {
            setusershell();
            strcpy(pw->pw_shell, getusershell());
            endusershell();
        }

        // create user session process
        process = new Session(QString("Session%1").arg(daemonApp->newSessionId()), this);

        // set session process params
        process->setUser(pw->pw_name);
        process->setDir(pw->pw_dir);
        process->setUid(pw->pw_uid);
        process->setGid(pw->pw_gid);

        // set process environment
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
#ifdef USE_PAM
        env.insert(m_pam->getEnv());
#else
        // we strdup'd the string before in this branch
        free(mapped);
#endif
        env.insert("HOME", pw->pw_dir);
        env.insert("PWD", pw->pw_dir);
        env.insert("SHELL", pw->pw_shell);
        env.insert("USER", pw->pw_name);
        env.insert("LOGNAME", pw->pw_name);
        env.insert("PATH", daemonApp->configuration()->defaultPath());
        env.insert("DISPLAY", display->name());
        env.insert("XAUTHORITY", QString("%1/.Xauthority").arg(pw->pw_dir));
        env.insert("XDG_SEAT", seat->name());
        env.insert("XDG_SEAT_PATH", daemonApp->displayManager()->seatPath(seat->name()));
        env.insert("XDG_SESSION_PATH", daemonApp->displayManager()->sessionPath(process->name()));
        env.insert("XDG_VTNR", QString::number(display->terminalId()));
        env.insert("DESKTOP_SESSION", sessionName);
        env.insert("GDMSESSION", sessionName);
#ifdef USE_PAM
        m_pam->putEnv(env);
#endif
        process->setProcessEnvironment(env);

        // redirect error output to ~/.xession-errors
        process->setStandardErrorFile(QString("%1/.xsession-errors").arg(pw->pw_dir));

        // start session
        process->start(daemonApp->configuration()->sessionCommand(), { command });

        // connect signal
        connect(process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(finished()));

        // wait for started
        if (!process->waitForStarted()) {
            // log error
            qDebug() << " DAEMON: Failed to start user session.";

            // return fail
            return false;
        }

        // log message
        qDebug() << " DAEMON: User session started.";

        // register to the display manager
        daemonApp->displayManager()->AddSession(process->name(), seat->name(), pw->pw_name);

        // set flag
        m_started = true;

        // return success
        return true;
    }

    void Authenticator::stop() {
        // check flag
        if (!m_started)
            return;

        // log message
        qDebug() << " DAEMON: User session stopping...";

        // terminate process
        process->terminate();

        // wait for finished
        if (!process->waitForFinished(5000))
            process->kill();
    }

    void Authenticator::finished() {
        // check flag
        if (!m_started)
            return;

        // reset flag
        m_started = false;

        // log message
        qDebug() << " DAEMON: User session ended.";

        // unregister from the display manager
        daemonApp->displayManager()->RemoveSession(process->name());

        // delete session process
        process->deleteLater();
        process = nullptr;

#ifdef USE_PAM
        delete m_pam;
        m_pam = nullptr;
#endif

        // emit signal
        emit stopped();
    }
}
