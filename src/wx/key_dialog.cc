/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "key_dialog.h"
#include "wx_util.h"
#include "dcpomatic_button.h"
#include <iostream>

using std::cout;

KeyDialog::KeyDialog (wxWindow* parent, dcp::Key key)
	: TableDialog (parent, _("Key"), 3, 1, true)
{
	add (_("Key"), true);

        wxClientDC dc (parent);
        wxSize size = dc.GetTextExtent (wxT ("0123456790ABCDEF0123456790ABCDEF"));
        size.SetHeight (-1);

        wxTextValidator validator (wxFILTER_INCLUDE_CHAR_LIST);
        wxArrayString list;

        wxString n (wxT ("0123456789abcdefABCDEF"));
        for (size_t i = 0; i < n.Length(); ++i) {
                list.Add (n[i]);
        }

        validator.SetIncludes (list);

	_key = add (new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, size, 0, validator));
	_key->SetValue (std_to_wx (key.hex ()));
	_key->SetMaxLength (32);

	_random = add (new Button(this, _("Random")));

	_key->Bind (wxEVT_TEXT, boost::bind (&KeyDialog::key_changed, this));
	_random->Bind (wxEVT_BUTTON, boost::bind (&KeyDialog::random, this));

	layout ();
}

dcp::Key
KeyDialog::key () const
{
	return dcp::Key (wx_to_std (_key->GetValue ()));
}

void
KeyDialog::key_changed ()
{
	wxButton* ok = dynamic_cast<wxButton *> (FindWindowById (wxID_OK, this));
	ok->Enable (_key->GetValue().Length() == 32);
}

void
KeyDialog::random ()
{
	_key->SetValue (dcp::Key().hex ());
}
