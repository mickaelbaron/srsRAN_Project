/*
 *
 * Copyright 2021-2024 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "helpers.h"
#include "srsran/fapi_adaptor/mac/messages/ssb.h"
#include "srsran/mac/mac_cell_result.h"
#include <gtest/gtest.h>

using namespace srsran;
using namespace fapi_adaptor;
using namespace unittests;

TEST(mac_fapi_ssb_pdu_conversor_test, valid_pdu_should_pass)
{
  srsran::dl_ssb_pdu pdu = build_valid_dl_ssb_pdu();

  fapi::dl_ssb_pdu fapi_pdu;
  convert_ssb_mac_to_fapi(fapi_pdu, pdu);

  ASSERT_EQ(pdu.pci, fapi_pdu.phys_cell_id);
  ASSERT_EQ(static_cast<unsigned>(pdu.pss_to_sss_epre), static_cast<unsigned>(fapi_pdu.beta_pss_profile_nr));
  ASSERT_EQ(pdu.ssb_index, fapi_pdu.ssb_block_index);
  ASSERT_EQ(pdu.subcarrier_offset.to_uint(), fapi_pdu.ssb_subcarrier_offset);
  ASSERT_EQ(pdu.offset_to_pointA.to_uint(), fapi_pdu.ssb_offset_pointA.to_uint());

  // Maintenance v3 parameters.
  ASSERT_EQ(static_cast<unsigned>(pdu.ssb_case), static_cast<unsigned>(fapi_pdu.ssb_maintenance_v3.case_type));
  ASSERT_EQ(pdu.L_max, fapi_pdu.ssb_maintenance_v3.L_max);
  ASSERT_EQ(static_cast<unsigned>(pdu.scs), static_cast<unsigned>(fapi_pdu.ssb_maintenance_v3.scs));

  // MIB.
  ASSERT_EQ(fapi::bch_payload_type::phy_full, fapi_pdu.bch_payload_flag);
  ASSERT_EQ(pdu.mib_data.pdcch_config_sib1, fapi_pdu.bch_payload.phy_mib_pdu.pdcch_config_sib1);
  auto dmrs_typeA_pos = pdu.mib_data.dmrs_typeA_pos == dmrs_typeA_position::pos2 ? fapi::dmrs_typeA_pos::pos2
                                                                                 : fapi::dmrs_typeA_pos::pos3;
  ASSERT_EQ(dmrs_typeA_pos, fapi_pdu.bch_payload.phy_mib_pdu.dmrs_typeA_position);
  ASSERT_EQ(pdu.mib_data.cell_barred, fapi_pdu.bch_payload.phy_mib_pdu.cell_barred);
  ASSERT_EQ(pdu.mib_data.intra_freq_reselection, fapi_pdu.bch_payload.phy_mib_pdu.intrafreq_reselection);
}
