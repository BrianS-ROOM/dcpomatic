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

#include "video_content_scale.h"
#include "video_content.h"
#include "ratio.h"
#include "util.h"
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>
#include <boost/optional.hpp>
#include <iostream>

#include "i18n.h"

using std::vector;
using std::string;
using std::min;
using std::cout;
using boost::shared_ptr;
using boost::optional;

vector<VideoContentScale> VideoContentScale::_scales;

VideoContentScale::VideoContentScale (Ratio const * r)
	: _ratio (r)
	, _scale (true)
{

}

VideoContentScale::VideoContentScale ()
	: _ratio (0)
	, _scale (false)
{

}

VideoContentScale::VideoContentScale (bool scale)
	: _ratio (0)
	, _scale (scale)
{

}

VideoContentScale::VideoContentScale (shared_ptr<cxml::Node> node)
	: _ratio (0)
	, _scale (true)
{
	optional<string> r = node->optional_string_child ("Ratio");
	if (r) {
		_ratio = Ratio::from_id (r.get ());
	} else {
		_scale = node->bool_child ("Scale");
	}
}

void
VideoContentScale::as_xml (xmlpp::Node* node) const
{
	if (_ratio) {
		node->add_child("Ratio")->add_child_text (_ratio->id ());
	} else {
		node->add_child("Scale")->add_child_text (_scale ? "1" : "0");
	}
}

string
VideoContentScale::id () const
{
	if (_ratio) {
		return _ratio->id ();
	}

	return (_scale ? "S1" : "S0");
}

string
VideoContentScale::name () const
{
	if (_ratio) {
		return _ratio->image_nickname ();
	}

	if (_scale) {
		return _("No stretch");
	}

	return _("No scale");
}

/** @param display_container Size of the container that we are displaying this content in.
 *  @param film_container The size of the film's image.
 *  @return Size, in pixels that the VideoContent's image should be scaled to (taking into account its pixel aspect ratio)
 */
dcp::Size
VideoContentScale::size (shared_ptr<const VideoContent> c, dcp::Size display_container, dcp::Size film_container) const
{
	/* Work out the size of the content if it were put inside film_container */

	dcp::Size video_size_after_crop = c->size_after_crop();
	video_size_after_crop.width *= c->sample_aspect_ratio().get_value_or(1);

	dcp::Size size;

	if (_ratio) {
		/* Stretch to fit the requested ratio */
		size = fit_ratio_within (_ratio->ratio (), film_container);
	} else if (_scale || video_size_after_crop.width > film_container.width || video_size_after_crop.height > film_container.height) {
		/* Scale, preserving aspect ratio; this is either if we have been asked to scale with no stretch
		   or if the unscaled content is too big for film_container.
		*/
		size = fit_ratio_within (video_size_after_crop.ratio(), film_container);
	} else {
		/* No stretch nor scale */
		size = video_size_after_crop;
	}

	/* Now scale it down if the display container is smaller than the film container */
	if (display_container != film_container) {
		float const scale = min (
			float (display_container.width) / film_container.width,
			float (display_container.height) / film_container.height
			);

		size.width = lrintf (size.width * scale);
		size.height = lrintf (size.height * scale);
	}

	return size;
}

void
VideoContentScale::setup_scales ()
{
	vector<Ratio const *> ratios = Ratio::all ();
	for (vector<Ratio const *>::const_iterator i = ratios.begin(); i != ratios.end(); ++i) {
		_scales.push_back (VideoContentScale (*i));
	}

	_scales.push_back (VideoContentScale (true));
	_scales.push_back (VideoContentScale (false));
}

bool
operator== (VideoContentScale const & a, VideoContentScale const & b)
{
	return (a.ratio() == b.ratio() && a.scale() == b.scale());
}

bool
operator!= (VideoContentScale const & a, VideoContentScale const & b)
{
	return (a.ratio() != b.ratio() || a.scale() != b.scale());
}
