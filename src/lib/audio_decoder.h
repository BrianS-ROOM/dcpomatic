/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

/** @file src/lib/audio_decoder.h
 *  @brief Parent class for audio decoders.
 */

#ifndef DCPOMATIC_AUDIO_DECODER_H
#define DCPOMATIC_AUDIO_DECODER_H

#include "decoder.h"
#include "content_audio.h"
#include "audio_stream.h"
#include "decoder_part.h"
#include <boost/enable_shared_from_this.hpp>

class AudioBuffers;
class AudioContent;
class AudioDecoderStream;
class Log;

/** @class AudioDecoder.
 *  @brief Parent class for audio decoders.
 */
class AudioDecoder : public boost::enable_shared_from_this<AudioDecoder>, public DecoderPart
{
public:
	AudioDecoder (Decoder* parent, boost::shared_ptr<const AudioContent>, boost::shared_ptr<Log> log);

	/** Try to fetch some audio from a specific place in this content.
	 *  @param frame Frame to start from (after resampling, if applicable)
	 *  @param length Frames to get (after resampling, if applicable)
	 *  @param accurate true to try hard to return frames from exactly `frame', false if we don't mind nearby frames.
	 *  @return Time-stamped audio data which may or may not be from the location (and of the length) requested.
	 */
	ContentAudio get (AudioStreamPtr stream, Frame time, Frame length, bool accurate);

	void set_fast ();

	void give (AudioStreamPtr stream, boost::shared_ptr<const AudioBuffers>, ContentTime);
	void flush ();
	void seek (ContentTime t, bool accurate);

private:
	/** An AudioDecoderStream object to manage each stream in _audio_content */
	std::map<AudioStreamPtr, boost::shared_ptr<AudioDecoderStream> > _streams;
};

#endif
