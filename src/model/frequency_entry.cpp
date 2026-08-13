// SPDX-License-Identifier: MIT

#include "frequency_entry.h"

#include "radio_model.h"

#include <charconv>

namespace model {

bool parse_frequency_mhz(std::string_view text, uint32_t& frequency_hz) {
    if (text.empty()) return false;

    const auto decimal = text.find('.');
    if (decimal != std::string_view::npos && text.find('.', decimal + 1) != std::string_view::npos) {
        return false;
    }

    const auto whole_text = text.substr(0, decimal);
    const auto fraction_text = decimal == std::string_view::npos
        ? std::string_view{}
        : text.substr(decimal + 1);
    if (whole_text.empty() || whole_text.size() > 3 || fraction_text.size() > 3) {
        return false;
    }

    uint32_t whole_mhz = 0;
    const auto whole_conversion = std::from_chars(
        whole_text.data(), whole_text.data() + whole_text.size(), whole_mhz);
    if (whole_conversion.ec != std::errc{} ||
        whole_conversion.ptr != whole_text.data() + whole_text.size()) {
        return false;
    }

    uint32_t fractional_khz = 0;
    if (!fraction_text.empty()) {
        const auto fraction_conversion = std::from_chars(
            fraction_text.data(), fraction_text.data() + fraction_text.size(), fractional_khz);
        if (fraction_conversion.ec != std::errc{} ||
            fraction_conversion.ptr != fraction_text.data() + fraction_text.size()) {
            return false;
        }
        for (size_t digit = fraction_text.size(); digit < 3; ++digit) fractional_khz *= 10;
    }

    const uint32_t parsed_hz = whole_mhz * 1'000'000U + fractional_khz * 1'000U;
    if (parsed_hz < RadioModel::kMinimumFrequencyHz ||
        parsed_hz > RadioModel::kMaximumFrequencyHz) {
        return false;
    }
    frequency_hz = parsed_hz;
    return true;
}

} // namespace model
