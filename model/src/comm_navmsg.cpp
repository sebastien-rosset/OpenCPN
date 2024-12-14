/**************************************************************************
 *   Copyright (C) 2022  David Register                                    *
 *   Copyright (C) 2022 - 2024  Alec Leamas                                *
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
 **************************************************************************/

/**
 * \file
 * Implement comm_navmsg.h
 */

// For compilers that support precompilation, includes "wx.h".
#include <wx/wxprec.h>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif  // precompiled headers

#include <algorithm>
#include <string>
#include <iomanip>

#include "model/comm_driver.h"
#include "N2kMessages.h"

std::string NavAddr::BusToString(NavAddr::Bus b) {
  switch (b) {
    case NavAddr::Bus::N0183:
      return "nmea0183";
      break;
    case NavAddr::Bus::N2000:
      return "nmea2000";
      break;
    case NavAddr::Bus::Signalk:
      return "SignalK";
      break;
    case NavAddr::Bus::Onenet:
      return "Onenet";
      break;
    case NavAddr::Bus::Plugin:
      return "Plugin";
      break;
    case NavAddr::Bus::TestBus:
      return "TestBus";
      break;
    case NavAddr::Bus::Undef:
      return "??";
      break;
  }
  return "????";
}

NavAddr::Bus NavAddr::StringToBus(const std::string& s) {
  if (s == "nmea0183") return NavAddr::Bus::N0183;
  if (s == "nmea2000") return NavAddr::Bus::N2000;
  if (s == "SignalK") return NavAddr::Bus::Signalk;
  if (s == "Onenet") return NavAddr::Bus::Onenet;
  if (s == "TestBus") return NavAddr::Bus::TestBus;
  return NavAddr::Bus::Undef;
}

static std::string CharToString(unsigned char c) {
  using namespace std;
  stringstream ss;
  ss << setfill('0') << hex << setw(2) << (c & 0x00ff);
  return ss.str();
}

std::string Nmea2000Msg::to_string() const {
  std::string s;
  std::for_each(payload.begin(), payload.end(),
                [&s](unsigned char c) { s.append(CharToString(c)); });

  return NavMsg::to_string() + " " + PGN.to_string() + " " + s;
}

std::optional<uint64_t> Nmea2000Msg::GetTimestamp() const {
  if (parsed_timestamp.has_value()) {
    return parsed_timestamp;
  }

  parsed_timestamp = ParseTimestamp();
  return parsed_timestamp;
}

std::optional<uint64_t> Nmea2000Msg::ParseTimestamp() const {
  if (payload.size() < 14) {  // Need at least 8 bytes header + 6 bytes data
    return std::nullopt;
  }

  const size_t data_start = 8;  // Skip header

  switch (PGN.pgn) {
    case 126992: {  // System Time
      // Data after 8-byte header:
      // Offset 0: Time Source
      // Offset 1: Reserved
      // Offset 2-3: Days since 1970
      // Offset 4-7: Seconds since midnight
      if (payload.size() < data_start + 8) {
        return std::nullopt;
      }

      uint16_t days_since_1970 =
          (payload[data_start + 3] << 8) | payload[data_start + 2];
      uint32_t seconds_raw =
          (payload[data_start + 7] << 24) | (payload[data_start + 6] << 16) |
          (payload[data_start + 5] << 8) | payload[data_start + 4];
      double seconds = seconds_raw * 0.0001;

      uint64_t timestamp = days_since_1970 * 24ULL * 60ULL * 60ULL * 1000000ULL;
      timestamp += static_cast<uint64_t>(seconds * 1000000.0);
      return timestamp;
    }
    case 129029: {  // GNSS Position Data
      // Date/Time starts at byte 34 in data section
      const size_t time_offset = 34;
      if (payload.size() < data_start + time_offset + 6) {
        return std::nullopt;
      }

      uint16_t days_since_1970 = (payload[data_start + time_offset + 1] << 8) |
                                 payload[data_start + time_offset];
      uint32_t seconds_raw = (payload[data_start + time_offset + 5] << 24) |
                             (payload[data_start + time_offset + 4] << 16) |
                             (payload[data_start + time_offset + 3] << 8) |
                             payload[data_start + time_offset + 2];
      double seconds = seconds_raw * 0.0001;

      uint64_t timestamp = days_since_1970 * 24ULL * 60ULL * 60ULL * 1000000ULL;
      timestamp += static_cast<uint64_t>(seconds * 1000000.0);
      return timestamp;
    }

    case 129033: {  // Local Time Offset
      uint16_t days_since_1970 =
          (payload[data_start + 1] << 8) | payload[data_start];
      uint32_t seconds_raw =
          (payload[data_start + 5] << 24) | (payload[data_start + 4] << 16) |
          (payload[data_start + 3] << 8) | payload[data_start + 2];
      double seconds = seconds_raw * 0.0001;

      uint64_t timestamp = days_since_1970 * 24ULL * 60ULL * 60ULL * 1000000ULL;
      timestamp += static_cast<uint64_t>(seconds * 1000000.0);
      return timestamp;
    }

    default:
      return std::nullopt;
  }
}