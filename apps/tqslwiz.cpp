/***************************************************************************
                          tqslwiz.cpp  -  description
                             -------------------
    begin                : Tue Nov 5 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

using namespace std;

#include "tqslwiz.h"
#include "wxutil.h"

BEGIN_EVENT_TABLE(TQSLWizard, wxWizard)
	EVT_WIZARD_PAGE_CHANGED(-1, TQSLWizard::OnPageChanged)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(TQSLWizCertPage, TQSLWizPage)
	EVT_COMBOBOX(-1, TQSLWizCertPage::OnComboBoxEvent)
	EVT_SIZE(TQSLWizCertPage::OnSize)
END_EVENT_TABLE()

void
TQSLWizard::OnPageChanged(wxWizardEvent& ev) {
	((TQSLWizPage *)GetCurrentPage())->SetFocus();
	ExtWizard::OnPageChanged(ev);
}

TQSLWizard::TQSLWizard(tQSL_Location locp, wxWindow *parent, wxHtmlHelpController *help,
	const wxString& title) :
	ExtWizard(parent, help, title), loc(locp), _curpage(-1) {

	char buf[256];
	if (!tqsl_getStationLocationCaptureName(locp, buf, sizeof buf)) {
		wxString s(buf);
		SetLocationName(s);
	}
	tqsl_setStationLocationCapturePage(locp, 1);
}

TQSLWizPage *
TQSLWizard::GetPage(bool final) {
	int page_num;
	page_changing = false;
	if (final)
		page_num = 0;
	else if (tqsl_getStationLocationCapturePage(loc, &page_num))
		return 0;
	if (_pages[page_num]) {
		if (page_num == 0)
			((TQSLWizFinalPage *)_pages[0])->prev = GetCurrentTQSLPage();
		return _pages[page_num];
	}
	if (page_num == 0)
		_pages[page_num] = new TQSLWizFinalPage(this, loc, GetCurrentTQSLPage());
	else
		_pages[page_num] = new TQSLWizCertPage(this, loc);
	if (page_num == 0)
		((TQSLWizFinalPage *)_pages[0])->prev = GetCurrentTQSLPage();
	return _pages[page_num];
}

void TQSLWizCertPage::OnSize(wxSizeEvent& ev) {
	TQSLWizPage::OnSize(ev);
	UpdateFields();
}


TQSLWizPage *
TQSLWizCertPage::GetPrev() const {
	int rval;
	if (tqsl_hasPrevStationLocationCapture(loc, &rval) || !rval) {
		GetParent()->page_changing = false;
		return 0;
	}
	if ((TQSLWizard *)GetParent()->page_changing) {
		tqsl_prevStationLocationCapture(loc);
		return GetParent()->GetPage();
	}
	return GetParent()->GetPage();
}

TQSLWizPage *
TQSLWizCertPage::GetNext() const {
	TQSLWizPage *newp;
	bool final = false;
	if (GetParent()->page_changing) {
		int rval;
		if (!tqsl_hasNextStationLocationCapture(loc, &rval) && rval) {
			tqsl_nextStationLocationCapture(loc);
		} else
			final = true;
	}
	newp = GetParent()->GetPage(final);
	return newp;
}

void
TQSLWizCertPage::UpdateFields(int noupdate_field) {
	wxSize text_size = getTextSize(this);

	if (noupdate_field >= 0)
		tqsl_updateStationLocationCapture(loc);
	for (int i = noupdate_field+1; i < (int)controls.size(); i++) {
		int changed;
		tqsl_getLocationFieldChanged(loc, i, &changed);
		if (noupdate_field >= 0 && !changed)
			continue;
		int in_type;
		tqsl_getLocationFieldInputType(loc, i, &in_type);
		if (in_type == TQSL_LOCATION_FIELD_DDLIST || in_type == TQSL_LOCATION_FIELD_LIST) {
			// Update this list
			int text_width = 0;
			char gabbi_name[40];
			tqsl_getLocationFieldDataGABBI(loc, i, gabbi_name, sizeof gabbi_name);
			int selected;
			tqsl_getLocationFieldIndex(loc, i, &selected);
			int new_sel = 0;
			wxString old_sel = ((wxComboBox *)controls[i])->GetStringSelection();
			((wxComboBox *)controls[i])->Clear();
			int nitems;
			tqsl_getNumLocationFieldListItems(loc, i, &nitems);
			for (int j = 0; j < nitems && j < 2000; j++) {
				char item[80];
				tqsl_getLocationFieldListItem(loc, i, j, item, sizeof(item));
				wxString item_text(item);
				((wxComboBox *)controls[i])->Append(item_text);
				if (!strcmp(item_text, old_sel.c_str()))
					new_sel = j;
				wxCoord w, h;
				((wxComboBox *)controls[i])->GetTextExtent(item_text, &w, &h);
				if (w > text_width)
					text_width = w;
			}
			if (text_width > 0) {
				int w, h;
				((wxComboBox *)controls[i])->GetSize(&w, &h);
				((wxComboBox *)controls[i])->SetSize(text_width + text_size.GetWidth()*4, h);
			}
			if (noupdate_field < 0)
				new_sel = selected;
			((wxComboBox *)controls[i])->SetSelection(new_sel);
			tqsl_setLocationFieldIndex(loc, i, new_sel);
			if (noupdate_field >= 0)
				tqsl_updateStationLocationCapture(loc);
			((wxComboBox *)controls[i])->Enable(nitems > 1);
		} else if (in_type == TQSL_LOCATION_FIELD_TEXT) {
			int len;
			tqsl_getLocationFieldDataLength(loc, i, &len);
			int w, h;
			((wxTextCtrl *)controls[i])->GetSize(&w, &h);
			((wxTextCtrl *)controls[i])->SetSize((len+1)*text_size.GetWidth(), h);
			if (noupdate_field < 0) {
				char buf[256];
				tqsl_getLocationFieldCharData(loc, i, buf, sizeof buf);
				((wxTextCtrl *)controls[i])->SetValue(buf);
			}
		}
	}
}

void
TQSLWizCertPage::OnComboBoxEvent(wxCommandEvent& event) {
	int control_idx = event.GetId() - TQSL_ID_LOW;
	if (control_idx < 0 || control_idx >= (int)controls.size())
		return;
	int in_type;
	tqsl_getLocationFieldInputType(loc, control_idx, &in_type);
	switch (in_type) {
		case TQSL_LOCATION_FIELD_DDLIST:
		case TQSL_LOCATION_FIELD_LIST:
			tqsl_setLocationFieldIndex(loc, control_idx, event.m_commandInt);
			UpdateFields(control_idx);
			break;
	}
}

TQSLWizCertPage::TQSLWizCertPage(TQSLWizard *parent, tQSL_Location locp)
	: TQSLWizPage(parent, locp) {
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	int control_width = getTextSize(this).GetWidth() * 20;
	
	tqsl_getStationLocationCapturePage(loc, &loc_page);
	wxScreenDC sdc;
	int label_w = 0;
	int numf;
	tqsl_getNumLocationField(loc, &numf);
	for (int i = 0; i < numf; i++) {
		wxCoord w, h;
		char label[256];
		tqsl_getLocationFieldDataLabel(loc, i, label, sizeof label);
		sdc.GetTextExtent(label, &w, &h);
		if (w > label_w)
			label_w = w;
	}
	wxCoord max_label_w, max_label_h;
	// Measure width of longest expected label string
	sdc.GetTextExtent("Longest label expected", &max_label_w, &max_label_h);
	for (int i = 0; i < numf; i++) {
		wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);
		char label[256];
		tqsl_getLocationFieldDataLabel(loc, i, label, sizeof label);
		hsizer->Add(new wxStaticText(this, -1, label, wxDefaultPosition,
			wxSize(label_w, -1), wxALIGN_RIGHT|wxST_NO_AUTORESIZE), 0, wxTOP, 5);
		wxWindow *control_p = 0;
		char gabbi_name[256];
		tqsl_getLocationFieldDataGABBI(loc, i, gabbi_name, sizeof gabbi_name);
		int in_type;
		tqsl_getLocationFieldInputType(loc, i, &in_type);
		switch(in_type) {
			case TQSL_LOCATION_FIELD_DDLIST:
			case TQSL_LOCATION_FIELD_LIST:
				control_p = new wxComboBox(this, TQSL_ID_LOW+i, "", wxDefaultPosition, wxSize(control_width, -1),
					0, 0, wxCB_DROPDOWN|wxCB_READONLY);
				break;
			case TQSL_LOCATION_FIELD_TEXT:
				control_p = new wxTextCtrl(this, TQSL_ID_LOW+i, "", wxDefaultPosition, wxSize(control_width, -1));
				break;
		}
		controls.push_back(control_p);
		hsizer->Add(control_p, 0, wxLEFT|wxTOP, 5);
		sizer->Add(hsizer, 0, wxLEFT|wxRIGHT, 10);
	}

	UpdateFields();
	AdjustPage(sizer, "stnloc1.htm");
}

TQSLWizCertPage::~TQSLWizCertPage() {
}

bool
TQSLWizCertPage::TransferDataFromWindow() {
	GetParent()->page_changing = true;
	tqsl_setStationLocationCapturePage(loc, loc_page);
	for (int i = 0; i < (int)controls.size(); i++) {
		int in_type;
		tqsl_getLocationFieldInputType(loc, i, &in_type);
		switch(in_type) {
			case TQSL_LOCATION_FIELD_DDLIST:
			case TQSL_LOCATION_FIELD_LIST:
				break;
			case TQSL_LOCATION_FIELD_TEXT:
				tqsl_setLocationFieldCharData(loc, i, ((wxTextCtrl *)controls[i])->GetValue().c_str());
				break;
		}
	}
	return true;
}

BEGIN_EVENT_TABLE(TQSLWizFinalPage, TQSLWizPage)
	EVT_LISTBOX(TQSL_ID_LOW, TQSLWizFinalPage::OnListbox)
	EVT_LISTBOX_DCLICK(TQSL_ID_LOW, TQSLWizFinalPage::OnListbox)
	EVT_TEXT(TQSL_ID_LOW+1, TQSLWizFinalPage::check_valid)
END_EVENT_TABLE()

void
TQSLWizFinalPage::OnListbox(wxCommandEvent &) {
	if (namelist->GetSelection() >= 0) {
		const char *cp = (const char *)(namelist->GetClientData(namelist->GetSelection()));
		if (cp)
			newname->SetValue(cp);
	}
}

TQSLWizFinalPage::TQSLWizFinalPage(TQSLWizard *parent, tQSL_Location locp, TQSLWizPage *i_prev) :
	TQSLWizPage(parent, locp), prev(i_prev) {
	wxSize text_size = getTextSize(this);
	int control_width = text_size.GetWidth()*30;
	
	int y = text_size.GetHeight();
	sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *st = new wxStaticText(this, -1, "Station Data input complete");
	sizer->Add(st, 0, wxALIGN_CENTER|wxTOP, 10);
	st = new wxStaticText(this, -1, "Select or enter name of this station location");
	sizer->Add(st, 0, wxALIGN_CENTER|wxBOTTOM, 10);

	// Title
	sizer->Add(st, 0, wxALIGN_CENTER|wxBOTTOM, 10);
	// List of existing location names
	namelist = new wxListBox(this, TQSL_ID_LOW, wxDefaultPosition, wxSize(control_width, text_size.GetHeight()*10),
		0, 0, wxLB_SINGLE|wxLB_HSCROLL|wxLB_NEEDED_SB);
	sizer->Add(namelist, 0, wxEXPAND);
	int n;
	tqsl_getNumStationLocations(locp, &n);
	for (int i = 0; i < n; i++) {
		char buf[256];
		tqsl_getStationLocationName(loc, i, buf, sizeof buf);
		item_data.push_back(wxString(buf));
		char cbuf[256];
		tqsl_getStationLocationCallSign(loc, i, cbuf, sizeof cbuf);
		wxString s(buf);
		s += (wxString(" (") + wxString(cbuf) + wxString(")"));
		namelist->Append(s, (void *)item_data.back().c_str());
	}
	if (namelist->Number() > 0)
		namelist->SetSelection(0, FALSE);
	// New name
	st = new wxStaticText(this, -1, "Station Location Name");
	sizer->Add(st, 0, wxALIGN_LEFT|wxTOP, 10);
	newname = new wxTextCtrl(this, TQSL_ID_LOW+1, "", wxPoint(0, y), wxSize(control_width, -1));
	sizer->Add(newname, 0, wxEXPAND);
	newname->SetValue(parent->GetLocationName());
	AdjustPage(sizer, "stnloc2.htm");
}

bool
TQSLWizFinalPage::TransferDataFromWindow() {
	if (validate())	// Must be a "back"
		return true;
	wxString s = newname->GetValue();
	((TQSLWizard *)GetParent())->SetLocationName(s);
	return true;
}

const char *
TQSLWizFinalPage::validate() {
	wxString val = newname->GetValue();
	const char *errmsg = 0;
	val.Trim();
	if (val == "")
		errmsg = "Station name must be provided";
	return errmsg;
}