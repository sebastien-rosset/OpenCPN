/***************************************************************************
 *   Copyright (C) 2014 by David S. Register                               *
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
 * GRIB Data Visualization and Rendering Factory.
 *
 * Provides comprehensive visualization capabilities for GRIB weather data in
 * OpenCPN, including:
 * - Wind barbs and particle animations
 * - Pressure isobars and directional arrows
 * - Color-coded overlay maps for various parameters
 * - Numerical data displays and labels
 *
 * The factory manages both OpenGL and bitmap-based rendering paths, handles
 * resource allocation, and provides efficient caching of rendered elements.
 * It serves as the central hub for converting raw GRIB data into meaningful
 * visual representations for mariners.
 */
#ifndef _GRIBOVERLAYFACTORY_H_
#define _GRIBOVERLAYFACTORY_H_

#include <map>

#include <wx/geometry.h>

#include "pi_gl.h"

#include "pi_ocpndc.h"
#include "pi_TexFont.h"

/**
 * Container for rendered GRIB data visualizations in texture or bitmap form.
 *
 * This class manages the rendered representation of GRIB weather data,
 * supporting both OpenGL texture-based rendering and bitmap-based rendering. It
 * handles resource allocation and cleanup for both rendering paths.
 */
class GribOverlay {
public:
  GribOverlay(void) {
    m_iTexture = 0;
    m_pDCBitmap = nullptr, m_pRGBA = nullptr;
  }

  ~GribOverlay(void) {
#ifdef ocpnUSE_GL
    if (m_iTexture) {
      glDeleteTextures(1, &m_iTexture);
    }
#endif
    delete m_pDCBitmap, delete[] m_pRGBA;
  }

  unsigned int m_iTexture, m_iTextureDim[2]; /* opengl mode */
  unsigned int m_iTexDataDim[2];

  wxBitmap *m_pDCBitmap; /* dc mode */
  unsigned char *m_pRGBA;

  int m_width;
  int m_height;

  double m_dwidth, m_dheight;
};

#define MAX_PARTICLE_HISTORY 8
#include <vector>
#include <list>
/**
 * Individual particle for wind/current animation.
 *
 * Represents a single particle in the animation system with position history
 * and rendering attributes.
 */
struct Particle {
  /** Duration this particle should exist in animation cycles. */
  int m_Duration;

  // history is a ringbuffer. because so many particles are
  // used, it is a slight optimization over std::list
  int m_HistoryPos, m_HistorySize, m_Run;
  struct ParticleNode {
    float m_Pos[2];
    float m_Screen[2];
    wxUint8 m_Color[3];
  } m_History[MAX_PARTICLE_HISTORY];
};

/**
 * Manager for particle animation system.
 *
 * Handles collections of particles and their rendering data arrays.
 */
struct ParticleMap {
public:
  ParticleMap(int settings)
      : m_Setting(settings),
        history_size(0),
        array_size(0),
        color_array(nullptr),
        vertex_array(nullptr),
        color_float_array(nullptr) {
    // XXX should be done in default PlugIn_ViewPort CTOR
    last_viewport.bValid = false;
  }

  ~ParticleMap() {
    delete[] color_array;
    delete[] vertex_array;
    delete[] color_float_array;
  }

  std::vector<Particle> m_Particles;

  // particles are rebuilt whenever any of these fields change
  time_t m_Reference_Time;
  int m_Setting;
  int history_size;

  unsigned int array_size;
  unsigned char *color_array;
  float *vertex_array;
  float *color_float_array;

  PlugIn_ViewPort last_viewport;
};

class LineBuffer {
public:
  LineBuffer() {
    count = 0;
    lines = nullptr;
  }
  ~LineBuffer() { delete[] lines; }

  void pushLine(float x0, float y0, float x1, float y1);
  void pushPetiteBarbule(int b, int l);
  void pushGrandeBarbule(int b, int l);
  void pushTriangle(int b, int l);
  void Finalize();

  int count;
  float *lines;

private:
  std::list<float> buffer;
};

class GRIBUICtrlBar;
class GribRecord;
class GribTimelineRecordSet;
class GRIBLayer;
class GRIBLayerSet;

