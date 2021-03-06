/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#include "verify_dcp_dialog.h"
#include "wx_util.h"
#include "lib/verify_dcp_job.h"
#include <dcp/verify.h>
#include <wx/richtext/richtextctrl.h>
#include <boost/foreach.hpp>

using std::list;
using boost::shared_ptr;

VerifyDCPDialog::VerifyDCPDialog (wxWindow* parent, shared_ptr<VerifyDCPJob> job)
	: wxDialog (parent, wxID_ANY, _("DCP verification"))
{
	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);
	_text = new wxRichTextCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (400, 300), wxRE_READONLY);
	sizer->Add (_text, 1, wxEXPAND | wxALL, 6);

	wxStdDialogButtonSizer* buttons = CreateStdDialogButtonSizer (0);
	sizer->Add (CreateSeparatedSizer(buttons), wxSizerFlags().Expand().DoubleBorder());
	buttons->SetAffirmativeButton (new wxButton (this, wxID_OK));
	buttons->Realize ();

	SetSizer (sizer);
	sizer->Layout ();
	sizer->SetSizeHints (this);

	_text->GetCaret()->Hide ();

	if (job->finished_ok() && job->notes().empty()) {
		_text->BeginStandardBullet (N_("standard/circle"), 1, 50);
		_text->WriteText (_("DCP validates OK."));
		_text->EndStandardBullet ();
		return;
	}

	/* We might have an error that did not come from dcp::verify; report it if so */
	if (job->finished_in_error() && job->error_summary() != "") {
		_text->BeginSymbolBullet (N_("!"), 1, 50);
		_text->WriteText(std_to_wx(job->error_summary()));
		_text->Newline();
	}

	BOOST_FOREACH (dcp::VerificationNote i, job->notes()) {
		switch (i.type()) {
		case dcp::VerificationNote::VERIFY_WARNING:
			_text->BeginStandardBullet (N_("standard/diamond"), 1, 50);
			break;
		case dcp::VerificationNote::VERIFY_ERROR:
			_text->BeginSymbolBullet (N_("!"), 1, 50);
			break;
		}

		wxString text;
		switch (i.code()) {
		case dcp::VerificationNote::GENERAL_READ:
			text = std_to_wx(*i.note());
			break;
		case dcp::VerificationNote::CPL_HASH_INCORRECT:
			text = _("The hash of the CPL in the PKL does not agree with the CPL file.  This probably means that the CPL file is corrupt.");
			break;
		case dcp::VerificationNote::INVALID_PICTURE_FRAME_RATE:
			text = _("The picture in a reel has an invalid frame rate");
			break;
		case dcp::VerificationNote::PICTURE_HASH_INCORRECT:
			text = wxString::Format(
				_("The hash of the picture asset %s does not agree with the PKL file.  This probably means that the asset file is corrupt."),
				std_to_wx(i.file()->filename().string()).data()
				);
			break;
		case dcp::VerificationNote::PKL_CPL_PICTURE_HASHES_DISAGREE:
			text = _("The PKL and CPL hashes disagree for a picture asset.");
			break;
		case dcp::VerificationNote::SOUND_HASH_INCORRECT:
			text = wxString::Format(
				_("The hash of the sound asset %s does not agree with the PKL file.  This probably means that the asset file is corrupt."),
				std_to_wx(i.file()->filename().string()).data()
				);
			break;
		case dcp::VerificationNote::PKL_CPL_SOUND_HASHES_DISAGREE:
			text = _("The PKL and CPL hashes disagree for a sound asset.");
			break;
		}

		_text->WriteText (text);
		_text->Newline ();

		switch (i.type()) {
		case dcp::VerificationNote::VERIFY_WARNING:
			_text->EndStandardBullet ();
			break;
		case dcp::VerificationNote::VERIFY_ERROR:
			_text->EndSymbolBullet ();
			break;
		}
	}
}
