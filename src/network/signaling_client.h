#ifndef SIGNALING_CLIENT_H
#define SIGNALING_CLIENT_H

#include <QObject>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonArray>

class SignalingClient : public QObject {
    Q_OBJECT
public:
    explicit SignalingClient(const QString &serverUrl, QObject *parent = nullptr);
    
    void connectToServer();
    void createRoom(const QString &roomName, const QString &userName, const QString &password = "");
    void joinRoom(const QString &roomCode, const QString &userName, const QString &password = "");
    void leaveRoom();
    void sendChatMessage(const QString &userName, const QString &message);
    void sendMediaState(bool audioMuted, bool screenSharing);
    void sendMediaChunk(const QString &mediaKind, const QByteArray &payload);

    // Auth & friends
    void registerAccount(const QString &userName, const QString &email, const QString &password);
    void login(const QString &userName, const QString &password);
    void logout();
    void requestPasswordReset(const QString &userNameOrEmail);
    void resendVerification(const QString &userNameOrEmail);
    void sendFriendRequest(const QString &userName);
    void acceptFriendRequest(const QString &userName);
    void rejectFriendRequest(const QString &userName);
    void cancelFriendRequest(const QString &userName);
    void removeFriend(const QString &userName);
    void requestFriendsList();

    // Calling
    void callFriend(const QString &userName, const QString &roomCode);
    void declineCall(const QString &fromUserName);
    void cancelCall(const QString &toUserName);
    void acceptCall(const QString &fromUserName);

    // ---- Servers / channels / DM (Discord-benzeri) ----
    void listServers();
    void createServer(const QString &name);
    void joinServerByInvite(const QString &inviteCode);
    void leaveServer(int serverId);
    void deleteServer(int serverId);
    void renameServer(int serverId, const QString &name);
    void listMembers(int serverId);

    void listChannels(int serverId);
    void createChannel(int serverId, const QString &name, const QString &channelType);
    void deleteChannel(int channelId);
    void renameChannel(int channelId, const QString &name);

    void listChannelMessages(int channelId, qint64 beforeId = 0, int limit = 50);
    void sendChannelMessage(int channelId, const QString &content);
    void deleteChannelMessage(qint64 messageId);
    void editChannelMessage(qint64 messageId, const QString &content);

    void listDmThreads();
    void listDmMessages(const QString &peerUsername, qint64 beforeId = 0, int limit = 50);
    void sendDm(const QString &peerUsername, const QString &content);
    // E2E şifrelenmiş DM gönderir (ciphertext base64, nonce base64, sender pub key b64)
    void sendDmEncrypted(const QString &peerUsername,
                         const QString &ciphertextB64,
                         const QString &nonceB64,
                         const QString &senderPubB64);
    void markDmRead(const QString &peerUsername);
    void deleteDm(qint64 messageId);
    void editDm(qint64 messageId, const QString &content);
    void sendTypingChannel(int channelId);
    void sendTypingDm(const QString &peerUsername);

    // ---- 2FA ----
    void verifyLogin2fa(const QString &code);
    void getSecurityStatus();
    void startTotpSetup();
    void confirmTotpSetup(const QString &code);
    void disableTotp(const QString &code);
    void setPhoneNumber(const QString &e164);
    void sendPhoneCode();
    void verifyPhoneCode(const QString &code);
    void toggleSms2fa(bool enable);
    void toggleEmail2fa(bool enable, const QString &code = QString());

    // ---- Parolasız giriş ----
    void requestLoginCode(const QString &identifier);
    void verifyLoginCode(const QString &identifier, const QString &code);

    // ---- E2E DM Public Key ----
    void announcePublicKey(const QString &publicKeyB64);
    void requestPublicKey(const QString &username);

    // ---- Voice channels ----
    void joinVoiceChannel(int channelId);
    void leaveVoiceChannel();
    void sendVoiceChunk(int channelId, const QByteArray &pcm);
    void setVoiceState(int channelId, bool muted, bool deafened);
    void listVoiceParticipants(int channelId);
    void startVoiceShare(int channelId, const QString &kind);
    void stopVoiceShare(int channelId, const QString &kind);
    void sendVoiceMediaChunk(int channelId, const QString &kind, const QByteArray &jpeg);

signals:
    void connected();
    void disconnected();
    void roomCreated(const QString &roomCode, const QString &roomName);
    void roomJoined(const QString &roomCode, const QString &roomName);
    void participantJoined(const QString &participantId, const QString &userName);
    void participantLeft(const QString &participantId);
    void error(const QString &errorMessage);
    void mediaOffer(const QString &participantId, const QJsonObject &offer);
    void mediaAnswer(const QString &participantId, const QJsonObject &answer);
    void chatMessageReceived(const QString &userName, const QString &message, const QString &timestamp);
    void participantMediaStateChanged(const QString &participantId, bool audioMuted, bool screenSharing);
    void mediaChunkReceived(const QString &participantId, const QString &mediaKind, const QByteArray &payload);
    void participantsListed(const QJsonArray &participants);

