/***************************************************************************
 *   Copyright (C) 2025 by OpenCPN development team                        *
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

#ifndef METEOGRAM_INTEGRATION_H
#define METEOGRAM_INTEGRATION_H

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include <wx/aui/aui.h>
#include <vector>
#include <memory>

class GRIBUICtrlBar;
class MeteogramWindow;
class ArrayOfGribRecordSets;

/**
 * Manager class for meteogram windows within the GRIB plugin
 * Handles creation, lifecycle, and coordination of meteogram windows
 */
class MeteogramManager {
public:
  MeteogramManager(GRIBUICtrlBar* gribDialog, wxAuiManager* auiManager);
  ~MeteogramManager();

  // Meteogram creation
  void ShowMeteogramAtPoint(double lat, double lon, const wxString& name = "");
  void ShowMeteogramForRoute(
      const std::vector<std::pair<double, double>>& routePoints,
      const wxString& routeName = "");
  void ShowMeteogramAtCursor();  // Use current cursor position

  // Window management
  void CloseAllMeteograms();
  void RefreshAllMeteograms();
  void UpdateGribData(ArrayOfGribRecordSets* recordSets);

  // Access
  size_t GetMeteogramCount() const { return m_meteogramWindows.size(); }
  bool HasMeteograms() const { return !m_meteogramWindows.empty(); }

private:
  GRIBUICtrlBar* m_gribDialog;
  wxAuiManager* m_auiManager;
  std::vector<std::unique_ptr<MeteogramWindow>> m_meteogramWindows;

  void CleanupClosedWindows();
  void OnMeteogramClosed(MeteogramWindow* window);

  friend class MeteogramWindow;  // Allow windows to notify manager when closing
};

// Integration functions for GRIB plugin
namespace MeteogramIntegration {

/**
 * Initialize meteogram support in GRIB plugin
 */
void InitializeMeteogramSupport(GRIBUICtrlBar* gribDialog,
                                wxAuiManager* auiManager);

/**
 * Clean up meteogram support
 * Call this from GRIB plugin destruction
 */
void CleanupMeteogramSupport();

/**
 * Add meteogram menu items to context menu
 */
void AddMeteogramMenuItems(wxMenu& menu);

/**
 * Handle meteogram menu selections
 */
bool HandleMeteogramMenuEvent(int menuId);

/**
 * Update meteogram data when GRIB data changes
 */
void UpdateMeteogramData(ArrayOfGribRecordSets* recordSets);

/**
 * Get the global meteogram manager instance
 */
MeteogramManager* GetMeteogramManager();

}  // namespace MeteogramIntegration

// Menu IDs for meteogram integration
enum {
  ID_METEOGRAM_AT_CURSOR = 16000,
  ID_METEOGRAM_FOR_ROUTE,
  ID_METEOGRAM_CLOSE_ALL
};

#endif  // METEOGRAM_INTEGRATION_H