#pragma once

// =====================================================================
//  VoLaura Opus codec wrapper — production-grade voice encoding
// ---------------------------------------------------------------------
//  Hedef: Discord/Zoom üstü ses kalitesi.
//
//  Tasarım:
//   • RAII (custom deleter + unique_ptr)
//   • OpusConfig struct ile parametre kontrolü
//   • OPUS_APPLICATION_VOIP, complexity=10, FEC %15
//   • DTX kapalı (sürekli akış → daha pürüzsüz ses, robotik feel kaybolur)
//   • Cek/Validation: PCM boyutu + clipping detection
//   • Decoder: PLC + INBAND_FEC ile loss recovery
//   • Bitrate / loss% runtime tunable
//
//  Tipik kullanım:
//      OpusConfig cfg;
//      cfg.bitrateBps = 96000;          // 96 kbps — yüksek kalite VoIP
//      cfg.useDtx     = false;          // sürekli akış
//
//      OpusEncoderWrapper enc;
//      enc.initialize(cfg);
//      QByteArray pkt = enc.encodePcm(pcm);
//
//      OpusDecoderWrapper dec;
//      dec.initialize(cfg);
//      QByteArray pcm = dec.decodePacket(pkt);          // normal
//      QByteArray rec = dec.decodeFec(nextPkt);         // kaybolan paketi kurtar
//      QByteArray con = dec.concealLost();              // pure PLC (no FEC)
// =====================================================================

#include <QByteArray>

struct OpusEncoder;
struct OpusDecoder;

// ---------- Konfigurasyon ----------
struct OpusConfig {
    enum class Application {
        Voip,        // OPUS_APPLICATION_VOIP — speech-optimized
        Audio,       // OPUS_APPLICATION_AUDIO — general purpose / music
        LowDelay     // OPUS_APPLICATION_RESTRICTED_LOWDELAY — minimum latency
    };

    int  sampleRate     = 48000;       // Opus: 8000/12000/16000/24000/48000
    int  channels       = 1;           // 1 = mono (VoIP için optimal)
    int  bitrateBps     = 96000;       // 96 kbps — speech HD quality
    int  frameMs        = 20;          // 20 ms = 960 sample @ 48 kHz
    int  complexity     = 10;          // 0..10 (10 = max kalite, ~%5 daha CPU)
    int  packetLossPct  = 15;          // FEC için tolerance (%5..%30)

    bool useFec         = true;        // OPUS_SET_INBAND_FEC
    bool useDtx         = false;       // OPUS_SET_DTX — KAPALI: sürekli akış
                                       //   (DTX açık → sessizlikte comfort noise
                                       //    paketi → decoder fill yapar → robotik feel)
    bool useVbr         = true;        // OPUS_SET_VBR
    bool constrainedVbr = false;       // unconstrained → en iyi kalite
    int  bandwidthKHz   = -1;          // -1 = OPUS_AUTO; else 4/6/8/12/20

    Application app     = Application::Voip;

    int frameSamples() const { return (sampleRate * frameMs) / 1000; }
    int frameBytes()   const { return frameSamples() * channels * 2; } // S16
};

// ---------- Encoder ----------
class OpusEncoderWrapper {
public:
    OpusEncoderWrapper() = default;
    ~OpusEncoderWrapper();

    OpusEncoderWrapper(const OpusEncoderWrapper&)            = delete;
    OpusEncoderWrapper& operator=(const OpusEncoderWrapper&) = delete;

    // Kolay kurulum — varsayılan VoIP config + ayarlanabilir bitrate.
    bool init(int bitrateBps = 96000);

    // Tam kontrol için modern API.
    bool initialize(const OpusConfig &cfg);

    void close();
    bool isReady() const { return enc_ != nullptr; }

    // Runtime tuning (init sonrası çağrılabilir)
    bool setBitrate(int bps);
    bool setPacketLossPct(int pct);
    bool setComplexity(int c);

    // 20 ms PCM frame (1920 byte mono S16) → Opus paket
    // Daha büyük girdi → ilk frame encode edilir, gerisi kullanıcının buffer'ında kalmalı
    // Yetersiz/hatalı girdi → boş QByteArray (lastError() set edilir)
    QByteArray encodePcm(const QByteArray &pcmS16Mono);

    // Tanılama
    enum class Error {
        Ok = 0, NotInitialized, InvalidInput, EncodeFailed, AllocFailed
    };
    Error lastError() const { return lastError_; }

    int frameBytes()   const { return cfg_.frameBytes(); }
    int frameSamples() const { return cfg_.frameSamples(); }
    const OpusConfig &config() const { return cfg_; }

    // Backward compat — eski sabit kullanımları için
    static constexpr int kSampleRate     = 48000;
    static constexpr int kChannels       = 1;
    static constexpr int kFrameSamples   = 960;
    static constexpr int kFrameBytes     = 1920;
    static constexpr int kMaxPacketBytes = 1500;

private:
    OpusEncoder *enc_       = nullptr;
    OpusConfig   cfg_       = {};
    Error        lastError_ = Error::NotInitialized;
};

// ---------- Decoder ----------
class OpusDecoderWrapper {
public:
    OpusDecoderWrapper() = default;
    ~OpusDecoderWrapper();

    OpusDecoderWrapper(const OpusDecoderWrapper&)            = delete;
    OpusDecoderWrapper& operator=(const OpusDecoderWrapper&) = delete;

    bool init();
    bool initialize(const OpusConfig &cfg);
    void close();
    bool isReady() const { return dec_ != nullptr; }

    // Standart decode. Boş paket → PLC ile bir frame üretilir.
    QByteArray decodePacket(const QByteArray &opusPacket);

    // FEC decode: kaybolan bir önceki paketi sonraki paketin INBAND-FEC
    // verisinden geri yapılandırır. Caller önce bunu çağırır, sonra
    // sonraki paket için decodePacket() ile devam eder.
    QByteArray decodeFec(const QByteArray &nextOpusPacket);

    // Pure PLC: input olmadan kayıp gizleme (FEC yoksa fallback)
    QByteArray concealLost();

    enum class Error {
        Ok = 0, NotInitialized, InvalidInput, DecodeFailed, AllocFailed
    };
    Error lastError() const { return lastError_; }

    int frameBytes()   const { return cfg_.frameBytes(); }
    int frameSamples() const { return cfg_.frameSamples(); }
    const OpusConfig &config() const { return cfg_; }

    static constexpr int kSampleRate   = 48000;
    static constexpr int kChannels     = 1;
    static constexpr int kFrameSamples = 960;
    static constexpr int kFrameBytes   = 1920;

private:
    OpusDecoder *dec_       = nullptr;
    OpusConfig   cfg_       = {};
    Error        lastError_ = Error::NotInitialized;
};
