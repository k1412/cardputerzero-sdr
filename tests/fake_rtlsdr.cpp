// SPDX-License-Identifier: MIT

#include <algorithm>
#include <cstdint>
#include <cstring>

extern "C" {

struct rtlsdr_dev {
    uint32_t center_frequency{0};
    uint32_t sample_rate{0};
    int gain_mode{0};
    int gain{0};
};

uint32_t rtlsdr_get_device_count() {
    return 1;
}

const char* rtlsdr_get_device_name(uint32_t index) {
    return index == 0 ? "Fake RTL2832U" : nullptr;
}

int rtlsdr_get_device_usb_strings(uint32_t index, char* manufacturer, char* product, char* serial) {
    if (index != 0) return -1;
    std::strcpy(manufacturer, "Zero SDR Tests");
    std::strcpy(product, "RTL2832U FC0012");
    std::strcpy(serial, "TEST0001");
    return 0;
}

int rtlsdr_open(rtlsdr_dev** output, uint32_t index) {
    if (!output || index != 0) return -1;
    *output = new rtlsdr_dev{};
    return 0;
}

int rtlsdr_close(rtlsdr_dev* device) {
    delete device;
    return 0;
}

int rtlsdr_set_center_freq(rtlsdr_dev* device, uint32_t value) {
    if (!device || value == 0) return -1;
    device->center_frequency = value;
    return 0;
}

int rtlsdr_set_sample_rate(rtlsdr_dev* device, uint32_t value) {
    if (!device || value == 0) return -1;
    device->sample_rate = value;
    return 0;
}

int rtlsdr_set_tuner_gain_mode(rtlsdr_dev* device, int value) {
    if (!device) return -1;
    device->gain_mode = value;
    return 0;
}

int rtlsdr_set_tuner_gain(rtlsdr_dev* device, int value) {
    if (!device) return -1;
    device->gain = value;
    return 0;
}

int rtlsdr_set_agc_mode(rtlsdr_dev* device, int) {
    return device ? 0 : -1;
}

int rtlsdr_reset_buffer(rtlsdr_dev* device) {
    return device ? 0 : -1;
}

int rtlsdr_read_sync(rtlsdr_dev* device, void* output, int length, int* bytes_read) {
    if (!device || !output || length <= 0 || !bytes_read) return -1;
    auto* bytes = static_cast<uint8_t*>(output);
    for (int index = 0; index < length; ++index) {
        bytes[index] = static_cast<uint8_t>(index & 0xff);
    }
    *bytes_read = length;
    return 0;
}

} // extern "C"
