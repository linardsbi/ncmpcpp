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

#include "lastfm_authenticate.h"
#include "curl_handle.h"
#include "settings.h"
#include "statusbar.h"
#include <boost/uuid/detail/md5.hpp>
#include <boost/algorithm/hex.hpp>
#include <boost/format.hpp>
#include <fstream>

namespace LastFm {
    boost::optional<std::string_view>
    extractValueFromData(std::string_view key, std::string_view data, std::size_t value_size = 0) {
        std::string full_key = "<";
        full_key += key;
        full_key += ">";
        const auto key_pos = data.find(full_key);
        if (key_pos == std::string::npos) {
            return {};
        }

        const auto value_start_pos = key_pos + full_key.length();

        if (value_size > 0) {
            return data.substr(value_start_pos, value_size);
        }

        full_key.insert(full_key.begin() + 1, '/'); // <key> -> </key>
        const auto value_end_pos = data.find(full_key);

        if (value_end_pos == std::string::npos) {
            return {};
        }

        return data.substr(key_pos + full_key.length() - 1, value_end_pos - value_start_pos);
    }

    Service::Result GetSession::processData(const std::string &data) {
        auto key = extractValueFromData("key", data, 32);

        if (actionFailed(data) || key->empty()) {
            return {false, "Invalid response"};
        }

        m_session_token.assign(key.value());

        // if the request was valid up to this point, assume that there is a name in the response
        auto username = extractValueFromData("name", data);
        m_username.assign(username.value());

        if (storeSessionToken()) {
            return {true, "Session token stored"};
        }

        return {false, "Failed to store session token"};
    }

    bool GetSession::storeSessionToken() {
        if (auto file = std::ofstream(Config.ncmpcpp_directory + "lastfm.session")) {
            file << m_session_token;
            return file.good();
        }

        return false;
    }

    Service::Result GetSession::fetchLocal() {
        if (!m_session_token.empty()) {
            throw std::runtime_error("Attempting to fetch session token when token is already saved");
        }

        if (auto file = std::ifstream(Config.ncmpcpp_directory + "lastfm.session")) {
            std::getline(file, m_session_token);
            if (m_session_token.length() != 32) {
                throw std::runtime_error("Session token length is invalid");
            }
            return {true, "Using stored token"};
        }
        return {false, "Couldn't fetch local token"};
    }

    Service::Result GetToken::processData(const std::string &data) {
        if (!m_request_token.empty()) {
            // todo: is the request token invalid?
            return {true, {}};
        }

        Result result;
        result.first = false;

        auto token = extractValueFromData("token", data, 32);

        if (actionFailed(data) || token->empty()) {
            result.second = "Invalid response";
            return result;
        }

        m_request_token.assign(token.value());

        result.first = true;
        result.second = "Success";
        return result;
    }

    Service::Result AuthService::fetch() {
        Result result;
        result.first = false;

        std::string url(m_api_url);
        std::string data;
        CURLcode code;
        if (request_type() == Curl::RequestType::GET) {
            url += "?";
            url += formatURLParams();
            code = Curl::perform(data, url);
        } else {
            Curl::Options options;
            options.post_params = std::move(formatURLParams());
            code = Curl::perform(data, url, Curl::RequestType::POST, options);
        }

        if (code != CURLE_OK)
            result.second = curl_easy_strerror(code);
        else if (actionFailed(data))
            result.second = "Invalid response";
        else {
            result = processData(data);
        }

        return result;
    }

    std::string makeMD5From(const std::string &string) {
        using boost::uuids::detail::md5;

        md5 hash;
        md5::digest_type digest;

        hash.process_bytes(string.data(), string.size());
        hash.get_digest(digest);

        const auto intDigest = std::bit_cast<const int *>(&digest);
        std::string result;
        boost::algorithm::hex(intDigest, intDigest + (sizeof(md5::digest_type) / sizeof(int)),
                              std::back_inserter(result));

        return result;
    }

    std::string AuthService::makeApiSignature() {
        std::string formatted;
        Arguments sig_arguments = m_arguments;
        sig_arguments["method"] = methodName();

        for (const auto &arg : sig_arguments) {
            if (arg.first == "api_sig") continue;
            formatted += arg.first + arg.second;
        }

        formatted += m_shared_secret;

        return makeMD5From(formatted);
    }

    std::string AuthService::formatURLParams() {
        std::string url = "method=";
        url += methodName();
        for (const auto &arg : m_arguments) {
            url += "&";
            url += arg.first;
            url += "=";
            url += Curl::escape(arg.second);
        }
        return std::move(url);
    }

    Service::Result Authenticated::openBrowserWindow(std::string_view api_key, std::string_view req_token) {
        const char *cmd{nullptr};

#ifdef _WIN32
        cmd = "start";
#elif defined(__linux__)
        cmd = "xdg-open";
#elif defined(__APPLE__)
        cmd = "open";
#endif
        const auto path =
                boost::format("%1% 'https://www.last.fm/api/auth/?api_key=%2%&token=%3%'") % cmd %
                Curl::escape(api_key) % Curl::escape(req_token);

        auto result = system(path.str().c_str());
        if (result == -1) {
            std::string response = "No such command: ";
            response += cmd;
            return {false, std::move(response)};
        }
        return {true, "success"}; // todo: check if actually authenticated somehow?
    }
}