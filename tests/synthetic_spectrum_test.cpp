// SPDX-License-Identifier: MIT

#include "synthetic_spectrum.h"

#include <algorithm>
#include <cassert>

int main() {
    dsp::SyntheticSpectrum spectrum;
    const auto on_station = spectrum.next(97'400'000);
    const auto off_station = spectrum.next(99'000'000);

    assert(on_station.sequence == 1);
    assert(off_station.sequence == 2);
    assert(std::all_of(on_station.level.begin(), on_station.level.end(), [](uint8_t level) {
        return level <= 100;
    }));

    const auto center = on_station.level[on_station.level.size() / 2];
    assert(center > 70);
    assert(center > off_station.level[off_station.level.size() / 2]);
}
