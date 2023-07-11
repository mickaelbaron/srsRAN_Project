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

#include "lib/cu_cp/cu_cp_impl.h"
#include "test_helpers.h"
#include "tests/unittests/e1ap/common/test_helpers.h"
#include "tests/unittests/f1ap/common/test_helpers.h"
#include "tests/unittests/f1ap/cu_cp/f1ap_cu_test_helpers.h"
#include "tests/unittests/ngap/test_helpers.h"
#include "srsran/cu_cp/cu_cp_types.h"
#include "srsran/support/executors/manual_task_worker.h"
#include <gtest/gtest.h>

namespace srsran {
namespace srs_cu_cp {

/// Fixture class for CU-CP test
class cu_cp_test : public ::testing::Test
{
protected:
  cu_cp_test();
  ~cu_cp_test() override;

  void attach_ue(gnb_du_ue_f1ap_id_t du_ue_id, gnb_cu_ue_f1ap_id_t cu_ue_id, rnti_t crnti, du_index_t du_index);
  void authenticate_ue(amf_ue_id_t         amf_ue_id,
                       ran_ue_id_t         ran_ue_id,
                       du_index_t          du_index,
                       gnb_du_ue_f1ap_id_t du_ue_id,
                       gnb_cu_ue_f1ap_id_t cu_ue_id);
  void setup_security(amf_ue_id_t         amf_ue_id,
                      ran_ue_id_t         ran_ue_id,
                      du_index_t          du_index,
                      gnb_du_ue_f1ap_id_t du_ue_id,
                      gnb_cu_ue_f1ap_id_t cu_ue_id);
  void test_preamble_ue_creation(du_index_t          du_index,
                                 gnb_du_ue_f1ap_id_t du_ue_id,
                                 gnb_cu_ue_f1ap_id_t cu_ue_id,
                                 pci_t               pci,
                                 rnti_t              crnti,
                                 amf_ue_id_t         amf_ue_id,
                                 ran_ue_id_t         ran_ue_id);
  bool check_minimal_paging_result();
  bool check_paging_result();

  srslog::basic_logger& test_logger  = srslog::fetch_basic_logger("TEST");
  srslog::basic_logger& cu_cp_logger = srslog::fetch_basic_logger("CU-CP");

  dummy_e1ap_pdu_notifier e1ap_pdu_notifier;
  dummy_ngap_amf_notifier ngap_amf_notifier;

  manual_task_worker ctrl_worker{128};

  std::unique_ptr<cu_cp> cu_cp_obj;

  dummy_cu_cp_f1c_gateway f1c_gw;
};

} // namespace srs_cu_cp
} // namespace srsran
