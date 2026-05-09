#pragma once
#include <QByteArray>

// RNNoise sarmalayıcısı — ML tabanlı gürültü bastırma (klavye, fan, klima, dış ortam).
// 48 kHz mono S16 PCM ile çalışır. Frame size = 480 sample (10 ms).
// Opus encoder'ın 960 sample (20 ms) frame'lerini 2x ardışık 480 sample
// olarak işliyoruz.
//
// Kullanım:
//   NoiseSuppressor ns; ns.init();
//   QByteArray clean = ns.processFrame(pcmS16); // 1920 byte → 1920 byte

struct DenoiseState; // RNNoise'tan opaque

class NoiseSuppressor {
public:
    NoiseSuppressor();
    ~NoiseSuppressor();
    bool init();
    void close();
    bool isReady() const { return state_ != nullptr; }
    // 1920 byte (960 sample S16 mono @48kHz) bekler. İçeride 2 x 480 sample işler.
    // Aynı boyutta clean PCM döner.
    QByteArray processFrame(const QByteArray &pcmS16);
    // Suppression açık/kapalı (CPU tasarrufu için kapatılabilir).
    void setEnabled(bool on) { enabled_ = on; }
    bool isEnabled() const { return enabled_; }
private:
    DenoiseState *state_ = nullptr;
    bool enabled_ = true;
};
