/*
    Copyright (C) 2014-2018 Carl Hetherington <cth@carlh.net>

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

#include "dcp_content.h"
#include "video_content.h"
#include "audio_content.h"
#include "dcp_examiner.h"
#include "job.h"
#include "film.h"
#include "config.h"
#include "overlaps.h"
#include "compose.hpp"
#include "dcp_decoder.h"
#include "log.h"
#include "dcpomatic_log.h"
#include "text_content.h"
#include <dcp/dcp.h>
#include <dcp/raw_convert.h>
#include <dcp/exceptions.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel.h>
#include <libxml++/libxml++.h>
#include <boost/foreach.hpp>
#include <iterator>
#include <iostream>

#include "i18n.h"

using std::string;
using std::cout;
using std::distance;
using std::pair;
using std::vector;
using std::list;
using boost::shared_ptr;
using boost::scoped_ptr;
using boost::optional;
using boost::function;
using boost::dynamic_pointer_cast;
using dcp::raw_convert;

int const DCPContentProperty::NEEDS_ASSETS       = 600;
int const DCPContentProperty::NEEDS_KDM          = 601;
int const DCPContentProperty::REFERENCE_VIDEO    = 602;
int const DCPContentProperty::REFERENCE_AUDIO    = 603;
int const DCPContentProperty::REFERENCE_TEXT     = 604;
int const DCPContentProperty::NAME               = 605;
int const DCPContentProperty::TEXTS              = 606;
int const DCPContentProperty::CPL                = 607;

DCPContent::DCPContent (boost::filesystem::path p)
	: _encrypted (false)
	, _needs_assets (false)
	, _kdm_valid (false)
	, _reference_video (false)
	, _reference_audio (false)
	, _three_d (false)
{
	LOG_GENERAL ("Creating DCP content from %1", p.string());

	read_directory (p);
	set_default_colour_conversion ();

	for (int i = 0; i < TEXT_COUNT; ++i) {
		_reference_text[i] = false;
	}
}

DCPContent::DCPContent (cxml::ConstNodePtr node, int version)
	: Content (node)
{
	video = VideoContent::from_xml (this, node, version);
	audio = AudioContent::from_xml (this, node, version);
	text = TextContent::from_xml (this, node, version);

	for (int i = 0; i < TEXT_COUNT; ++i) {
		_reference_text[i] = false;
	}

	if (video && audio) {
		audio->set_stream (
			AudioStreamPtr (
				new AudioStream (
					node->number_child<int> ("AudioFrameRate"),
					/* AudioLength was not present in some old metadata versions */
					node->optional_number_child<Frame>("AudioLength").get_value_or (
						video->length() * node->number_child<int>("AudioFrameRate") / video_frame_rate().get()
						),
					AudioMapping (node->node_child ("AudioMapping"), version)
					)
				)
			);
	}

	_name = node->string_child ("Name");
	_encrypted = node->bool_child ("Encrypted");
	_needs_assets = node->optional_bool_child("NeedsAssets").get_value_or (false);
	if (node->optional_node_child ("KDM")) {
		_kdm = dcp::EncryptedKDM (node->string_child ("KDM"));
	}
	_kdm_valid = node->bool_child ("KDMValid");
	_reference_video = node->optional_bool_child ("ReferenceVideo").get_value_or (false);
	_reference_audio = node->optional_bool_child ("ReferenceAudio").get_value_or (false);
	if (version >= 37) {
		_reference_text[TEXT_OPEN_SUBTITLE] = node->optional_bool_child("ReferenceOpenSubtitle").get_value_or(false);
		_reference_text[TEXT_CLOSED_CAPTION] = node->optional_bool_child("ReferenceClosedCaption").get_value_or(false);
	} else {
		_reference_text[TEXT_OPEN_SUBTITLE] = node->optional_bool_child("ReferenceSubtitle").get_value_or(false);
		_reference_text[TEXT_CLOSED_CAPTION] = false;
	}
	if (node->optional_string_child("Standard")) {
		string const s = node->optional_string_child("Standard").get();
		if (s == "Interop") {
			_standard = dcp::INTEROP;
		} else if (s == "SMPTE") {
			_standard = dcp::SMPTE;
		} else {
			DCPOMATIC_ASSERT (false);
		}
	}
	_three_d = node->optional_bool_child("ThreeD").get_value_or (false);

	optional<string> ck = node->optional_string_child("ContentKind");
	if (ck) {
		_content_kind = dcp::content_kind_from_string (*ck);
	}
	_cpl = node->optional_string_child("CPL");
	BOOST_FOREACH (cxml::ConstNodePtr i, node->node_children("ReelLength")) {
		_reel_lengths.push_back (raw_convert<int64_t> (i->content ()));
	}
}

