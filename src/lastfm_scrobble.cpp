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

#include "curl_handle.h"
#include "lastfm_scrobble.h"
#include "mpdpp.h"


namespace {
    // todo: don't do the same thing twice! (for scrobbles and updating)
    LastFm::Service::Result setup_notify(std::string_view session_token, auto &arguments) {
        auto song = Mpd.GetCurrentSong();

        arguments["artist"] = song.getArtist();
        arguments["album"] = song.getAlbum();
        arguments["track"] = song.getTitle();
        arguments["duration"] = std::to_string(song.getDuration());

        arguments["sk"] = session_token;

        return {true, {}};
    }
}

namespace LastFm {
    Service::Result UpdateNowPlaying::notify() {
        auto result = ::setup_notify(m_session_token, m_arguments);

        if (!result.first) {
            return result;
        }

        m_arguments["api_sig"] = makeApiSignature();
        result = fetch();

        return result;
    }

    Service::Result UpdateNowPlaying::processData(const std::string &data) {
        // todo: error handling
        return {true, data};
    }

    Service::Result Scrobble::processData(const std::string &data) {
        // todo: error handling
        return {true, data};
    }

    Service::Result Scrobble::notify(std::uint64_t timestamp) {
        auto result = ::setup_notify(m_session_token, m_arguments);

        if (!result.first) {
            return result;
        }

        m_arguments["timestamp"] = std::to_string(timestamp);
        m_arguments["api_sig"] = makeApiSignature();
        result = fetch();

        return result;
    }

    void AuthenticatedServices::run() {
        using namespace std::chrono;

        if (m_scrobbler.getSessionToken().empty()) {
            return;
        }

        const auto song = Mpd.GetCurrentSong();
        const auto state = Mpd.getStatus().playerState();

        if (song.empty() || !song.empty() && state != MPD::PlayerState::psPlay) {
            return;
        }

        if (song != prev_song) {
            prev_song = song;
            m_song_start_time = system_clock::now().time_since_epoch();
            // todo: ideally updating should be prevented if the user is rapidly switching between songs.
            // todo: if lastfm can't find the song, don't retry.
            m_now_playing_updater.notify();
            m_song_was_scrobbled = false;
        }

        // Docs: The track must be longer than 30 seconds.
        if (song.getDuration() <= 30) {
            return;
        }

        // Docs: And the track has been played for at least half its duration, or for 4 minutes
        // (whichever occurs earlier.)
        // todo: what about pausing the song?
        const auto time_played = system_clock::now().time_since_epoch() - m_song_start_time;
        const seconds track_duration{song.getDuration()};

        if (!m_song_was_scrobbled && time_played >= (track_duration / 2) || time_played >= minutes{4}) {
            // todo: it might be a better idea to push a song onto a list when the conditions are met
            //       and attempt to notify the server after the song has ended.
            //       If that succeeds, the scrobble should be popped from the list.
            m_scrobbler.notify(duration_cast<seconds>(m_song_start_time).count());
            m_song_was_scrobbled = true;
        }

        // If the song was repeated, allow scrobbling again.
        // Although this will make it possible for multiple scrobbles on the same listen
        // (i.e. if the track was paused for a long time).
        if (time_played > track_duration) {
            m_song_was_scrobbled = false;
            prev_song = MPD::Song();
        }
    }
}
