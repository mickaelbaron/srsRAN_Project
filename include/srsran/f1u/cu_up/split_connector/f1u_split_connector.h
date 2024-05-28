/*
 *
 *
 * Copyright 2021-2024 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#pragma once

#include "srsran/f1u/cu_up/f1u_bearer_logger.h"
#include "srsran/f1u/cu_up/f1u_gateway.h"
#include "srsran/gtpu/gtpu_config.h"
#include "srsran/gtpu/gtpu_demux.h"
#include "srsran/gtpu/gtpu_tunnel_common_tx.h"
#include "srsran/gtpu/gtpu_tunnel_nru.h"
#include "srsran/gtpu/gtpu_tunnel_nru_rx.h"
#include "srsran/gtpu/ngu_gateway.h"
#include "srsran/pcap/dlt_pcap.h"
#include "srsran/srslog/srslog.h"
#include <cstdint>
#include <unordered_map>

namespace srsran::srs_cu_up {

class gtpu_tx_udp_gw_adapter : public gtpu_tunnel_common_tx_upper_layer_notifier
{
public:
  /// \brief Interface for the GTP-U to pass PDUs to the IO gateway
  /// \param sdu PDU to be transmitted.
  void on_new_pdu(byte_buffer buf, const sockaddr_storage& addr) override
  {
    if (handler != nullptr) {
      handler->handle_pdu(std::move(buf), addr);
    }
  }

  void connect(srs_cu_up::ngu_tnl_pdu_session& handler_) { handler = &handler_; }

  void disconnect() { handler = nullptr; }

  srs_cu_up::ngu_tnl_pdu_session* handler;
};

class gtpu_rx_f1u_adapter : public srsran::gtpu_tunnel_nru_rx_lower_layer_notifier
{
public:
  /// \brief Interface for the GTP-U to pass a SDU (i.e. NR-U DL message) into the lower layer.
  /// \param dl_message NR-U DL message with optional T-PDU.
  void on_new_sdu(nru_dl_message dl_message) override {}

  /// \brief Interface for the GTP-U to pass a SDU (i.e. NR-U UL message) into the lower layer.
  /// \param ul_message NR-U UL message with optional T-PDU.
  void on_new_sdu(nru_ul_message ul_message) override
  {
    if (handler != nullptr) {
      handler->on_new_pdu(std::move(ul_message));
    }
  }

  void connect(f1u_cu_up_gateway_bearer_rx_notifier& handler_) { handler = &handler_; }

  void disconnect() { handler = nullptr; }

  f1u_cu_up_gateway_bearer_rx_notifier* handler;
};

/// Adapter between Network Gateway (Data) and GTP-U demux
class network_gateway_data_gtpu_demux_adapter : public srsran::network_gateway_data_notifier_with_src_addr
{
public:
  network_gateway_data_gtpu_demux_adapter()           = default;
  ~network_gateway_data_gtpu_demux_adapter() override = default;

  void connect_gtpu_demux(gtpu_demux_rx_upper_layer_interface& gtpu_demux_) { gtpu_demux = &gtpu_demux_; }

  void on_new_pdu(byte_buffer pdu, const sockaddr_storage& src_addr) override
  {
    srsran_assert(gtpu_demux != nullptr, "GTP-U handler must not be nullptr");
    gtpu_demux->handle_pdu(std::move(pdu), src_addr);
  }

private:
  gtpu_demux_rx_upper_layer_interface* gtpu_demux = nullptr;
};

/// \brief Object used to represent a bearer at the CU F1-U gateway
/// On the co-located case this is done by connecting both entities directly.
///
/// It will keep a notifier to the DU NR-U RX and provide the methods to pass
/// an SDU to it.
class f1u_split_gateway_cu_bearer : public f1u_cu_up_gateway_bearer
{
public:
  f1u_split_gateway_cu_bearer(uint32_t                              ue_index_,
                              drb_id_t                              drb_id,
                              const up_transport_layer_info&        ul_tnl_info_,
                              f1u_cu_up_gateway_bearer_rx_notifier& cu_rx_,
                              ngu_tnl_pdu_session&                  udp_session,
                              task_executor&                        ul_exec_,
                              srs_cu_up::f1u_bearer_disconnector&   disconnector_) :
    ul_exec(ul_exec_),
    ue_index(ue_index_),
    logger("CU-F1-U", {ue_index_, drb_id, ul_tnl_info_}),
    disconnector(disconnector_),
    ul_tnl_info(ul_tnl_info_),
    cu_rx(cu_rx_)
  {
    gtpu_to_network_adapter.connect(udp_session);
    gtpu_to_f1u_adapter.connect(cu_rx);
  }

  ~f1u_split_gateway_cu_bearer() override { stop(); }

  void stop() override
  {
    if (not stopped) {
      disconnector.disconnect_cu_bearer(ul_tnl_info);
    }
    stopped = true;
  }

  void on_new_pdu(nru_dl_message msg) override
  {
    if (tunnel == nullptr) {
      logger.log_debug("DL GTPU tunnel not connected. Discarding SDU.");
      return;
    }
    tunnel->get_tx_lower_layer_interface()->handle_sdu(std::move(msg));
  }

  void attach_tunnel(std::unique_ptr<gtpu_tunnel_nru> tunnel_) { tunnel = std::move(tunnel_); }

  gtpu_tx_udp_gw_adapter gtpu_to_network_adapter;
  gtpu_rx_f1u_adapter    gtpu_to_f1u_adapter;

  gtpu_tunnel_common_rx_upper_layer_interface* get_tunnel_rx_interface()
  {
    return tunnel->get_rx_upper_layer_interface();
  }

  /// Holds the RX executor associated with the F1-U bearer.
  task_executor& ul_exec;
  uint32_t       ue_index;

private:
  bool                                stopped = false;
  srs_cu_up::f1u_bearer_logger        logger;
  srs_cu_up::f1u_bearer_disconnector& disconnector;
  up_transport_layer_info             ul_tnl_info;
  std::unique_ptr<gtpu_tunnel_nru>    tunnel;

public:
  /// Holds notifier that will point to NR-U bearer on the UL path
  f1u_cu_up_gateway_bearer_rx_notifier& cu_rx;

  /// Holds the DL UP TNL info associated with the F1-U bearer.
  optional<up_transport_layer_info> dl_tnl_info;
};

/// \brief Object used to connect the DU and CU-UP F1-U bearers
/// On the co-located case this is done by connecting both entities directly.
///
/// Note that CU and DU bearer creation and removal can be performed from different threads and are therefore
/// protected by a common mutex.
class f1u_split_connector final : public f1u_cu_up_gateway
{
public:
  f1u_split_connector(ngu_gateway* udp_gw_, gtpu_demux* demux_, dlt_pcap& gtpu_pcap_, uint16_t peer_port_ = GTPU_PORT) :
    logger_cu(srslog::fetch_basic_logger("CU-F1-U")),
    peer_port(peer_port_),
    udp_gw(udp_gw_),
    demux(demux_),
    gtpu_pcap(gtpu_pcap_)
  {
    udp_session = udp_gw->create(gw_data_gtpu_demux_adapter);
    gw_data_gtpu_demux_adapter.connect_gtpu_demux(*demux);
  }

  f1u_cu_up_gateway* get_f1u_cu_up_gateway() { return this; }

  std::optional<uint16_t> get_bind_port() { return udp_session->get_bind_port(); }

  std::unique_ptr<f1u_cu_up_gateway_bearer> create_cu_bearer(uint32_t                              ue_index,
                                                             drb_id_t                              drb_id,
                                                             const srs_cu_up::f1u_config&          config,
                                                             const up_transport_layer_info&        ul_up_tnl_info,
                                                             f1u_cu_up_gateway_bearer_rx_notifier& rx_notifier,
                                                             task_executor&                        ul_exec,
                                                             timer_factory                         ue_dl_timer_factory,
                                                             unique_timer& ue_inactivity_timer) override;

  void attach_dl_teid(const up_transport_layer_info& ul_up_tnl_info,
                      const up_transport_layer_info& dl_up_tnl_info) override;

  void disconnect_cu_bearer(const up_transport_layer_info& ul_up_tnl_info) override;

private:
  srslog::basic_logger& logger_cu;
  // Key is the UL UP TNL Info (CU-CP address and UL TEID reserved by CU-CP)
  std::unordered_map<up_transport_layer_info, f1u_split_gateway_cu_bearer*> cu_map;
  std::mutex map_mutex; // shared mutex for access to cu_map

  uint16_t                                peer_port;
  ngu_gateway*                            udp_gw;
  std::unique_ptr<ngu_tnl_pdu_session>    udp_session;
  gtpu_demux*                             demux;
  network_gateway_data_gtpu_demux_adapter gw_data_gtpu_demux_adapter;
  dlt_pcap&                               gtpu_pcap;
};

} // namespace srsran::srs_cu_up
