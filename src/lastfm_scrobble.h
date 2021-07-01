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

#ifndef NCMPCPP_LASTFM_SCROBBLE_H
#define NCMPCPP_LASTFM_SCROBBLE_H

#include <string_view>
#include <chrono>
#include <boost/date_time/posix_time/conversion.hpp>
#include "lastfm_service.h"
#include "lastfm_authenticate.h"
#include "song.h"
#include "statusbar.h"
#include "global.h"

namespace LastFm {
    class Scrobble : public AuthService {
    public:
        Result notify(std::uint64_t timestamp);

    protected:
        Result processData(const std::string &) override;

        const char *methodName() override { return "track.scrobble"; }

        const char *name() override { return "Scrobble"; }

        [[nodiscard]] constexpr Curl::RequestType request_type() const override { return Curl::RequestType::POST; };
    };

    class UpdateNowPlaying : public AuthService {
    public:
        Result notify();

    protected:
        Result processData(const std::string &) override;

        const char *name() override { return "Update now playing"; }

        const char *methodName() override { return "track.updateNowPlaying"; }

        [[nodiscard]] constexpr Curl::RequestType request_type() const override { return Curl::RequestType::POST; };

    };

    class AuthenticatedServices : public Authenticated {
    public:
        bool authenticate() {
            using Global::Timer;

            if (Timer - last_retry > boost::posix_time::seconds(10)) {
                try {
                    if (!m_setup_done) {
                        // todo: it would be simpler to click a button and open the browser then
                        m_setup_done = setupFetch();
                    } else {
                        fetchSessionToken();
                    }

                    if (isAuthenticated()) {
                        auto session_token = getSessionToken();
                        m_now_playing_updater.setSessionToken(session_token);
                        m_scrobbler.setSessionToken(session_token);
                    }
                } catch (std::runtime_error &err) {
                    Statusbar::printf("Auth Failed: %1%", err.what());
                    m_setup_done = false;
                }
            }

            last_retry = Timer;
            return true;
        }

        void run();

        Scrobble *scrobbler() {
            if (!isAuthenticated()) return nullptr;
            return &m_scrobbler;
        }

        UpdateNowPlaying *now_playing_updater() {
            if (!isAuthenticated()) return nullptr;
            return &m_now_playing_updater;
        }

    private:
        boost::posix_time::ptime last_retry = boost::posix_time::from_time_t(0);
        Scrobble m_scrobbler;
        UpdateNowPlaying m_now_playing_updater;
        MPD::Song prev_song;
        std::chrono::system_clock::duration m_song_start_time;

        bool m_setup_done{false};
        bool m_song_was_scrobbled{false};
    };

}


#endif //NCMPCPP_LASTFM_SCROBBLE_H