/**
 * Factory class for creating and managing GRIB data visualizations.
 *
 * This class is responsible for rendering all GRIB weather data visualizations
 * in OpenCPN. It handles multiple visualization types including wind barbs,
 * isobars, particles, directional arrows, and numeric overlays.
 */
class GRIBOverlayFactory : public wxEvtHandler {
public:
  GRIBOverlayFactory(GRIBUICtrlBar &dlg);
  ~GRIBOverlayFactory();

  void SetSettings(bool hiDefGraphics, bool GradualColors,
                   bool BarbedArrowHead = true) {
    m_hiDefGraphics = hiDefGraphics;
    m_bGradualColors = GradualColors;
    m_bDrawBarbedArrowHead = BarbedArrowHead;
    ClearCachedData();
  }
  void SetMessageFont();
  void SetMessage(wxString message) { m_Message = message; }
  void SetTimeZone(int TimeZone) { m_TimeZone = TimeZone; }
  void SetParentSize(int w, int h) {
    m_ParentSize.SetWidth(w);
    m_ParentSize.SetHeight(h);
  }

  void SetGribLayerSet(GRIBLayerSet *layerSet);

  bool RenderGribOverlay(wxDC &dc, PlugIn_ViewPort *vp);
  bool RenderGLGribOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp);

  void Reset();
  void ClearCachedData(void);
  void ClearCachedLabel(void) { m_labelCache.clear(); }
  void ClearParticles() {
    delete m_ParticleMap;
    m_ParticleMap = nullptr;
  }

  /** The GRIB layer manager. */
  GRIBLayerSet *m_pGribLayerSet;

  void DrawMessageZoomOut(PlugIn_ViewPort *vp);
  void GetGraphicColor(int settings, double val, unsigned char &r,
                       unsigned char &g, unsigned char &b);
  wxColour GetGraphicColor(int settings, double val);

  wxSize m_ParentSize;

  pi_ocpnDC *m_oDC;

