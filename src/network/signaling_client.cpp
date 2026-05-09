#include "signaling_client.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <QByteArray>

SignalingClient::SignalingClient(const QString &serverUrl, QObject *parent)
    : QObject(parent), serverUrl(serverUrl) {
    
    webSocket = new QWebSocket();
    clientId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    connect(webSocket, &QWebSocket::connected, this, &SignalingClient::onConnected);
    connect(webSocket, &QWebSocket::disconnected, this, &SignalingClient::onDisconnected);
    connect(webSocket, &QWebSocket::textMessageReceived, this, &SignalingClient::onTextMessageReceived);
    connect(webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this, &SignalingClient::onError);
}

void SignalingClient::connectToServer() {
    webSocket->open(QUrl(serverUrl));
}

void SignalingClient::createRoom(const QString &roomName, const QString &userName, const QString &password) {
    QJsonObject msg;
    msg["type"] = "create_room";
    msg["clientId"] = clientId;
    msg["roomName"] = roomName;
    msg["userName"] = userName;
    if (!password.isEmpty()) {
        msg["password"] = password;
    }
    sendMessage(msg);
}

void SignalingClient::joinRoom(const QString &roomCode, const QString &userName, const QString &password) {
    QJsonObject msg;
    msg["type"] = "join_room";
    msg["roomCode"] = roomCode;
    msg["clientId"] = clientId;
    msg["userName"] = userName;
    if (!password.isEmpty()) {
        msg["password"] = password;
    }
    sendMessage(msg);
}

void SignalingClient::leaveRoom() {
    QJsonObject msg;
    msg["type"] = "leave_room";
    msg["roomCode"] = currentRoomCode;
    msg["clientId"] = clientId;
    sendMessage(msg);
}

void SignalingClient::sendChatMessage(const QString &userName, const QString &message) {
    if (currentRoomCode.isEmpty() || message.trimmed().isEmpty()) {
        return;
    }

    QJsonObject msg;
    msg["type"] = "chat_message";
    msg["roomCode"] = currentRoomCode;
    msg["clientId"] = clientId;
    msg["userName"] = userName;
    msg["message"] = message;
    sendMessage(msg);
}

void SignalingClient::sendMediaState(bool audioMuted, bool screenSharing) {
    if (currentRoomCode.isEmpty()) {
        return;
    }

    QJsonObject msg;
    msg["type"] = "media_state";
    msg["roomCode"] = currentRoomCode;
    msg["clientId"] = clientId;
    msg["audioMuted"] = audioMuted;
    msg["screenSharing"] = screenSharing;
    sendMessage(msg);
}

void SignalingClient::sendMediaChunk(const QString &mediaKind, const QByteArray &payload) {
    if (currentRoomCode.isEmpty() || mediaKind.isEmpty() || payload.isEmpty()) {
        return;
    }

    QJsonObject msg;
    msg["type"] = "media_chunk";
    msg["roomCode"] = currentRoomCode;
    msg["clientId"] = clientId;
    msg["mediaKind"] = mediaKind;
    msg["payload"] = QString::fromLatin1(payload.toBase64());
    sendMessage(msg);
}

void SignalingClient::registerAccount(const QString &userName, const QString &email, const QString &password) {
    QJsonObject msg;
    msg["type"] = "register";
    msg["userName"] = userName;
    msg["email"] = email;
    msg["password"] = password;
    sendMessage(msg);
}

void SignalingClient::requestPasswordReset(const QString &userNameOrEmail) {
    QJsonObject msg;
    msg["type"] = "request_password_reset";
    msg["userName"] = userNameOrEmail;
    sendMessage(msg);
}

void SignalingClient::resendVerification(const QString &userNameOrEmail) {
    QJsonObject msg;
    msg["type"] = "resend_verification";
    msg["userName"] = userNameOrEmail;
    sendMessage(msg);
}

void SignalingClient::login(const QString &userName, const QString &password) {
    QJsonObject msg;
    msg["type"] = "login";
    msg["userName"] = userName;
    msg["password"] = password;
    sendMessage(msg);
}

void SignalingClient::logout() {
    QJsonObject msg; msg["type"] = "logout"; sendMessage(msg);
}

