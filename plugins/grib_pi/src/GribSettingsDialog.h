/***************************************************************************
 *   Copyright (C) 2014 by Sean D'Epagnier                                 *
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
 * GRIB Display Settings Configuration Interface.
 *
 * Provides comprehensive configuration options for customizing GRIB weather
 * data visualization, including:
 * - Units and display formats for each meteorological parameter
 * - Overlay styling (transparency, colors, arrows, isobars)
 * - Animation and playback settings
 * - Data interpolation options
 * - Layout and GUI preferences
 *
 * The settings system supports:
 * - Multiple meteorological parameters (wind, pressure, waves, etc.)
 * - Different visualization methods (arrows, particles, contours)
 * - Unit conversion and calibration
 * - Persistent storage of user preferences
 * - JSON import/export of configurations
 *
 * Key features:
 * - Per-parameter customization of display elements
 * - Flexible unit systems (metric, imperial, nautical)
 * - Real-time preview of setting changes
 * - Profile management for different use cases
 */
#ifndef __GRIBSETTINGSDIALOG_H__
#define __GRIBSETTINGSDIALOG_H__

#include "GribUIDialogBase.h"

#include "jsonval.h"

//----------------------------------------------------------------------------------------------------------
//    Grib OverlaySettings Specification
//----------------------------------------------------------------------------------------------------------
struct GribOverlaySettings {
  static wxString NameFromIndex(int index);

  void Read();
  void Write();
  void SaveSettingGroups(wxFileConfig* pConf, int settings, int group);

  wxString SettingsToJSON(wxString json);
  bool JSONToSettings(wxString json);
  bool UpdateJSONval(wxJSONValue& v, int settings, int group);

  double CalibrationOffset(int settings);
  double CalibrationFactor(int settings, double input, bool reverse = false);
  double CalibrateValue(int settings, double input) {
    return (input + CalibrationOffset(settings)) *
           CalibrationFactor(settings, input);
  }
  int GetMinFromIndex(int index);
  wxString GetAltitudeFromIndex(int index, int unit);
  double GetmstobfFactor(double input);
  double GetbftomsFactor(double input);
  wxString GetUnitSymbol(int settings);
  double GetMin(int settings);
  double GetMax(int settings);
  // playback options
  bool m_bInterpolate;
  bool m_bLoopMode;
  int m_LoopStartPoint;
  int m_SlicesPerUpdate;
  int m_UpdatesPerSecond;
  // display
  int m_iOverlayTransparency;
  // gui
  int m_iCtrlandDataStyle;
  wxString m_iCtrlBarCtrlVisible[2];

  enum SettingsType {
    WIND,
    WIND_GUST,
    PRESSURE,
    WAVE,
    CURRENT,
    PRECIPITATION,
    CLOUD,
    AIR_TEMPERATURE,
    SEA_TEMPERATURE,
    CAPE,
    COMP_REFL,
    GEO_ALTITUDE,
    REL_HUMIDITY,
    SETTINGS_COUNT
  };
  enum Units0 { KNOTS, M_S, MPH, KPH, BFS };
  enum Units1 { MILLIBARS, MMHG, INHG };
  enum Units2 { METERS, FEET };
  enum Units3 { CELCIUS, FAHRENHEIT };
  enum Units4 { MILLIMETERS, INCHES };
  enum Units5 { PERCENTAGE };
  enum Units6 { JPKG };
  enum Units7 { DBZ };

  struct OverlayDataSettings {
    /** Units to display values in (varies by data type - e.g., knots, m/s,
     * etc.). */
    int m_Units;
    /** Whether to show wind barbs or current arrows. */
    bool m_bBarbedArrows;
    /** Visibility threshold for barbed arrows (independent of data plot
     * display). */
    bool m_iBarbedVisibility;
    /** Color scheme for barbed arrows (0: Constant, 1: Wind speed colors). */
    int m_iBarbedColour;
    /** Use fixed spacing between barbed arrows instead of dynamic spacing. */
    bool m_bBarbArrFixSpac;
    /**
     * Spacing between barbed arrows in pixels (when fixed spacing is enabled).
     *
     * The minimum spacing ensures that no two arrows are closer than minspace
     * pixels on screen, where minspace is the larger of:
     * - The user-specified spacing from settings.
     * - 120% of the wind arrow size (to prevent arrows from overlapping).
     */
    int m_iBarbArrSpacing;
    /** Whether to display isobars (lines of equal pressure, height, etc.). */
    bool m_bIsoBars;
    /** Show abbreviated pressure numbers on isobars (e.g., "04" for 1004 hPa).
     */
    bool m_bAbbrIsoBarsNumbers;
    /** Visibility threshold for isobars (independent of data plot display). */
    bool m_iIsoBarVisibility;
    /** Value spacing between adjacent isobars (e.g., 4 hPa between lines). */
    double m_iIsoBarSpacing;
    /** Whether to show directional arrows for waves, wind, current. */
    bool m_bDirectionArrows;
    /** Arrow style (0: Single, 1: Double, 2: Size varies with magnitude). */
    int m_iDirectionArrowForm;
    /** Use fixed spacing between direction arrows instead of dynamic spacing.
     */
    bool m_bDirArrFixSpac;
    /** Arrow size in pixels (when not using magnitude-dependent sizing). */
    int m_iDirectionArrowSize;
    /** Spacing between direction arrows in pixels (when fixed spacing is
     * enabled). */
    int m_iDirArrSpacing;
    /** Whether to display the color-coded overlay map. */
    bool m_bOverlayMap;
    /** Color scheme for overlay map (index into predefined color tables). */
    int m_iOverlayMapColors;
    /** Color scheme for overlay map (index into predefined color tables). */
    bool m_bNumbers;
    /** Use fixed spacing between numbers instead of dynamic spacing. */
    bool m_bNumFixSpac;
    /** Spacing between numbers in pixels (when fixed spacing is enabled). */
    int m_iNumbersSpacing;
    /** Whether to display particle animation for wind/current flow. */
    bool m_bParticles;
    /** Number of particles per unit area (higher = denser animation). */
    double m_dParticleDensity;

  } Settings[SETTINGS_COUNT];
};

class GRIBUICtrlBar;

class GribSettingsDialog : public GribSettingsDialogBase {
public:
  GribSettingsDialog(GRIBUICtrlBar& parent, GribOverlaySettings& extSettings,
                     int& lastdatatype, int fileIntervalIndex);
  void WriteSettings();

  void SetSettingsDialogSize();
  void SaveLastPage();
  int GetPageIndex() { return m_SetBookpageIndex; }

private:
  void SetDataTypeSettings(int settings);
  void ReadDataTypeSettings(int settings);
  void PopulateUnits(int settings);
  void ShowFittingSettings(int settings);
  void ShowSettings(int params, bool show = true);
  void OnDataTypeChoice(wxCommandEvent& event);
  void OnUnitChange(wxCommandEvent& event);
  void OnTransparencyChange(wxScrollEvent& event);
  void OnApply(wxCommandEvent& event);
  void OnIntepolateChange(wxCommandEvent& event);
  void OnSpacingModeChange(wxCommandEvent& event);
  void OnPageChange(wxNotebookEvent& event);
  void OnCtrlandDataStyleChanged(wxCommandEvent& event);

  GRIBUICtrlBar& m_parent;

  GribOverlaySettings m_Settings, &m_extSettings;
  int& m_lastdatatype;
  int m_SetBookpageIndex;
};

#endif