private:
  void InitColorsTable();

  /**
   * Maps UI settings identifiers to GRIB record indices for data access.
   *
   * This function translates between the UI-level settings enumeration and the
   * internal GRIB record indices used to access weather data. It handles both
   * scalar quantities (single index) and vector quantities (requiring two
   * components).
   *
   * For vector quantities like wind and current:
   * - idx receives the X/U component index
   * - idy receives the Y/V component index
   *
   * For polar quantities like waves:
   * - idx receives the magnitude index (e.g., wave height)
   * - idy receives the direction index
   * - polar is set to true
   *
   * For scalar quantities (pressure, temperature, etc.):
   * - Only idx is set
   * - idy remains -1
   * - polar remains false
   *
   * Most parameters are only available at surface level (m_Altitude == 0).
   * Wind data is unique in being available at multiple altitude levels, with
   * the indices offset by m_Altitude.
   *
   * @param i        Input settings identifier from GribOverlaySettings
   * @param idx      Output primary data index (X component or scalar value)
   * @param idy      Output secondary data index (Y component or direction)
   * @param polar    Output flag indicating if data is in polar coordinates or
   * cartesian coordinates.
   *
   * @note If m_Altitude > 0 for parameters other than wind, no indices are
   *       assigned (remain -1) since the data is only available at surface.
   */
  void SettingsIdToGribId(int i, int &idx, int &idy, bool &polar);
  bool DoRenderGribOverlay(PlugIn_ViewPort *vp);
  /**
   * Renders wind barbs showing both wind direction and speed.
   *
   * Creates standard meteorological wind barbs that indicate:
   * - Wind direction with the barb's orientation.
   * - Wind speed using standardized feather notation:
   *   * Short barb: 5 knots
   *   * Long barb: 10 knots
   *   * Triangular flag: 50 knots
   *   * Combinations show total wind speed
   *
   * Wind barbs can be placed using either:
   * - Fixed spacing: Regular grid with configurable spacing.
   * - Minimum spacing: Dynamic placement with guaranteed minimum separation.
   *
   * Features:
   * - Optional arrowhead on the barb shaft.
   * - Southern hemisphere wind barbs drawn with reversed feather orientation.
   * - Color coding based on wind speed ranges or fixed color.
   * - Anti-aliased rendering in OpenGL mode.
   *
   * @param settings Wind data settings (type must be WIND)
   * @param vp Current viewport parameters
   */
  void RenderGribBarbedArrows(int config, PlugIn_ViewPort *vp);
  /**
   * Renders isobars (lines of equal value) for a GRIB parameter.
   *
   * Calculates and renders isolines at specified intervals for meteorological
   * parameters like pressure, wind, temperature, etc. These lines connect
   * points of equal value across the chart.
   *
   * @param config Settings index indicating which parameter type to render
   * (e.g. pressure, temperature)
   * @param pGR Array of GRIB records to use for isobar calculation
   * @param pIsobarArray Array to store calculated isobar line segments. Each
   * element is a collection of points defining an isoline path. The array is
   * populated by this function and used for actual rendering.
   * @param vp Current viewport information including boundaries and scale
   */
  void RenderGribIsobar(int config, GribRecord **pGR,
                        wxArrayPtrVoid **pIsobarArray, PlugIn_ViewPort *vp);
  /**
   * Renders directional arrows for vector GRIB data like current or wave
   * direction.
   *
   * Creates arrows showing direction of phenomena like currents or waves.
   * Arrows can be:
   * - Single arrows of fixed or variable width
   * - Double arrows (two parallel lines with arrowhead)
   * - Optionally scaled based on magnitude
   *
   * Arrow placement uses either:
   * - Fixed spacing: Regular grid with configurable spacing between arrows
   * - Minimum spacing: Dynamic placement ensuring minimum distance between
   * arrows
   *
   * In minimum spacing mode, arrows follow the GRIB data grid while maintaining
   * minimum separation. This provides more natural data representation while
   * avoiding visual clutter.
   *
   * Arrows are drawn using either:
   * - DC mode: Standard bitmap drawing
   * - OpenGL mode: Hardware accelerated line rendering with anti-aliasing
   *
   * @param settings GRIB data type identifier (current, waves etc.)
   * @param pGR Array of GRIB records containing vector components
   * @param vp Current viewport parameters
   */
  void RenderGribDirectionArrows(int config, GribRecord **pGR,
                                 PlugIn_ViewPort *vp);
  /**
   * Renders color-coded overlay map of GRIB data.
   *
   * Generates bitmap/texture visualization using:
   * - OpenGL texture for hardware rendering
   * - wxBitmap for software rendering
   * - Bilinear interpolation between grid points
   * - Configurable transparency and color scales
   * - Masking for undefined grid values
   *
   * @param settings Overlay type (wind, pressure, etc.)
   * @param pGR Array of GRIB records
   * @param vp Viewport parameters
   */
  void RenderGribOverlayMap(int config, GribRecord **pGR, PlugIn_ViewPort *vp);
  /**
   * Renders numerical values from GRIB data as text labels on the chart.
   *
   * This function creates and draws text labels showing numerical values from
   * GRIB data, such as wind speed, pressure, temperature etc. Labels can be
   * drawn with either fixed or minimum spacing between them.
   *
   * For fixed spacing:
   * - Labels are drawn in a regular grid pattern with configurable spacing
   * - Each grid point shows the interpolated value at that location
   * - Grid covers the visible chart area
   *
   * For minimum spacing:
   * - Labels are placed with at least the minimum configured distance between
   * them
   * - Placement follows the underlying GRIB data grid points
   * - Avoids visual clutter while maintaining data visibility
   *
   * The labels are color-coded based on the value ranges and use alpha
   * transparency. In OpenGL mode, labels are rendered using texture fonts for
   * better performance.
   *
   * @param settings GRIB data type identifier (wind, pressure, waves etc.)
   * @param pGR Array of GRIB records containing the data
   * @param vp Current viewport parameters
   */
  void RenderGribNumbers(int config, GribRecord **pGR, PlugIn_ViewPort *vp);
  /**
   * Renders flow particles to visualize wind or current movement.
   *
   * Particles provide an animated visualization of flow fields (wind or
   * current) by simulating the movement of particles through the vector field.
   * Each particle:
   * - Maintains a history trail showing its recent path
   * - Updates position based on interpolated vector data
   * - Adjusts color based on vector magnitude
   *
   * @param settings Index into GribOverlaySettings for display parameters
   * @param vp Current viewport parameters
   */
  void RenderGribParticles(int settings, PlugIn_ViewPort *vp);
  void DrawLineBuffer(LineBuffer &buffer);
  void OnParticleTimer(wxTimerEvent &event);

  wxString GetRefString(GribRecord *rec, int map);
  void DrawMessageWindow(wxString msg, int x, int y, wxFont *mfont);

  void DrawProjectedPosition(int x, int y);

  void drawDoubleArrow(int x, int y, double ang, wxColour arrowColor,
                       int arrowWidth, int arrowSizeIdx, double scale);
  void drawSingleArrow(int x, int y, double ang, wxColour arrowColor,
                       int arrowWidth, int arrowSizeIdx, double scale);
  void drawWindArrowWithBarbs(int settings, int x, int y, double vkn,
                              double ang, bool south, wxColour arrowColor,
                              double rotate_angle);
  void drawLineBuffer(LineBuffer &buffer, int x, int y, double ang,
                      double scale, bool south = false, bool head = true);

  void DrawNumbers(wxPoint p, double value, int settings, wxColour back_color);
  void FillGrid(GribRecord *pGR);

  wxString getLabelString(double value, int settings);
  wxImage &getLabel(double value, int settings, wxColour back_colour);

