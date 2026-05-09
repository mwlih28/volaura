// =====================================================================
//  VoLaura Opus codec implementation
// =====================================================================

#include "opus_codec.h"

#include <opus.h>

#include <QDebug>
#include <QDateTime>
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace {

// Application enum → Opus C macro
int toOpusApplication(OpusConfig::Application a) {
    switch (a) {
        case OpusConfig::Application::Voip:     return OPUS_APPLICATION_VOIP;
        case OpusConfig::Application::Audio:    return OPUS_APPLICATION_AUDIO;
        case OpusConfig::Application::LowDelay: return OPUS_APPLICATION_RESTRICTED_LOWDELAY;
    }
    return OPUS_APPLICATION_VOIP;
}

// Bandwidth kHz → Opus macro
int toOpusBandwidth(int kHz) {
    switch (kHz) {
        case 4:  return OPUS_BANDWIDTH_NARROWBAND;
        case 6:  return OPUS_BANDWIDTH_MEDIUMBAND;
        case 8:  return OPUS_BANDWIDTH_WIDEBAND;
        case 12: return OPUS_BANDWIDTH_SUPERWIDEBAND;
        case 20: return OPUS_BANDWIDTH_FULLBAND;
        default: return OPUS_AUTO;
    }
}

// Validate config. Returns true if usable.
bool validate(const OpusConfig &c) {
    static const int kValidRates[] = {8000, 12000, 16000, 24000, 48000};
    bool rateOk = false;
    for (int r : kValidRates) if (c.sampleRate == r) { rateOk = true; break; }
    if (!rateOk) {
        qWarning() << "[opus] invalid sampleRate:" << c.sampleRate;
        return false;
    }
    if (c.channels != 1 && c.channels != 2) {
        qWarning() << "[opus] invalid channels:" << c.channels;
        return false;
    }
    static const int kValidFrames[] = {5, 10, 20, 40, 60};
    bool frameOk = false;
    for (int f : kValidFrames) if (c.frameMs == f) { frameOk = true; break; }
    if (!frameOk) {
        qWarning() << "[opus] invalid frameMs:" << c.frameMs;
        return false;
    }
    if (c.bitrateBps < 6000 || c.bitrateBps > 510000) {
        qWarning() << "[opus] invalid bitrate:" << c.bitrateBps;
        return false;
    }
    return true;
}

// Light clipping detection — telemetri için (UI'a kapanma sinyali olabilir)
bool detectClipping(const opus_int16 *pcm, int samples) {
    constexpr int16_t kClipThreshold = 32500; // ~99% peak
    int clipped = 0;
    for (int i = 0; i < samples; ++i) {
        if (pcm[i] >= kClipThreshold || pcm[i] <= -kClipThreshold) {
            ++clipped;
            if (clipped > samples / 100) return true; // >1% örnek tepe noktada
        }
    }
    return false;
}

} // namespace

// =====================================================================
//  Encoder
// =====================================================================

OpusEncoderWrapper::~OpusEncoderWrapper() { close(); }

bool OpusEncoderWrapper::init(int bitrateBps) {
    OpusConfig cfg;
    cfg.bitrateBps = bitrateBps;
    return initialize(cfg);
}