void
DCPContent::read_directory (boost::filesystem::path p)
{
	read_sub_directory (p);

	bool have_assetmap = false;
	BOOST_FOREACH (boost::filesystem::path i, paths()) {
		if (i.filename() == "ASSETMAP" || i.filename() == "ASSETMAP.xml") {
			have_assetmap = true;
		}
	}

	if (!have_assetmap) {
		throw DCPError ("No ASSETMAP or ASSETMAP.xml file found: is this a DCP?");
	}
}

void
DCPContent::read_sub_directory (boost::filesystem::path p)
{
	LOG_GENERAL ("DCPContent::read_sub_directory reads %1", p.string());
	for (boost::filesystem::directory_iterator i(p); i != boost::filesystem::directory_iterator(); ++i) {
		if (boost::filesystem::is_regular_file (i->path())) {
			LOG_GENERAL ("Inside there's regular file %1", i->path().string());
			add_path (i->path());
		} else if (boost::filesystem::is_directory (i->path ())) {
			LOG_GENERAL ("Inside there's directory %1", i->path().string());
			read_sub_directory (i->path());
		}
	}
}

void
DCPContent::examine (shared_ptr<const Film> film, shared_ptr<Job> job)
{
	bool const needed_assets = needs_assets ();
	bool const needed_kdm = needs_kdm ();
	string const old_name = name ();
	int const old_texts = text.size ();

	ChangeSignaller<Content> cc_texts (this, DCPContentProperty::TEXTS);
	ChangeSignaller<Content> cc_assets (this, DCPContentProperty::NEEDS_ASSETS);
	ChangeSignaller<Content> cc_kdm (this, DCPContentProperty::NEEDS_KDM);
	ChangeSignaller<Content> cc_name (this, DCPContentProperty::NAME);

	if (job) {
		job->set_progress_unknown ();
	}
	Content::examine (film, job);

	shared_ptr<DCPExaminer> examiner (new DCPExaminer (shared_from_this ()));

	if (examiner->has_video()) {
		{
			boost::mutex::scoped_lock lm (_mutex);
			video.reset (new VideoContent (this));
		}
		video->take_from_examiner (examiner);
		set_default_colour_conversion ();
	}

	if (examiner->has_audio()) {
		{
			boost::mutex::scoped_lock lm (_mutex);
			audio.reset (new AudioContent (this));
		}
		AudioStreamPtr as (new AudioStream (examiner->audio_frame_rate(), examiner->audio_length(), examiner->audio_channels()));
		audio->set_stream (as);
		AudioMapping m = as->mapping ();
		m.make_default (film ? film->audio_processor() : 0);
		as->set_mapping (m);
	}

	int texts = 0;
	{
		boost::mutex::scoped_lock lm (_mutex);
		_name = examiner->name ();
		for (int i = 0; i < TEXT_COUNT; ++i) {
			for (int j = 0; j < examiner->text_count(static_cast<TextType>(i)); ++j) {
				shared_ptr<TextContent> c(new TextContent(this, static_cast<TextType>(i), static_cast<TextType>(i)));
				if (i == TEXT_CLOSED_CAPTION) {
					c->set_dcp_track (examiner->dcp_text_track(j));
				}
				text.push_back (c);
			}
		}
		texts = text.size ();
		_encrypted = examiner->encrypted ();
		_needs_assets = examiner->needs_assets ();
		_kdm_valid = examiner->kdm_valid ();
		_standard = examiner->standard ();
		_three_d = examiner->three_d ();
		_content_kind = examiner->content_kind ();
		_cpl = examiner->cpl ();
		_reel_lengths = examiner->reel_lengths ();
	}

	if (old_texts == texts) {
		cc_texts.abort ();
	}

	if (needed_assets == needs_assets()) {
		cc_assets.abort ();
	}

	if (needed_kdm == needs_kdm()) {
		cc_kdm.abort ();
	}

	if (old_name == name()) {
		cc_name.abort ();
	}

	if (video) {
		video->set_frame_type (_three_d ? VIDEO_FRAME_TYPE_3D : VIDEO_FRAME_TYPE_2D);
	}
}

