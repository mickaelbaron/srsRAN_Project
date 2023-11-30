/*
 *
 * Copyright 2021-2023 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "ue_scheduler_impl.h"
#include "../policy/scheduler_policy_factory.h"

using namespace srsran;

ue_scheduler_impl::ue_scheduler_impl(const scheduler_ue_expert_config& expert_cfg_,
                                     sched_configuration_notifier&     mac_notif,
                                     scheduler_metrics_handler&        metric_handler,
                                     scheduler_event_logger&           sched_ev_logger) :
  expert_cfg(expert_cfg_),
  sched_strategy(create_scheduler_strategy(scheduler_strategy_params{"time_rr", &srslog::fetch_basic_logger("SCHED")})),
  ue_db(mac_notif),
  ue_alloc(expert_cfg, ue_db, srslog::fetch_basic_logger("SCHED")),
  event_mng(expert_cfg, ue_db, mac_notif, metric_handler, sched_ev_logger),
  logger(srslog::fetch_basic_logger("SCHED"))
{
}

void ue_scheduler_impl::add_cell(const ue_scheduler_cell_params& params)
{
  ue_res_grid_view.add_cell(*params.cell_res_alloc);
  cells[params.cell_index] = std::make_unique<cell>(expert_cfg, params, ue_db);
  event_mng.add_cell(params.cell_res_alloc->cfg, cells[params.cell_index]->srb0_sched);
  ue_alloc.add_cell(params.cell_index, *params.pdcch_sched, *params.uci_alloc, *params.cell_res_alloc);
}

void ue_scheduler_impl::run_sched_strategy(slot_point slot_tx, du_cell_index_t cell_index)
{
  // Update all UEs state.
  ue_db.slot_indication(slot_tx);

  if (not ue_res_grid_view.get_cell_cfg_common(cell_index).is_dl_enabled(slot_tx)) {
    // This slot is inactive for PDCCH in this cell. We therefore, can skip the scheduling strategy.
    // Note: we are currently assuming that all cells have the same TDD pattern and that the scheduling strategy
    // only allocates PDCCHs for the current slot_tx.
    return;
  }

  // Perform round-robin prioritization of UL and DL scheduling. This gives unfair preference to DL over UL. This is
  // done to avoid the issue of sending wrong DAI value in DCI format 0_1 to UE while the PDSCH is allocated
  // right after allocating PUSCH in the same slot, resulting in gNB expecting 1 HARQ ACK bit to be multiplexed in
  // UCI in PUSCH and UE sending 4 HARQ ACK bits (DAI = 3).
  // Example: K1==K2=4 and PUSCH is allocated before PDSCH.
  if (expert_cfg.enable_csi_rs_pdsch_multiplexing or (*cells[cell_index]->cell_res_alloc)[0].result.dl.csi_rs.empty()) {
    sched_strategy->dl_sched(ue_alloc, ue_res_grid_view, ue_db);
  }
  sched_strategy->ul_sched(ue_alloc, ue_res_grid_view, ue_db);
}

void ue_scheduler_impl::update_harq_pucch_counter(cell_resource_allocator& cell_alloc)
{
  // We need to update the PUCCH counter after the SR/CSI scheduler because the allocation of CSI/SR can add/remove
  // PUCCH grants.
  const unsigned HARQ_SLOT_DELAY = 0;
  const auto&    slot_alloc      = cell_alloc[HARQ_SLOT_DELAY];

  // Spans through the PUCCH grant list and update the HARQ-ACK PUCCH grant counter for the corresponding RNTI and HARQ
  // process id.
  for (const auto& pucch : slot_alloc.result.ul.pucchs) {
    if ((pucch.format == pucch_format::FORMAT_1 and pucch.format_1.harq_ack_nof_bits > 0) or
        (pucch.format == pucch_format::FORMAT_2 and pucch.format_2.harq_ack_nof_bits > 0)) {
      ue* user = ue_db.find_by_rnti(pucch.crnti);
      // This is to handle the case of a UE that gets removed after the PUCCH gets allocated and before this PUCCH is
      // expected to be sent.
      if (user == nullptr) {
        logger.warning(
            "rnti={:#x}: No user with such RNTI found in the ue scheduler database. Skipping PUCCH grant counter",
            pucch.crnti,
            slot_alloc.slot);
        continue;
      }
      srsran_assert(pucch.format == pucch_format::FORMAT_1 or pucch.format == pucch_format::FORMAT_2,
                    "rnti={:#x}: Only PUCCH format 1 and format 2 are supported",
                    pucch.crnti);
      const unsigned nof_harqs_per_rnti_per_slot =
          pucch.format == pucch_format::FORMAT_1 ? pucch.format_1.harq_ack_nof_bits : pucch.format_2.harq_ack_nof_bits;
      // Each PUCCH grants can potentially carry ACKs for different HARQ processes (as many as the harq_ack_nof_bits)
      // expecting to be acknowledged on the same slot.
      for (unsigned harq_bit_idx = 0; harq_bit_idx != nof_harqs_per_rnti_per_slot; ++harq_bit_idx) {
        dl_harq_process* h_dl = user->get_pcell().harqs.find_dl_harq_waiting_ack_slot(slot_alloc.slot, harq_bit_idx);
        if (h_dl == nullptr) {
          logger.warning(
              "ue={} rnti={:#x}: No DL HARQ process with state waiting-for-ack found at slot={} for harq-bit-index",
              user->ue_index,
              user->crnti,
              slot_alloc.slot,
              harq_bit_idx);
          continue;
        };
        h_dl->increment_pucch_counter();
      }
    }
  }
}

void ue_scheduler_impl::puxch_grant_sanitizer(cell_resource_allocator& cell_alloc)
{
  const unsigned HARQ_SLOT_DELAY = 0;
  const auto&    slot_alloc      = cell_alloc[HARQ_SLOT_DELAY];

  if (not cell_alloc.cfg.is_ul_enabled(slot_alloc.slot)) {
    return;
  }

  // Spans through the PUCCH grant list and check if there is any PUCCH grant scheduled for a UE that has a PUSCH.
  for (const auto& pucch : slot_alloc.result.ul.pucchs) {
    const auto* pusch_grant =
        std::find_if(slot_alloc.result.ul.puschs.begin(),
                     slot_alloc.result.ul.puschs.end(),
                     [&pucch](const ul_sched_info& pusch) { return pusch.pusch_cfg.rnti == pucch.crnti; });

    if (pusch_grant != slot_alloc.result.ul.puschs.end()) {
      unsigned harq_bits = 0;
      unsigned csi_bits  = 0;
      unsigned sr_bits   = 0;
      if (pucch.format == pucch_format::FORMAT_1) {
        harq_bits = pucch.format_1.harq_ack_nof_bits;
        sr_bits   = sr_nof_bits_to_uint(pucch.format_1.sr_bits);
      } else if (pucch.format == pucch_format::FORMAT_2) {
        harq_bits = pucch.format_2.harq_ack_nof_bits;
        csi_bits  = pucch.format_2.csi_part1_bits;
        sr_bits   = sr_nof_bits_to_uint(pucch.format_2.sr_bits);
      }
      logger.error("rnti={:#x}: has both PUCCH and PUSCH grants scheduled at slot {}, PUCCH  format={} with nof "
                   "harq-bits={} csi-1-bits={} sr-bits={}",
                   pucch.crnti,
                   slot_alloc.slot,
                   static_cast<unsigned>(pucch.format),
                   harq_bits,
                   csi_bits,
                   sr_bits);
    }
  }
}

void ue_scheduler_impl::run_slot(slot_point slot_tx, du_cell_index_t cell_index)
{
  // Process any pending events that are directed at UEs.
  event_mng.run(slot_tx, cell_index);

  // Mark the start of a new slot in the UE grid allocator.
  ue_alloc.slot_indication();

  // Schedule periodic UCI (SR and CSI) before any UL grants.
  cells[cell_index]->uci_sched.run_slot(*cells[cell_index]->cell_res_alloc, slot_tx);

  // Run cell-specific SRB0 scheduler.
  cells[cell_index]->srb0_sched.run_slot(*cells[cell_index]->cell_res_alloc);

  // Synchronize all carriers. Last thread to reach this synchronization point, runs UE scheduling strategy.
  sync_point.wait(
      slot_tx, ue_alloc.nof_cells(), [this, slot_tx, cell_index]() { run_sched_strategy(slot_tx, cell_index); });

  // Update the PUCCH counter after the UE DL and UL scheduler.
  update_harq_pucch_counter(*cells[cell_index]->cell_res_alloc);

  // TODO: remove this.
  puxch_grant_sanitizer(*cells[cell_index]->cell_res_alloc);
}

void ue_scheduler_impl::handle_error_indication(slot_point                            sl_tx,
                                                du_cell_index_t                       cell_index,
                                                scheduler_slot_handler::error_outcome event)
{
  if (cells[cell_index] == nullptr or cells[cell_index]->cell_res_alloc == nullptr) {
    logger.error("cell={}: Discarding error indication. Cause: cell with provided index is not configured", cell_index);
    return;
  }
  cell_resource_allocator& res_grid = *cells[cell_index]->cell_res_alloc;

  const cell_slot_resource_allocator* prev_slot_result = res_grid.get_history(sl_tx);
  if (prev_slot_result == nullptr) {
    logger.warning("cell={}, slot={}: Discarding error indication. Cause: Scheduler results associated with the slot "
                   "of the error indication have already been erased",
                   cell_index,
                   sl_tx);
    return;
  }

  // Cancel scheduled HARQs. This is important to avoid the softbuffer incorrect initialization in the lower layers
  // during newTxs.
  if (event.pdcch_discarded) {
    for (const pdcch_dl_information& pdcch : prev_slot_result->result.dl.dl_pdcchs) {
      ue* u = ue_db.find_by_rnti(pdcch.ctx.rnti);
      if (u == nullptr) {
        // UE has been removed.
        continue;
      }
      harq_id_t h_id;
      switch (pdcch.dci.type) {
        case dci_dl_rnti_config_type::tc_rnti_f1_0:
          h_id = to_harq_id(pdcch.dci.tc_rnti_f1_0.harq_process_number);
          break;
        case dci_dl_rnti_config_type::c_rnti_f1_0:
          h_id = to_harq_id(pdcch.dci.c_rnti_f1_0.harq_process_number);
          break;
        default:
          // For SI-RNTI, P-RNTI, RA-RNTI, there is no HARQ process associated.
          continue;
      }
      u->get_pcell().harqs.dl_harq(h_id).cancel_harq(0);
    }
    for (const pdcch_ul_information& pdcch : prev_slot_result->result.dl.ul_pdcchs) {
      ue* u = ue_db.find_by_rnti(pdcch.ctx.rnti);
      if (u == nullptr) {
        // UE has been removed.
        continue;
      }
      harq_id_t h_id;
      switch (pdcch.dci.type) {
        case dci_ul_rnti_config_type::c_rnti_f0_0:
          h_id = to_harq_id(pdcch.dci.c_rnti_f0_0.harq_process_number);
          break;
        case dci_ul_rnti_config_type::c_rnti_f0_1:
          h_id = to_harq_id(pdcch.dci.c_rnti_f0_1.harq_process_number);
          break;
        default:
          // TC-RNTI (e.g. Msg3) is managed outside of UE scheduler. Furthermore, NDI is not used for Msg3.
          continue;
      }
      u->get_pcell().harqs.ul_harq(h_id).cancel_harq();
    }
  }
  if (event.pdsch_discarded) {
    for (const dl_msg_alloc& grant : prev_slot_result->result.dl.ue_grants) {
      ue* u = ue_db.find_by_rnti(grant.pdsch_cfg.rnti);
      if (u == nullptr) {
        // UE has been removed.
        continue;
      }
      for (unsigned cw_idx = 0; cw_idx != grant.pdsch_cfg.codewords.size(); ++cw_idx) {
        u->get_pcell().harqs.dl_harq(grant.pdsch_cfg.harq_id).cancel_harq(cw_idx);
      }
    }
  }
  if (event.pusch_and_pucch_discarded) {
    for (const ul_sched_info& grant : prev_slot_result->result.ul.puschs) {
      ue* u = ue_db.find_by_rnti(grant.pusch_cfg.rnti);
      if (u == nullptr) {
        // UE has been removed.
        continue;
      }

      // Cancel UL HARQs due to missed PUSCH.
      u->get_pcell().harqs.ul_harq(grant.pusch_cfg.harq_id).cancel_harq();

      // Cancel DL HARQs due to missed UCI.
      if (grant.uci.has_value() and grant.uci->harq.has_value() and grant.uci->harq->harq_ack_nof_bits > 0) {
        u->get_pcell().harqs.cancel_dl_harqs(sl_tx);
      }
    }
    for (const auto& pucch : prev_slot_result->result.ul.pucchs) {
      ue* u = ue_db.find_by_rnti(pucch.crnti);
      if (u == nullptr) {
        // UE has been removed.
        continue;
      }
      bool has_harq_ack = false;
      switch (pucch.format) {
        case pucch_format::FORMAT_1:
          has_harq_ack = pucch.format_1.harq_ack_nof_bits > 0;
          break;
        case pucch_format::FORMAT_2:
          has_harq_ack = pucch.format_2.harq_ack_nof_bits > 0;
          break;
        default:
          break;
      }
      if (has_harq_ack) {
        // Cancel DL HARQs due to missed UCI.
        u->get_pcell().harqs.cancel_dl_harqs(sl_tx);
      }
    }
  }
}