bool OpusEncoderWrapper::initialize(const OpusConfig &cfg) {
    close();
    if (!validate(cfg)) {
        lastError_ = Error::InvalidInput;
        return false;
    }
    cfg_ = cfg;

    int err = OPUS_OK;
    enc_ = opus_encoder_create(cfg_.sampleRate, cfg_.channels,
                                toOpusApplication(cfg_.app), &err);
    if (err != OPUS_OK || !enc_) {
        qWarning() << "[opus] encoder_create fail:" << opus_strerror(err);
        enc_ = nullptr;
        lastError_ = Error::AllocFailed;
        return false;
    }

    // ---- Kalite ayarları ----
    // Bitrate: VBR ile dinamik tavan
    opus_encoder_ctl(enc_, OPUS_SET_BITRATE(cfg_.bitrateBps));
    opus_encoder_ctl(enc_, OPUS_SET_VBR(cfg_.useVbr ? 1 : 0));
    opus_encoder_ctl(enc_, OPUS_SET_VBR_CONSTRAINT(cfg_.constrainedVbr ? 1 : 0));

    // Complexity: 10 = en yüksek kalite (CPU pahasına ~%5)
    opus_encoder_ctl(enc_, OPUS_SET_COMPLEXITY(std::clamp(cfg_.complexity, 0, 10)));

    // Signal type — VOIP için zaten speech-tuned ama explicit set helps
    if (cfg_.app == OpusConfig::Application::Voip) {
        opus_encoder_ctl(enc_, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    } else if (cfg_.app == OpusConfig::Application::Audio) {
        opus_encoder_ctl(enc_, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
    } else {
        opus_encoder_ctl(enc_, OPUS_SET_SIGNAL(OPUS_AUTO));
    }

    // Bandwidth — auto bırakırsa Opus bitrate'e göre optimal seçer
    opus_encoder_ctl(enc_, OPUS_SET_BANDWIDTH(toOpusBandwidth(cfg_.bandwidthKHz)));
    // Maksimum bandwidth tavanı: FULLBAND (20 kHz) — kısıtlama yok
    opus_encoder_ctl(enc_, OPUS_SET_MAX_BANDWIDTH(OPUS_BANDWIDTH_FULLBAND));

    // ---- Loss / FEC ----
    opus_encoder_ctl(enc_, OPUS_SET_INBAND_FEC(cfg_.useFec ? 1 : 0));
    opus_encoder_ctl(enc_, OPUS_SET_PACKET_LOSS_PERC(
        std::clamp(cfg_.packetLossPct, 0, 100)));

    // ---- DTX (KAPALI = robotik feel'i önler) ----
    opus_encoder_ctl(enc_, OPUS_SET_DTX(cfg_.useDtx ? 1 : 0));

    // ---- Force CBR threshold for very low bitrate (LBRR sıkışmasını önler) ----
    // (Opus 1.3+: prediction disabled → daha temiz transient handling)
    opus_encoder_ctl(enc_, OPUS_SET_PREDICTION_DISABLED(0));

    // LSB depth — 16-bit input
    opus_encoder_ctl(enc_, OPUS_SET_LSB_DEPTH(16));

    // Expert frame duration → caller'ın frame size'ına eşle
    int dur = OPUS_FRAMESIZE_ARG;
    switch (cfg_.frameMs) {
        case  5: dur = OPUS_FRAMESIZE_5_MS;   break;
        case 10: dur = OPUS_FRAMESIZE_10_MS;  break;
        case 20: dur = OPUS_FRAMESIZE_20_MS;  break;
        case 40: dur = OPUS_FRAMESIZE_40_MS;  break;
        case 60: dur = OPUS_FRAMESIZE_60_MS;  break;
    }
    opus_encoder_ctl(enc_, OPUS_SET_EXPERT_FRAME_DURATION(dur));

    lastError_ = Error::Ok;
    qInfo() << "[opus enc] initialized — sr=" << cfg_.sampleRate
            << "ch=" << cfg_.channels
            << "br=" << cfg_.bitrateBps << "bps"
            << "frame=" << cfg_.frameMs << "ms"
            << "fec=" << cfg_.useFec
            << "dtx=" << cfg_.useDtx
            << "complexity=" << cfg_.complexity;
    return true;
}

void OpusEncoderWrapper::close() {
    if (enc_) {
        opus_encoder_destroy(enc_);
        enc_ = nullptr;
    }
    lastError_ = Error::NotInitialized;
}

bool OpusEncoderWrapper::setBitrate(int bps) {
    if (!enc_) { lastError_ = Error::NotInitialized; return false; }
    bps = std::clamp(bps, 6000, 510000);
    if (opus_encoder_ctl(enc_, OPUS_SET_BITRATE(bps)) == OPUS_OK) {
        cfg_.bitrateBps = bps;
        return true;
    }
    return false;
}

bool OpusEncoderWrapper::setPacketLossPct(int pct) {
    if (!enc_) { lastError_ = Error::NotInitialized; return false; }
    pct = std::clamp(pct, 0, 100);
    if (opus_encoder_ctl(enc_, OPUS_SET_PACKET_LOSS_PERC(pct)) == OPUS_OK) {
        cfg_.packetLossPct = pct;
        return true;
    }
    return false;
}

bool OpusEncoderWrapper::setComplexity(int c) {
    if (!enc_) { lastError_ = Error::NotInitialized; return false; }
    c = std::clamp(c, 0, 10);
    if (opus_encoder_ctl(enc_, OPUS_SET_COMPLEXITY(c)) == OPUS_OK) {
        cfg_.complexity = c;
        return true;
    }
    return false;
}

QByteArray OpusEncoderWrapper::encodePcm(const QByteArray &pcm) {
    if (!enc_) {
        lastError_ = Error::NotInitialized;
        return {};
    }
    const int frameBytes = cfg_.frameBytes();
    if (pcm.size() < frameBytes) {
        lastError_ = Error::InvalidInput;
        return {};
    }
    // PCM hizalama kontrolü (S16 = 2 byte boundary)
    if (pcm.size() % 2 != 0) {
        lastError_ = Error::InvalidInput;
        return {};
    }

    const opus_int16 *pcm16 = reinterpret_cast<const opus_int16*>(pcm.constData());

    // Clipping uyarısı (1 saniyede 1'den fazla logla, spam önle)
    static thread_local qint64 lastClipLog = 0;
    if (detectClipping(pcm16, cfg_.frameSamples())) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - lastClipLog > 1000) {
            qWarning() << "[opus enc] clipping detected — kaynak çok yüksek (mic gain ↓)";
            lastClipLog = now;
        }
    }

    QByteArray out;
    out.resize(kMaxPacketBytes);

    int n = opus_encode(enc_, pcm16, cfg_.frameSamples(),
                        reinterpret_cast<unsigned char*>(out.data()),
                        out.size());
    if (n < 0) {
        qWarning() << "[opus enc] encode fail:" << opus_strerror(n);
        lastError_ = Error::EncodeFailed;
        return {};
    }
    if (n == 1 || n == 0) {
        // Opus DTX bayrağı: 1-byte/0-byte paketi sessizlik gösterir.
        // DTX kapalıysa bu çıkış gelmemeli; geldiyse geçerli kabul et.
    }
    out.resize(n);
    lastError_ = Error::Ok;
    return out;
}

