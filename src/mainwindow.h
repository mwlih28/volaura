#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QSet>
#include <QAbstractButton>
#include <QWidget>
#include "network/signaling_client.h"
#include "network/update_checker.h"
#include "ui/notification_center.h"
#include "audio/opus_codec.h"
#include "audio/noise_suppressor.h"
#include "audio/audio_processor.h"
#include <memory>

class QLabel;
class QLineEdit;
class QVBoxLayout;
class QListWidget;
class QPushButton;
class QToolButton;
class QCheckBox;
class QTextEdit;
class QTimer;
class QResizeEvent;
class QCamera;
class QVideoSink;
class QVideoFrame;
class QMediaCaptureSession;
class QGraphicsBlurEffect;
class QGridLayout;
class QScrollArea;
class QDialog;
class QSizeGrip;
class QAudioSink;
class QAudioSource;
class QIODevice;
class QPaintEvent;
class QSystemTrayIcon;
class QSoundEffect;

// Animasyonlu checkbox: tik çizimi scale + opacity ile canlandırılır.
// Normal QCheckBox'a drop-in alternatif (setChecked/isChecked/toggled signal çalışır).
class AnimatedCheckBox : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(qreal tickProgress READ tickProgress WRITE setTickProgress)
public:
    explicit AnimatedCheckBox(QWidget *parent = nullptr);
    QSize sizeHint() const override { return QSize(22, 22); }
    qreal tickProgress() const { return m_tick; }
    void setTickProgress(qreal v);
protected:
    void paintEvent(QPaintEvent *e) override;
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;
private:
    qreal m_tick = 0.0;      // 0..1 tik çizimi ilerlemesi
    bool  m_hover = false;
};

// Açılış ekranı — logo + gradient aura + pulse animasyonu.
// finish() ile ana pencereye devreder.
class VoLauraSplash : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal pulse  READ pulse  WRITE setPulse)
    Q_PROPERTY(qreal orbit  READ orbit  WRITE setOrbit)
    Q_PROPERTY(qreal logoIn READ logoIn WRITE setLogoIn)
    Q_PROPERTY(qreal sweep  READ sweep  WRITE setSweep)
public:
    explicit VoLauraSplash(const QPixmap &logo, QWidget *parent = nullptr);
    qreal pulse()  const { return m_pulse;  }
    qreal orbit()  const { return m_orbit;  }
    qreal logoIn() const { return m_logoIn; }
    qreal sweep()  const { return m_sweep;  }
    void setPulse(qreal v)  { m_pulse  = v; update(); }
    void setOrbit(qreal v)  { m_orbit  = v; update(); }
    void setLogoIn(qreal v) { m_logoIn = v; update(); }
    void setSweep(qreal v)  { m_sweep  = v; update(); }
    void finish(QWidget *mainWin);
protected:
    void paintEvent(QPaintEvent *e) override;
private:
    QPixmap m_logo;
    qreal m_pulse  = 0.0;
    qreal m_orbit  = 0.0;
    qreal m_logoIn = 0.0;
    qreal m_sweep  = 0.0;
};

// Modern arka plan — slate-blue gradient + ağ düğümleri (glassmorphism)
class NetworkBackground : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal phase READ phase WRITE setPhase)
public:
    explicit NetworkBackground(QWidget *parent = nullptr);
    qreal phase() const { return m_phase; }
    void setPhase(qreal v) { m_phase = v; update(); }
