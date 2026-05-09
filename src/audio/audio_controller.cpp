// =====================================================================
//  AudioController — implementation skeleton (Phase 1)
// ---------------------------------------------------------------------
//  Şu an sadece configurable state + simple delegations. Mic capture
//  ve playback path'leri MainWindow'dan kademeli olarak migrate
//  edilecek. Bu dosya derlenip linklenir, fakat asıl ses akışı hâlâ
//  MainWindow tarafından yönetilir. Sonraki adımlarda
//  startChannelCapture/onChannelOpusReceived gibi metodlar gerçek
//  implementasyonlarını alacak.
// =====================================================================

#include "audio_controller.h"

#include <QAudioSource>
#include <QAudioSink>
#include <QIODevice>
#include <QtMath>
#include <cstdint>

namespace {

// PCM S16 mono için linear gain — saturasyon clipping.
QByteArray applyGain(const QByteArray &pcm, float gain) {
    if (gain == 1.0f || pcm.isEmpty()) return pcm;
    QByteArray out = pcm;
    auto *p = reinterpret_cast<int16_t*>(out.data());
    const int n = out.size() / 2;
    for (int i = 0; i < n; ++i) {
        int v = int(float(p[i]) * gain);
        if (v >  32767) v =  32767;
        else if (v < -32768) v = -32768;
        p[i] = int16_t(v);
    }
    return out;
}

} // namespace

AudioController::AudioController(QObject *parent)
    : QObject(parent) {}

AudioController::~AudioController() {
    stopChannelCapture();
    stopCallCapture();
    stopAllChannelPlayback();
    stopAllCallPlayback();
}

// ---------- Konfigurasyon ----------
void AudioController::setMicDeviceId(const QByteArray &id) {
    micDeviceId_ = id;
}
void AudioController::setSpeakerDeviceId(const QByteArray &id) {
    speakerDeviceId_ = id;
}
void AudioController::setMicGain(float gain) {
    micGain_ = qBound(0.3f, gain, 3.0f);
}
void AudioController::setBitrate(int bps) {
    bitrate_ = qBound(16000, bps, 192000);
}
void AudioController::setAecEnabled(bool on) {
    aecEnabled_ = on;
    audioProcessor_.setAecEnabled(on);
}
void AudioController::setAgcEnabled(bool on) {
    agcEnabled_ = on;
    audioProcessor_.setAgcEnabled(on);
}
void AudioController::setNoiseSuppressionEnabled(bool on) {
    nsEnabled_ = on;
    noiseSuppressor_.setEnabled(on);
}

// ---------- Mute / Deafen ----------
void AudioController::setMuted(bool on)    { muted_ = on; }
void AudioController::setDeafened(bool on) { deafened_ = on; }

// ---------- Per-user volume ----------
void AudioController::setUserVolume(const QString &username, float v) {
    v = qBound(0.0f, v, 2.0f);
    if (qFuzzyCompare(v, 1.0f)) userVolume_.remove(username);
    else userVolume_.insert(username, v);

    // Sink->setVolume sadece 0..1 (>1 PCM amplify ile sağlanır)
    const float sinkVol = qMin(1.0f, v);
    // Channel sinks
    for (auto it = userIdToName_.begin(); it != userIdToName_.end(); ++it) {
        if (it.value() == username) {
            if (auto *sink = channelSinks_.value(it.key()).data()) {
                sink->setVolume(sinkVol);
            }
        }
    }
    // Call sinks
    for (auto it = participantToName_.begin(); it != participantToName_.end(); ++it) {
        if (it.value() == username) {
            if (auto *sink = callSinks_.value(it.key()).data()) {
                sink->setVolume(sinkVol);
            }
        }
    }
}

float AudioController::getUserVolume(const QString &username) const {
    return userVolume_.value(username, 1.0f);
}

void AudioController::setUsernameForUserId(qint64 userId, const QString &username) {
    userIdToName_.insert(userId, username);
}
void AudioController::setUsernameForParticipant(const QString &participantId, const QString &username) {
    participantToName_.insert(participantId, username);
}
void AudioController::clearUsernameMaps() {
    userIdToName_.clear();
    participantToName_.clear();
}

// ---------- Channel ----------
void AudioController::startChannelCapture() {
    // TODO (Phase 2): MainWindow::startVoiceCapture()'tan migrate
    channelCapturing_ = true;
}
void AudioController::stopChannelCapture() {
    // TODO (Phase 2)
    channelCapturing_ = false;
}
void AudioController::onChannelOpusReceived(qint64 /*userId*/, const QByteArray & /*opus*/) {
    // TODO (Phase 3): MainWindow::onVoiceChunkReceived()'tan migrate
}
void AudioController::removeChannelPeer(qint64 userId) {
    if (auto sink = channelSinks_.take(userId).data()) {
        sink->stop();
        sink->deleteLater();
    }
    channelSinkIO_.remove(userId);
    channelDecoders_.remove(userId);
}
void AudioController::stopAllChannelPlayback() {
    for (auto it = channelSinks_.begin(); it != channelSinks_.end(); ++it) {
        if (auto *s = it.value().data()) { s->stop(); s->deleteLater(); }
    }
    channelSinks_.clear();
    channelSinkIO_.clear();
    channelDecoders_.clear();
}

// ---------- Call ----------
void AudioController::startCallCapture() {
    // TODO (Phase 2)
    callCapturing_ = true;
}
void AudioController::stopCallCapture() {
    // TODO (Phase 2)
    callCapturing_ = false;
}
void AudioController::onCallOpusReceived(const QString & /*participantId*/, const QByteArray & /*opus*/) {
    // TODO (Phase 3)
}
void AudioController::removeCallPeer(const QString &participantId) {
    if (auto sink = callSinks_.take(participantId).data()) {
        sink->stop();
        sink->deleteLater();
    }
    callSinkIO_.remove(participantId);
    callDecoders_.remove(participantId);
}
void AudioController::stopAllCallPlayback() {
    for (auto it = callSinks_.begin(); it != callSinks_.end(); ++it) {
        if (auto *s = it.value().data()) { s->stop(); s->deleteLater(); }
    }
    callSinks_.clear();
    callSinkIO_.clear();
    callDecoders_.clear();
}

bool AudioController::isReady() const {
    return encoder_.isReady();
}

// ---------- Internal helpers ----------
QAudioSink *AudioController::getOrCreateChannelSink(qint64 userId) {
    if (auto sink = channelSinks_.value(userId).data()) return sink;
    // TODO: real sink creation in Phase 3
    return nullptr;
}
QAudioSink *AudioController::getOrCreateCallSink(const QString &participantId) {
    if (auto sink = callSinks_.value(participantId).data()) return sink;
    // TODO: real sink creation in Phase 3
    return nullptr;
}
QByteArray AudioController::applyPlaybackGain(const QByteArray &pcm, const QString &username) {
    const float v = getUserVolume(username);
    if (v > 1.0f) return applyGain(pcm, v);
    return pcm;
}
void AudioController::ensureDspReady() {
    if (!encoder_.isReady())          encoder_.init(bitrate_);
    if (!noiseSuppressor_.isReady())  noiseSuppressor_.init();
    if (!audioProcessor_.isReady())   audioProcessor_.init(200);
    audioProcessor_.setAecEnabled(aecEnabled_);
    audioProcessor_.setAgcEnabled(agcEnabled_);
    noiseSuppressor_.setEnabled(nsEnabled_);
}
