/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  mux_dialog.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <root@sirelvis.de>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief muxing dialog
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include "wx/wxprec.h"

#include "wx/wx.h"
#include "wx/clipbrd.h"
#include "wx/file.h"
#include "wx/confbase.h"
#include "wx/fileconf.h"
#include "wx/notebook.h"
#include "wx/listctrl.h"
#include "wx/statusbr.h"
#include "wx/statline.h"

#include "mmg.h"
#include "common.h"

mux_dialog::mux_dialog(wxWindow *parent):
  wxDialog(parent, -1, "mkvmerge is running", wxDefaultPosition,
#ifdef SYS_WINDOWS
           wxSize(500, 560),
#else
           wxSize(500, 520),
#endif
           wxCAPTION) {
  char c;
  long value;
  wxString line, tmp;
  wxInputStream *out;

  c = 0;
  new wxStaticBox(this, -1, _("Status and progress"), wxPoint(10, 5),
                  wxSize(480, 70));
  st_label = new wxStaticText(this, -1, _(""), wxPoint(15, 25),
                              wxSize(450, -1));
  g_progress = new wxGauge(this, -1, 100, wxPoint(120, 50), wxSize(250, 15));

  new wxStaticBox(this, -1, _("Output"), wxPoint(10, 80), wxSize(480, 400));
  new wxStaticText(this, -1, _("mkvmerge output:"), wxPoint(15, 100));
  tc_output =
    new wxTextCtrl(this, -1, _(""), wxPoint(15, 120), wxSize(455, 140),
                   wxTE_READONLY | wxTE_LINEWRAP | wxTE_MULTILINE);
  new wxStaticText(this, -1, _("Warnings:"), wxPoint(15, 270));
  tc_warnings =
    new wxTextCtrl(this, -1, _(""), wxPoint(15, 290), wxSize(455, 80),
                   wxTE_READONLY | wxTE_LINEWRAP | wxTE_MULTILINE);
  new wxStaticText(this, -1, _("Errors:"), wxPoint(15, 380));
  tc_errors =
    new wxTextCtrl(this, -1, _(""), wxPoint(15, 400), wxSize(455, 70),
                   wxTE_READONLY | wxTE_LINEWRAP | wxTE_MULTILINE);

  b_ok = new wxButton(this, ID_B_MUX_OK, _("Ok"), wxPoint(1, 500));
  b_save_log =
    new wxButton(this, ID_B_MUX_SAVELOG, _("Save log"), wxPoint(1, 500));
  b_abort =
    new wxButton(this, ID_B_MUX_ABORT, _("Abort"), wxPoint(1, 500));
  b_ok->Move(wxPoint((int)(250 - 2 * b_save_log->GetSize().GetWidth()),
                     500 - b_ok->GetSize().GetHeight() / 2));
  b_abort->Move(wxPoint((int)(250 - 0.5 * b_save_log->GetSize().GetWidth()),
                        500 - b_ok->GetSize().GetHeight() / 2));
  b_save_log->Move(wxPoint((int)(250 + b_save_log->GetSize().GetWidth()),
                           500 - b_ok->GetSize().GetHeight() / 2));
  b_ok->Enable(false);

  update_window("Muxing in progress.");
  Show(true);

  process = new mux_process(this);

#ifdef SYS_UNIX
  int i;
  wxArrayString &arg_list =
    static_cast<mmg_dialog *>(parent)->get_command_line_args();
  char **args = (char **)safemalloc((arg_list.Count() + 1) * sizeof(char *));
  for (i = 0; i < arg_list.Count(); i++)
    args[i] = safestrdup(arg_list[i].c_str());
  args[i] = NULL;

  pid = wxExecute(args, wxEXEC_ASYNC, process);
  for (i = 0; i < arg_list.Count(); i++)
    safefree(args[i]);
  safefree(args);
#else
  pid = wxExecute(static_cast<mmg_dialog *>(parent)->get_command_line(),
                  wxEXEC_ASYNC, process);
#endif
  out = process->GetInputStream();

  line = "";
  log = "";
  while (1) {
    if (!out->Eof()) {
      c = out->GetC();
      if ((unsigned char)c != 0xff)
        log.Append(c);
    }
    while (app->Pending())
      app->Dispatch();

    if ((c == '\n') || (c == '\r') || out->Eof()) {
      if (line.Find("Warning:") == 0)
        tc_warnings->AppendText(line + "\n");
      else if (line.Find("Error:") == 0)
        tc_errors->AppendText(line + "\n");
      else if (line.Find("progress") == 0) {
        if (line.Find("%)") != 0) {
          line.Remove(line.Find("%)"));
          tmp = line.AfterLast('(');
          tmp.ToLong(&value);
          if ((value >= 0) && (value <= 100))
            update_gauge(value);
        }
      } else {
        if (line.Find("Pass 1:") == 0)
          st_label->SetLabel("Muxing in progress (pass 1 of 2: finding "
                             "points for splitting).");
        else if (line.Find("Pass 2:") == 0)
          st_label->SetLabel("Muxing in progress (pass 2 of 2: muxing "
                             "and splitting).");
        if (line.Length() > 0)
          tc_output->AppendText(line + "\n");
      }
      line = "";
    } else if ((unsigned char)c != 0xff)
      line.Append(c);

    if (out->Eof())
      break;
  }

  b_ok->Enable(true);
  b_abort->Enable(false);
  b_ok->SetFocus();
  ShowModal();
}

mux_dialog::~mux_dialog() {
  delete process;
}

void mux_dialog::update_window(wxString text) {
  st_label->SetLabel(text);
}

void mux_dialog::update_gauge(long value) {
  g_progress->SetValue(value);
}

void mux_dialog::on_ok(wxCommandEvent &evt) {
  Close(true);
}

void mux_dialog::on_save_log(wxCommandEvent &evt) {
  wxFile *file;
  wxString s;
  wxFileDialog dlg(NULL, "Choose an output file", last_open_dir, "",
                   _T("Log files (*.txt)|*.txt|" ALLFILES),
                   wxSAVE | wxOVERWRITE_PROMPT);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    file = new wxFile(dlg.GetPath(), wxFile::write);
    s = log + "\n";
    file->Write(s);
    delete file;
  }
}

void mux_dialog::on_abort(wxCommandEvent &evt) {
#if defined(SYS_WINDOWS)
  wxKill(pid, wxSIGKILL);
#else
  wxKill(pid, wxSIGTERM);
#endif
}

mux_process::mux_process(mux_dialog *mdlg):
  wxProcess(wxPROCESS_REDIRECT), 
  dlg(mdlg) {
}

void mux_process::OnTerminate(int pid, int status) {
  wxString s;

  s.Printf("mkvmerge %s with a return code of %d. %s\n",
           (status != 0) && (status != 1) ? "FAILED" : "finished", status,
           status == 0 ? "Everything went fine." :
           status == 1 ? "There were warnings"
#if defined(SYS_WINDOWS)
           ", or the process was terminated."
#else
           "."
#endif
           : status == 2 ? "There were ERRORs." : "");
  dlg->update_window(s);
}

IMPLEMENT_CLASS(mux_dialog, wxDialog);
BEGIN_EVENT_TABLE(mux_dialog, wxDialog)
  EVT_BUTTON(ID_B_MUX_OK, mux_dialog::on_ok)
  EVT_BUTTON(ID_B_MUX_SAVELOG, mux_dialog::on_save_log)
  EVT_BUTTON(ID_B_MUX_ABORT, mux_dialog::on_abort)
END_EVENT_TABLE();