    // Auth & friends signals
    void registerResult(bool ok, const QString &userNameOrError);
    void registerVerifyPending(const QString &email, const QString &message);
    void loginResult(bool ok, const QString &userNameOrError);
    void loginNeedsVerification(const QString &userName, const QString &email, const QString &message);
    void passwordResetSent(bool ok, const QString &message);
    void verificationSent(bool ok, const QString &message);
    void friendsListUpdated(const QJsonArray &friends, const QJsonArray &pendingIn, const QJsonArray &pendingOut);
    void friendRequestReceived(const QString &fromUserName);
    void friendAdded(const QString &userName, bool online);
    void friendRemoved(const QString &userName);
    void friendStatusChanged(const QString &userName, bool online);
    void friendOpResult(bool ok, const QString &op, const QString &userNameOrError);

    // Calling signals
    void incomingCall(const QString &fromUserName, const QString &roomCode);
    void callDeclined(const QString &fromUserName);
    void callCancelled(const QString &fromUserName);
    void callAccepted(const QString &fromUserName);
    void callUnreachable(const QString &userName);
    void callError(const QString &userName, const QString &error);

    // ---- Discord-benzeri signaller ----
    void serversListed(const QJsonArray &servers);
    void serverCreated(const QJsonObject &server);
    void serverJoined(const QJsonObject &server);
    void serverLeft(int serverId);
    void serverDeleted(int serverId);
    void serverRenamed(int serverId, const QString &name);
    void membersListed(int serverId, const QJsonArray &members);
    void memberJoined(int serverId, const QJsonObject &member);
    void memberLeft(int serverId, const QString &username);

    void channelsListed(int serverId, const QJsonArray &channels);
    void channelCreated(int serverId, const QJsonObject &channel);
    void channelDeleted(int serverId, int channelId);
    void channelRenamed(int serverId, int channelId, const QString &name);

    void channelMessagesListed(int channelId, const QJsonArray &messages);
    void channelMessageReceived(int serverId, int channelId, const QJsonObject &message);
    void channelMessageDeleted(int serverId, int channelId, qint64 messageId);
    void channelMessageEdited(int serverId, int channelId, qint64 messageId, const QString &content);

    void dmThreadsListed(const QJsonArray &threads);
    void dmMessagesListed(const QString &peerUsername, const QJsonArray &messages);
    void dmReceived(const QJsonObject &message);
    void dmDeleted(const QString &peerUsername, qint64 messageId);
    void dmEdited(const QString &peerUsername, qint64 messageId, const QString &content);
    void typingChannelReceived(int serverId, int channelId, const QString &username);
    void typingDmReceived(const QString &fromUsername);

    // ---- 2FA ----
    void login2faRequired(const QString &userName, bool totp, bool sms, bool email,
                          const QString &phoneHint, const QString &emailHint);
    void login2faResult(bool ok, const QString &userName, const QString &email, const QString &error);
    void securityStatus(bool totpEnabled, bool smsEnabled, bool emailEnabled, bool phoneVerified,
                        const QString &phone, const QString &phoneMasked,
                        const QString &emailMasked, bool twilioConfigured);
    void toggleEmail2faResult(bool ok, bool enabled, const QString &error);
    // Parolasız giriş
    void requestLoginCodeResult(bool ok, const QString &channel, const QString &maskedTarget,
                                bool dev, const QString &userName, const QString &error);
    void verifyLoginCodeResult(bool ok, const QString &userName, const QString &error);
    // E2E DM
    void announcePublicKeyResult(bool ok, const QString &error);
    void publicKeyResult(const QString &username, const QString &publicKeyB64, bool found);
    void totpSetup(const QString &secretBase32, const QString &otpauthUrl);
    void totpConfirmResult(bool ok, const QString &error);
    void totpDisableResult(bool ok, const QString &error);
    void setPhoneResult(bool ok, const QString &error);
    void sendPhoneCodeResult(bool ok, bool dev, const QString &error);
    void verifyPhoneCodeResult(bool ok, const QString &error);
    void toggleSms2faResult(bool ok, bool enabled, const QString &error);

    // ---- Voice ----
    void voiceJoined(int channelId, const QJsonArray &participants);
    void voiceLeft();
    void voiceMemberJoined(int channelId, const QJsonObject &member);
    void voiceMemberLeft(int channelId, qint64 userId, const QString &username);
    void voiceStateChanged(int channelId, qint64 userId, bool muted, bool deafened);
    void voiceChunkReceived(int channelId, qint64 userId, const QByteArray &pcm);
    void voiceParticipantsListed(int channelId, const QJsonArray &participants);
    void voiceShareStarted(int channelId, qint64 userId, const QString &username, const QString &kind);
    void voiceShareStopped(int channelId, qint64 userId, const QString &username, const QString &kind);
    void voiceMediaChunkReceived(int channelId, qint64 userId, const QString &username,
                                 const QString &kind, const QByteArray &jpeg);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);
    void onError(QAbstractSocket::SocketError error);

private:
    void sendMessage(const QJsonObject &message);
    
    QWebSocket *webSocket;
    QString serverUrl;
    QString currentRoomCode;
    QString clientId;
};

#endif