string
DCPContent::summary () const
{
	boost::mutex::scoped_lock lm (_mutex);
	return String::compose (_("%1 [DCP]"), _name);
}

string
DCPContent::technical_summary () const
{
	string s = Content::technical_summary() + " - ";
	if (video) {
		s += video->technical_summary() + " - ";
	}
	if (audio) {
		s += audio->technical_summary() + " - ";
	}
	return s;
}

void
DCPContent::as_xml (xmlpp::Node* node, bool with_paths) const
{
	node->add_child("Type")->add_child_text ("DCP");

	Content::as_xml (node, with_paths);

	if (video) {
		video->as_xml (node);
	}

	if (audio) {
		audio->as_xml (node);
		node->add_child("AudioFrameRate")->add_child_text (raw_convert<string> (audio->stream()->frame_rate()));
		node->add_child("AudioLength")->add_child_text (raw_convert<string> (audio->stream()->length()));
		audio->stream()->mapping().as_xml (node->add_child("AudioMapping"));
	}

	BOOST_FOREACH (shared_ptr<TextContent> i, text) {
		i->as_xml (node);
	}

	boost::mutex::scoped_lock lm (_mutex);
	node->add_child("Name")->add_child_text (_name);
	node->add_child("Encrypted")->add_child_text (_encrypted ? "1" : "0");
	node->add_child("NeedsAssets")->add_child_text (_needs_assets ? "1" : "0");
	if (_kdm) {
		node->add_child("KDM")->add_child_text (_kdm->as_xml ());
	}
	node->add_child("KDMValid")->add_child_text (_kdm_valid ? "1" : "0");
	node->add_child("ReferenceVideo")->add_child_text (_reference_video ? "1" : "0");
	node->add_child("ReferenceAudio")->add_child_text (_reference_audio ? "1" : "0");
	node->add_child("ReferenceOpenSubtitle")->add_child_text(_reference_text[TEXT_OPEN_SUBTITLE] ? "1" : "0");
	node->add_child("ReferenceClosedCaption")->add_child_text(_reference_text[TEXT_CLOSED_CAPTION] ? "1" : "0");
	if (_standard) {
		switch (_standard.get ()) {
		case dcp::INTEROP:
			node->add_child("Standard")->add_child_text ("Interop");
			break;
		case dcp::SMPTE:
			node->add_child("Standard")->add_child_text ("SMPTE");
			break;
		default:
			DCPOMATIC_ASSERT (false);
		}
	}
	node->add_child("ThreeD")->add_child_text (_three_d ? "1" : "0");
	if (_content_kind) {
		node->add_child("ContentKind")->add_child_text(dcp::content_kind_to_string(*_content_kind));
	}
	if (_cpl) {
		node->add_child("CPL")->add_child_text (_cpl.get ());
	}
	BOOST_FOREACH (int64_t i, _reel_lengths) {
		node->add_child("ReelLength")->add_child_text (raw_convert<string> (i));
	}
}

DCPTime
DCPContent::full_length (shared_ptr<const Film> film) const
{
	if (!video) {
		return DCPTime();
	}
	FrameRateChange const frc (film, shared_from_this());
	return DCPTime::from_frames (llrint(video->length() * frc.factor()), film->video_frame_rate());
}

DCPTime
DCPContent::approximate_length () const
{
	if (!video) {
		return DCPTime();
	}
	return DCPTime::from_frames (video->length(), 24);
}

string
DCPContent::identifier () const
{
	string s = Content::identifier() + "_";

	if (video) {
		s += video->identifier() + "_";
	}

	BOOST_FOREACH (shared_ptr<TextContent> i, text) {
		s += i->identifier () + " ";
	}

	s += string (_reference_video ? "1" : "0");
	for (int i = 0; i < TEXT_COUNT; ++i) {
		s += string (_reference_text[i] ? "1" : "0");
	}
	return s;
}

void
DCPContent::add_kdm (dcp::EncryptedKDM k)
{
	_kdm = k;
}

void
DCPContent::add_ov (boost::filesystem::path ov)
{
	read_directory (ov);
}

bool
DCPContent::can_be_played () const
{
	return !needs_kdm() && !needs_assets();
}

bool
DCPContent::needs_kdm () const
{
	boost::mutex::scoped_lock lm (_mutex);
	return _encrypted && !_kdm_valid;
}

bool
DCPContent::needs_assets () const
{
	boost::mutex::scoped_lock lm (_mutex);
	return _needs_assets;
}