void SignalingClient::sendFriendRequest(const QString &userName) {
    QJsonObject msg; msg["type"] = "send_friend_request"; msg["userName"] = userName; sendMessage(msg);
}
void SignalingClient::acceptFriendRequest(const QString &userName) {
    QJsonObject msg; msg["type"] = "accept_friend_request"; msg["userName"] = userName; sendMessage(msg);
}
void SignalingClient::rejectFriendRequest(const QString &userName) {
    QJsonObject msg; msg["type"] = "reject_friend_request"; msg["userName"] = userName; sendMessage(msg);
}
void SignalingClient::cancelFriendRequest(const QString &userName) {
    QJsonObject msg; msg["type"] = "cancel_friend_request"; msg["userName"] = userName; sendMessage(msg);
}
void SignalingClient::removeFriend(const QString &userName) {
    QJsonObject msg; msg["type"] = "remove_friend"; msg["userName"] = userName; sendMessage(msg);
}
void SignalingClient::requestFriendsList() {
    QJsonObject msg; msg["type"] = "list_friends"; sendMessage(msg);
}

void SignalingClient::callFriend(const QString &userName, const QString &roomCode) {
    QJsonObject msg;
    msg["type"] = "call_friend";
    msg["userName"] = userName;
    msg["roomCode"] = roomCode;
    sendMessage(msg);
}
void SignalingClient::declineCall(const QString &fromUserName) {
    QJsonObject msg; msg["type"] = "call_decline"; msg["userName"] = fromUserName; sendMessage(msg);
}
void SignalingClient::cancelCall(const QString &toUserName) {
    QJsonObject msg; msg["type"] = "call_cancel"; msg["userName"] = toUserName; sendMessage(msg);
}
void SignalingClient::acceptCall(const QString &fromUserName) {
    QJsonObject msg; msg["type"] = "call_accept"; msg["userName"] = fromUserName; sendMessage(msg);
}

// ---- Servers ----
void SignalingClient::listServers() {
    QJsonObject m; m["type"] = "list_servers"; sendMessage(m);
}
void SignalingClient::createServer(const QString &name) {
    QJsonObject m; m["type"] = "create_server"; m["name"] = name; sendMessage(m);
}
void SignalingClient::joinServerByInvite(const QString &inviteCode) {
    QJsonObject m; m["type"] = "join_server"; m["inviteCode"] = inviteCode; sendMessage(m);
}
void SignalingClient::leaveServer(int serverId) {
    QJsonObject m; m["type"] = "leave_server"; m["serverId"] = serverId; sendMessage(m);
}
void SignalingClient::deleteServer(int serverId) {
    QJsonObject m; m["type"] = "delete_server"; m["serverId"] = serverId; sendMessage(m);
}
void SignalingClient::renameServer(int serverId, const QString &name) {
    QJsonObject m; m["type"] = "rename_server"; m["serverId"] = serverId; m["name"] = name; sendMessage(m);
}
void SignalingClient::listMembers(int serverId) {
    QJsonObject m; m["type"] = "list_members"; m["serverId"] = serverId; sendMessage(m);
}

// ---- Channels ----
void SignalingClient::listChannels(int serverId) {
    QJsonObject m; m["type"] = "list_channels"; m["serverId"] = serverId; sendMessage(m);
}
void SignalingClient::createChannel(int serverId, const QString &name, const QString &channelType) {
    QJsonObject m; m["type"] = "create_channel";
    m["serverId"] = serverId; m["name"] = name; m["channelType"] = channelType;
    sendMessage(m);
}
void SignalingClient::deleteChannel(int channelId) {
    QJsonObject m; m["type"] = "delete_channel"; m["channelId"] = channelId; sendMessage(m);
}
void SignalingClient::renameChannel(int channelId, const QString &name) {
    QJsonObject m; m["type"] = "rename_channel"; m["channelId"] = channelId; m["name"] = name; sendMessage(m);
}

