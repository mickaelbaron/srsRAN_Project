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
/// \brief LDPC encoder - Declaration of generic implementation.
#pragma once

#include "ldpc_encoder_impl.h"

namespace srsran {

/// Generic LDPC encoder implementation without any optimization.
class ldpc_encoder_generic : public ldpc_encoder_impl
{
  void select_strategy() override;
  void load_input(span<const uint8_t> in) override { message = in; }
  void preprocess_systematic_bits() override;
  void encode_high_rate() override { (this->*high_rate)(); }
  void encode_ext_region() override;
  void write_codeblock(span<uint8_t> out) override;

  /// Pointer type shortcut.
  using high_rate_strategy = void (ldpc_encoder_generic::*)();
  /// Pointer to a high-rate strategy member.
  high_rate_strategy high_rate;

  /// Carries out the high-rate region encoding for BG1 and lifting size index 6.
  void high_rate_bg1_i6();
  /// Carries out the high-rate region encoding for BG1 and lifting size index in {0, 1, 2, 3, 4, 5, 7}.
  void high_rate_bg1_other();
  /// Carries out the high-rate region encoding for BG2 and lifting size index in {3, 7}.
  void high_rate_bg2_i3_7();
  /// Carries out the high-rate region encoding for BG2 and lifting size index in {0, 1, 2, 4, 5, 6}.
  void high_rate_bg2_other();

  /// Local copy of the message to encode.
  span<const uint8_t> message = {};
  // Set up registers for the largest LS.
  /// Register to store auxiliary computation results.
  std::array<std::array<uint8_t, ldpc::MAX_LIFTING_SIZE>, ldpc::MAX_BG_M> auxiliary = {};
  /// Register to store computed encoded bits.
  std::array<uint8_t, static_cast<size_t>(ldpc::MAX_BG_N_FULL* ldpc::MAX_LIFTING_SIZE)> codeblock = {};
};

} // namespace srsran