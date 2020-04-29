/*
    Copyright (C) 2016-2020 Carl Hetherington <cth@carlh.net>

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

#include "playhead_to_timecode_dialog.h"

using namespace dcpomatic;

PlayheadToTimecodeDialog::PlayheadToTimecodeDialog (wxWindow* parent, dcpomatic::DCPTime time, int fps)
	: TableDialog (parent, _("Go to timecode"), 2, 1, true)
	, _fps (fps)
{
	add (_("Go to"), true);
	_timecode = add (new Timecode<DCPTime> (this, false));
	_timecode->set_focus ();
	_timecode->set_hint (time, fps);

	layout ();
}

DCPTime
PlayheadToTimecodeDialog::get () const
{
	return _timecode->get (_fps);
}