vector<boost::filesystem::path>
DCPContent::directories () const
{
	return dcp::DCP::directories_from_files (paths());
}

void
DCPContent::add_properties (shared_ptr<const Film> film, list<UserProperty>& p) const
{
	Content::add_properties (film, p);
	if (video) {
		video->add_properties (p);
	}
	if (audio) {
		audio->add_properties (film, p);
	}
}

void
DCPContent::set_default_colour_conversion ()
{
	/* Default to no colour conversion for DCPs */
	if (video) {
		video->unset_colour_conversion ();
	}
}

void
DCPContent::set_reference_video (bool r)
{
	ChangeSignaller<Content> cc (this, DCPContentProperty::REFERENCE_VIDEO);

	{
		boost::mutex::scoped_lock lm (_mutex);
		_reference_video = r;
	}
}

void
DCPContent::set_reference_audio (bool r)
{
	ChangeSignaller<Content> cc (this, DCPContentProperty::REFERENCE_AUDIO);

	{
		boost::mutex::scoped_lock lm (_mutex);
		_reference_audio = r;
	}
}

void
DCPContent::set_reference_text (TextType type, bool r)
{
	ChangeSignaller<Content> cc (this, DCPContentProperty::REFERENCE_TEXT);

	{
		boost::mutex::scoped_lock lm (_mutex);
		_reference_text[type] = r;
	}
}

list<DCPTimePeriod>
DCPContent::reels (shared_ptr<const Film> film) const
{
	list<int64_t> reel_lengths = _reel_lengths;
	if (reel_lengths.empty ()) {
		/* Old metadata with no reel lengths; get them here instead */
		try {
			scoped_ptr<DCPExaminer> examiner (new DCPExaminer (shared_from_this()));
			reel_lengths = examiner->reel_lengths ();
		} catch (...) {
			/* Could not examine the DCP; guess reels */
			reel_lengths.push_back (length_after_trim(film).frames_round(film->video_frame_rate()));
		}
	}

	list<DCPTimePeriod> p;

	/* This content's frame rate must be the same as the output DCP rate, so we can
	   convert `directly' from ContentTime to DCPTime.
	*/

	/* The starting point of this content on the timeline */
	DCPTime pos = position() - DCPTime (trim_start().get());

	BOOST_FOREACH (int64_t i, reel_lengths) {
		/* This reel runs from `pos' to `to' */
		DCPTime const to = pos + DCPTime::from_frames (i, film->video_frame_rate());
		if (to > position()) {
			p.push_back (DCPTimePeriod (max(position(), pos), min(end(film), to)));
			if (to > end(film)) {
				break;
			}
		}
		pos = to;
	}

	return p;
}

list<DCPTime>
DCPContent::reel_split_points (shared_ptr<const Film> film) const
{
	list<DCPTime> s;
	BOOST_FOREACH (DCPTimePeriod i, reels(film)) {
		s.push_back (i.from);
	}
	return s;
}

bool
DCPContent::can_reference (shared_ptr<const Film> film, function<bool (shared_ptr<const Content>)> part, string overlapping, string& why_not) const
{
	/* We must be using the same standard as the film */
	if (_standard) {
		if (_standard.get() == dcp::INTEROP && !film->interop()) {
			/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
			why_not = _("it is Interop and the film is set to SMPTE.");
			return false;
		} else if (_standard.get() == dcp::SMPTE && film->interop()) {
			/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
			why_not = _("it is SMPTE and the film is set to Interop.");
			return false;
		}
	}

	/* And the same frame rate */
	if (!video_frame_rate() || (lrint(video_frame_rate().get()) != film->video_frame_rate())) {
		/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
		why_not = _("it has a different frame rate to the film.");
		return false;
	}

	list<DCPTimePeriod> const fr = film->reels ();

	list<DCPTimePeriod> reel_list;
	try {
		reel_list = reels (film);
	} catch (dcp::DCPReadError &) {
		/* We couldn't read the DCP; it's probably missing */
		return false;
	} catch (dcp::KDMDecryptionError &) {
		/* We have an incorrect KDM */
		return false;
	}

	/* fr must contain reels().  It can also contain other reels, but it must at
	   least contain reels().
	*/
	BOOST_FOREACH (DCPTimePeriod i, reel_list) {
		if (find (fr.begin(), fr.end(), i) == fr.end ()) {
			/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
			why_not = _("its reel lengths differ from those in the film; set the reel mode to 'split by video content'.");
			return false;
		}
	}

	ContentList a = overlaps (film, film->content(), part, position(), end(film));
	if (a.size() != 1 || a.front().get() != this) {
		why_not = overlapping;
		return false;
	}

	return true;
}

