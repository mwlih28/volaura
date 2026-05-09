#include "noise_suppressor.h"
#include <QDebug>
#include <cstdint>

#ifdef VOLAURA_HAVE_RNNOISE
extern "C" {
#include <rnnoise.h>
}
#endif

using i16 = int16_t;

NoiseSuppressor::NoiseSuppressor() = default;
NoiseSuppressor::~NoiseSuppressor() { close(); }

bool NoiseSuppressor::init() {
#ifdef VOLAURA_HAVE_RNNOISE
    close();
    state_ = rnnoise_create(nullptr);
    if (!state_) {
        qWarning() << "[rnnoise] create failed";
        return false;
    }
    return true;
#else
    return false;
#endif
}

void NoiseSuppressor::close() {
#ifdef VOLAURA_HAVE_RNNOISE
    if (state_) {
        rnnoise_destroy(state_);
        state_ = nullptr;
    }
#endif
}

QByteArray NoiseSuppressor::processFrame(const QByteArray &pcm) {
#ifdef VOLAURA_HAVE_RNNOISE
    if (!enabled_ || !state_) return pcm;
    // 1920 byte = 960 sample S16. RNNoise frame size = 480 sample → 2 frames.
    constexpr int kRnnFrame = 480;
    constexpr int kInputSamples = 960;
    if (pcm.size() < int(kInputSamples * 2)) return pcm;

    const i16 *in16 = reinterpret_cast<const i16*>(pcm.constData());
    QByteArray out;
    out.resize(int(kInputSamples * 2));
    i16 *out16 = reinterpret_cast<i16*>(out.data());

    float buf[kRnnFrame];
    for (int chunk = 0; chunk < 2; ++chunk) {
        const int off = chunk * kRnnFrame;
        for (int i = 0; i < kRnnFrame; ++i) {
            buf[i] = float(in16[off + i]);
        }
        rnnoise_process_frame(state_, buf, buf);
        for (int i = 0; i < kRnnFrame; ++i) {
            float v = buf[i];
            if (v >  32767.f) v =  32767.f;
            if (v < -32768.f) v = -32768.f;
            out16[off + i] = i16(v);
        }
    }
    return out;
#else
    return pcm;
#endif
}
