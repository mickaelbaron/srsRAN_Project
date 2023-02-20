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

#include "radio_zmq_rx_stream.h"
#include "radio_zmq_tx_stream.h"
#include "srsgnb/radio/radio_session.h"
#include "srsgnb/support/executors/task_executor.h"
#include <zmq.h>

namespace srsgnb {

/// Describes a ZeroMQ radio based session.
class radio_session_zmq_impl : public radio_session,
                               public radio_management_plane,
                               public baseband_gateway,
                               public baseband_gateway_transmitter,
                               public baseband_gateway_receiver
{
private:
  /// Default sockets send and receive timeout in milliseconds.
  static constexpr unsigned DEFAULT_TRX_TIMEOUT_MS = 10;
  /// Default linger timeout in milliseconds.
  static constexpr unsigned DEFAULT_LINGER_TIMEOUT_MS = 0;
  /// Default buffer size in samples.
  static constexpr unsigned DEFAULT_BUFFER_SIZE_SAMPLES = 614400;

  /// Radio session logger.
  srslog::basic_logger& logger;
  /// ZMQ context.
  void* zmq_context;
  /// Stores transmit streams.
  std::vector<std::unique_ptr<radio_zmq_tx_stream>> tx_streams;
  /// Stores receive streams.
  std::vector<std::unique_ptr<radio_zmq_rx_stream>> rx_streams;
  /// Indicates the session has been created succesfully.
  bool successful = false;
  /// Interface to notificate events.
  radio_notification_handler& notification_handler;

public:
  /// \brief Default constructor.
  /// \param[in] config Provides the required parameters to start a ZMQ radio based session.
  /// \param[in] async_task_executor Provides a task executor to perform asynchronous tasks.
  /// \param[in] notification_handler Provides a radio event notification handler.
  /// \note Use is_successful() to check that the instance was successfully initialized.
  radio_session_zmq_impl(const radio_configuration::radio& config,
                         task_executor&                    async_task_executor,
                         radio_notification_handler&       notification_handler);

  /// Default destructor.
  ~radio_session_zmq_impl();

  /// Indicates if the instance was successfully initialized.
  bool is_successful() const { return successful; }

  // See interface for documentation.
  radio_management_plane& get_management_plane() override { return *this; };

  // See interface for documentation.
  baseband_gateway& get_baseband_gateway() override { return *this; }

  // See interface for documentation.
  baseband_gateway_transmitter& get_transmitter() override { return *this; }

  // See interface for documentation.
  baseband_gateway_receiver& get_receiver() override { return *this; }

  // See interface for documentation.
  void stop() override;

  // See interface for documentation.
  void transmit(unsigned                                      stream_id,
                const baseband_gateway_transmitter::metadata& metadata,
                baseband_gateway_buffer&                      data) override;

  // See interface for documentation.
  baseband_gateway_receiver::metadata receive(baseband_gateway_buffer& data, unsigned stream_id) override;

  // See interface for documentation.
  bool set_tx_gain(unsigned port_id, double gain_dB) override;

  // See interface for documentation.
  bool set_rx_gain(unsigned port_id, double gain_dB) override;
};

} // namespace srsgnb