static
bool check_video (shared_ptr<const Content> c)
{
	return static_cast<bool>(c->video);
}

bool
DCPContent::can_reference_video (shared_ptr<const Film> film, string& why_not) const
{
	if (!video) {
		why_not = _("There is no video in this DCP");
		return false;
	}

	Resolution video_res = RESOLUTION_2K;
	if (video->size().width > 2048 || video->size().height > 1080) {
		video_res = RESOLUTION_4K;
	}

	if (film->resolution() != video_res) {
		if (video_res == RESOLUTION_4K) {
			/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
			why_not = _("it is 4K and the film is 2K.");
		} else {
			/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
			why_not = _("it is 2K and the film is 4K.");
		}
		return false;
	} else if (film->frame_size() != video->size()) {
		/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
		why_not = _("its video frame size differs from the film's.");
		return false;
	}

	/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
	return can_reference (film, bind (&check_video, _1), _("it overlaps other video content; remove the other content."), why_not);
}

static
bool check_audio (shared_ptr<const Content> c)
{
	return static_cast<bool>(c->audio);
}

bool
DCPContent::can_reference_audio (shared_ptr<const Film> film, string& why_not) const
{
	shared_ptr<DCPDecoder> decoder;
	try {
		decoder.reset (new DCPDecoder (film, shared_from_this(), false));
	} catch (dcp::DCPReadError &) {
		/* We couldn't read the DCP, so it's probably missing */
		return false;
	} catch (DCPError &) {
		/* We couldn't read the DCP, so it's probably missing */
		return false;
	} catch (dcp::KDMDecryptionError &) {
		/* We have an incorrect KDM */
		return false;
	}

        BOOST_FOREACH (shared_ptr<dcp::Reel> i, decoder->reels()) {
                if (!i->main_sound()) {
			/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
                        why_not = _("it does not have sound in all its reels.");
                        return false;
                }
        }

	/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
	return can_reference (film, bind (&check_audio, _1), _("it overlaps other audio content; remove the other content."), why_not);
}

static
bool check_text (shared_ptr<const Content> c)
{
	return !c->text.empty();
}

bool
DCPContent::can_reference_text (shared_ptr<const Film> film, TextType type, string& why_not) const
{
	shared_ptr<DCPDecoder> decoder;
	try {
		decoder.reset (new DCPDecoder (film, shared_from_this(), false));
	} catch (dcp::DCPReadError &) {
		/* We couldn't read the DCP, so it's probably missing */
		return false;
	} catch (dcp::KDMDecryptionError &) {
		/* We have an incorrect KDM */
		return false;
	}

        BOOST_FOREACH (shared_ptr<dcp::Reel> i, decoder->reels()) {
                if (type == TEXT_OPEN_SUBTITLE && !i->main_subtitle()) {
			/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
                        why_not = _("it does not have open subtitles in all its reels.");
                        return false;
                }
		if (type == TEXT_CLOSED_CAPTION && i->closed_captions().empty()) {
			/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
                        why_not = _("it does not have closed captions in all its reels.");
                        return false;
		}
        }

	/// TRANSLATORS: this string will follow "Cannot reference this DCP: "
	return can_reference (film, bind (&check_text, _1), _("it overlaps other text content; remove the other content."), why_not);
}

void
DCPContent::take_settings_from (shared_ptr<const Content> c)
{
	shared_ptr<const DCPContent> dc = dynamic_pointer_cast<const DCPContent> (c);
	if (!dc) {
		return;
	}

	_reference_video = dc->_reference_video;
	_reference_audio = dc->_reference_audio;
	for (int i = 0; i < TEXT_COUNT; ++i) {
		_reference_text[i] = dc->_reference_text[i];
	}
}

void
DCPContent::set_cpl (string id)
{
	ChangeSignaller<Content> cc (this, DCPContentProperty::CPL);

	{
		boost::mutex::scoped_lock lm (_mutex);
		_cpl = id;
	}
}

bool
DCPContent::kdm_timing_window_valid () const
{
	if (!_kdm) {
		return true;
	}

	dcp::LocalTime now;
	return _kdm->not_valid_before() < now && now < _kdm->not_valid_after();
}
