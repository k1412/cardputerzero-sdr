// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <string_view>

namespace model {

// Parses a user-entered MHz value without floating-point rounding. Accepted
// examples include "97", "97.4", and "103.900".
bool parse_frequency_mhz(std::string_view text, uint32_t& frequency_hz);

} // namespace model
