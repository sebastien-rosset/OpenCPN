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

#include "GribRecord.h"
#include "meteogram_integration.h"
#include "meteogram_panel.h"
#include "GribUIDialog.h"
#include "grib_pi.h"

#include <wx/menu.h>

// Global meteogram manager instance
static std::unique_ptr<MeteogramManager> g_meteogramManager;

// External cursor position variables (defined in existing GRIB code)
extern double m_cursor_lat, m_cursor_lon;

//=============================================================================
// MeteogramManager Implementation
//=============================================================================

MeteogramManager::MeteogramManager(GRIBUICtrlBar* gribDialog,
                                   wxAuiManager* auiManager)
    : m_gribDialog(gribDialog), m_auiManager(auiManager) {}

MeteogramManager::~MeteogramManager() { CloseAllMeteograms(); }

void MeteogramManager::ShowMeteogramAtPoint(double lat, double lon,
                                            const wxString& name) {
  if (!m_gribDialog || !m_auiManager) return;

  // Clean up any closed windows first
  CleanupClosedWindows();

  // Create new meteogram window
  wxString meteogramName = name;
  if (meteogramName.IsEmpty()) {
    meteogramName = wxString::Format("Position %.3f°, %.3f°", lat, lon);
  }

  auto meteogramWindow =
      std::unique_ptr<MeteogramWindow>(MeteogramWindow::CreateForPoint(
          m_gribDialog, m_auiManager, lat, lon, meteogramName));

  // Set current GRIB data
  if (m_gribDialog->m_bGRIBActiveFile &&
      m_gribDialog->m_bGRIBActiveFile->IsOK()) {
    ArrayOfGribRecordSets* recordSets =
        m_gribDialog->m_bGRIBActiveFile->GetRecordSetArrayPtr();
    meteogramWindow->SetGribRecordSets(recordSets);
  }

  // Show the window
  meteogramWindow->ShowPane(true);

  // Store the window
  m_meteogramWindows.push_back(std::move(meteogramWindow));
}

void MeteogramManager::ShowMeteogramForRoute(
    const std::vector<std::pair<double, double>>& routePoints,
    const wxString& routeName) {
  if (!m_gribDialog || !m_auiManager || routePoints.empty()) return;

  // Clean up any closed windows first
  CleanupClosedWindows();

  // Create new meteogram window for route
  auto meteogramWindow =
      std::unique_ptr<MeteogramWindow>(MeteogramWindow::CreateForRoute(
          m_gribDialog, m_auiManager, routePoints, routeName));

  // Set current GRIB data
  if (m_gribDialog->m_bGRIBActiveFile &&
      m_gribDialog->m_bGRIBActiveFile->IsOK()) {
    ArrayOfGribRecordSets* recordSets =
        m_gribDialog->m_bGRIBActiveFile->GetRecordSetArrayPtr();
    meteogramWindow->SetGribRecordSets(recordSets);
  }

  // Show the window
  meteogramWindow->ShowPane(true);

  // Store the window
  m_meteogramWindows.push_back(std::move(meteogramWindow));
}

void MeteogramManager::ShowMeteogramAtCursor() {
  ShowMeteogramAtPoint(m_cursor_lat, m_cursor_lon, "Cursor Position");
}

void MeteogramManager::CloseAllMeteograms() {
  for (auto& window : m_meteogramWindows) {
    if (window) {
      window->HidePane();
    }
  }
  m_meteogramWindows.clear();
}

void MeteogramManager::RefreshAllMeteograms() {
  CleanupClosedWindows();

  for (auto& window : m_meteogramWindows) {
    if (window) {
      window->RefreshData();
    }
  }
}

void MeteogramManager::UpdateGribData(ArrayOfGribRecordSets* recordSets) {
  CleanupClosedWindows();

  for (auto& window : m_meteogramWindows) {
    if (window) {
      window->SetGribRecordSets(recordSets);
    }
  }
}

