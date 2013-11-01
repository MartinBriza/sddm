/*
 * PAM Authenticator backend
 *
 * Based on the work of:
 *  SDDM Authenticator implementation: Abdurrahman AVCI <abdurrahmanavci@gmail.com>
 *  SLiM PAM implementation: Martin Parm
 *
 * Copyright (C) 2013  Martin Bříza <mbriza@redhat.com>
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

#ifdef USE_PAM
#include "PAM.h"

#include <QtCore/QDebug>

namespace SDDM {

    bool PamService::putEnv(const QProcessEnvironment& env) {
        foreach (const QString& s, env.toStringList()) {
            result = pam_putenv(handle, s.toAscii());
            if (result != PAM_SUCCESS) {
                qWarning() << " AUTH: PAM: putEnv:" << pam_strerror(handle, result);
                return false;
            }
        }
        return true;
    }

    QProcessEnvironment PamService::getEnv() {
        QProcessEnvironment env;
        // get pam environment
        char **envlist = pam_getenvlist(handle);
        if (envlist == NULL) {
            qWarning() << " AUTH: PAM: getEnv: Returned NULL";
            return env;
        }

        // copy it to the env map
        for (int i = 0; envlist[i] != nullptr; ++i) {
            QString s(envlist[i]);

            // find equal sign
            int index = s.indexOf('=');

            // add to the hash
            if (index != -1)
                env.insert(s.left(index), s.mid(index + 1));

            free(envlist[i]);
        }
        free(envlist);
        return env;
    }

    bool PamService::chAuthTok(int flags) {
        result = pam_chauthtok(handle, flags | m_silent);
        if (result != PAM_SUCCESS) {
            qWarning() << " AUTH: PAM: chAuthTok:" << pam_strerror(handle, result);
        }
        return result == PAM_SUCCESS;
    }

    bool PamService::acctMgmt(int flags) {
        result = pam_acct_mgmt(handle, flags | m_silent);
        if (result == PAM_NEW_AUTHTOK_REQD) {
            // TODO see if this should really return the value or just true regardless of the outcome
            return chAuthTok(PAM_CHANGE_EXPIRED_AUTHTOK);
        }
        else if (result != PAM_SUCCESS) {
            qWarning() << " AUTH: PAM: acctMgmt:" << pam_strerror(handle, result);
            return false;
        }
        return true;
    }

    bool PamService::authenticate(int flags) {
        result = pam_authenticate(handle, flags | m_silent);
        if (result != PAM_SUCCESS) {
            qWarning() << " AUTH: PAM: authenticate:" << pam_strerror(handle, result);
        }
        return result == PAM_SUCCESS;
    }

    bool PamService::setCred(int flags) {
        result = pam_setcred(handle, flags | m_silent);
        if (result != PAM_SUCCESS) {
            qWarning() << " AUTH: PAM: setCred:" << pam_strerror(handle, result);
        }
        return result == PAM_SUCCESS;
    }

    bool PamService::openSession() {
        result = pam_open_session(handle, m_silent);
        if (result != PAM_SUCCESS) {
            qWarning() << " AUTH: PAM: openSession:" << pam_strerror(handle, result);
        }
        return result == PAM_SUCCESS;
    }

    bool PamService::closeSession() {
        result = pam_close_session(handle, m_silent);
        if (result != PAM_SUCCESS) {
            qWarning() << " AUTH: PAM: closeSession:" << pam_strerror(handle, result);
        }
        return result == PAM_SUCCESS;
    }

    bool PamService::setItem(int item_type, const void* item) {
        result = pam_set_item(handle, item_type, item);
        if (result != PAM_SUCCESS) {
            qWarning() << " AUTH: PAM: setItem:" << pam_strerror(handle, result);
        }
        return result == PAM_SUCCESS;
    }

    const void* PamService::getItem(int item_type) {
        const void *item;
        result = pam_get_item(handle, item_type, &item);
        if (result != PAM_SUCCESS) {
            qWarning() << " AUTH: PAM: getItem:" << pam_strerror(handle, result);
        }
        return item;
    }

    int PamService::converse(int n, const struct pam_message **msg, struct pam_response **resp, void *data) {
        PamService *c = static_cast<PamService *>(data);
        return c->doConverse(n, msg, resp);
    }

    int PamService::doConverse(int n, const struct pam_message **msg, struct pam_response **resp) {
        struct pam_response *aresp;

        // check size of the message buffer
        if ((n <= 0) || (n > PAM_MAX_NUM_MSG))
            return PAM_CONV_ERR;

        // create response buffer
        if ((aresp = (struct pam_response *) calloc(n, sizeof(struct pam_response))) == nullptr)
            return PAM_BUF_ERR;

        bool failed = false;

        // if we don't require password, bail on any request from PAM
        if (passwordless) {
            for (int i = 0; i < n; ++i) {
                switch(msg[i]->msg_style) {
                    case PAM_ERROR_MSG:
                    case PAM_TEXT_INFO:
                        qDebug() << " AUTH: PAM: Message" << msg[i]->msg;
                        break;
                    case PAM_PROMPT_ECHO_OFF:
                    case PAM_PROMPT_ECHO_ON:
                    default:
                        failed = true;
                        break;
                }
            }
        }
        // else, respond to the messages
        else {
            for (int i = 0; i < n; ++i) {
                aresp[i].resp_retcode = 0;
                aresp[i].resp = nullptr;
                switch (msg[i]->msg_style) {
                    case PAM_PROMPT_ECHO_OFF:
                        // set password - WARNING this is just assumption it's a password, beware!
                        aresp[i].resp = strdup(qPrintable(password));
                        if (aresp[i].resp == nullptr)
                            failed = true;
                        // clear password
                        password = "";
                        break;
                    case PAM_PROMPT_ECHO_ON:
                        // set user - WARNING again, just an assumption, in more complicated environments this won't suffice!
                        aresp[i].resp = strdup(qPrintable(user));
                        if (aresp[i].resp == nullptr)
                            failed = true;
                        // clear user
                        user = "";
                        break;
                    case PAM_ERROR_MSG:
                    case PAM_TEXT_INFO:
                        qDebug() << " AUTH: PAM: Message:" << msg[i]->msg;
                        break;
                    default:
                        failed = true;
                }
            }
        }

        if (failed) {
            for (int i = 0; i < n; ++i) {
                if (aresp[i].resp != nullptr) {
                    memset(aresp[i].resp, 0, strlen(aresp[i].resp));
                    free(aresp[i].resp);
                }
            }
            memset(aresp, 0, n * sizeof(struct pam_response));
            free(aresp);
            *resp = nullptr;
            return PAM_CONV_ERR;
        }

        *resp = aresp;
        return PAM_SUCCESS;
    }

    bool PamService::start(const char *service_name, const char *user, const struct pam_conv *pam_conversation) {
        result = pam_start(service_name, user, pam_conversation, &handle);
        if (result != PAM_SUCCESS) {
            qWarning() << " AUTH: PAM: start" << pam_strerror(handle, result);
            return false;
        }
        else {
            qDebug() << " AUTH: PAM: Starting...";
        }
        return true;
    }

    bool PamService::end(int flags) {
        result = pam_end(handle, result | flags);
        if (result != PAM_SUCCESS) {
            qWarning() << " AUTH: PAM: end:" << pam_strerror(handle, result);
            return false;
        }
        else {
            qDebug() << " AUTH: PAM: Ended.";
        }
        return true;
    }

    PamService::PamService(const QString &user, const QString &password, bool passwordless, QObject *parent)
    : QObject(parent), user(user), password(password), passwordless(passwordless) {
        // create context
        m_converse = { &PamService::converse, this };
        // start service
        if (passwordless)
            start("sddm-passwordless", nullptr, &m_converse);
        else
            start("sddm", nullptr, &m_converse);
    }

    PamService::~PamService() {
        // stop service
        end();
    }
};

#include "PAM.moc"

#endif // USE_PAM
