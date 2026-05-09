#pragma once

// =====================================================================
//  AudioController — VoLaura ses pipeline'ı (mic capture + playback)
// ---------------------------------------------------------------------
//  Kapsam:
//   • Mic capture (sesli kanal + 1:1 çağrı)
//   • Opus encode/decode (kanal + çağrı, per-peer decoder)
//   • Audio sinks (per-peer playback)
//   • DSP pipeline: SpeexAEC + AGC + Dereverb + RNNoise + MicGain
//   • Per-user playback gain (volume)
//   • Mute / Deafen
//   • Voice activity detection (RMS threshold)
//
//  Kullanım:
//   AudioController bir QObject. UI tarafı (MainWindow) sadece signals/slots
//   üzerinden konuşur. Network paketleri (Opus) ham QByteArray olarak
//   alıp/verir; signaling protokolünü bilmez.
//
//  NOT: Bu sınıf adım adım MainWindow'dan extract edilmektedir. Şu an
//  iskelet halinde; geçici olarak hem yeni hem eski kod paralel çalışır.
// =====================================================================

#include <QObject>
#include <QByteArray>
#include <QMap>
#include <QPointer>
#include <QString>
#include <memory>

#include "opus_codec.h"
#include "noise_suppressor.h"
#include "audio_processor.h"

class QAudioSource;
class QAudioSink;
class QIODevice;

class AudioController : public QObject {
    Q_OBJECT
public:
    explicit AudioController(QObject *parent = nullptr);
    ~AudioController() override;

    // -------- Konfigurasyon (Settings dialog'undan çağrılır) --------
    void setMicDeviceId(const QByteArray &id);
    void setSpeakerDeviceId(const QByteArray &id);
    void setMicGain(float gain);                  // 0.3..3.0
    void setBitrate(int bps);                     // 16k..192k
    void setAecEnabled(bool on);
    void setAgcEnabled(bool on);
    void setNoiseSuppressionEnabled(bool on);

    // -------- Mute / Deafen --------
    void setMuted(bool on);
    void setDeafened(bool on);
    bool isMuted() const     { return muted_; }
    bool isDeafened() const  { return deafened_; }

    // -------- Per-user playback volume (0..2) --------
    void  setUserVolume(const QString &username, float v);  // 1.0 = normal
    float getUserVolume(const QString &username) const;

    // -------- Username eşleme tabloları (gain için) --------
    void setUsernameForUserId(qint64 userId, const QString &username);
    void setUsernameForParticipant(const QString &participantId, const QString &username);
    void clearUsernameMaps();

    // -------- Sesli kanal (channel) --------
    void startChannelCapture();
    void stopChannelCapture();
    bool isChannelCapturing() const { return channelCapturing_; }
    // Network'ten gelen Opus paketi
    void onChannelOpusReceived(qint64 userId, const QByteArray &opus);
    void removeChannelPeer(qint64 userId);
    void stopAllChannelPlayback();

    // -------- 1:1 çağrı (room) --------
    void startCallCapture();
    void stopCallCapture();
    bool isCallCapturing() const { return callCapturing_; }
    void onCallOpusReceived(const QString &participantId, const QByteArray &opus);
    void removeCallPeer(const QString &participantId);
    void stopAllCallPlayback();

    // -------- Tanı bilgisi --------
    bool isReady() const;

signals:
    // Mic'ten encode edilen Opus paketi — UI/Net layer relay etmeli
    void channelOpusFrameReady(const QByteArray &opus);
    void callOpusFrameReady(const QByteArray &opus);

    // Voice activity (kullanıcı konuşuyor mu)
    void selfSpeaking(bool active);
    void peerSpeaking(const QString &username, bool active);

    // Hata / bilgi
    void warning(const QString &message);

private:
    // Settings cache
    QByteArray micDeviceId_;
    QByteArray speakerDeviceId_;
    float      micGain_       = 1.0f;
    int        bitrate_       = 64000;
    bool       aecEnabled_    = true;
    bool       agcEnabled_    = true;
    bool       nsEnabled_     = true;
    bool       muted_         = false;
    bool       deafened_      = false;

    // Per-user playback gain (username → 0..2; default 1.0)
    QMap<QString, float> userVolume_;
    // userId → username (channel)
    QMap<qint64, QString> userIdToName_;
    // participantId → username (call)
    QMap<QString, QString> participantToName_;

    // Channel capture state
    bool                 channelCapturing_ = false;
    QPointer<QAudioSource> channelSource_;
    QIODevice           *channelSourceIO_ = nullptr;
    QByteArray           channelMicBuffer_;
    QMap<qint64, QPointer<QAudioSink>> channelSinks_;
    QMap<qint64, QIODevice*>           channelSinkIO_;
    QMap<qint64, std::shared_ptr<OpusDecoderWrapper>> channelDecoders_;

    // Call capture state
    bool                 callCapturing_ = false;
    QPointer<QAudioSource> callSource_;
    QIODevice           *callSourceIO_ = nullptr;
    QByteArray           callMicBuffer_;
    QMap<QString, QPointer<QAudioSink>> callSinks_;
    QMap<QString, QIODevice*>           callSinkIO_;
    QMap<QString, std::shared_ptr<OpusDecoderWrapper>> callDecoders_;

    // DSP pipeline (paylaşımlı)
    OpusEncoderWrapper encoder_;
    NoiseSuppressor    noiseSuppressor_;
    AudioProcessor     audioProcessor_;

    // Helpers
    QAudioSink *getOrCreateChannelSink(qint64 userId);
    QAudioSink *getOrCreateCallSink(const QString &participantId);
    QByteArray  applyPlaybackGain(const QByteArray &pcm, const QString &username);
    void        ensureDspReady();
};
