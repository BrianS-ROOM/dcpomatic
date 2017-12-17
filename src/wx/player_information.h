/*
    Copyright (C) 2017 Carl Hetherington <cth@carlh.net>

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

#include <wx/wx.h>
#include <boost/scoped_ptr.hpp>

class FilmViewer;

class PlayerInformation : public wxPanel
{
public:
	PlayerInformation (wxWindow* parent, FilmViewer* viewer);

	void triggered_update ();

private:

	void periodic_update ();

	FilmViewer* _viewer;
	wxSizer* _sizer;
	wxStaticText** _dcp;
	wxStaticText* _dropped;
	wxStaticText* _decode_resolution;
	boost::scoped_ptr<wxTimer> _timer;
};