protected:
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
private:
    void rebuildNodes();
    void rebuildBackgroundCache();
    struct Node { QPointF pos; qreal r; qreal phaseOffset; };
    struct Edge { int a; int b; qreal alpha; };
    QVector<Node> m_nodes;
    QVector<Edge> m_edges;
    QPixmap m_bgCache; // gradient + statik çizgiler (her frame yeniden çizilmez)
    qreal m_phase = 0.0;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void showCreateRoomDialog();
    void showJoinRoomDialog();
    void showSettingsDialog();
    void showAddFriendDialog();
    void showRequestsDialog();
    void createRoom();
    void joinRoom(const QString &code);
    void sendCurrentMessage();
    void toggleMute();
    void toggleScreenShare();
    void toggleCamera();

    // Auth & friends
    void onLoginResult(bool ok, const QString &userNameOrError);
    void onRegisterResult(bool ok, const QString &userNameOrError);
    void onRegisterVerifyPending(const QString &email, const QString &message);
    void onLoginNeedsVerification(const QString &userName, const QString &email, const QString &message);
    void onPasswordResetSent(bool ok, const QString &message);
    void onVerificationSent(bool ok, const QString &message);
    void showForgotPasswordDialog();
    void onFriendsListUpdated(const QJsonArray &friends, const QJsonArray &pendingIn, const QJsonArray &pendingOut);
    void onFriendRequestReceived(const QString &fromUserName);
    void onFriendAdded(const QString &userName, bool online);
    void onFriendRemoved(const QString &userName);
    void onFriendStatusChanged(const QString &userName, bool online);
    void onFriendOpResult(bool ok, const QString &op, const QString &userNameOrError);

    // Calling
    void startCallToFriend(const QString &userName);
    void onIncomingCall(const QString &fromUserName, const QString &roomCode);
    void onCallDeclined(const QString &fromUserName);
    void onCallCancelled(const QString &fromUserName);
    void onCallAccepted(const QString &fromUserName);
    void onCallUnreachable(const QString &userName);
    void onCallError(const QString &userName, const QString &error);
    void showScreenSourcePicker();

    void onRoomCreated(const QString &roomCode, const QString &roomName);
    void onRoomJoined(const QString &roomCode, const QString &roomName);
    void onError(const QString &error);
    void onChatMessageReceived(const QString &userName, const QString &message, const QString &timestamp);
    void onParticipantJoined(const QString &participantId, const QString &userName);
    void onParticipantLeft(const QString &participantId);
    void onParticipantMediaStateChanged(const QString &participantId, bool audioMuted, bool screenSharing);
    void onParticipantsListed(const QJsonArray &participants);
    void onMediaChunkReceived(const QString &participantId, const QString &mediaKind, const QByteArray &payload);

    // ---- Discord-benzeri (servers / channels / DM) ----
    void onServersListed(const QJsonArray &servers);
    void onServerCreated(const QJsonObject &server);
    void onServerJoined(const QJsonObject &server);
    void onServerLeft(int serverId);
    void onServerDeleted(int serverId);
    void onServerRenamed(int serverId, const QString &name);
    void onChannelsListed(int serverId, const QJsonArray &channels);
    void onChannelCreated(int serverId, const QJsonObject &channel);
    void onChannelDeleted(int serverId, int channelId);
    void onChannelRenamed(int serverId, int channelId, const QString &name);
    void onChannelMessagesListed(int channelId, const QJsonArray &messages);
    void onChannelMessageReceived(int serverId, int channelId, const QJsonObject &message);
    void onChannelMessageDeleted(int serverId, int channelId, qint64 messageId);
    void onChannelMessageEdited(int serverId, int channelId, qint64 messageId, const QString &content);
    void onDmThreadsListed(const QJsonArray &threads);
    void onDmMessagesListed(const QString &peerUsername, const QJsonArray &messages);
    void onDmReceived(const QJsonObject &message);

    void onMembersListed(int serverId, const QJsonArray &members);
    void onMemberJoined(int serverId, const QJsonObject &member);
    void onMemberLeft(int serverId, const QString &username);

    // ---- Voice channels ----
    void onVoiceJoined(int channelId, const QJsonArray &participants);
    void onVoiceLeft();
    void onVoiceMemberJoined(int channelId, const QJsonObject &member);
    void onVoiceMemberLeft(int channelId, qint64 userId, const QString &username);
    void onVoiceStateChanged(int channelId, qint64 userId, bool muted, bool deafened);
    void onVoiceChunkReceived(int channelId, qint64 userId, const QByteArray &pcm);
    void joinVoiceChannelClicked();
    void leaveVoiceChannelClicked();
    void toggleVoiceMute();
    void toggleVoiceDeafen();
    void onMicReadyRead();

    void showServerSetupDialog();
    void showCreateServerDialog();
    void showJoinServerDialog();
    void showInviteCodeDialog(int serverId);

    void captureAndSendScreenFrame();
    void sendVoiceActivityPing();
    void sendCameraFrame();
    void onCameraFrameChanged(const QVideoFrame &frame);

