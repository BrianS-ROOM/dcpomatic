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

#include "lib/content_text.h"
#include <wx/wx.h>
#include <wx/listctrl.h>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

class Decoder;
class FilmViewer;
class Film;

class TextView : public wxDialog
{
public:
	TextView (
		wxWindow *,
		boost::shared_ptr<Film>,
		boost::shared_ptr<Content> content,
		boost::shared_ptr<TextContent> caption,
		boost::shared_ptr<Decoder>,
		boost::weak_ptr<FilmViewer> viewer
		);

private:
	void data_start (ContentStringText cts);
	void data_stop (dcpomatic::ContentTime time);
	void subtitle_selected (wxListEvent &);

	wxListCtrl* _list;
	int _subs;
	boost::optional<FrameRateChange> _frc;
	boost::optional<int> _last_count;
	std::vector<dcpomatic::ContentTime> _start_times;
	boost::weak_ptr<Content> _content;
	boost::weak_ptr<FilmViewer> _film_viewer;
};
