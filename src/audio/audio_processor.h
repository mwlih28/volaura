#pragma once
#include <QByteArray>
#include <QMutex>

// Speex DSP sarmalayıcısı:
//   - AEC (Acoustic Echo Cancellation) — yankı bastırma (hoparlörden mic'e dönen ses)
//   - AGC (Automatic Gain Control) — ses düzeyini sabit tut
//   - Preprocess (ek noise suppression, dereverb)
//
// Pipeline:
//   far-end (peer audio) ─► pushFarEnd()
//   near-end (mic) ─► processCapture() ─► AEC + AGC ─► clean PCM
//
// 48 kHz mono S16, 480 sample / 10ms frame.

struct SpeexEchoState_;
struct SpeexPreprocessState_;

class AudioProcessor {
public:
    AudioProcessor();
    ~AudioProcessor();

    // sampleRate ve frameSize değiştirilmez (48kHz, 480 sample = 10ms).
    // filterLengthMs: yankı kuyruğu uzunluğu (default 200ms).
    bool init(int filterLengthMs = 200);
    void close();
    bool isReady() const { return echo_ != nullptr && pre_ != nullptr; }

    // Hoparlöre giden mixed PCM'i (tüm peer'ların toplamı) buraya yaz.
    // Gerektikçe internal ring'e biriktirilir; AEC processCapture sırasında çeker.
    void pushFarEnd(const QByteArray &pcmS16Mono);

    // 1920 byte (960 sample / 20ms) S16 mono mic frame işler.
    // İçeride 2 × 480 sample chunk halinde AEC + Preprocess uygulanır.
    QByteArray processCapture(const QByteArray &micPcm);

    // AGC açık/kapalı (default açık).
    void setAgcEnabled(bool on);
    void setAecEnabled(bool on) { aecEnabled_ = on; }
    bool isAecEnabled() const { return aecEnabled_; }
    bool isAgcEnabled() const { return agcEnabled_; }

    static constexpr int kSampleRate = 48000;
    static constexpr int kFrameSamples = 480;          // 10 ms
    static constexpr int kFrameBytes   = kFrameSamples * 2;
    static constexpr int kInputBytes   = 1920;         // 20 ms (2 frames)

private:
    SpeexEchoState_      *echo_ = nullptr;
    SpeexPreprocessState_*pre_  = nullptr;
    // Far-end ring (mono S16), bytes
    QByteArray farEndRing_;
    QMutex     farEndMutex_;
    bool aecEnabled_ = true;
    bool agcEnabled_ = true;
};
