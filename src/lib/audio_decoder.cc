/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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

#include "audio_decoder.h"
#include "audio_buffers.h"
#include "audio_content.h"
#include "dcpomatic_log.h"
#include "log.h"
#include "resampler.h"
#include "compose.hpp"
#include <boost/foreach.hpp>
#include <iostream>

#include "i18n.h"

using std::cout;
using std::map;
using std::pair;
using boost::shared_ptr;
using boost::optional;
using namespace dcpomatic;

AudioDecoder::AudioDecoder (Decoder* parent, shared_ptr<const AudioContent> content, bool fast)
	: DecoderPart (parent)
	, _content (content)
	, _fast (fast)
{
	/* Set up _positions so that we have one for each stream */
	BOOST_FOREACH (AudioStreamPtr i, content->streams ()) {
		_positions[i] = 0;
	}
}

void
AudioDecoder::emit (shared_ptr<const Film> film, AudioStreamPtr stream, shared_ptr<const AudioBuffers> data, ContentTime time)
{
	if (ignore ()) {
		return;
	}

	if (_positions[stream] == 0) {
		/* This is the first data we have received since initialisation or seek.  Set
		   the position based on the ContentTime that was given.  After this first time
		   we just count samples, as it seems that ContentTimes are unreliable from
		   FFmpegDecoder (not quite continuous; perhaps due to some rounding error).
		*/
		if (_content->delay() > 0) {
			/* Insert silence to give the delay */
			silence (_content->delay ());
		}
		time += ContentTime::from_seconds (_content->delay() / 1000.0);
		_positions[stream] = time.frames_round (_content->resampled_frame_rate(film));
	}

	shared_ptr<Resampler> resampler;
	ResamplerMap::iterator i = _resamplers.find(stream);
	if (i != _resamplers.end ()) {
		resampler = i->second;
	} else {
		if (stream->frame_rate() != _content->resampled_frame_rate(film)) {
			LOG_GENERAL (
				"Creating new resampler from %1 to %2 with %3 channels",
				stream->frame_rate(),
				_content->resampled_frame_rate(film),
				stream->channels()
				);

			resampler.reset (new Resampler (stream->frame_rate(), _content->resampled_frame_rate(film), stream->channels()));
			if (_fast) {
				resampler->set_fast ();
			}
			_resamplers[stream] = resampler;
		}
	}

	if (resampler) {
		shared_ptr<const AudioBuffers> ro = resampler->run (data);
		if (ro->frames() == 0) {
			return;
		}
		data = ro;
	}

	Data(stream, ContentAudio (data, _positions[stream]));
	_positions[stream] += data->frames();
}

/** @return Time just after the last thing that was emitted from a given stream */
ContentTime
AudioDecoder::stream_position (shared_ptr<const Film> film, AudioStreamPtr stream) const
{
	PositionMap::const_iterator i = _positions.find (stream);
	DCPOMATIC_ASSERT (i != _positions.end ());
	return ContentTime::from_frames (i->second, _content->resampled_frame_rate(film));
}

ContentTime
AudioDecoder::position (shared_ptr<const Film> film) const
{
	optional<ContentTime> p;
	for (PositionMap::const_iterator i = _positions.begin(); i != _positions.end(); ++i) {
		ContentTime const ct = stream_position (film, i->first);
		if (!p || ct < *p) {
			p = ct;
		}
	}

	return p.get_value_or(ContentTime());
}

void
AudioDecoder::seek ()
{
	for (ResamplerMap::iterator i = _resamplers.begin(); i != _resamplers.end(); ++i) {
		i->second->flush ();
		i->second->reset ();
	}

	for (PositionMap::iterator i = _positions.begin(); i != _positions.end(); ++i) {
		i->second = 0;
	}
}

void
AudioDecoder::flush ()
{
	for (ResamplerMap::iterator i = _resamplers.begin(); i != _resamplers.end(); ++i) {
		shared_ptr<const AudioBuffers> ro = i->second->flush ();
		if (ro->frames() > 0) {
			Data (i->first, ContentAudio (ro, _positions[i->first]));
			_positions[i->first] += ro->frames();
		}
	}

	if (_content->delay() < 0) {
		/* Finish off with the gap caused by the delay */
		silence (-_content->delay ());
	}
}

void
AudioDecoder::silence (int milliseconds)
{
	BOOST_FOREACH (AudioStreamPtr i, _content->streams ()) {
		int const samples = ContentTime::from_seconds(milliseconds / 1000.0).frames_round(i->frame_rate());
		shared_ptr<AudioBuffers> silence (new AudioBuffers (i->channels(), samples));
		silence->make_silent ();
		Data (i, ContentAudio (silence, _positions[i]));
	}
}
