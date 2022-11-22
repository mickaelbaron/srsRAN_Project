/*
 *
 * Copyright 2013-2022 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "e1_cu_cp_test_helpers.h"
#include "lib/e1_interface/common/e1ap_asn1_packer.h"
#include "test_helpers.h"
#include "unittests/gateways/test_helpers.h"
#include <gtest/gtest.h>

using namespace srsgnb;
using namespace srs_cu_cp;

/// Fixture class for E1AP ASN1 packer
class e1_asn1_packer_test : public ::testing::Test
{
protected:
  void SetUp() override
  {
    srslog::fetch_basic_logger("TEST").set_level(srslog::basic_levels::debug);
    srslog::fetch_basic_logger("E1-ASN1-PCK").set_level(srslog::basic_levels::debug);
    srslog::init();

    gw     = std::make_unique<dummy_network_gateway_data_handler>();
    e1     = std::make_unique<dummy_e1_message_handler>();
    packer = std::make_unique<srsgnb::e1ap_asn1_packer>(*gw, *e1);
  }

  void TearDown() override
  {
    // flush logger after each test
    srslog::flush();
  }

  std::unique_ptr<dummy_network_gateway_data_handler> gw;
  std::unique_ptr<dummy_e1_message_handler>           e1;
  std::unique_ptr<srsgnb::e1ap_asn1_packer>           packer;
  srslog::basic_logger&                               test_logger = srslog::fetch_basic_logger("TEST");
};

/// Test successful packing and unpacking
TEST_F(e1_asn1_packer_test, when_packing_successful_then_unpacking_successful)
{
  // Action 1: Create valid e1 message
  e1_message e1_setup_request = generate_valid_cu_up_e1_setup_request();

  // Action 2: Pack message and forward to gateway
  packer->handle_message(e1_setup_request);

  // Action 3: Unpack message and forward to e1
  packer->handle_packed_pdu(std::move(gw->last_pdu));

  // Assert that the originally created message is equal to the unpacked message
  ASSERT_EQ(e1->last_msg.pdu.type(), e1_setup_request.pdu.type());
}

/// Test unsuccessful packing
TEST_F(e1_asn1_packer_test, when_packing_unsuccessful_then_message_not_forwarded)
{
  // Action 1: Generate, pack and forward valid message to bring gw into known state
  e1_message e1_setup_request = generate_valid_cu_up_e1_setup_request();
  packer->handle_message(e1_setup_request);
  // store size of valid pdu
  int valid_pdu_size = gw->last_pdu.length();

  // Action 2: Create invalid e1 message
  e1_message e1_msg               = generate_cu_up_e1_setup_request_base();
  auto&      setup_req            = e1_msg.pdu.init_msg().value.gnb_cu_up_e1_setup_request();
  setup_req->supported_plmns.id   = ASN1_E1AP_ID_SUPPORTED_PLMNS;
  setup_req->supported_plmns.crit = asn1::crit_opts::reject;

  // Action 3: Pack message and forward to gateway
  packer->handle_message(e1_msg);

  // check that msg was not forwarded to gw
  ASSERT_EQ(gw->last_pdu.length(), valid_pdu_size);
}

// TODO: test unsuccessful unpacking
