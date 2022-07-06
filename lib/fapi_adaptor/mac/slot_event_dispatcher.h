/*
 *
 * Copyright 2013-2022 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#ifndef SRSGNB_LIB_FAPI_ADAPTOR_MAC_SLOT_EVENT_DISPATCHER_H
#define SRSGNB_LIB_FAPI_ADAPTOR_MAC_SLOT_EVENT_DISPATCHER_H

#include "srsgnb/mac/mac_cell_slot_handler.h"
#include "srsgnb/ran/slot_point.h"

namespace srsgnb {
namespace fapi_adaptor {

class mac_to_fapi_translator;

/// Handles each new slot event by dispatching them to multiple clients.
class slot_event_dispatcher
{
public:
  explicit slot_event_dispatcher(mac_to_fapi_translator&);

  /// Handles the new slot by dispatching it to the registered clients.
  ///
  /// \param slot New slot to handle.
  void handle_new_slot(slot_point slot);

  /// Sets the mac_cell_slot_handler to the given one. This handler will be notified for each new slot.
  ///
  /// \param handler Handler to set.
  void set_mac_cell_slot_handler(mac_cell_slot_handler& handler);

private:
  std::reference_wrapper<mac_cell_slot_handler> mac_slot_handler;
  mac_to_fapi_translator&                       translator;
};

} // namespace fapi_adaptor
} // namespace srsgnb

#endif // SRSGNB_LIB_FAPI_ADAPTOR_MAC_SLOT_EVENT_DISPATCHER_H
