/***************************************************************************
                          qsodatadialog.cpp  -  description
                             -------------------
    begin                : Sat Dec 7 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include "qsodatadialog.h"
#include <string>
#include <vector>
#include <algorithm>
#include "tqslvalidator.h"
#include "wx/valgen.h"
#include "wx/spinctrl.h"
#include "wx/statline.h"
#include "tqsllib.h"
#include "tqslexcept.h"

using namespace std;

#define TQSL_ID_LOW 6000

#ifdef __WIN32__
	#define TEXT_HEIGHT 24
	#define LABEL_HEIGHT 18
	#define TEXT_WIDTH 8
	#define TEXT_POINTS 10
	#define VSEP 3
	#define GEOM1 4
#else
	#define TEXT_HEIGHT 18
	#define LABEL_HEIGHT TEXT_HEIGHT
	#define TEXT_WIDTH 8
	#define TEXT_POINTS 12
	#define VSEP 4
	#define GEOM1 6
#endif

#define SKIP_HEIGHT (TEXT_HEIGHT+VSEP)

#define QD_CALL	TQSL_ID_LOW
#define QD_DATE	TQSL_ID_LOW+1
#define QD_TIME	TQSL_ID_LOW+2
#define QD_MODE	TQSL_ID_LOW+3
#define QD_BAND	TQSL_ID_LOW+4
#define QD_FREQ	TQSL_ID_LOW+5
#define QD_OK TQSL_ID_LOW+6
#define QD_CANCEL TQSL_ID_LOW+7
#define QD_RECNO TQSL_ID_LOW+8
#define QD_RECDOWN TQSL_ID_LOW+9
#define QD_RECUP TQSL_ID_LOW+10
#define QD_RECBOTTOM TQSL_ID_LOW+11
#define QD_RECTOP TQSL_ID_LOW+12
#define QD_RECNEW TQSL_ID_LOW+13
#define QD_RECDELETE TQSL_ID_LOW+14
#define QD_RECNOLABEL TQSL_ID_LOW+15

static void set_font(wxWindow *w, wxFont& font) {
#ifndef __WIN32__
	w->SetFont(font);
#endif
}

// Images for buttons.

#include "left.xpm"
#include "right.xpm"
#include "bottom.xpm"
#include "top.xpm"

class valid_list : public std::vector<wxString> {
public:
	valid_list() {}
	valid_list(const char **values, int nvalues);
	wxString *GetChoices() const;
};

valid_list::valid_list(const char **values, int nvalues) {
	while(nvalues--)
		push_back(*(values++));
}

wxString *
valid_list::GetChoices() const {
	wxString *ary = new wxString[size()];
	wxString *sit = ary;
	const_iterator it;
	for (it = begin(); it != end(); it++)
		*sit++ = *it;
	return ary;
}

static valid_list valid_modes;
static valid_list valid_bands;

static int
init_valid_lists() {
	if (valid_bands.size() > 0)
		return 0;
	if (tqsl_init())
		return 1;
	int count;
	if (tqsl_getNumMode(&count))
		return 1;
	const char *cp;
	for (int i = 0; i < count; i++) {
		if (tqsl_getMode(i, &cp, 0))
			return 1;
		valid_modes.push_back(cp);
	}
	if (tqsl_getNumBand(&count))
		return 1;
	for (int i = 0; i < count; i++) {
		if (tqsl_getBand(i, &cp, 0, 0, 0))
			return 1;
		valid_bands.push_back(cp);
	}
	return 0;
}

#define LABEL_WIDTH (22*TEXT_WIDTH)

BEGIN_EVENT_TABLE(QSODataDialog, wxDialog)
	EVT_BUTTON(QD_OK, QSODataDialog::OnOk)
	EVT_BUTTON(QD_CANCEL, QSODataDialog::OnCancel)
	EVT_BUTTON(QD_RECDOWN, QSODataDialog::OnRecDown)
	EVT_BUTTON(QD_RECUP, QSODataDialog::OnRecUp)
	EVT_BUTTON(QD_RECBOTTOM, QSODataDialog::OnRecBottom)
	EVT_BUTTON(QD_RECTOP, QSODataDialog::OnRecTop)
	EVT_BUTTON(QD_RECNEW, QSODataDialog::OnRecNew)
	EVT_BUTTON(QD_RECDELETE, QSODataDialog::OnRecDelete)
END_EVENT_TABLE()

QSODataDialog::QSODataDialog(wxWindow *parent, QSORecordList *reclist, wxWindowID id, const wxString& title)
	: wxDialog(parent, id, title), _reclist(reclist), _isend(false) {
	wxBoxSizer *topsizer = new wxBoxSizer(wxVERTICAL);
	wxFont font = GetFont();
	font.SetPointSize(TEXT_POINTS);
	set_font(this, font);

#define QD_MARGIN 3

	if (init_valid_lists())
		throw TQSLException(tqsl_getErrorString());
	// Call sign
	wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(new wxStaticText(this, -1, "Call Sign:", wxDefaultPosition,
		wxSize(LABEL_WIDTH,TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	sizer->Add(new wxTextCtrl (this, QD_CALL, "", wxDefaultPosition, wxSize(14*TEXT_WIDTH,TEXT_HEIGHT),
		0, wxTextValidator(wxFILTER_NONE, &rec._call)), 0, wxALL, QD_MARGIN);
	topsizer->Add(sizer, 0);
	// Date
	sizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(new wxStaticText(this, -1, "GMT Date (YYYY-MM-DD):", wxDefaultPosition,
		wxSize(LABEL_WIDTH,TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	sizer->Add(new wxTextCtrl (this, QD_DATE, "", wxDefaultPosition, wxSize(14*TEXT_WIDTH,TEXT_HEIGHT),
		0, TQSLDateValidator(&rec._date)), 0, wxALL, QD_MARGIN);
	topsizer->Add(sizer, 0);
	// Time
	sizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(new wxStaticText(this, -1, "GMT Time (HHMM):", wxDefaultPosition,
		wxSize(LABEL_WIDTH,TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	sizer->Add(new wxTextCtrl (this, QD_TIME, "", wxDefaultPosition, wxSize(14*TEXT_WIDTH,TEXT_HEIGHT),
		0, TQSLTimeValidator(&rec._time)), 0, wxALL, QD_MARGIN);
	topsizer->Add(sizer, 0);
	// Mode
	sizer = new wxBoxSizer(wxHORIZONTAL);
	wxString *choices = valid_modes.GetChoices();
	sizer->Add(new wxStaticText(this, -1, "Mode:", wxDefaultPosition,
		wxSize(LABEL_WIDTH,TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	sizer->Add(new wxChoice(this, QD_MODE, wxDefaultPosition, wxDefaultSize,
		valid_modes.size(), choices, 0, wxGenericValidator(&_mode)), 0, wxALL, QD_MARGIN);
	delete[] choices;
	topsizer->Add(sizer, 0);
	// Band
	sizer = new wxBoxSizer(wxHORIZONTAL);
	choices = valid_bands.GetChoices();
	sizer->Add(new wxStaticText(this, -1, "Band:", wxDefaultPosition,
		wxSize(LABEL_WIDTH,TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	sizer->Add(new wxChoice(this, QD_BAND, wxDefaultPosition, wxDefaultSize,
		valid_bands.size(), choices, 0, wxGenericValidator(&_band)), 0, wxALL, QD_MARGIN);
	delete[] choices;
	topsizer->Add(sizer, 0);
	// Frequency
	sizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(new wxStaticText(this, -1, "Frequency:", wxDefaultPosition,
		wxSize(LABEL_WIDTH,TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	sizer->Add(new wxTextCtrl (this, QD_FREQ, "", wxDefaultPosition, wxSize(14*TEXT_WIDTH,TEXT_HEIGHT),
		0, wxTextValidator(wxFILTER_NONE, &rec._freq)), 0, wxALL, QD_MARGIN);
	topsizer->Add(sizer, 0);

	if (_reclist != 0) {
		if (_reclist->empty())
			_reclist->push_back(QSORecord());
		topsizer->Add(new wxStaticLine(this, -1), 0, wxEXPAND|wxLEFT|wxRIGHT, 10);
		_recno_label_ctrl = new wxStaticText(this, QD_RECNOLABEL, "", wxDefaultPosition,
			wxSize(20*TEXT_WIDTH,TEXT_HEIGHT), wxST_NO_AUTORESIZE|wxALIGN_CENTER);
		topsizer->Add(_recno_label_ctrl, 0, wxALIGN_CENTER|wxALL, 5);
		_recno = 1;
		sizer = new wxBoxSizer(wxHORIZONTAL);
		_recbottom_ctrl = new wxBitmapButton(this, QD_RECBOTTOM, wxBitmap(bottom), wxDefaultPosition, wxSize(18, TEXT_HEIGHT)),
		sizer->Add(_recbottom_ctrl, 0, wxTOP|wxBOTTOM, 5);
		_recdown_ctrl = new wxBitmapButton(this, QD_RECDOWN, wxBitmap(left), wxDefaultPosition, wxSize(18, TEXT_HEIGHT));
		sizer->Add(_recdown_ctrl, 0, wxTOP|wxBOTTOM, 5);
		_recno_ctrl = new wxTextCtrl(this, QD_RECNO, "1", wxDefaultPosition,
			wxSize(4*TEXT_WIDTH,TEXT_HEIGHT));
		_recno_ctrl->Enable(FALSE);
		sizer->Add(_recno_ctrl, 0, wxALL, 5);
		_recup_ctrl = new wxBitmapButton(this, QD_RECUP, wxBitmap(right), wxDefaultPosition, wxSize(18, TEXT_HEIGHT));
		sizer->Add(_recup_ctrl, 0, wxTOP|wxBOTTOM, 5);
		_rectop_ctrl = new wxBitmapButton(this, QD_RECTOP, wxBitmap(top), wxDefaultPosition, wxSize(18, TEXT_HEIGHT)),
		sizer->Add(_rectop_ctrl, 0, wxTOP|wxBOTTOM, 5);
		if (_reclist->size() > 0)
			rec = *(_reclist->begin());
		topsizer->Add(sizer, 0, wxALIGN_CENTER);
		sizer = new wxBoxSizer(wxHORIZONTAL);
		sizer->Add(new wxButton(this, QD_RECNEW, "Add QSO"), 0, wxALL, 5);
		sizer->Add(new wxButton(this, QD_RECDELETE, "Delete"), 0, wxALL, 5);
		topsizer->Add(sizer, 0, wxALIGN_CENTER);
	}

	topsizer->Add(new wxStaticLine(this, -1), 0, wxEXPAND|wxLEFT|wxRIGHT, 10);
	sizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(new wxButton(this, QD_CANCEL, "Cancel"), 0, wxALL, 10);
	sizer->Add(new wxButton(this, QD_OK, "Ok"), 0, wxALL, 10);
	topsizer->Add(sizer, 0, wxALIGN_CENTER);

	UpdateControls();

	SetAutoLayout(TRUE);
	SetSizer(topsizer);
	topsizer->Fit(this);
	topsizer->SetSizeHints(this);

	CentreOnParent();
}

QSODataDialog::~QSODataDialog(){
}

bool
QSODataDialog::TransferDataFromWindow() {
	rec._call.Trim(FALSE).Trim(TRUE);
	if (!wxDialog::TransferDataFromWindow())
		return false;
	if (_mode < 0 || _mode >= (int)valid_modes.size())
		return false;
	rec._mode = valid_modes[_mode];
	if (_band < 0 || _band >= (int)valid_bands.size())
		return false;
	rec._band = valid_bands[_band];
	rec._freq.Trim(FALSE).Trim(TRUE);
	if (!_isend && rec._call == "") {
		wxMessageBox("Call Sign cannot be empty", "QSO Data Error",
			wxOK | wxICON_EXCLAMATION, this);
		return false;
	}
	if (_reclist != 0)
			(*_reclist)[_recno-1] = rec;
	return true;
}

bool
QSODataDialog::TransferDataToWindow() {
	valid_list::iterator it;
	if ((it = find(valid_modes.begin(), valid_modes.end(), rec._mode.Upper())) != valid_modes.end())
		_mode = distance(valid_modes.begin(), it);
	else
		wxLogWarning(wxString("QSO Data: Invalid Mode ignored - ") + rec._mode.Upper());
	if ((it = find(valid_bands.begin(), valid_bands.end(), rec._band.Upper())) != valid_bands.end())
		_band = distance(valid_bands.begin(), it);
	return wxDialog::TransferDataToWindow();
}

void
QSODataDialog::OnOk(wxCommandEvent&) {
	_isend = true;
	TransferDataFromWindow();
	_isend = false;
	if (rec._call == "" && _recno == (int)_reclist->size()) {
		_reclist->erase(_reclist->begin() + _recno - 1);
		EndModal(wxID_OK);
	} else if (Validate() && TransferDataFromWindow())
		EndModal(wxID_OK);
}

void
QSODataDialog::OnCancel(wxCommandEvent&) {
	EndModal(wxID_CANCEL);
}


void
QSODataDialog::SetRecno(int new_recno) {
	if (_reclist == 0 || new_recno < 1)
		return;
   	if (TransferDataFromWindow()) {
//   		(*_reclist)[_recno-1] = rec;
		if (new_recno > (int)_reclist->size()) {
			new_recno = _reclist->size() + 1;
			QSORecord blankrec;
			_reclist->push_back(blankrec);
		}
   		_recno = new_recno;
   		rec = (*_reclist)[_recno-1];
   		TransferDataToWindow();
   		UpdateControls();
   	}
}

void
QSODataDialog::OnRecDown(wxCommandEvent&) {
	SetRecno(_recno - 1);
}

void
QSODataDialog::OnRecUp(wxCommandEvent&) {
	SetRecno(_recno + 1);
}

void
QSODataDialog::OnRecBottom(wxCommandEvent&) {
	SetRecno(1);
}

void
QSODataDialog::OnRecTop(wxCommandEvent&) {
	if (_reclist == 0)
		return;
	SetRecno(_reclist->size());
}

void
QSODataDialog::OnRecNew(wxCommandEvent&) {
	if (_reclist == 0)
		return;
	SetRecno(_reclist->size()+1);
}

void
QSODataDialog::OnRecDelete(wxCommandEvent&) {
	if (_reclist == 0)
		return;
	_reclist->erase(_reclist->begin() + _recno - 1);
	if (_reclist->empty())
		_reclist->push_back(QSORecord());
	if (_recno > (int)_reclist->size())
		_recno = _reclist->size();
	rec = (*_reclist)[_recno-1];
	TransferDataToWindow();
	UpdateControls();
}

void
QSODataDialog::UpdateControls() {
	if (_reclist == 0)
		return;
	_recdown_ctrl->Enable(_recno > 1);
	_recbottom_ctrl->Enable(_recno > 1);
	_recup_ctrl->Enable(_recno < (int)_reclist->size());
	_rectop_ctrl->Enable(_recno < (int)_reclist->size());
   	_recno_ctrl->SetValue(wxString::Format("%d", _recno));
	_recno_label_ctrl->SetLabel(wxString::Format("%d QSO Record%s", _reclist->size(),
		(_reclist->size() == 1) ? "" : "s"));
}
