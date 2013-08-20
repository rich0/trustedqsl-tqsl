/***************************************************************************
                          loadcertwiz.cpp  -  description
                             -------------------
    begin                : Wed Aug 6 2003
    copyright            : (C) 2003 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include "loadcertwiz.h"
#include "getpassword.h"
#include "tqslctrls.h"
#include "tqsllib.h"
#include "tqslerrno.h"
#include "tqslapp.h"
#include "tqsltrace.h"

wxString
notifyData::Message() const {
	
	if (diagFile) {
		fprintf(diagFile, "%s\n"
			"Root Certificates:\t\tLoaded: %d  Duplicate: %d  Error: %d\n"
			"CA Certificates:\t\tLoaded: %d  Duplicate: %d  Error: %d\n"
			"Callsign Certificates:\tLoaded: %d  Duplicate: %d  Error: %d\n"
			"Private Keys:\t\t\tLoaded: %d  Duplicate: %d  Error: %d\n"
			"Configuration Data:\tLoaded: %d  Duplicate: %d  Error: %d\n",
			_S(status),
			root.loaded, root.duplicate, root.error,
			ca.loaded, ca.duplicate, ca.error,
			user.loaded, user.duplicate, user.error,
			pkey.loaded, pkey.duplicate, pkey.error,
			config.loaded, config.duplicate, config.error);
	}
	if (status.IsEmpty())
		return wxString(wxT("\nImport completed successfully"));
	return status;
}

int
notifyImport(int type, const char *message, void *data) {
	tqslTrace("notifyImport", "type=%d, message=%s, data=0x%lx", type, message, (void *)data);
	if (TQSL_CERT_CB_RESULT_TYPE(type) == TQSL_CERT_CB_PROMPT) {
		const char *nametype = 0;
		const char *configkey = 0;
		bool default_prompt = false;
		switch (TQSL_CERT_CB_CERT_TYPE(type)) {
			case TQSL_CERT_CB_ROOT:
				nametype = "Trusted root";
				configkey = "NotifyRoot";
				//default_prompt = true;
				break;
			case TQSL_CERT_CB_CA:
				nametype = "Certificate Authority";
				configkey = "NotifyCA";
				break;
			case TQSL_CERT_CB_USER:
				nametype = "Callsign";
				configkey = "NotifyUser";
				break;
		}
		if (!nametype)
			return 0;
		wxConfig *config = (wxConfig *)wxConfig::Get();
		bool b;
		config->Read(wxString(configkey, wxConvLocal), &b, default_prompt);
		if (!b)
			return 0;
		wxString s(wxT("Okay to install "));
		s = s + wxString(nametype, wxConvLocal) + wxT(" certificate?\n\n") + wxString(message, wxConvLocal);
		if (wxMessageBox(s, wxT("Install Certificate"), wxYES_NO) == wxYES)
			return 0;
		return 1;
	} // end TQSL_CERT_CB_PROMPT
	if (TQSL_CERT_CB_CALL_TYPE(type) == TQSL_CERT_CB_RESULT && data) {
		// Keep count
		notifyData *nd = (notifyData *)data;
		notifyData::counts *counts = 0;
		switch (TQSL_CERT_CB_CERT_TYPE(type)) {
			case TQSL_CERT_CB_ROOT:
				counts = &(nd->root);
				break;
			case TQSL_CERT_CB_CA:
				counts = &(nd->ca);
				break;
			case TQSL_CERT_CB_USER:
				counts = &(nd->user);
				break;
			case TQSL_CERT_CB_PKEY:
				counts = &(nd->pkey);
				break;
			case TQSL_CERT_CB_CONFIG:
				counts = &(nd->config);
				break;
		}
		if (counts) {
			switch (TQSL_CERT_CB_RESULT_TYPE(type)) {
				case TQSL_CERT_CB_DUPLICATE:
					if (TQSL_CERT_CB_CERT_TYPE(type) == TQSL_CERT_CB_USER) {
						if (message) {
							nd->status = nd->status + wxString(message, wxConvLocal) + wxT("\n");
						} else {
							nd->status = nd->status + wxT("This callsign certificate is already installed");
						}
					}
					counts->duplicate++;
					break;
				case TQSL_CERT_CB_ERROR:
					if (message)
						nd->status = nd->status + wxString(message, wxConvLocal) + wxT("\n");
					counts->error++;
					// wxMessageBox(wxString(message, wxConvLocal), wxT("Error"));
					break;
				case TQSL_CERT_CB_LOADED:
					if (TQSL_CERT_CB_CERT_TYPE(type) == TQSL_CERT_CB_USER)
						nd->status = nd->status + wxString("Callsign Certificate ", wxConvLocal) +
							wxString(message, wxConvLocal) + wxT("\n");
					counts->loaded++;
					break;
			}
		}
	}
	if (TQSL_CERT_CB_RESULT_TYPE(type) == TQSL_CERT_CB_ERROR)
		return 1;	// Errors get posted later
	if (TQSL_CERT_CB_CALL_TYPE(type) == TQSL_CERT_CB_RESULT
		|| TQSL_CERT_CB_RESULT_TYPE(type) == TQSL_CERT_CB_DUPLICATE) {
//		wxMessageBox(message, "Certificate Notice");
		return 0;
	}
	return 1;
}

static wxHtmlHelpController *pw_help = 0;
static wxString pw_helpfile;

static int
GetNewPassword(char *buf, int bufsiz, void *) {
	tqslTrace("GetNewPassword");
	GetNewPasswordDialog dial(0, wxT("New Password"),
		wxT("Enter a password for this callsign certificate.\n\n")
		wxT("If you are using a computer system that is\n")
		wxT("shared with others, you should specify a\n")
		wxT("password to protect this certificate. However, if\n")
		wxT("you are using a computer in a private residence\n")
		wxT("no password need be specified.\n\n")
		wxT("This password will have to be entered each time\n")
		wxT("you use this callsign certificate for signing or\n")
		wxT("when saving the key.\n\n")
		wxT("Leave the password blank and click 'Ok' unless you want\n")
		wxT("to use a password.\n\n"), true, pw_help, pw_helpfile);
	if (dial.ShowModal() == wxID_OK) {
		strncpy(buf, dial.Password().mb_str(), bufsiz);
		buf[bufsiz-1] = 0;
		return 0;
	}
	return 1;
}

static void
export_new_cert(ExtWizard *_parent, const char *filename) {
	tqslTrace("export_new_cert", "_parent=0x%lx, filename=%s", _parent, filename);
	long newserial;
	if (!tqsl_getSerialFromTQSLFile(filename, &newserial)) {

		MyFrame *frame = (MyFrame *)(((LoadCertWiz *)_parent)->Parent());
		TQ_WXCOOKIE cookie;
		int nproviders = frame->cert_tree->GetNumIssuers();		// Number of certificate issuers - currently 1
		wxTreeItemId root = frame->cert_tree->GetRootItem();
		wxTreeItemId item, prov;
		if (nproviders > 1) {
			prov = frame->cert_tree->GetFirstChild(root, cookie); // First child is the providers
			item = frame->cert_tree->GetFirstChild(prov, cookie);// Then it's certs
		} else {
			item = frame->cert_tree->GetFirstChild(root, cookie); // First child is the certs
		}

		while (item.IsOk()) {
			tQSL_Cert cert;
			CertTreeItemData *id = frame->cert_tree->GetItemData(item);
			if (id && (cert = id->getCert())) {
				long serial;
				if (!tqsl_getCertificateSerial(cert, &serial)) {
					if (serial == newserial) {
						wxCommandEvent e;
	    					if (wxMessageBox(
wxT("You will not be able to use this tq6 file to recover your\n")
wxT("callsign certificate if it gets lost. For security purposes, you should\n")
wxT("back up your certificate on removable media for safe-keeping.\n\n")
wxT("Would you like to back up your callsign certificate now?"), wxT("Warning"), wxYES_NO|wxICON_QUESTION, _parent) == wxNO) {
							return;
						}
						frame->cert_tree->SelectItem(item);
						frame->OnCertExport(e);
						break;
					}
				}
			}
			if (nproviders > 1) {
				item = frame->cert_tree->GetNextChild(prov, cookie);
			} else {
				item = frame->cert_tree->GetNextChild(root, cookie);
			}
		}
	}
}

LoadCertWiz::LoadCertWiz(wxWindow *parent, wxHtmlHelpController *help, const wxString& title) :
	ExtWizard(parent, help, title), _nd(0) {
	tqslTrace("LoadCertWiz::LoadCertWiz", "parent=0x%lx, title=%s", (void *)parent, _S(title));

	LCW_FinalPage *final = new LCW_FinalPage(this);
	LCW_P12PasswordPage *p12pw = new LCW_P12PasswordPage(this);
	wxWizardPageSimple::Chain(p12pw, final);
	_first = p12pw;
	_parent = parent;
	_final = final;
	_p12pw = p12pw;

	wxString ext(wxT("p12"));
	wxString wild(wxT("Callsign Certificate container files (*.p12)|*.p12|Certificate Request response files (*.tq6)|*.tq6"));
	wild += wxT("|All files (*.*)|*.*");

	wxString path = wxConfig::Get()->Read(wxT("CertFilePath"), wxT(""));
	wxString filename = wxFileSelector(wxT("Select Certificate File"), path,
		wxT(""), ext, wild, wxOPEN|wxFILE_MUST_EXIST);
	if (filename == wxT("")) {
		// Cancelled!
		_first = 0;
	} else {
		ResetNotifyData();
		wxString path, basename, ext;
		wxSplitPath(filename, &path, &basename, &ext);
		
		wxConfig::Get()->Write(wxT("CertFilePath"), path);
		if (ext.MakeLower() == wxT("tq6")) {
			_first = _final;
			_final->SetPrev(0);
			if (tqsl_importTQSLFile(filename.mb_str(), notifyImport, GetNotifyData()))
				wxMessageBox(wxString(tqsl_getErrorString(), wxConvLocal), wxT("Error"));
			else {
				wxConfig::Get()->Write(wxT("RequestPending"), wxT(""));
				export_new_cert(this, filename.mb_str());
			}
		} else {
			// First try with no password
			if (!tqsl_importPKCS12File(filename.mb_str(), "", 0, GetNewPassword, notifyImport, GetNotifyData())) {
				_first = _final;
				_final->SetPrev(0);
			} else {
				_first=_p12pw;
				_p12pw->SetPrev(0);
				p12pw->SetFilename(filename);
			}
		}
	}
	AdjustSize();
	CenterOnParent();
}

LCW_FinalPage::LCW_FinalPage(LoadCertWiz *parent) : LCW_Page(parent) {
	tqslTrace("LCW_FinalPage::LCW_FinalPage", "parent=0x%lx", (void *)parent);
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *st = new wxStaticText(this, -1, wxT("Loading complete"));
	sizer->Add(st, 0, wxALL, 10);
	wxSize tsize = getTextSize(this);
	tc_status = new wxTextCtrl(this, -1, wxT(""),wxDefaultPosition, tsize.Scale(40, 16), wxTE_MULTILINE|wxTE_READONLY);
	sizer->Add(tc_status, 1, wxALL|wxEXPAND, 10);
	AdjustPage(sizer);
}

void
LCW_FinalPage::refresh() {
	tqslTrace("LCW_FinalPage::refresh");
	const notifyData *nd = ((LoadCertWiz *)_parent)->GetNotifyData();
	if (nd)
		tc_status->SetValue(nd->Message());
	else
		tc_status->SetValue(wxT("No status information available"));
}

LCW_P12PasswordPage::LCW_P12PasswordPage(LoadCertWiz *parent) : LCW_Page(parent) {
	tqslTrace("LCW_P12PasswordPage::LCW_P12PasswordPage", "parent=0x%lx", (void *)parent);
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *st = new wxStaticText(this, -1, wxT("Enter the password to unlock the .p12 file:"));
	sizer->Add(st, 0, wxALL, 10);

	_pwin = new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
	sizer->Add(_pwin, 0, wxLEFT|wxRIGHT|wxEXPAND, 10);
	tc_status = new wxStaticText(this, -1, wxT(""));
	sizer->Add(tc_status, 0, wxALL, 10);

	AdjustPage(sizer, wxT("lcf1.htm"));
}

bool
LCW_P12PasswordPage::TransferDataFromWindow() {
	tqslTrace("LCW_P12PasswordPage::TransferDataFromWindow");
	wxString _pw = _pwin->GetValue();
	pw_help = Parent()->GetHelp();
	pw_helpfile = wxT("lcf2.htm");
	if (tqsl_importPKCS12File(_filename.mb_str(), _pw.mb_str(), 0, GetNewPassword, notifyImport,
		((LoadCertWiz *)_parent)->GetNotifyData())) {
		if (tQSL_Error == TQSL_PASSWORD_ERROR) {
			tc_status->SetLabel(wxT("Password error"));
			return false;
		} else
			wxMessageBox(wxString(tqsl_getErrorString(), wxConvLocal), wxT("Error"));
	}
	tc_status->SetLabel(wxT(""));
	return true;
}
