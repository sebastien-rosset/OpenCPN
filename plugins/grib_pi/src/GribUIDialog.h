/***************************************************************************
 *   Copyright (C) 2010 by David S. Register                               *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 ***************************************************************************/
/**
 * \file
 * GRIB Weather Data Control Interface.
 *
 * This module provides the primary user interface controls for the GRIB plugin,
 * including:
 * - Timeline controls for navigating forecast times
 * - Layer controls for selecting visible weather parameters
 * - Display settings for customizing visualizations
 * - File management for loading and organizing GRIB data
 *
 * The interface is designed to give users complete control over how weather
 * data is displayed while maintaining efficiency for real-time navigation and
 * animation playback. Key features include:
 * - Temporal interpolation between forecast times
 * - Multi-file data management
 * - Customizable overlay settings
 * - Cursor tracking for data inspection
 * - Animation controls
 */
#ifndef __GRIBUICTRLBAR_H__
#define __GRIBUICTRLBAR_H__

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif  // precompiled headers
#include <wx/fileconf.h>
#include <wx/glcanvas.h>

#include "GribUIDialogBase.h"
#include "CursorData.h"
#include "GribSettingsDialog.h"
#include "GribRequestDialog.h"
#include "GribReader.h"
#include "GribRecordSet.h"
#include "IsoLine.h"
#include "GrabberWin.h"
#include "grib_layer_set.h"
#include "grib_file.h"
#include "grib_layer_set.h"
#include "grib_timeline_record_set.h"

#ifndef PI
#define PI 3.1415926535897931160E0 /* pi */
#endif

class GRIBUICtrlBar;
class GRIBUICData;
class GRIBRecord;
class GribRecordTree;
class GRIBOverlayFactory;
class GribRecordSet;
class GribRequestSetting;
class GribGrabberWin;
class GribSpacerWin;

class wxFileConfig;
class grib_pi;
class wxGraphicsContext;

enum ZoneSelection {
  AUTO_SELECTION,
  SAVED_SELECTION,
  START_SELECTION,
  DRAW_SELECTION,
  COMPLETE_SELECTION
};

/// @brief Structure used to store XyGrib configuration. It is used to
/// recover/store model and parameter choices from/to OpenCPN configuration
/// file.
typedef struct {
  int atmModelIndex;
  int waveModelIndex;
  int resolutionIndex;
  int durationIndex;
  int runIndex;
  int intervalIndex;
  bool wind;
  bool gust;
  bool pressure;
  bool temperature;
  bool cape;
  bool reflectivity;
  bool cloudCover;
  bool precipitation;
  bool waveHeight;
  bool windWaves;
} XyGribConfig_t;

//----------------------------------------------------------------------------------------------------------
//    GRIB CtrlBar Specification
//----------------------------------------------------------------------------------------------------------
class GRIBUICtrlBar : public GRIBUICtrlBarBase {
  friend class GribRequestSetting;

public:
  GRIBUICtrlBar(wxWindow *parent, wxWindowID id, const wxString &title,
                const wxPoint &pos, const wxSize &size, long style,
                grib_pi *ppi);
  ~GRIBUICtrlBar();

  void OpenFile(bool newestFile = false);

  void ContextMenuItemCallback(int id);
  void SetFactoryOptions();

  wxDateTime TimelineTime();

  void StopPlayBack();
  void TimelineChanged();
  void CreateActiveFileFromNames();
  void PopulateComboDataList();
  void ComputeBestForecastForNow();
  void SetViewPort(PlugIn_ViewPort *vp);
  void SetDataBackGroundColor();
  void SetTimeLineMax(bool SetValue);
  void SetCursorLatLon(double lat, double lon);
  void UpdateTrackingControl();
  void SetDialogsStyleSizePosition(bool force_recompute = false);
  void SetRequestBitmap(int type);
  void OnMouseEvent(wxMouseEvent &event);
  GRIBUICData *GetCDataDialog() { return m_gGRIBUICData; }
  bool InDataPlot(int id) {
    return id > wxID_ANY && id < (int)GribOverlaySettings::GEO_ALTITUDE;
  }
  void SetScaledBitmap(double factor);
  wxBitmap GetScaledBitmap(wxBitmap bitmap, const wxString svgFileName,
                           double scale_factor);
  void OpenFileFromJSON(wxString json);

  //

  wxWindow *pParent;
  /** Settings that control how GRIB data is displayed and overlaid. */
  GribOverlaySettings m_OverlaySettings;
  /** Current set of GRIB records for timeline playback. */
  GribTimelineRecordSet *m_pTimelineSet;

