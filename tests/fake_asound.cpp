// SPDX-License-Identifier: MIT

#include <cstdint>
#include <cstring>

extern "C" {

struct snd_pcm {
    unsigned int sample_rate{0};
    bool fail_writes{false};
};

int snd_pcm_open(snd_pcm** output, const char* device_name, int, int) {
    if (!output) return -1;
    *output = new snd_pcm{};
    (*output)->fail_writes = device_name && std::strcmp(device_name, "fail") == 0;
    return 0;
}

int snd_pcm_close(snd_pcm* pcm) {
    delete pcm;
    return 0;
}

int snd_pcm_set_params(snd_pcm* pcm, int, int, unsigned int channels,
                       unsigned int sample_rate, int, unsigned int) {
    if (!pcm || channels != 1 || sample_rate != 32'000) return -2;
    pcm->sample_rate = sample_rate;
    return 0;
}

long snd_pcm_writei(snd_pcm* pcm, const void*, unsigned long frames) {
    if (!pcm) return -3;
    return pcm->fail_writes ? -5 : static_cast<long>(frames);
}

int snd_pcm_recover(snd_pcm* pcm, int, int) {
    return pcm && !pcm->fail_writes ? 0 : -1;
}
int snd_pcm_drop(snd_pcm* pcm) { return pcm ? 0 : -1; }
const char* snd_strerror(int) { return "fake ALSA error"; }

} // extern "C"
