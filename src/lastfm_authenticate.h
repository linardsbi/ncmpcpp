/***************************************************************************
 *   Copyright (C) 2021 by Linards Biezbardis                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.              *
 ***************************************************************************/

#ifndef NCMPCPP_LASTFM_AUTHENTICATE_H
#define NCMPCPP_LASTFM_AUTHENTICATE_H

#include <utility>

#include "lastfm_service.h"
#include "song.h"
#include "curl_handle.h"

namespace LastFm {
    class AuthService : public Service {
//#if !defined(__unix__) && !defined(_WIN32) && !defined(__APPLE__)
//        static_assert(false, "Please disable LastFm scrobbling, your system is unsupported.");
//#endif
    public:
        AuthService()
                : Service({{"api_key", m_api_key.data()}}) {}

        explicit AuthService(Arguments &&args)
                : Service(std::move(args)) {}

        Result fetch() override;

        [[nodiscard]] std::string_view getSessionToken() const {
            return m_session_token;
        }

        void setSessionToken(std::string_view session_token) {
            m_session_token.assign(session_token);
        }

        constexpr static std::string_view m_api_key = "0ac035787b80bb27fc056e4ece261aff";
    protected:
        std::string makeApiSignature();

        std::string formatURLParams();

        bool argumentsOk() override { return true; }

        [[nodiscard]] virtual constexpr Curl::RequestType request_type() const { return Curl::RequestType::GET; };

        constexpr static std::string_view m_shared_secret = "8ab955dd9325e6cc9c26cfb4c339c049";
        constexpr static std::string_view m_api_url = "https://ws.audioscrobbler.com/2.0/";

        std::string m_session_token;
    private:
        void beautifyOutput(NC::Scrollpad &w) override {}

    };

    class GetSession final : public AuthService {
    public:
        GetSession() = default;

        explicit GetSession(Arguments &&args)
                : AuthService(std::move(args)) {
            m_arguments["api_sig"] = makeApiSignature();
        }

        void addRequestToken(std::string_view token) {
            m_arguments["token"] = token;
            m_arguments["api_sig"] = makeApiSignature();
        }

        const char *name() override { return "Get a session token"; }

        Result fetchLocal();

    protected:
        bool storeSessionToken();

        const char *methodName() override { return "auth.getSession"; }

        Result processData(const std::string &) override;

        bool argumentsOk() override { return true; }

    private:
        void beautifyOutput(NC::Scrollpad &w) override {}

        std::string m_username;
    };

    class GetToken : public AuthService {
    public:
        GetToken() = default;

        explicit GetToken(Arguments &&args)
                : AuthService(std::move(args)) {}

        const char *name() override { return "Get a request token"; }

        [[nodiscard]] std::string_view getRequestToken() const {
            return m_request_token;
        }

    protected:
        const char *methodName() override { return "auth.getToken"; }

        Result processData(const std::string &) override;

    private:
        std::string m_request_token;
    };

    class Authenticated {
    public:
        bool setupFetch() {
            // todo: check if the token is valid
            if (!m_session.getSessionToken().empty() || m_session.fetchLocal().first || m_session.fetch().first) {
                m_authenticated = true;
                return true;
            }

            Service::Result result;

            if (m_token.getRequestToken().empty()) {
                result = m_token.fetch();
                if (!result.first) throw std::runtime_error(result.second);
            }

            m_session.addRequestToken(m_token.getRequestToken());

            result = openBrowserWindow(AuthService::m_api_key, m_token.getRequestToken());
            if (!result.first) {
                throw std::runtime_error(result.second);
            }

            return true;
        }

        std::string_view fetchSessionToken() {
            auto result = m_session.fetch();
            if (!result.first) {
                throw std::runtime_error(result.second);
            }

            m_authenticated = true;
            return m_session.getSessionToken();
        }

        [[nodiscard]] std::string_view getSessionToken() const {
            return m_session.getSessionToken();
        }

        [[nodiscard]] bool isAuthenticated() const {
            return m_authenticated;
        }

    private:
        static Service::Result openBrowserWindow(std::string_view api_key, std::string_view req_token);

        GetSession m_session;
        GetToken m_token;
        bool m_authenticated{false};
    };
}


#endif //NCMPCPP_LASTFM_AUTHENTICATE_H