  /** Timer for controlling GRIB animation playback. */
  wxTimer m_tPlayStop;
  /** Plugin instance that owns this control bar. */
  grib_pi *pPlugIn;
  GribRequestSetting *pReq_Dialog;
  /** Layer-based GRIB files. */
  GRIBLayerSet m_LayerSet;
  bool m_bDataPlot[GribOverlaySettings::GEO_ALTITUDE];  // only for no altitude
                                                        // parameters
  bool m_CDataIsShown;
  int m_ZoneSelAllowed;
  int m_old_DialogStyle;
  double m_ScaledFactor;
  void DoZoomToCenter();
  const wxString GetGribDir() {
    if (m_grib_dir.IsEmpty() || !wxDirExists(m_grib_dir)) {
      m_grib_dir = GetpPrivateApplicationDataLocation()
                       ->Append(wxFileName::GetPathSeparator())
                       .Append("grib");

      if (!wxDirExists(m_grib_dir)) wxMkdir(m_grib_dir);

      wxString dir_spec;
      int response = PlatformDirSelectorDialog(
          this, &dir_spec, _("Choose GRIB File Directory"), m_grib_dir);

      if (response == wxID_OK) {
        m_grib_dir = (dir_spec);
      }
    }
    return m_grib_dir;
  }

  void GetProjectedLatLon(int &x, int &y);
  bool ProjectionEnabled() {
    if (m_ProjectBoatPanel)
      return m_ProjectBoatPanel->ProjectionEnabled();
    else
      return false;
  }
  double m_highlight_latmax;
  double m_highlight_lonmax;
  double m_highlight_latmin;
  double m_highlight_lonmin;
  /** Directory containing GRIB files. */
  wxString m_grib_dir;
  /** List of GRIB filenames being displayed. */
  wxArrayString m_file_names;

private:
  void OnClose(wxCloseEvent &event);
  void OnSize(wxSizeEvent &event);
  void OnPaint(wxPaintEvent &event);
  void OnSettings(wxCommandEvent &event);
  void OnPlayStop(wxCommandEvent &event);
  void OnPlayStopTimer(wxTimerEvent &event);
  void OnMove(wxMoveEvent &event);
  void OnMenuEvent(wxMenuEvent &event);
  void MenuAppend(wxMenu *menu, int id, wxString label, wxItemKind kind,
                  wxBitmap bitmap = wxNullBitmap, wxMenu *submenu = nullptr);
  void OnZoomToCenterClick(wxCommandEvent &event);
  void OnPrev(wxCommandEvent &event);
  void OnRecordForecast(wxCommandEvent &event) {
    StopPlayBack();
    m_InterpolateMode = false;
    m_pNowMode = false;
    TimelineChanged();
  }
  void OnNext(wxCommandEvent &event);
  void OnNow(wxCommandEvent &event) {
    StopPlayBack();
    ComputeBestForecastForNow();
  }
  void OnAltitude(wxCommandEvent &event);
  void OnOpenFile(wxCommandEvent &event);
  void OnRequest(wxCommandEvent &event);
  void OnCompositeDialog(wxCommandEvent &event);

  void OnTimeline(wxScrollEvent &event);
  void OnShowCursorData(wxCommandEvent &event);

  wxArrayString GetFilesInDirectory();
  void SetGribTimelineRecordSet(GribTimelineRecordSet *pTimelineSet);
  int GetNearestIndex(wxDateTime time, int model);
  int GetNearestValue(wxDateTime time, int model);
  wxDateTime GetNow();
  void RestoreSelectionString();
  void SaveSelectionString() {
    m_SelectionIsSaved = true;
    m_Selection_index = m_cRecordForecast->GetSelection();
    m_Selection_label = m_cRecordForecast->GetString(m_Selection_index);
  }

  //    Data
  CursorData *m_gCursorData;
  GribGrabberWin *m_gGrabber;
  GRIBUICData *m_gGRIBUICData;

  PlugIn_ViewPort *m_vp;
  int m_lastdatatype;

  int m_TimeLineHours;
  int m_FileIntervalIndex;
  bool m_InterpolateMode;
  bool m_pNowMode;
  bool m_HasAltitude;

  bool m_SelectionIsSaved;
  int m_Selection_index;
  wxString m_Selection_label;
  wxSize m_DialogsOffset;
  double m_projected_lat;
  double m_projected_lon;
  // XyGrib panel configuration
  XyGribConfig_t xyGribConfig;
  bool m_gtk_started;
};

//----------------------------------------------------------------------------------------------------------
//    GRIB CursorData Dialog Specification
//----------------------------------------------------------------------------------------------------------
class GRIBUICData : public GRIBUICDataBase {
public:
  GRIBUICData(GRIBUICtrlBar &parent);
  ~GRIBUICData() {}

  // GribGrabberWin      *m_gGrabber;
  GRIBUICtrlBar &m_gpparent;
  CursorData *m_gCursorData;

private:
  void OnMove(wxMoveEvent &event);
};

#endif
