/*
 * PAM Authenticator backend
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

#ifndef PAM_H
#define PAM_H
#ifdef USE_PAM

#include <QtCore/QObject>
#include <QtCore/QProcess>

#include <security/pam_appl.h>

namespace SDDM {
    /**
     * Class wrapping the standard Linux-PAM library calls
     *
     * Almost everything is left the same except the following things:
     *
     * Mainly, state returns - if you call pam_start, pam_open_session and then
     * pam_start again, the session will get closed and the conversation closed
     *
     * You don't need to pass PAM_SILENT to every call if you want PAM to be quiet.
     * You can set the flag globally by using the \ref setSilence method.
     *
     * \ref acctMgmt doesn't require you to handle the PAM_NEW_AUTHTOK_REQD condition,
     * it calls chAuthTok on its own.
     *
     * Error messages are automatically reported to qDebug
     */
    class PamService : public QObject {
        Q_OBJECT

    public:
        /**
         * ctor
         *
         * \param service PAM service name, e.g. "sddm"
         * \param user username
         * \param password user's password
         * \param passwordless true if no password is required
         * \param parent parent QObject
         */
        PamService(const QString &user, const QString &password, bool passwordless, QObject *parent = 0);

        ~PamService();

        /**
         * Conversation function for the pam_conv structure
         *
         * Calls ((PamService*)pam_conv.appdata_ptr)->doConverse() with its parameters
         */
        static int converse(int n, const struct pam_message **msg, struct pam_response **resp, void *data);

        /**
         * The main conversation method
         *
         * This method relates to this particular session / login attempt only, it's called on the object
         * that it belongs to.
         *
         * So far its architecture is quite silly and relies both on previously entered username and password.
         * In the future, I'm intending to use it to request information from the greeter by calling its methods to
         * supply the required information. (And this won't be just passwords, we'd like to have the possibility to
         * log in using just a USB key, fingerprint reader, whatever you want.)
         *
         * I hope this task will be easier to implement by moving the whole thing into a class.
         */
        int doConverse(int n, const struct pam_message **msg, struct pam_response **resp);

        /**
         * pam_set_item - set and update PAM informations
         *
         * \param item_type PAM item type
         * \param item item pointer
         *
         * \return true on success
         */
        bool setItem(int item_type, const void *item);

        /**
         * pam_get_item - getting PAM informations
         *
         * \param item_type
         *
         * \return item pointer or NULL on failure
         */
        const void *getItem(int item_type);

        /**
         * pam_open_session - start PAM session management
         *
         * \return true on success
         */
        bool openSession();

        /**
         * pam_close_session - terminate PAM session management
         *
         * \return true on success
         */
        bool closeSession();

        /**
         * pam_setcred - establish / delete user credentials
         *
         * \param flags PAM flag(s)
         *
         * \return true on success
         */
        bool setCred(int flags = 0);

        /**
         * pam_authenticate - account authentication
         *
         * \param flags PAM flag(s)
         *
         * \return true on success
         */
        bool authenticate(int flags = 0);

        /**
         * pam_acct_mgmt - PAM account validation management
         *
         * @note Automatically calls setCred if the password is expired
         *
         * \param flags PAM flag(s)
         *
         * \return true on success
         */
        bool acctMgmt(int flags = 0);

        /**
         * pam_chauthtok - updating authentication tokens
         *
         * \param flags PAM flag(s)
         *
         * \return true on success
         */
        bool chAuthTok(int flags = 0);

        /**
         * pam_getenv - get PAM environment
         *
         * \return Complete process environment
         */
        QProcessEnvironment getEnv();

        /**
         * pam_putenv - set or change PAM environment
         *
         * \param env environment to be merged into the PAM one
         *
         * \return true on success
         */
        bool putEnv(const QProcessEnvironment& env);

        /**
         * pam_end - termination of PAM transaction
         *
         * \param flags to be OR'd with the status (PAM_DATA_SILENT)
         * \return true on success
         */
        bool end(int flags = 0);

        /**
         * pam_start - initialization of PAM transaction
         *
         * \param service PAM service name, e.g. "sddm"
         * \param user username
         * \param pam_conversation pointer to the PAM conversation structure to be used
         *
         * \return true on success
         */
        bool start(const char *service_name, const char *user, const struct pam_conv *pam_conversation);

        /**
         * Set PAM_SILENT upon the contained calls
         * \param silent true if silent
         */
        inline void setSilence(bool silent){
            if (silent)
                m_silent |= PAM_SILENT;
            else
                m_silent &= (~PAM_SILENT);
        }

    private:
        int m_silent { 0 }; ///< flag mask for silence of the contained calls

        struct pam_conv m_converse; ///< the current conversation
        pam_handle_t *handle { nullptr }; ///< PAM handle
        int result { PAM_SUCCESS }; ///< PAM result

        QString user { "" }; ///< user
        QString password { "" }; ///< password
        bool passwordless { false }; ///< true if doesn't require password
    };
};

#endif // USE_PAM
#endif // PAM_H