// =====================================================================
//  Decoder
// =====================================================================

OpusDecoderWrapper::~OpusDecoderWrapper() { close(); }

bool OpusDecoderWrapper::init() {
    OpusConfig cfg; // defaults — encoder ile uyumlu
    return initialize(cfg);
}

bool OpusDecoderWrapper::initialize(const OpusConfig &cfg) {
    close();
    if (!validate(cfg)) {
        lastError_ = Error::InvalidInput;
        return false;
    }
    cfg_ = cfg;

    int err = OPUS_OK;
    dec_ = opus_decoder_create(cfg_.sampleRate, cfg_.channels, &err);
    if (err != OPUS_OK || !dec_) {
        qWarning() << "[opus dec] decoder_create fail:" << opus_strerror(err);
        dec_ = nullptr;
        lastError_ = Error::AllocFailed;
        return false;
    }
    // Decoder gain — neutral (encoder zaten doğru seviyede iletiyor).
    // Gerekirse çıkış sinyalini boost etmek için OPUS_SET_GAIN kullanılabilir
    // ama biz per-user volume'i PCM seviyesinde uyguladığımız için gerek yok.
    opus_decoder_ctl(dec_, OPUS_SET_GAIN(0));

    lastError_ = Error::Ok;
    qInfo() << "[opus dec] initialized — sr=" << cfg_.sampleRate
            << "ch=" << cfg_.channels
            << "frame=" << cfg_.frameMs << "ms";
    return true;
}

void OpusDecoderWrapper::close() {
    if (dec_) {
        opus_decoder_destroy(dec_);
        dec_ = nullptr;
    }
    lastError_ = Error::NotInitialized;
}

QByteArray OpusDecoderWrapper::decodePacket(const QByteArray &pkt) {
    if (!dec_) { lastError_ = Error::NotInitialized; return {}; }

    QByteArray out;
    out.resize(cfg_.frameBytes());
    opus_int16 *pcm16 = reinterpret_cast<opus_int16*>(out.data());

    int n;
    if (pkt.isEmpty()) {
        // Pure PLC — Opus üretir
        n = opus_decode(dec_, nullptr, 0, pcm16, cfg_.frameSamples(), 0);
    } else {
        n = opus_decode(dec_,
                        reinterpret_cast<const unsigned char*>(pkt.constData()),
                        pkt.size(), pcm16, cfg_.frameSamples(), 0);
    }
    if (n < 0) {
        qWarning() << "[opus dec] decode fail:" << opus_strerror(n);
        lastError_ = Error::DecodeFailed;
        return {};
    }
    out.resize(n * 2 * cfg_.channels);
    lastError_ = Error::Ok;
    return out;
}

QByteArray OpusDecoderWrapper::decodeFec(const QByteArray &nextPkt) {
    if (!dec_) { lastError_ = Error::NotInitialized; return {}; }
    if (nextPkt.isEmpty()) {
        // FEC dependent on next packet — yoksa pure PLC döndür
        return concealLost();
    }
    QByteArray out;
    out.resize(cfg_.frameBytes());
    opus_int16 *pcm16 = reinterpret_cast<opus_int16*>(out.data());

    // decode_fec=1 → sonraki paketin LBRR/FEC verisinden kayıp paketi kurtar
    int n = opus_decode(dec_,
                        reinterpret_cast<const unsigned char*>(nextPkt.constData()),
                        nextPkt.size(), pcm16, cfg_.frameSamples(), /*decode_fec=*/1);
    if (n < 0) {
        qWarning() << "[opus dec] FEC decode fail:" << opus_strerror(n);
        // Düş: pure PLC dene
        return concealLost();
    }
    out.resize(n * 2 * cfg_.channels);
    lastError_ = Error::Ok;
    return out;
}

QByteArray OpusDecoderWrapper::concealLost() {
    if (!dec_) { lastError_ = Error::NotInitialized; return {}; }
    QByteArray out;
    out.resize(cfg_.frameBytes());
    opus_int16 *pcm16 = reinterpret_cast<opus_int16*>(out.data());
    int n = opus_decode(dec_, nullptr, 0, pcm16, cfg_.frameSamples(), 0);
    if (n < 0) {
        lastError_ = Error::DecodeFailed;
        return {};
    }
    out.resize(n * 2 * cfg_.channels);
    return out;
}
