/*
 *
 * Copyright 2013-2022 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

/// \file
/// \brief Demodulation mapper unit test.
///
/// The test takes as input vectors containing noisy modulated symbols and the corresponding noise variances. The
/// symbols are demodulated and the resulting bits (both soft and hard versions) are compared with the expected values,
/// also provided by test vectors.

#include "demodulation_mapper_test_data.h"
#include "srsgnb/phy/upper/channel_modulation/channel_modulation_factories.h"
#include "srsgnb/support/srsgnb_test.h"
#include <gtest/gtest.h>
#include <random>

using namespace srsgnb;

namespace srsgnb {
std::ostream& operator<<(std::ostream& os, const test_case_t& tc)
{
  return os << fmt::format("{} modulation, {} symbols", to_string(tc.scheme), tc.nsymbols);
}

} // namespace srsgnb
namespace {

class DemodulatorFixture : public ::testing::TestWithParam<test_case_t>
{
protected:
  static void SetUpTestSuite()
  {
    if (factory) {
      return;
    }
    factory = create_channel_modulation_sw_factory();
    ASSERT_NE(factory, nullptr);
  }

  void SetUp() override
  {
    // Assert factory again for compatibility with GTest < 1.11.
    ASSERT_NE(factory, nullptr);

    // Create a demodulator.
    test_case_t test_case = GetParam();
    demodulator           = factory->create_demodulation_mapper();
    ASSERT_NE(demodulator, nullptr);

    // Read test input and output.
    mod = test_case.scheme;

    unsigned nof_symbols = test_case.nsymbols;
    symbols              = test_case.symbols.read();
    ASSERT_EQ(symbols.size(), nof_symbols) << "Error reading modulated symbols.";

    noise_var = test_case.noise_var.read();
    ASSERT_EQ(noise_var.size(), nof_symbols) << "Error reading noise variances.";

    ASSERT_TRUE(std::all_of(noise_var.cbegin(), noise_var.cend(), [](float f) { return f > 0; }))
        << "Noise variances should take positive values.";

    unsigned nof_bits  = nof_symbols * get_bits_per_symbol(mod);
    soft_bits_expected = test_case.soft_bits.read();
    ASSERT_EQ(soft_bits_expected.size(), nof_bits) << "Error reading soft bits.";
  }

  static std::shared_ptr<channel_modulation_factory> factory;
  std::unique_ptr<demodulation_mapper>               demodulator        = nullptr;
  std::vector<cf_t>                                  symbols            = {};
  std::vector<float>                                 noise_var          = {};
  std::vector<log_likelihood_ratio>                  soft_bits_expected = {};
  modulation_scheme                                  mod                = {};
};

std::shared_ptr<channel_modulation_factory> DemodulatorFixture::factory = nullptr;

TEST_P(DemodulatorFixture, DemodulatorTest)
{
  test_case_t test_case = GetParam();

  // Load expected results.
  unsigned nof_symbols = test_case.nsymbols;
  unsigned nof_bits    = nof_symbols * get_bits_per_symbol(mod);

  const std::vector<uint8_t> hard_bits_true = test_case.hard_bits.read();
  ASSERT_EQ(hard_bits_true.size(), nof_bits) << "Error reading hard bits.";

  // Run test.
  std::vector<log_likelihood_ratio> soft_bits(nof_bits);
  demodulator->demodulate_soft(soft_bits, symbols, noise_var, mod);

  ASSERT_EQ(span<const log_likelihood_ratio>(soft_bits_expected), span<const log_likelihood_ratio>(soft_bits))
      << fmt::format("Soft bits are not sufficiently precise (mod={}).", to_string(test_case.scheme));

  std::vector<uint8_t> hard_bits(nof_bits);
  std::transform(
      soft_bits.cbegin(), soft_bits.cend(), hard_bits.begin(), [](log_likelihood_ratio a) { return a.to_hard_bit(); });
  ASSERT_EQ(span<const uint8_t>(hard_bits), span<const uint8_t>(hard_bits_true)) << "Hard bits do not match.";
}

// Check that noise_var equal to zero implies LLR = 0.
TEST_P(DemodulatorFixture, DemodulatorNoiseZero)
{
  // By taking 12 symbols, we test both the AVX2 implementation (first 8 symbols) and the classic one (last 4 symbols).
  unsigned    nof_symbols = 12;
  span<float> noise_bad   = span<float>(noise_var).first(nof_symbols);
  // Set even-indexed noise variances to zero.
  for (unsigned i_symbol = 0; i_symbol < nof_symbols; i_symbol += 2) {
    noise_bad[i_symbol] = 0;
  }

  unsigned                          bits_per_symbol = get_bits_per_symbol(mod);
  std::vector<log_likelihood_ratio> soft_bits(nof_symbols * bits_per_symbol);
  demodulator->demodulate_soft(soft_bits, span<const cf_t>(symbols).first(nof_symbols), noise_bad, mod);

  bool are_zeros_ok = true, are_others_ok = true;
  for (unsigned i_symbol = 0, offset = 0; i_symbol < nof_symbols; i_symbol += 2) {
    span<const log_likelihood_ratio> local_soft =
        span<const log_likelihood_ratio>(soft_bits).subspan(offset, bits_per_symbol);
    offset += bits_per_symbol;
    are_zeros_ok = are_zeros_ok && std::all_of(local_soft.begin(), local_soft.end(), [](log_likelihood_ratio a) {
                     return (a.to_int() == 0);
                   });
    local_soft   = span<const log_likelihood_ratio>(soft_bits).subspan(offset, bits_per_symbol);
    span<const log_likelihood_ratio> true_soft =
        span<const log_likelihood_ratio>(soft_bits_expected).subspan(offset, bits_per_symbol);
    offset += bits_per_symbol;
    are_others_ok = are_others_ok && std::equal(local_soft.begin(), local_soft.end(), true_soft.begin());
  }
  ASSERT_TRUE(are_zeros_ok) << "Division by zero went wrong.";
  ASSERT_TRUE(are_others_ok) << "Division by zero should not affect other soft bits.";
}

// Check that noise_var equal to infinity implies LLR = 0.
TEST_P(DemodulatorFixture, DemodulatorNoiseInfinity)
{
  // By taking 12 symbols, we test both the AVX2 implementation (first 8 symbols) and the classic one (last 4 symbols).
  unsigned    nof_symbols = 12;
  span<float> noise_bad   = span<float>(noise_var).first(nof_symbols);
  // Set even-indexed noise variances to infinity.
  for (unsigned i_symbol = 0; i_symbol < nof_symbols; i_symbol += 2) {
    noise_bad[i_symbol] = std::numeric_limits<float>::infinity();
  }

  unsigned                          bits_per_symbol = get_bits_per_symbol(mod);
  std::vector<log_likelihood_ratio> soft_bits(nof_symbols * bits_per_symbol);
  demodulator->demodulate_soft(soft_bits, span<const cf_t>(symbols).first(nof_symbols), noise_bad, mod);

  bool are_zeros_ok = true, are_others_ok = true;
  for (unsigned i_symbol = 0, offset = 0; i_symbol < nof_symbols; i_symbol += 2) {
    span<const log_likelihood_ratio> local_soft =
        span<const log_likelihood_ratio>(soft_bits).subspan(offset, bits_per_symbol);
    offset += bits_per_symbol;
    are_zeros_ok = are_zeros_ok && std::all_of(local_soft.begin(), local_soft.end(), [](log_likelihood_ratio a) {
                     return (a.to_int() == 0);
                   });
    local_soft   = span<const log_likelihood_ratio>(soft_bits).subspan(offset, bits_per_symbol);
    span<const log_likelihood_ratio> true_soft =
        span<const log_likelihood_ratio>(soft_bits_expected).subspan(offset, bits_per_symbol);
    offset += bits_per_symbol;
    are_others_ok = are_others_ok && std::equal(local_soft.begin(), local_soft.end(), true_soft.begin());
  }
  ASSERT_TRUE(are_zeros_ok) << "Division by infinity went wrong.";
  ASSERT_TRUE(are_others_ok) << "Division by infinity should not affect other soft bits.";
}

INSTANTIATE_TEST_SUITE_P(DemodulatorSuite, DemodulatorFixture, ::testing::ValuesIn(demodulation_mapper_test_data));

} // namespace