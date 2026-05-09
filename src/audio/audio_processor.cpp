#include "audio_processor.h"
#include <QDebug>
#include <cstdint>
#include <cstring>

#ifdef VOLAURA_HAVE_SPEEXDSP
extern "C" {
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>
}
#endif

using i16 = int16_t;

AudioProcessor::AudioProcessor() = default;
AudioProcessor::~AudioProcessor() { close(); }

bool AudioProcessor::init(int filterLengthMs) {
#ifdef VOLAURA_HAVE_SPEEXDSP
    close();
    const int frameSize  = kFrameSamples;            // 480
    const int filterTaps = (kSampleRate * filterLengthMs) / 1000; // örn 9600 @200ms
    echo_ = reinterpret_cast<SpeexEchoState_*>(speex_echo_state_init(frameSize, filterTaps));
    if (!echo_) { qWarning() << "[speexdsp] echo init fail"; return false; }
    int sr = kSampleRate;
    speex_echo_ctl(reinterpret_cast<SpeexEchoState*>(echo_), SPEEX_ECHO_SET_SAMPLING_RATE, &sr);

    pre_ = reinterpret_cast<SpeexPreprocessState_*>(
        speex_preprocess_state_init(frameSize, kSampleRate));
    if (!pre_) {
        qWarning() << "[speexdsp] preprocess init fail";
        speex_echo_state_destroy(reinterpret_cast<SpeexEchoState*>(echo_));
        echo_ = nullptr;
        return false;
    }
    // AEC bağla
    speex_preprocess_ctl(reinterpret_cast<SpeexPreprocessState*>(pre_),
                         SPEEX_PREPROCESS_SET_ECHO_STATE, echo_);
    // AGC: konuşma seviyesini ~ -6 dBFS hedefle
    int on = 1;
    speex_preprocess_ctl(reinterpret_cast<SpeexPreprocessState*>(pre_),
                         SPEEX_PREPROCESS_SET_AGC, &on);
    int target = 24000;
    speex_preprocess_ctl(reinterpret_cast<SpeexPreprocessState*>(pre_),
                         SPEEX_PREPROCESS_SET_AGC_TARGET, &target);
    int maxGain = 30;
    speex_preprocess_ctl(reinterpret_cast<SpeexPreprocessState*>(pre_),
                         SPEEX_PREPROCESS_SET_AGC_MAX_GAIN, &maxGain);
    // Speex NS hafif aktif (RNNoise zaten var ama yedek olarak kalsın)
    speex_preprocess_ctl(reinterpret_cast<SpeexPreprocessState*>(pre_),
                         SPEEX_PREPROCESS_SET_DENOISE, &on);
    int suppress = -25; // dB; çok agresif değil
    speex_preprocess_ctl(reinterpret_cast<SpeexPreprocessState*>(pre_),
                         SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &suppress);
    // Yankı bastırma sonrası rezidüel echo bastırma
    int echoSuppress       = -40;
    int echoSuppressActive = -15;
    speex_preprocess_ctl(reinterpret_cast<SpeexPreprocessState*>(pre_),
                         SPEEX_PREPROCESS_SET_ECHO_SUPPRESS, &echoSuppress);
    speex_preprocess_ctl(reinterpret_cast<SpeexPreprocessState*>(pre_),
                         SPEEX_PREPROCESS_SET_ECHO_SUPPRESS_ACTIVE, &echoSuppressActive);
    // Dereverb (oda yankısı azalt)
    speex_preprocess_ctl(reinterpret_cast<SpeexPreprocessState*>(pre_),
                         SPEEX_PREPROCESS_SET_DEREVERB, &on);
    qInfo() << "[speexdsp] AEC+AGC+NS hazır (filter" << filterTaps << "tap @"
            << filterLengthMs << "ms)";
    return true;
#else
    Q_UNUSED(filterLengthMs);
    return false;
#endif
}

void AudioProcessor::close() {
#ifdef VOLAURA_HAVE_SPEEXDSP
    if (pre_) {
        speex_preprocess_state_destroy(reinterpret_cast<SpeexPreprocessState*>(pre_));
        pre_ = nullptr;
    }
    if (echo_) {
        speex_echo_state_destroy(reinterpret_cast<SpeexEchoState*>(echo_));
        echo_ = nullptr;
    }
#endif
    QMutexLocker lk(&farEndMutex_);
    farEndRing_.clear();
}

void AudioProcessor::setAgcEnabled(bool on) {
    agcEnabled_ = on;
#ifdef VOLAURA_HAVE_SPEEXDSP
    if (pre_) {
        int v = on ? 1 : 0;
        speex_preprocess_ctl(reinterpret_cast<SpeexPreprocessState*>(pre_),
                             SPEEX_PREPROCESS_SET_AGC, &v);
    }
#endif
}

void AudioProcessor::pushFarEnd(const QByteArray &pcm) {
    if (pcm.isEmpty()) return;
    QMutexLocker lk(&farEndMutex_);
    farEndRing_.append(pcm);
    // Aşırı birikme olmasın — max 500ms (48000 * 2 byte * 0.5 = 48000)
    const int maxBytes = kSampleRate * 2 / 2; // 500 ms mono S16
    if (farEndRing_.size() > maxBytes) {
        farEndRing_.remove(0, farEndRing_.size() - maxBytes);
    }
}

QByteArray AudioProcessor::processCapture(const QByteArray &mic) {
#ifdef VOLAURA_HAVE_SPEEXDSP
    if (!isReady() || mic.size() < kInputBytes) return mic;

    QByteArray out;
    out.resize(kInputBytes);
    const i16 *micPtr = reinterpret_cast<const i16*>(mic.constData());
    i16 *outPtr = reinterpret_cast<i16*>(out.data());

    // 2 × 10ms frame
    for (int chunk = 0; chunk < 2; ++chunk) {
        const int off = chunk * kFrameSamples;

        // Far-end frame'i çek (varsa); yoksa sıfır ile doldur (AEC sıfırla bypass'a yakın)
        i16 farFrame[kFrameSamples];
        {
            QMutexLocker lk(&farEndMutex_);
            const int neededBytes = kFrameBytes;
            if (farEndRing_.size() >= neededBytes) {
                std::memcpy(farFrame, farEndRing_.constData(), neededBytes);
                farEndRing_.remove(0, neededBytes);
            } else {
                std::memset(farFrame, 0, sizeof(farFrame));
            }
        }

        i16 cleanFrame[kFrameSamples];
        if (aecEnabled_) {
            speex_echo_cancellation(reinterpret_cast<SpeexEchoState*>(echo_),
                                    micPtr + off, farFrame, cleanFrame);
        } else {
            std::memcpy(cleanFrame, micPtr + off, sizeof(cleanFrame));
        }

        // Preprocess (NS + AGC + dereverb + residual echo suppress)
        speex_preprocess_run(reinterpret_cast<SpeexPreprocessState*>(pre_), cleanFrame);

        std::memcpy(outPtr + off, cleanFrame, sizeof(cleanFrame));
    }
    return out;
#else
    return mic;
#endif
}