// ---- Channel messages ----
void SignalingClient::listChannelMessages(int channelId, qint64 beforeId, int limit) {
    QJsonObject m; m["type"] = "list_messages"; m["channelId"] = channelId;
    if (beforeId > 0) m["beforeId"] = (double)beforeId;
    m["limit"] = limit;
    sendMessage(m);
}
void SignalingClient::sendChannelMessage(int channelId, const QString &content) {
    QJsonObject m; m["type"] = "send_message"; m["channelId"] = channelId; m["content"] = content;
    sendMessage(m);
}
void SignalingClient::deleteChannelMessage(qint64 messageId) {
    QJsonObject m; m["type"] = "delete_message"; m["messageId"] = (double)messageId; sendMessage(m);
}
void SignalingClient::editChannelMessage(qint64 messageId, const QString &content) {
    QJsonObject m; m["type"] = "edit_message"; m["messageId"] = (double)messageId; m["content"] = content;
    sendMessage(m);
}

// ---- DM ----
void SignalingClient::listDmThreads() {
    QJsonObject m; m["type"] = "list_dm_threads"; sendMessage(m);
}
void SignalingClient::listDmMessages(const QString &peerUsername, qint64 beforeId, int limit) {
    QJsonObject m; m["type"] = "list_dm_messages"; m["peerUsername"] = peerUsername;
    if (beforeId > 0) m["beforeId"] = (double)beforeId;
    m["limit"] = limit;
    sendMessage(m);
}
void SignalingClient::sendDm(const QString &peerUsername, const QString &content) {
    QJsonObject m; m["type"] = "send_dm"; m["peerUsername"] = peerUsername; m["content"] = content;
    sendMessage(m);
}
void SignalingClient::sendDmEncrypted(const QString &peerUsername,
                                      const QString &ciphertextB64,
                                      const QString &nonceB64,
                                      const QString &senderPubB64) {
    QJsonObject m;
    m["type"] = "send_dm";
    m["peerUsername"] = peerUsername;
    m["content"] = ciphertextB64;
    m["isEncrypted"] = true;
    m["nonce"] = nonceB64;
    m["senderPub"] = senderPubB64;
    sendMessage(m);
}
void SignalingClient::announcePublicKey(const QString &publicKeyB64) {
    QJsonObject m; m["type"] = "announce_pubkey"; m["publicKey"] = publicKeyB64;
    sendMessage(m);
}
void SignalingClient::requestPublicKey(const QString &username) {
    QJsonObject m; m["type"] = "get_pubkey"; m["username"] = username;
    sendMessage(m);
}
void SignalingClient::markDmRead(const QString &peerUsername) {
    QJsonObject m; m["type"] = "mark_dm_read"; m["peerUsername"] = peerUsername; sendMessage(m);
}
void SignalingClient::deleteDm(qint64 messageId) {
    QJsonObject m; m["type"] = "delete_dm"; m["messageId"] = (double)messageId; sendMessage(m);
}
void SignalingClient::editDm(qint64 messageId, const QString &content) {
    QJsonObject m; m["type"] = "edit_dm"; m["messageId"] = (double)messageId; m["content"] = content;
    sendMessage(m);
}
void SignalingClient::sendTypingChannel(int channelId) {
    QJsonObject m; m["type"] = "typing_channel"; m["channelId"] = channelId; sendMessage(m);
}
void SignalingClient::sendTypingDm(const QString &peerUsername) {
    QJsonObject m; m["type"] = "typing_dm"; m["peerUsername"] = peerUsername; sendMessage(m);
}
void SignalingClient::startVoiceShare(int channelId, const QString &kind) {
    QJsonObject m; m["type"] = "voice_share_start"; m["channelId"] = channelId; m["kind"] = kind;
    sendMessage(m);
}
void SignalingClient::stopVoiceShare(int channelId, const QString &kind) {
    QJsonObject m; m["type"] = "voice_share_stop"; m["channelId"] = channelId; m["kind"] = kind;
    sendMessage(m);
}
void SignalingClient::verifyLogin2fa(const QString &code) {
    QJsonObject m; m["type"] = "verify_login_2fa"; m["code"] = code; sendMessage(m);
}
void SignalingClient::requestLoginCode(const QString &identifier) {
    QJsonObject m; m["type"] = "request_login_code"; m["identifier"] = identifier;
    sendMessage(m);
}
void SignalingClient::verifyLoginCode(const QString &identifier, const QString &code) {
    QJsonObject m; m["type"] = "verify_login_code";
    m["identifier"] = identifier; m["code"] = code;
    sendMessage(m);
}
void SignalingClient::getSecurityStatus() {
    QJsonObject m; m["type"] = "get_security_status"; sendMessage(m);
}
void SignalingClient::startTotpSetup() {
    QJsonObject m; m["type"] = "start_totp_setup"; sendMessage(m);
}
void SignalingClient::confirmTotpSetup(const QString &code) {
    QJsonObject m; m["type"] = "confirm_totp_setup"; m["code"] = code; sendMessage(m);
}
void SignalingClient::disableTotp(const QString &code) {
    QJsonObject m; m["type"] = "disable_totp"; m["code"] = code; sendMessage(m);
}
void SignalingClient::setPhoneNumber(const QString &e164) {
    QJsonObject m; m["type"] = "set_phone"; m["phone"] = e164; sendMessage(m);
}
void SignalingClient::sendPhoneCode() {
    QJsonObject m; m["type"] = "send_phone_code"; sendMessage(m);
}
void SignalingClient::verifyPhoneCode(const QString &code) {
    QJsonObject m; m["type"] = "verify_phone_code"; m["code"] = code; sendMessage(m);
}
void SignalingClient::toggleSms2fa(bool enable) {
    QJsonObject m; m["type"] = "toggle_sms_2fa"; m["enable"] = enable; sendMessage(m);
}
void SignalingClient::toggleEmail2fa(bool enable, const QString &code) {
    QJsonObject m; m["type"] = "toggle_email_2fa"; m["enable"] = enable;
    if (!code.isEmpty()) m["code"] = code;
    sendMessage(m);
}

