/*
 *
 * Copyright 2021-2023 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#pragma once

namespace srsgnb {

namespace fapi {
class slot_data_message_notifier;
class slot_message_gateway;
class slot_time_message_notifier;
} // namespace fapi

class upper_phy_timing_notifier;
class upper_phy_rx_results_notifier;

namespace fapi_adaptor {

/// \brief PHY&ndash;FAPI bidirectional adaptor interface.
///
/// This adaptor is a collection of interfaces to translate FAPI messages into their PHY layer counterpart and vice
/// versa.
///
/// \note All implementations of this public interface must hold the ownership of all its internal components.
class phy_fapi_adaptor
{
public:
  virtual ~phy_fapi_adaptor() = default;

  /// \brief Returns a reference to the timing notifier used by the adaptor.
  virtual upper_phy_timing_notifier& get_timing_notifier() = 0;

  /// \brief Returns a reference to the slot-based message gateway used by the adaptor.
  virtual fapi::slot_message_gateway& get_slot_message_gateway() = 0;

  /// \brief Returns a reference to the results notifier used by the adaptor.
  virtual upper_phy_rx_results_notifier& get_rx_results_notifier() = 0;

  /// \brief Configures the slot-based, time-specific message notifier to the given one.
  virtual void set_slot_time_message_notifier(fapi::slot_time_message_notifier& fapi_time_notifier) = 0;

  /// \brief Configures the slot-based, data-specific message notifier to the given one.
  virtual void set_slot_data_message_notifier(fapi::slot_data_message_notifier& fapi_data_notifier) = 0;
};

} // namespace fapi_adaptor
} // namespace srsgnb