#ifdef ocpnUSE_GL
  /**
   * Main OpenGL texture rendering method for GRIB data overlay.
   *
   * Handles full-viewport rendering by splitting into grid cells and
   * performing per-cell texture mapping with proper projection and distortion.
   *
   * @param pGO Overlay object containing texture data
   * @param pGR Source GRIB record for grid coordinates
   * @param vp Current viewport parameters
   *
   * @note Requires OpenGL context and texture binding before call
   */
  void DrawGLTexture(GribOverlay *pGO, GribRecord *pGR, PlugIn_ViewPort *vp);
  void GetCalibratedGraphicColor(int settings, double val_in,
                                 unsigned char *data);
  /**
   * Creates OpenGL texture from GRIB data.
   *
   * @param pGO Overlay object to store texture
   * @param settings Data type settings (wind, pressure etc)
   * @param pGR Source GRIB record
   * @return True if texture created successfully
   *
   * @note Manages:
   *  - Downsampling for large grids
   *  - RGBA color mapping
   *  - Edge handling for global wrap
   *  - POT texture dimensions
   */
  bool CreateGribGLTexture(GribOverlay *pGO, int config, GribRecord *pGR);
  /**
   * Renders a single GRIB texture quad.
   *
   * @param pGO Overlay containing texture
   * @param pGR GRIB record for coordinates
   * @param uv Texture coordinates array
   * @param x,y Screen position
   * @param width,height Dimensions to render
   *
   * @note Handles:
   * - Texture coordinate mapping
   * - Alpha blending
   * - Shader program binding
   * - Matrix transformations
   */
  void DrawSingleGLTexture(GribOverlay *pGO, GribRecord *pGR, double uv[],
                           double x, double y, double xs, double ys);
#endif
  wxImage CreateGribImage(int config, GribRecord *pGR, PlugIn_ViewPort *vp,
                          int grib_pixel_size, const wxPoint &porg);

  double m_last_vp_scale;

  GribOverlay *m_pOverlay[GribOverlaySettings::SETTINGS_COUNT];

  wxString m_Message;
  wxString m_Message_Hiden;
  int m_TimeZone;

  wxDC *m_pdc;
#if wxUSE_GRAPHICS_CONTEXT
  wxGraphicsContext *m_gdc;
#endif

  wxFont *m_Font_Message;

  bool m_hiDefGraphics;
  bool m_bGradualColors;
  bool m_bDrawBarbedArrowHead;

  std::map<double, wxImage> m_labelCache;

  TexFont m_TexFontMessage, m_TexFontNumbers;

  GRIBUICtrlBar &m_dlg;
  GribOverlaySettings &m_Settings;

  ParticleMap *m_ParticleMap;
  wxTimer m_tParticleTimer;
  bool m_bUpdateParticles;

  LineBuffer m_WindArrowCache[14];
  LineBuffer m_SingleArrow[2], m_DoubleArrow[2];

  double m_pixelMM;
  int windArrowSize;
};

#endif