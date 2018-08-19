/*
    Copyright (C) 2013-2018 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef DCPOMATIC_PLAYER_H
#define DCPOMATIC_PLAYER_H

#include "player_text.h"
#include "active_text.h"
#include "content_text.h"
#include "film.h"
#include "content.h"
#include "position_image.h"
#include "piece.h"
#include "content_video.h"
#include "content_audio.h"
#include "audio_stream.h"
#include "audio_merger.h"
#include "empty.h"
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <list>

namespace dcp {
	class ReelAsset;
}

class PlayerVideo;
class Playlist;
class Font;
class AudioBuffers;
class ReferencedReelAsset;
class Shuffler;

class PlayerProperty
{
public:
	static int const VIDEO_CONTAINER_SIZE;
	static int const PLAYLIST;
	static int const FILM_CONTAINER;
	static int const FILM_VIDEO_FRAME_RATE;
	static int const DCP_DECODE_REDUCTION;
};

/** @class Player
 *  @brief A class which can play a Playlist.
 */
class Player : public boost::enable_shared_from_this<Player>, public boost::noncopyable
{
public:
	Player (boost::shared_ptr<const Film>, boost::shared_ptr<const Playlist> playlist);
	~Player ();

	bool pass ();
	void seek (DCPTime time, bool accurate);

	std::list<boost::shared_ptr<Font> > get_subtitle_fonts ();
	std::list<ReferencedReelAsset> get_reel_assets ();
	dcp::Size video_container_size () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _video_container_size;
	}

	void set_video_container_size (dcp::Size);
	void set_ignore_video ();
	void set_ignore_audio ();
	void set_ignore_text ();
	void set_always_burn_open_subtitles ();
	void set_fast ();
	void set_play_referenced ();
	void set_dcp_decode_reduction (boost::optional<int> reduction);

	boost::optional<DCPTime> content_time_to_dcp (boost::shared_ptr<Content> content, ContentTime t);

	boost::signals2::signal<void (ChangeType, int, bool)> Change;

	/** The change suggested by a MayChange did not happen */
	boost::signals2::signal<void ()> NotChanged;

	/** Emitted when a video frame is ready.  These emissions happen in the correct order. */
	boost::signals2::signal<void (boost::shared_ptr<PlayerVideo>, DCPTime)> Video;
	boost::signals2::signal<void (boost::shared_ptr<AudioBuffers>, DCPTime)> Audio;
	/** Emitted when a text is ready.  This signal may be emitted considerably
	 *  after the corresponding Video.
	 */
	boost::signals2::signal<void (PlayerText, TextType, DCPTimePeriod)> Text;

private:
	friend class PlayerWrapper;
	friend class Piece;
	friend struct player_time_calculation_test1;
	friend struct player_time_calculation_test2;
	friend struct player_time_calculation_test3;
	friend struct player_subframe_test;

	void setup_pieces ();
	void setup_pieces_unlocked ();
	void flush ();
	void film_changed (Film::Property);
	void playlist_change (ChangeType);
	void playlist_content_change (ChangeType, int, bool);
	std::list<PositionImage> transform_bitmap_texts (std::list<BitmapText>) const;
	Frame dcp_to_content_video (boost::shared_ptr<const Piece> piece, DCPTime t) const;
	DCPTime content_video_to_dcp (boost::shared_ptr<const Piece> piece, Frame f) const;
	Frame dcp_to_resampled_audio (boost::shared_ptr<const Piece> piece, DCPTime t) const;
	DCPTime resampled_audio_to_dcp (boost::shared_ptr<const Piece> piece, Frame f) const;
	ContentTime dcp_to_content_time (boost::shared_ptr<const Piece> piece, DCPTime t) const;
	DCPTime content_time_to_dcp (boost::shared_ptr<const Piece> piece, ContentTime t) const;
	boost::shared_ptr<PlayerVideo> black_player_video_frame (Eyes eyes) const;
	void video (boost::weak_ptr<Piece>, ContentVideo);
	void audio (boost::weak_ptr<Piece>, AudioStreamPtr, ContentAudio);
	void bitmap_text_start (boost::weak_ptr<Piece>, boost::weak_ptr<const TextContent>, ContentBitmapText);
	void plain_text_start (boost::weak_ptr<Piece>, boost::weak_ptr<const TextContent>, ContentStringText);
	void subtitle_stop (boost::weak_ptr<Piece>, boost::weak_ptr<const TextContent>, ContentTime, TextType);
	DCPTime one_video_frame () const;
	void fill_audio (DCPTimePeriod period);
	std::pair<boost::shared_ptr<AudioBuffers>, DCPTime> discard_audio (
		boost::shared_ptr<const AudioBuffers> audio, DCPTime time, DCPTime discard_to
		) const;
	boost::optional<PositionImage> open_subtitles_for_frame (DCPTime time) const;
	void emit_video (boost::shared_ptr<PlayerVideo> pv, DCPTime time);
	void do_emit_video (boost::shared_ptr<PlayerVideo> pv, DCPTime time);
	void emit_audio (boost::shared_ptr<AudioBuffers> data, DCPTime time);

	/** Mutex to protect the whole Player state.  When it's used for the preview we have
	    seek() and pass() called from the Butler thread and lots of other stuff called
	    from the GUI thread.
	*/
	mutable boost::mutex _mutex;

	boost::shared_ptr<const Film> _film;
	boost::shared_ptr<const Playlist> _playlist;

	/** true if we are suspended (i.e. pass() and seek() do nothing */
	bool _suspended;
	std::list<boost::shared_ptr<Piece> > _pieces;

	/** Size of the image in the DCP (e.g. 1990x1080 for flat) */
	dcp::Size _video_container_size;
	boost::shared_ptr<Image> _black_image;

	/** true if the player should ignore all video; i.e. never produce any */
	bool _ignore_video;
	bool _ignore_audio;
	/** true if the player should ignore all text; i.e. never produce any */
	bool _ignore_text;
	bool _always_burn_open_subtitles;
	/** true if we should try to be fast rather than high quality */
	bool _fast;
	/** true if we should `play' (i.e output) referenced DCP data (e.g. for preview) */
	bool _play_referenced;

	/** Time just after the last video frame we emitted, or the time of the last accurate seek */
	boost::optional<DCPTime> _last_video_time;
	boost::optional<Eyes> _last_video_eyes;
	/** Time just after the last audio frame we emitted, or the time of the last accurate seek */
	boost::optional<DCPTime> _last_audio_time;

	boost::optional<int> _dcp_decode_reduction;

	typedef std::map<boost::weak_ptr<Piece>, boost::shared_ptr<PlayerVideo> > LastVideoMap;
	LastVideoMap _last_video;

	AudioMerger _audio_merger;
	Shuffler* _shuffler;
	std::list<std::pair<boost::shared_ptr<PlayerVideo>, DCPTime> > _delay;

	class StreamState
	{
	public:
		StreamState () {}

		StreamState (boost::shared_ptr<Piece> p, DCPTime l)
			: piece(p)
			, last_push_end(l)
		{}

		boost::shared_ptr<Piece> piece;
		DCPTime last_push_end;
	};
	std::map<AudioStreamPtr, StreamState> _stream_states;

	Empty _black;
	Empty _silent;

	ActiveText _active_texts[TEXT_COUNT];
	boost::shared_ptr<AudioProcessor> _audio_processor;

	boost::signals2::scoped_connection _film_changed_connection;
	boost::signals2::scoped_connection _playlist_change_connection;
	boost::signals2::scoped_connection _playlist_content_change_connection;
};

#endif