void SignalingClient::sendVoiceMediaChunk(int channelId, const QString &kind, const QByteArray &jpeg) {
    if (jpeg.isEmpty()) return;
    QJsonObject m;
    m["type"] = "voice_media_chunk";
    m["channelId"] = channelId;
    m["kind"] = kind;
    m["payload"] = QString::fromLatin1(jpeg.toBase64());
    sendMessage(m);
}

// ---- Voice ----
void SignalingClient::joinVoiceChannel(int channelId) {
    QJsonObject m; m["type"] = "voice_join"; m["channelId"] = channelId; sendMessage(m);
}
void SignalingClient::leaveVoiceChannel() {
    QJsonObject m; m["type"] = "voice_leave"; sendMessage(m);
}
void SignalingClient::sendVoiceChunk(int channelId, const QByteArray &pcm) {
    if (pcm.isEmpty()) return;
    QJsonObject m; m["type"] = "voice_chunk"; m["channelId"] = channelId;
    m["payload"] = QString::fromLatin1(pcm.toBase64());
    sendMessage(m);
}
void SignalingClient::setVoiceState(int channelId, bool muted, bool deafened) {
    QJsonObject m; m["type"] = "voice_state"; m["channelId"] = channelId;
    m["muted"] = muted; m["deafened"] = deafened;
    sendMessage(m);
}
void SignalingClient::listVoiceParticipants(int channelId) {
    QJsonObject m; m["type"] = "voice_list"; m["channelId"] = channelId; sendMessage(m);
}

void SignalingClient::onConnected() {
    emit connected();
}

void SignalingClient::onDisconnected() {
    emit disconnected();
}

