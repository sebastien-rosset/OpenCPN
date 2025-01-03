/***************************************************************************
 *   Copyright (C) 2024 by OpenCPN Development Team                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 **************************************************************************/

#include "mock_defs.h"

// Implementation of mock definitions if needed

wxEventType wxEVT_DOWNLOAD_EVENT = wxNewEventType();

// Functions with extern "C" DECL_EXP
extern "C" {

void SendPluginMessage(wxString message_id, wxString message_body) {}

wxWindow *GetOCPNCanvasWindow() { return nullptr; }

int GetChartbarHeight() { return 0; }

void GetCanvasPixLL(PlugIn_ViewPort *vp, wxPoint *pp, double lat, double lon) {}

int InsertPlugInToolSVG(wxString label, wxString SVGfile,
                        wxString SVGfileRollover, wxString SVGfileToggled,
                        wxItemKind kind, wxString shortHelp, wxString longHelp,
                        wxObject *clientData, int position, int tool_sel,
                        opencpn_plugin *pplugin) {
  return 0;
}

bool AddLocaleCatalog(wxString catalog) { return false; }

void CanvasJumpToPosition(wxWindow *canvas, double lat, double lon,
                          double scale) {}

void PositionBearingDistanceMercator_Plugin(double lat, double lon, double brg,
                                            double dist, double *dlat,
                                            double *dlon) {}

void DimeWindow(wxWindow *win) {}

wxFont *OCPNGetFont(wxString TextElement, int default_size) { return nullptr; }

void SetToolbarItemState(int item, bool toggle) {}

void SetCanvasContextMenuItemViz(int item, bool viz) {}

void DistanceBearingMercator_Plugin(double lat0, double lon0, double lat1,
                                    double lon1, double *brg, double *dist) {}

void GetCanvasLLPix(PlugIn_ViewPort *vp, wxPoint p, double *plat,
                    double *plon) {}

int AddCanvasContextMenuItem(wxMenuItem *pitem, opencpn_plugin *pplugin) {
  return 0;
}

bool GetGlobalColor(wxString colorName, wxColour *pcolour) { return false; }

void RequestRefresh(wxWindow *window) {}

wxFileConfig *GetOCPNConfigObject() { return nullptr; }

wxString *GetpSharedDataLocation() { return new wxString(); }

}  // extern "C"

// Functions with just extern DECL_EXP

wxWindow *GetCanvasUnderMouse() { return nullptr; }

wxString GetLocaleCanonicalName() { return wxEmptyString; }

_OCPN_DLStatus OCPN_downloadFileBackground(const wxString &url,
                                           const wxString &outputFile,
                                           wxEvtHandler *handler,
                                           long *handle) {
  return OCPN_DL_NO_ERROR;
}

int PlatformDirSelectorDialog(wxWindow *parent, wxString *file_spec,
                              wxString Title, wxString initDir) {
  return 0;
}

wxString toSDMM_PlugIn(int NEflag, double a, bool hi_precision) {
  return wxEmptyString;
}

wxBitmap GetBitmapFromSVGFile(wxString filename, unsigned int width,
                              unsigned int height) {
  return wxBitmap();
}

double PlugInGetDisplaySizeMM() { return 0.0; }

int OCPNMessageBox_PlugIn(wxWindow *parent, const wxString &message,
                          const wxString &caption, int style, int x, int y) {
  return 0;
}

int GetCanvasCount() { return 1; }

wxString GetActiveStyleName() { return wxEmptyString; }

wxWindow *GetCanvasByIndex(int canvasIndex) { return nullptr; }

void OCPN_cancelDownloadFileBackground(long handle) {}

double GetOCPNGUIToolScaleFactor_PlugIn() { return 1.0; }

wxFont GetOCPNGUIScaledFont_PlugIn(wxString item) { return wxFont(); }

double OCPN_GetWinDIPScaleFactor() { return 1.0; }