private:
    void setupUi();
    void showWelcomeModal();
    void hideWelcomeModal();
    void showRoomInterface(const QString &roomCode);
    void appendSystemMessage(const QString &message);
    QString createTimeStamp() const;
    void updateParticipantLabel(const QString &participantId, const QString &userName, bool muted = false, bool sharing = false);
    void removeParticipantLabel(const QString &participantId);
    QWidget *buildParticipantCard(const QString &name, const QString &status, bool speaking, bool muted, bool sharing);
    void rebuildParticipantGrid();
    void appendChatBubble(const QString &userName, const QString &timestamp, const QString &message);
    QString avatarInitials(const QString &name) const;
    QString avatarColor(const QString &name) const;

    // Custom (frameless) title bar
    QWidget *buildTitleBar();
    QWidget *titleBar;
    QAbstractButton *titleMaxBtn;
    QSizeGrip *sizeGrip;
    bool windowDragging;
    QPoint windowDragOffset;

    // Auth & friends UI
    void showLoginScreen();
    void hideLoginScreen();
    void rebuildFriendsSidebar();
    QWidget *buildFriendAvatar(const QString &userName, bool online);
    void showMainUI();
    void updateRequestsBadge();

    QWidget *welcomeModal;
    QLabel *roomTitleLabel;
    QTextEdit *chatHistory;
    QScrollArea *chatScroll;
    QVBoxLayout *chatLayout;
    QLineEdit *messageInput;
    QListWidget *participantList;
    QLabel *remoteScreenLabel;
    QLabel *remoteCameraLabel;
    QPushButton *createRoomBtn;
    QPushButton *joinRoomBtn;
    QAbstractButton *muteBtn;
    QAbstractButton *screenShareBtn;
    QAbstractButton *cameraBtn;
    QLabel *connectionStatusLabel;

    SignalingClient *signalingClient;
    UpdateChecker      *updateChecker = nullptr;
    NotificationCenter *notifCenter   = nullptr;
    void initUpdateChecker();
    void onUpdateAvailable(const QString &version, const QString &notes,
                           const QUrl &url, const QString &sha, qint64 size);
    QMap<QString, QString> participantNames;

    QString currentRoomCode;
    QString currentRoomName;
    QString currentUserName;
    QString currentPassword;
    bool isMuted;
    bool isScreenSharing;
    bool isCameraOn;

    QTimer *screenShareTimer;
    QTimer *voicePingTimer;
    QTimer *cameraShareTimer;

    QCamera *camera;
    QVideoSink *cameraVideoSink;
    QMediaCaptureSession *cameraCaptureSession;
    QByteArray lastCameraFramePayload;
    QGraphicsBlurEffect *backgroundBlurEffect;

    QGridLayout *participantGrid;
    QWidget *participantGridWidget;
    QMap<QString, QWidget*> participantCards;
    QMap<QString, bool> participantMuted;
    QMap<QString, bool> participantSharing;

    // User-configurable settings
    QString settingsMicId;
    QString settingsSpeakerId;
    QString settingsCameraId;
    int settingsScreenIndex;      // 0..N screens, -1 = primary
    int settingsScreenFps;        // 5..60
    int settingsScreenHeight;     // 360/540/720/1080
    int settingsJpegQuality;      // 20..90
    // Ses işleme ayarları
    bool  settingsAec        = true;   // Yankı bastırma (AEC)
    bool  settingsAgc        = true;   // Otomatik gain (AGC)
    bool  settingsNs         = true;   // Noise suppression (RNNoise + Speex NS)
    float settingsMicGain    = 1.0f;   // 0.5..3.0
    int   settingsAudioBitrate = 64000; // 32000..128000 bps

    void loadSettings();
    void saveSettings();
    void applyCameraDevice();
    void applyScreenShareInterval();
    void applyAudioSettings();

    // Auth & friends state
    bool isLoggedIn;
    bool hasShownDisconnectToast = false;
    QString authUserName;
    QWidget *loginScreen;
    QLineEdit *loginUserInput;
    QLineEdit *loginEmailInput;    // visible only in register mode
    QLineEdit *loginPassInput;
    QLabel *loginErrorLabel;
    QLabel *loginInfoLabel;        // info / success messages
    QPushButton *loginSubmitBtn;
    QPushButton *loginToggleBtn;   // switch login/register
    QPushButton *loginForgotBtn;   // "Şifremi unuttum"
    QPushButton *loginResendBtn;   // resend verification email
    bool loginInRegisterMode;
    QString lastUnverifiedUser;    // remembered for resend

    // ---- 30 gün remember-me ----
    QString pendingAutoLoginPass;  // login submit anında tutulur, başarı olunca saklanır
    bool    autoLoginAttempted = false;
    bool    autoLoginSilent = false;   // remember-me auto-login sırasında true; 2FA gerekirse sessizce vazgeç
    void saveRememberMe(const QString &user, const QString &pass);
    void clearRememberMe();
    bool tryAutoLogin(); // true dönerse otomatik giriş başlatıldı
    void performLogout(); // Hesaptan çıkış (remember-me sil + ekrana dön)

    QVBoxLayout *friendsSidebarLayout;
    QAbstractButton *requestsBtn;
    QMap<QString, bool> friendsOnline;   // userName -> online
    QSet<QString> pendingInRequests;
    QSet<QString> pendingOutRequests;

    // Call state
    QString pendingCallTo;       // waiting for room_created to dial this friend
    QString activeCallPeer;      // currently in call with
    QDialog *ringingDialog;      // outgoing "aranıyor..." or incoming ringing dialog

    // ---- Discord-benzeri state ----
    enum class ChatMode { Idle, Channel, Dm, Room };
    ChatMode chatMode = ChatMode::Idle;
    int currentServerId = 0;
    int currentChannelId = 0;
    QString currentDmPeer;
    QMap<int, QJsonObject> serversById;          // serverId -> server json
    QMap<int, QList<QJsonObject>> channelsByServer; // serverId -> channel list
    QMap<int, QJsonObject> channelsById;         // channelId -> channel json

    // UI parts for Discord mode
    QVBoxLayout *serverSidebarLayout = nullptr;  // sidebar'da sunucu listesi
    QWidget     *channelListPanel   = nullptr;   // chat column üstünde kanal listesi
    QVBoxLayout *channelListLayout  = nullptr;
    QLabel      *serverNameHeader   = nullptr;   // chat column'un en üstü

    void rebuildServerSidebar();
    void rebuildChannelList();
    void selectServer(int serverId);
    void selectChannel(int channelId);
    void selectDm(const QString &peerUsername);
    void clearChatArea();
    void appendRichMessage(const QString &userName, const QDateTime &when, const QString &content,
                           bool edited = false, qint64 messageId = 0, bool canManage = false,
                           bool isDm = false);

    // Chat row tracking (sadece channel mesajları için messageId != 0)
    QMap<qint64, QWidget*> chatRowsByMessageId;
    QMap<qint64, QLabel*>  chatBodyByMessageId;
    void removeChatRow(qint64 messageId);
    void updateChatRow(qint64 messageId, const QString &newContent, bool edited);
    void editChannelMessagePrompt(qint64 messageId, const QString &currentContent);

    // ---- Voice channel state ----
    int  currentVoiceChannelId = 0;
    bool voiceMuted   = false;
    bool voiceDeafened = false;
    // Audio capture (QAudioSource)
    QAudioSource *voiceSource = nullptr;
    QIODevice    *voiceSourceIO = nullptr;
    QByteArray    voiceMicBuffer;  // 20 ms chunk biriktirme
    // Opus encoder (mic) ve per-peer decoder
    OpusEncoderWrapper voiceEncoder;
    QMap<qint64, std::shared_ptr<OpusDecoderWrapper>> voiceDecoders;
    // RNNoise gürültü bastırma (klavye, fan, klima, dış ses)
    NoiseSuppressor voiceNoiseSuppressor;
    // Speex DSP: AEC (yankı bastırma) + AGC + dereverb
    AudioProcessor voiceAudioProcessor;
    // Per-peer playback sinks
    QMap<qint64, QAudioSink*> voiceSinks;
    QMap<qint64, QIODevice*>  voiceSinkIO;
    // Members: userId -> {username,muted,deafened}
    QMap<qint64, QJsonObject> voiceMembers;

    // Voice UI
    QWidget     *voicePanel = nullptr;          // inline panel inside chat area
    QVBoxLayout *voicePanelLayout = nullptr;
    QWidget     *voiceMembersWidget = nullptr;
    QVBoxLayout *voiceMembersLayout = nullptr;
    QPushButton *voiceJoinBtn = nullptr;
    QPushButton *voiceLeaveBtn = nullptr;
    QPushButton *voiceMuteBtn = nullptr;
    QPushButton *voiceDeafenBtn = nullptr;
    QPushButton *voiceScreenShareBtn = nullptr;
    QPushButton *voiceCameraShareBtn = nullptr;
    QLabel      *voicePanelTitle = nullptr;
    QLabel      *voicePanelStatus = nullptr;

    // Voice media (ekran/kamera) state
    bool    voiceSharingScreen = false;
    bool    voiceSharingCamera = false;
    bool    voiceScreenEncodeBusy = false;
    QTimer *voiceScreenShareTimer = nullptr;
    QTimer *voiceCameraShareTimer = nullptr;
    QWidget     *voiceMediaGrid = nullptr;      // live stream tiles grid
    QGridLayout *voiceMediaGridLayout = nullptr;
    // (userId, kind) -> tile QWidget (kind: "screen"|"camera")
    QMap<QString, QWidget*> voiceMediaTiles;
    QMap<QString, QLabel*>  voiceMediaTileLabels;
    // Per-kullanıcı playback ses seviyesi (username → 0.0..2.0; varsayılan 1.0)
    QMap<QString, float> userVolume;
    float getUserVolume(const QString &username) const;
    void  setUserVolume(const QString &username, float v);
    void  showUserVolumeMenu(const QString &username, const QPoint &globalPos);
    static QString voiceTileKey(const QString &username, const QString &kind);
    void rebuildVoiceMediaGrid();
    QWidget *makeVoiceMediaTile(const QString &username, const QString &kind);
    void removeVoiceMediaTile(const QString &username, const QString &kind);
    void captureAndSendVoiceScreenFrame();
    void sendVoiceCameraFrameTick();
    void toggleVoiceScreenShare();
    bool pickScreenSourceForVoice();
    void toggleVoiceCameraShare();
    void stopAllVoiceShares();
    void onVoiceShareStarted(int channelId, qint64 userId, const QString &username, const QString &kind);
    void onVoiceShareStopped(int channelId, qint64 userId, const QString &username, const QString &kind);
    void onVoiceMediaChunk(int channelId, qint64 userId, const QString &username,
                           const QString &kind, const QByteArray &jpeg);
    void openVoiceTileFullscreen(const QString &username, const QString &kind);
    // ---- 2FA UI ----
    void showLogin2faDialog(bool totp, bool sms, bool email,
                            const QString &phoneHint, const QString &emailHint);
    AnimatedCheckBox *loginTermsCheck = nullptr;
    void showSecurityDialog();
    // Telefon/E-posta + kod ile parolasız giriş dialog'u
    void showPasswordlessLoginDialog();

    // ---- DM Resim eki ----
    // QFileDialog ile resim seçer, küçültüp JPEG'e sıkıştırıp base64 ile gönderir.
    // İçerik formatı: "[[img:b64]]" sona text eklenebilir. Max ~1 MB orijinal dosya.
    void pickAndSendImageAttachment();

    // ---- E2E DM ----
    // Login sonrası: cihaz keypair'ini hazırla ve sunucuya pub key bildir.
    void e2eEnsureAndAnnounce();
    // Peer'in pub key'i cache'te yoksa sunucudan iste.
    void e2eRequestPeerKey(const QString &username);
    // peerUsername -> pub key base64 (boş = bilmiyoruz / E2E yok)
    QHash<QString, QString> e2ePeerKeyCache;
    // En son DM yazılan ekran için, pub key gelince queued mesajları gönder.
    QHash<QString, QStringList> e2ePendingPlaintextByPeer;
    // fullscreen viewer (yalnızca 1 tane aynı anda)
    QDialog *voiceFullscreenDlg = nullptr;
    QLabel  *voiceFullscreenLabel = nullptr;
    QString  voiceFullscreenUser;
    QString  voiceFullscreenKind;

    // ---- Members panel (sağ sidebar) ----
    QWidget     *membersPanel = nullptr;
    QVBoxLayout *membersPanelLayout = nullptr;
    QLabel      *membersPanelTitle = nullptr;
    QMap<int, QJsonArray> membersByServer;     // serverId -> üye listesi
    void rebuildMembersPanel();

    // ---- Typing indicators ----
    QLabel *typingLabel = nullptr;
    QTimer *typingCooldownTimer = nullptr;  // gönderim throttle (~3sn)
    QTimer *typingExpireTimer   = nullptr;  // alınan typing'i 4sn sonra temizler
    QMap<QString, QDateTime> activeTypers;  // username -> son görülen zaman
    void   onLocalTypingSignal();
    void   refreshTypingLabel();

    // ---- Unread counters ----
    QMap<int, int>     unreadByChannel;   // channelId -> adet
    QMap<QString, int> unreadByDmPeer;    // peerUsername -> adet
    int  unreadForServer(int serverId) const;
    void clearUnreadChannel(int channelId);
    void clearUnreadDm(const QString &peer);
    void bumpUnreadChannel(int channelId);
    void bumpUnreadDm(const QString &peer);

    void showVoicePanel(int channelId);
    void hideVoicePanel();
    void rebuildVoiceMembersList();
    void startVoiceCapture();
    void stopVoiceCapture();
    void stopAllVoicePlayback();
    QAudioSink* getOrCreateVoiceSinkFor(qint64 userId);

    // ---- Dialog backdrop dim overlay (glassmorphism) ----
    QWidget *dialogBackdrop = nullptr;
    void showDialogBackdrop();
    void hideDialogBackdrop();

    // ---- Media control bar (mute/screen/camera/Call End) ----
    // Sadece aktif 1:1 çağrı veya voice channel'da görünür; idle'da gizli.
    QWidget *mediaControlBar = nullptr;
    QWidget *mediaColumn = nullptr;
    void updateMediaControlVisibility();

    // ---- DM action bar (sesli/görüntülü arama butonları) ----
    QWidget *dmActionBar = nullptr;
    QToolButton *dmCallBtn = nullptr;
    QToolButton *dmVideoBtn = nullptr;
    QToolButton *dmInfoBtn = nullptr;

    // ---- Voice activity indicator ----
    QSet<QString> speakingUsers;                       // su an konusan kullanicilar
    QMap<QString, QTimer*> speakingTimers;             // her kullanici icin auto-clear timer
    void setUserSpeaking(const QString &userName, bool active);
    void refreshFriendAvatarSpeaking(const QString &userName);
    // Mikrofon seviyesi (basit RMS) hesaplar -> aktif mi (~0.02 esik)
    bool computeVoiceActive(const QByteArray &pcm16);

    // ---- 1:1 Call audio (room-based media_chunk audio transport) ----
    // 1:1 arkadaş aramalarında room içinde PCM audio için ayrı capture/playback.
    QAudioSource *callVoiceSource = nullptr;
    QIODevice    *callVoiceSourceIO = nullptr;
    QByteArray    callMicBuffer; // 20ms chunk biriktirme
    QMap<QString, std::shared_ptr<OpusDecoderWrapper>> callVoiceDecoders;
    QMap<QString, QAudioSink*> callVoiceSinks;    // participantId -> sink
    QMap<QString, QIODevice*>  callVoiceSinkIO;
    void startCallVoiceCapture();
    void stopCallVoiceCapture();
    void stopAllCallVoicePlayback();
    QAudioSink* getOrCreateCallVoiceSinkFor(const QString &participantId);
    void onCallMicReadyRead();

    // UI helpers (modern, VoLaura temalı)
    // Returns user-entered text, or empty string if cancelled.
    // icon: emoji or unicode glyph to show at top (optional).
    QString promptInput(const QString &title, const QString &label,
                        const QString &initial = QString(),
                        const QString &placeholder = QString(),
                        const QString &icon = QString());
    bool promptConfirm(const QString &title, const QString &message,
                       const QString &confirmText = QString::fromUtf8("Onayla"),
                       const QString &cancelText = QString::fromUtf8("İptal"),
                       bool danger = false);
    // Non-blocking toast notification (top-right corner).
    // type: "info" | "success" | "error" | "warn"
    void showToast(const QString &message, const QString &type = QString("info"), int durationMs = 3200);

    // ---- Sistem bildirimleri (tray + ses) ----
    // Sistem tepsisi simgesi — pencere arka plandayken bildirim balonu için.
    QSystemTrayIcon *trayIcon = nullptr;
    void initSystemTray();
    // Pencere odakta değilse hem tray balonu hem ses çalar.
    // type: "dm" | "friend" | "call"
    void notifyUser(const QString &title, const QString &body, const QString &type = "dm");
    // type'a göre QUrl ile ses çal (resources'tan).
    void playNotificationSound(const QString &type);
    // Pencere aktif/odakta mı (focused + visible + not minimized)?
    bool isWindowFocused() const;
};

#endif