void SignalingClient::onTextMessageReceived(const QString &message) {
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) return;
    
    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();
    
    if (type == "room_created") {
        currentRoomCode = obj["roomCode"].toString();
        QString roomName = obj["roomName"].toString();
        emit roomCreated(currentRoomCode, roomName);
    }
    else if (type == "room_joined") {
        currentRoomCode = obj["roomCode"].toString();
        QString roomName = obj["roomName"].toString();
        emit roomJoined(currentRoomCode, roomName);
        emit participantsListed(obj["participants"].toArray());
    }
    else if (type == "participant_joined") {
        QString userName = obj["userName"].toString();
        emit participantJoined(obj["participantId"].toString(), userName);
    }
    else if (type == "participant_left") {
        emit participantLeft(obj["participantId"].toString());
    }
    else if (type == "error") {
        const QString messageText = obj["message"].toString();
        // Older servers may not recognize new media message types yet.
        if (messageText.contains("Bilinmeyen mesaj tipi", Qt::CaseInsensitive)) {
            return;
        }
        emit error(messageText);
    }
    else if (type == "media_offer") {
        emit mediaOffer(obj["from"].toString(), obj["offer"].toObject());
    }
    else if (type == "media_answer") {
        emit mediaAnswer(obj["from"].toString(), obj["answer"].toObject());
    }
    else if (type == "chat_message") {
        emit chatMessageReceived(
            obj["userName"].toString(),
            obj["message"].toString(),
            obj["timestamp"].toString()
        );
    }
    else if (type == "media_state") {
        emit participantMediaStateChanged(
            obj["participantId"].toString(),
            obj["audioMuted"].toBool(),
            obj["screenSharing"].toBool()
        );
    }
    else if (type == "media_chunk") {
        emit mediaChunkReceived(
            obj["participantId"].toString(),
            obj["mediaKind"].toString(),
            QByteArray::fromBase64(obj["payload"].toString().toLatin1())
        );
    }
    else if (type == "register_result") {
        const bool ok = obj["ok"].toBool();
        emit registerResult(ok, ok ? obj["userName"].toString() : obj["error"].toString());
        if (ok) {
            emit registerVerifyPending(obj["email"].toString(),
                                       obj["message"].toString());
        }
    }
    else if (type == "login_result") {
        const bool ok = obj["ok"].toBool();
        if (!ok && obj["errorCode"].toString() == "email_not_verified") {
            emit loginNeedsVerification(obj["userName"].toString(),
                                        obj["email"].toString(),
                                        obj["error"].toString());
        } else {
            emit loginResult(ok, ok ? obj["userName"].toString() : obj["error"].toString());
        }
    }
    else if (type == "verification_sent") {
        emit verificationSent(obj["ok"].toBool(), obj["message"].toString());
    }
    else if (type == "password_reset_sent") {
        emit passwordResetSent(obj["ok"].toBool(), obj["message"].toString());
    }
    else if (type == "friends_list") {
        emit friendsListUpdated(obj["friends"].toArray(), obj["pendingIn"].toArray(), obj["pendingOut"].toArray());
    }
    else if (type == "friend_request") {
        emit friendRequestReceived(obj["fromUserName"].toString());
    }
    else if (type == "friend_added") {
        emit friendAdded(obj["userName"].toString(), obj["online"].toBool());
    }
    else if (type == "friend_removed") {
        emit friendRemoved(obj["userName"].toString());
    }
    else if (type == "friend_status") {
        emit friendStatusChanged(obj["userName"].toString(), obj["online"].toBool());
    }
    else if (type == "friend_op_result") {
        const bool ok = obj["ok"].toBool();
        emit friendOpResult(ok, obj["op"].toString(), ok ? obj["userName"].toString() : obj["error"].toString());
    }
    else if (type == "incoming_call") {
        emit incomingCall(obj["fromUserName"].toString(), obj["roomCode"].toString());
    }
    else if (type == "call_declined") {
        emit callDeclined(obj["fromUserName"].toString());
    }
    else if (type == "call_cancelled") {
        emit callCancelled(obj["fromUserName"].toString());
    }
    else if (type == "call_accepted") {
        emit callAccepted(obj["fromUserName"].toString());
    }
    else if (type == "call_unreachable") {
        emit callUnreachable(obj["userName"].toString());
    }
    else if (type == "call_error") {
        emit callError(obj["userName"].toString(), obj["error"].toString());
    }
    // ---- Discord-benzeri ----
    else if (type == "servers_list") {
        emit serversListed(obj["servers"].toArray());
    }
    else if (type == "server_created") {
        emit serverCreated(obj["server"].toObject());
    }
    else if (type == "server_joined") {
        emit serverJoined(obj["server"].toObject());
    }
    else if (type == "server_left") {
        emit serverLeft(obj["serverId"].toInt());
    }
    else if (type == "server_deleted") {
        emit serverDeleted(obj["serverId"].toInt());
    }
    else if (type == "server_renamed") {
        emit serverRenamed(obj["serverId"].toInt(), obj["name"].toString());
    }
    else if (type == "members_list") {
        emit membersListed(obj["serverId"].toInt(), obj["members"].toArray());
    }
    else if (type == "member_joined") {
        emit memberJoined(obj["serverId"].toInt(), obj["member"].toObject());
    }
    else if (type == "member_left") {
        emit memberLeft(obj["serverId"].toInt(), obj["username"].toString());
    }
    else if (type == "channels_list") {
        emit channelsListed(obj["serverId"].toInt(), obj["channels"].toArray());
    }
    else if (type == "channel_created") {
        emit channelCreated(obj["serverId"].toInt(), obj["channel"].toObject());
    }
    else if (type == "channel_deleted") {
        emit channelDeleted(obj["serverId"].toInt(), obj["channelId"].toInt());
    }
    else if (type == "channel_renamed") {
        emit channelRenamed(obj["serverId"].toInt(), obj["channelId"].toInt(),
                            obj["name"].toString());
    }
    else if (type == "messages_list") {
        emit channelMessagesListed(obj["channelId"].toInt(), obj["messages"].toArray());
    }
    else if (type == "message_received") {
        emit channelMessageReceived(obj["serverId"].toInt(), obj["channelId"].toInt(),
                                    obj["message"].toObject());
    }
    else if (type == "message_deleted") {
        emit channelMessageDeleted(obj["serverId"].toInt(), obj["channelId"].toInt(),
                                   (qint64)obj["messageId"].toDouble());
    }
    else if (type == "message_edited") {
        emit channelMessageEdited(obj["serverId"].toInt(), obj["channelId"].toInt(),
                                  (qint64)obj["messageId"].toDouble(),
                                  obj["content"].toString());
    }
    else if (type == "dm_threads") {
        emit dmThreadsListed(obj["threads"].toArray());
    }
    else if (type == "dm_messages") {
        emit dmMessagesListed(obj["peerUsername"].toString(), obj["messages"].toArray());
    }
    else if (type == "dm_received") {
        emit dmReceived(obj["message"].toObject());
    }
    else if (type == "dm_deleted") {
        emit dmDeleted(obj["peerUsername"].toString(),
                       (qint64)obj["messageId"].toDouble());
    }
    else if (type == "dm_edited") {
        emit dmEdited(obj["peerUsername"].toString(),
                      (qint64)obj["messageId"].toDouble(),
                      obj["content"].toString());
    }
    else if (type == "typing_channel") {
        emit typingChannelReceived(obj["serverId"].toInt(),
                                   obj["channelId"].toInt(),
                                   obj["username"].toString());
    }
    else if (type == "typing_dm") {
        emit typingDmReceived(obj["fromUsername"].toString());
    }
    // ---- Voice ----
    else if (type == "voice_joined") {
        emit voiceJoined(obj["channelId"].toInt(), obj["participants"].toArray());
    }
    else if (type == "voice_left") {
        emit voiceLeft();
    }
    else if (type == "voice_member_joined") {
        QJsonObject m;
        m["userId"]   = obj["userId"];
        m["username"] = obj["username"];
        m["muted"]    = obj["muted"];
        m["deafened"] = obj["deafened"];
        emit voiceMemberJoined(obj["channelId"].toInt(), m);
    }
    else if (type == "voice_member_left") {
        emit voiceMemberLeft(obj["channelId"].toInt(),
                             (qint64)obj["userId"].toDouble(),
                             obj["username"].toString());
    }
    else if (type == "voice_state") {
        emit voiceStateChanged(obj["channelId"].toInt(),
                               (qint64)obj["userId"].toDouble(),
                               obj["muted"].toBool(),
                               obj["deafened"].toBool());
    }
    else if (type == "voice_chunk") {
        emit voiceChunkReceived(obj["channelId"].toInt(),
                                (qint64)obj["userId"].toDouble(),
                                QByteArray::fromBase64(obj["payload"].toString().toLatin1()));
    }
    else if (type == "voice_participants") {
        emit voiceParticipantsListed(obj["channelId"].toInt(), obj["participants"].toArray());
    }
    else if (type == "voice_share_started") {
        emit voiceShareStarted(obj["channelId"].toInt(),
                               (qint64)obj["userId"].toDouble(),
                               obj["username"].toString(),
                               obj["kind"].toString());
    }
    else if (type == "voice_share_stopped") {
        emit voiceShareStopped(obj["channelId"].toInt(),
                               (qint64)obj["userId"].toDouble(),
                               obj["username"].toString(),
                               obj["kind"].toString());
    }
    else if (type == "voice_media_chunk") {
        emit voiceMediaChunkReceived(obj["channelId"].toInt(),
                                     (qint64)obj["userId"].toDouble(),
                                     obj["username"].toString(),
                                     obj["kind"].toString(),
                                     QByteArray::fromBase64(obj["payload"].toString().toLatin1()));
    }
    // ---- 2FA ----
    else if (type == "login_2fa_required") {
        const QJsonObject methods = obj.value("methods").toObject();
        emit login2faRequired(obj.value("userName").toString(),
                              methods.value("totp").toBool(),
                              methods.value("sms").toBool(),
                              methods.value("email").toBool(),
                              methods.value("phoneHint").toString(),
                              methods.value("emailHint").toString());
    }
    else if (type == "login_2fa_result") {
        emit login2faResult(obj.value("ok").toBool(),
                            obj.value("userName").toString(),
                            obj.value("email").toString(),
                            obj.value("error").toString());
    }
    else if (type == "security_status") {
        emit securityStatus(obj.value("totpEnabled").toBool(),
                            obj.value("smsEnabled").toBool(),
                            obj.value("emailEnabled").toBool(),
                            obj.value("phoneVerified").toBool(),
                            obj.value("phone").toString(),
                            obj.value("phoneMasked").toString(),
                            obj.value("emailMasked").toString(),
                            obj.value("twilioConfigured").toBool());
    }
    else if (type == "toggle_email_2fa_result") {
        emit toggleEmail2faResult(obj.value("ok").toBool(),
                                  obj.value("enabled").toBool(),
                                  obj.value("error").toString());
    }
    else if (type == "request_login_code_result") {
        emit requestLoginCodeResult(obj.value("ok").toBool(),
                                    obj.value("channel").toString(),
                                    obj.value("target").toString(),
                                    obj.value("dev").toBool(),
                                    obj.value("userName").toString(),
                                    obj.value("error").toString());
    }
    else if (type == "verify_login_code_result") {
        emit verifyLoginCodeResult(obj.value("ok").toBool(),
                                   obj.value("userName").toString(),
                                   obj.value("error").toString());
    }
    else if (type == "announce_pubkey_result") {
        emit announcePublicKeyResult(obj.value("ok").toBool(),
                                     obj.value("error").toString());
    }
    else if (type == "pubkey_result") {
        emit publicKeyResult(obj.value("username").toString(),
                             obj.value("publicKey").toString(),
                             obj.value("found").toBool());
    }
    else if (type == "totp_setup") {
        emit totpSetup(obj.value("secretBase32").toString(),
                       obj.value("otpauthUrl").toString());
    }
    else if (type == "totp_confirm_result") {
        emit totpConfirmResult(obj.value("ok").toBool(), obj.value("error").toString());
    }
    else if (type == "totp_disable_result") {
        emit totpDisableResult(obj.value("ok").toBool(), obj.value("error").toString());
    }
    else if (type == "set_phone_result") {
        emit setPhoneResult(obj.value("ok").toBool(), obj.value("error").toString());
    }
    else if (type == "send_phone_code_result") {
        emit sendPhoneCodeResult(obj.value("ok").toBool(),
                                 obj.value("dev").toBool(),
                                 obj.value("error").toString());
    }
    else if (type == "verify_phone_code_result") {
        emit verifyPhoneCodeResult(obj.value("ok").toBool(), obj.value("error").toString());
    }
    else if (type == "toggle_sms_2fa_result") {
        emit toggleSms2faResult(obj.value("ok").toBool(),
                                obj.value("enabled").toBool(),
                                obj.value("error").toString());
    }
}

void SignalingClient::onError(QAbstractSocket::SocketError error) {
    emit this->error(webSocket->errorString());
}

void SignalingClient::sendMessage(const QJsonObject &message) {
    QJsonDocument doc(message);
    webSocket->sendTextMessage(doc.toJson(QJsonDocument::Compact));
}
