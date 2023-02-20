/*
 *
 * Copyright 2021-2023 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

/// \file
/// \brief  Logical Channel Group and UL Buffer Size format definition and levels according to 3GPP TS38.321
/// version 15.3.0

#pragma once

#include "srsgnb/adt/optional.h"
#include "srsgnb/scheduler/config/logical_channel_group.h"

namespace srsgnb {

/// TS 38.321, 6.1.3.1 - Buffer Status Report MAC CEs
enum class bsr_format { SHORT_BSR, LONG_BSR, SHORT_TRUNC_BSR, LONG_TRUNC_BSR };

inline const char* to_string(bsr_format bsr)
{
  static constexpr std::array<const char*, 4> names = {
      "Short BSR", "Long BSR", "Short Truncated BSR", "Long Truncated BSR"};
  return (size_t)bsr < names.size() ? names[(size_t)bsr] : "Invalid BSR format";
}

/// \c periodicBSR-Timer, as part of BSR-Config, TS 38.331.
enum class periodic_bsr_timer {
  sf1      = 1,
  sf5      = 5,
  sf10     = 10,
  sf16     = 16,
  sf20     = 20,
  sf32     = 32,
  sf40     = 40,
  sf64     = 64,
  sf80     = 80,
  sf128    = 128,
  sf160    = 160,
  sf320    = 320,
  sf640    = 640,
  sf1280   = 1280,
  sf2560   = 2560,
  infinity = 0
};

/// Return the value of \ref periodic_bsr_timer.
inline std::underlying_type<periodic_bsr_timer>::type periodic_bsr_timer_to_value(periodic_bsr_timer timer)
{
  return static_cast<std::underlying_type<periodic_bsr_timer>::type>(timer);
}

/// \c retxBSR-Timer, as part of BSR-Config, TS 38.331.
enum class retx_bsr_timer {
  sf10    = 10,
  sf20    = 20,
  sf40    = 40,
  sf80    = 80,
  sf160   = 160,
  sf320   = 320,
  sf640   = 640,
  sf1280  = 1280,
  sf2560  = 2560,
  sf5120  = 5120,
  sf10240 = 10240
};

/// Return the value of \ref retx_bsr_timer.
inline std::underlying_type<periodic_bsr_timer>::type retx_bsr_timer_to_value(retx_bsr_timer timer)
{
  return static_cast<std::underlying_type<periodic_bsr_timer>::type>(timer);
}

/// \c logicalChannelSR-DelayTimer, as part of BSR-Config, TS 38.331.
enum class logical_channel_sr_delay_timer {
  sf20   = 20,
  sf40   = 40,
  sf64   = 64,
  sf128  = 128,
  sf512  = 512,
  sf1024 = 1024,
  sf2560 = 2560
};

/// Return the value of \ref logical_channel_sr_delay_timer.
inline std::underlying_type<periodic_bsr_timer>::type lc_sr_delay_timer_to_value(logical_channel_sr_delay_timer timer)
{
  return static_cast<std::underlying_type<periodic_bsr_timer>::type>(timer);
}

/// \c BSR-Config, TS 38.331.
struct bsr_config {
  periodic_bsr_timer                       periodic_timer;
  retx_bsr_timer                           retx_timer;
  optional<logical_channel_sr_delay_timer> lc_sr_delay_timer;
};

} // namespace srsgnb