void MeteogramManager::CleanupClosedWindows() {
  // Remove windows that are no longer shown
  m_meteogramWindows.erase(
      std::remove_if(m_meteogramWindows.begin(), m_meteogramWindows.end(),
                     [](const std::unique_ptr<MeteogramWindow>& window) {
                       return !window || !window->IsPaneShown();
                     }),
      m_meteogramWindows.end());
}

void MeteogramManager::OnMeteogramClosed(MeteogramWindow* window) {
  // Called when a meteogram window is closed
  CleanupClosedWindows();
}

//=============================================================================
// Integration Functions
//=============================================================================

namespace MeteogramIntegration {

void InitializeMeteogramSupport(GRIBUICtrlBar* gribDialog,
                                wxAuiManager* auiManager) {
  if (!g_meteogramManager) {
    g_meteogramManager =
        std::make_unique<MeteogramManager>(gribDialog, auiManager);
  }
}

void CleanupMeteogramSupport() { g_meteogramManager.reset(); }

void AddMeteogramMenuItems(wxMenu& menu) {
  if (!g_meteogramManager) return;

  menu.AppendSeparator();
  menu.Append(ID_METEOGRAM_AT_CURSOR, _("Show Meteogram Here"));

  // Add route meteogram if there's an active route
  wxString activeRouteGUID = GetActiveRouteGUID();
  if (!activeRouteGUID.IsEmpty()) {
    auto activeRoute = GetRoute_Plugin(activeRouteGUID);
    if (activeRoute && activeRoute->pWaypointList &&
        activeRoute->pWaypointList->GetCount() > 0) {
      wxString routeName = activeRoute->m_NameString;
      if (routeName.IsEmpty()) {
        routeName = "Active Route";
      }
      menu.Append(ID_METEOGRAM_FOR_ROUTE,
                  wxString::Format(_("Show Meteogram for %s"), routeName));
    }
  }

  if (g_meteogramManager->HasMeteograms()) {
    menu.Append(ID_METEOGRAM_CLOSE_ALL, _("Close All Meteograms"));
  }
}

bool HandleMeteogramMenuEvent(int menuId) {
  if (!g_meteogramManager) return false;

  switch (menuId) {
    case ID_METEOGRAM_AT_CURSOR:
      g_meteogramManager->ShowMeteogramAtCursor();
      return true;

    case ID_METEOGRAM_FOR_ROUTE: {
      // Get active route points and show route meteogram
      wxString activeRouteGUID = GetActiveRouteGUID();
      if (!activeRouteGUID.IsEmpty()) {
        auto activeRoute = GetRoute_Plugin(activeRouteGUID);
        if (activeRoute && activeRoute->pWaypointList &&
            activeRoute->pWaypointList->GetCount() > 0) {
          // Extract route points
          std::vector<std::pair<double, double>> routePoints;
          Plugin_WaypointList::iterator it;
          for (it = activeRoute->pWaypointList->begin();
               it != activeRoute->pWaypointList->end(); ++it) {
            PlugIn_Waypoint* wp = *it;
            routePoints.push_back(std::make_pair(wp->m_lat, wp->m_lon));
          }

          wxString routeName = activeRoute->m_NameString;
          if (routeName.IsEmpty()) {
            routeName = "Active Route";
          }

          g_meteogramManager->ShowMeteogramForRoute(routePoints, routeName);
        }
      }
      return true;
    }

    case ID_METEOGRAM_CLOSE_ALL:
      g_meteogramManager->CloseAllMeteograms();
      return true;

    default:
      return false;
  }
}

void UpdateMeteogramData(ArrayOfGribRecordSets* recordSets) {
  if (g_meteogramManager) {
    g_meteogramManager->UpdateGribData(recordSets);
  }
}

MeteogramManager* GetMeteogramManager() { return g_meteogramManager.get(); }

}  // namespace MeteogramIntegration
