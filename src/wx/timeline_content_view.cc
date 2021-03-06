/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#include "timeline_content_view.h"
#include "timeline.h"
#include "wx_util.h"
#include "lib/content.h"
#include <wx/graphics.h>
#include <boost/foreach.hpp>

using std::list;
using boost::shared_ptr;

TimelineContentView::TimelineContentView (Timeline& tl, shared_ptr<Content> c)
	: TimelineView (tl)
	, _content (c)
	, _selected (false)
{
	_content_connection = c->Change.connect (bind (&TimelineContentView::content_change, this, _1, _3));
}

dcpomatic::Rect<int>
TimelineContentView::bbox () const
{
	DCPOMATIC_ASSERT (_track);

	shared_ptr<const Film> film = _timeline.film ();
	shared_ptr<const Content> content = _content.lock ();
	if (!film || !content) {
		return dcpomatic::Rect<int> ();
	}

	return dcpomatic::Rect<int> (
		time_x (content->position ()),
		y_pos (_track.get()),
		content->length_after_trim(film).seconds() * _timeline.pixels_per_second().get_value_or(0),
		_timeline.pixels_per_track()
		);
}

void
TimelineContentView::set_selected (bool s)
{
	_selected = s;
	force_redraw ();
}

bool
TimelineContentView::selected () const
{
	return _selected;
}

shared_ptr<Content>
TimelineContentView::content () const
{
	return _content.lock ();
}

void
TimelineContentView::set_track (int t)
{
	_track = t;
}

void
TimelineContentView::unset_track ()
{
	_track = boost::optional<int> ();
}

boost::optional<int>
TimelineContentView::track () const
{
	return _track;
}

void
TimelineContentView::do_paint (wxGraphicsContext* gc, list<dcpomatic::Rect<int> > overlaps)
{
	DCPOMATIC_ASSERT (_track);

	shared_ptr<const Film> film = _timeline.film ();
	shared_ptr<const Content> cont = content ();
	if (!film || !cont) {
		return;
	}

	DCPTime const position = cont->position ();
	DCPTime const len = cont->length_after_trim (film);

	wxColour selected (background_colour().Red() / 2, background_colour().Green() / 2, background_colour().Blue() / 2);

	gc->SetPen (*wxThePenList->FindOrCreatePen (foreground_colour(), 4, wxPENSTYLE_SOLID));
	if (_selected) {
		gc->SetBrush (*wxTheBrushList->FindOrCreateBrush (selected, wxBRUSHSTYLE_SOLID));
	} else {
		gc->SetBrush (*wxTheBrushList->FindOrCreateBrush (background_colour(), wxBRUSHSTYLE_SOLID));
	}

	/* Outline */
	wxGraphicsPath path = gc->CreatePath ();
	path.MoveToPoint    (time_x (position) + 2,	      y_pos (_track.get()) + 4);
	path.AddLineToPoint (time_x (position + len) - 1,     y_pos (_track.get()) + 4);
	path.AddLineToPoint (time_x (position + len) - 1,     y_pos (_track.get() + 1) - 4);
	path.AddLineToPoint (time_x (position) + 2,	      y_pos (_track.get() + 1) - 4);
	path.AddLineToPoint (time_x (position) + 2,	      y_pos (_track.get()) + 4);
	gc->StrokePath (path);
	gc->FillPath (path);

	/* Reel split points */
	gc->SetPen (*wxThePenList->FindOrCreatePen (foreground_colour(), 1, wxPENSTYLE_DOT));
	BOOST_FOREACH (DCPTime i, cont->reel_split_points(film)) {
		path = gc->CreatePath ();
		path.MoveToPoint (time_x (i), y_pos (_track.get()) + 4);
		path.AddLineToPoint (time_x (i), y_pos (_track.get() + 1) - 4);
		gc->StrokePath (path);
	}

	/* Overlaps */
	gc->SetBrush (*wxTheBrushList->FindOrCreateBrush (foreground_colour(), wxBRUSHSTYLE_CROSSDIAG_HATCH));
	for (list<dcpomatic::Rect<int> >::const_iterator i = overlaps.begin(); i != overlaps.end(); ++i) {
		gc->DrawRectangle (i->x, i->y + 4, i->width, i->height - 8);
	}

	/* Label text */
	wxString lab = label ();
	wxDouble lab_width;
	wxDouble lab_height;
	wxDouble lab_descent;
	wxDouble lab_leading;
	gc->SetFont (gc->CreateFont (*wxNORMAL_FONT, foreground_colour ()));
	gc->GetTextExtent (lab, &lab_width, &lab_height, &lab_descent, &lab_leading);
	gc->Clip (wxRegion (time_x (position), y_pos (_track.get()), len.seconds() * _timeline.pixels_per_second().get_value_or(0), _timeline.pixels_per_track()));
	gc->DrawText (lab, time_x (position) + 12, y_pos (_track.get() + 1) - lab_height - 4);
	gc->ResetClip ();
}

int
TimelineContentView::y_pos (int t) const
{
	return t * _timeline.pixels_per_track() + _timeline.tracks_y_offset();
}

void
TimelineContentView::content_change (ChangeType type, int p)
{
	if (type != CHANGE_TYPE_DONE) {
		return;
	}

	ensure_ui_thread ();

	if (p == ContentProperty::POSITION || p == ContentProperty::LENGTH) {
		force_redraw ();
	}
}

wxString
TimelineContentView::label () const
{
	return std_to_wx(content()->summary());
}
