#include "p2pnetwork.h"
#include <QDebug>

P2PNetwork::P2PNetwork(Blockchain* bc, QObject* parent) : QObject(parent), server(nullptr), blockchain(bc) {}
P2PNetwork::~P2PNetwork() { for (auto* p : peers) { p->disconnectFromHost(); p->deleteLater(); } }

void P2PNetwork::startServer(quint16 port) {
    server = new QTcpServer(this);
    connect(server, &QTcpServer::newConnection, this, &P2PNetwork::onNewConnection);
    if (server->listen(QHostAddress::Any, port))
        emit statusMessage(QString("Listening on port %1").arg(port));
}

void P2PNetwork::connectToPeer(const QString& host, quint16 port) {
    QTcpSocket* socket = new QTcpSocket();   // 没有父对象

    connect(socket, &QAbstractSocket::connected, this, [this, socket, host, port]() {
        peers.append(socket);
        connect(socket, &QTcpSocket::readyRead, this, &P2PNetwork::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &P2PNetwork::onDisconnected);
        emit peerConnected(host + ":" + QString::number(port));
        emit statusMessage("Connected to " + host + ":" + QString::number(port));
        requestFullChain(socket);
        });

    connect(socket, &QAbstractSocket::errorOccurred, this, [this, socket](QAbstractSocket::SocketError) {
        emit statusMessage("Connection failed: " + socket->errorString());
        socket->deleteLater();   // 主动连接失败则安全删除
        });

    socket->connectToHost(host, port);
}

void P2PNetwork::broadcastBlock(const Block& block) { sendMessageToAll(MSG_NEW_BLOCK, blockToJson(block)); }
void P2PNetwork::broadcastTransaction(const Transaction& tx) { sendMessageToAll(MSG_NEW_TX, txToJson(tx)); }
void P2PNetwork::requestFullChain(QTcpSocket* target) { sendMessage(target, MSG_REQUEST_CHAIN, {}); }

void P2PNetwork::onNewConnection() {
    while (QTcpSocket* socket = server->nextPendingConnection()) {
        // socket 已经是 server 的子对象，server 会自动删除它
        peers.append(socket);
        connect(socket, &QTcpSocket::readyRead, this, &P2PNetwork::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &P2PNetwork::onDisconnected);
    }
}

void P2PNetwork::onReadyRead() {
    auto* sender = qobject_cast<QTcpSocket*>(QObject::sender());
    if (!sender) return;
    processMessage(sender, sender->readAll());
}

void P2PNetwork::onDisconnected() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(QObject::sender());
    if (!socket) return;

    peers.removeAll(socket);

    // 只有“主动连接”的套接字才手动删除；
    // 传入套接字由 server 父对象负责删除。
    // 但由于我们无法简单区分，可以统一不删除，让 socket 自然销毁。
    // 这里不调用 deleteLater()，避免与 server 析构冲突。
}

void P2PNetwork::processMessage(QTcpSocket* sender, const QByteArray& data) {
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return;
    QJsonObject msg = doc.object();
    int type = msg["type"].toInt();
    QJsonObject payload = msg["payload"].toObject();

    switch (type) {
    case MSG_NEW_BLOCK: emit blockReceived(blockFromJson(payload)); break;
    case MSG_NEW_TX:    emit transactionReceived(txFromJson(payload)); break;
    case MSG_REQUEST_CHAIN: {
        QJsonArray arr;
        for (auto& b : blockchain->chain) arr.append(blockToJson(b));
        sendMessage(sender, MSG_RESPONSE_CHAIN, { {"chain", arr} });
        break;
    }
    case MSG_RESPONSE_CHAIN: {
        std::vector<Block> chain;
        for (auto v : payload["chain"].toArray()) chain.push_back(blockFromJson(v.toObject()));
        emit chainReceived(chain);
        break;
    }
    case MSG_FORK_CHAIN: {
        // 收到一串区块（分叉链），逐个添加
        QJsonArray arr = payload["chain"].toArray();
        for (auto v : arr) {
            Block b = blockFromJson(v.toObject());
            blockchain->processBlock(b);
        }
        break;
    }
    }
}

void P2PNetwork::sendMessage(QTcpSocket* r, MessageType t, const QJsonObject& p) {
    QJsonObject m; m["type"] = t; m["payload"] = p; r->write(QJsonDocument(m).toJson());
}
void P2PNetwork::sendMessageToAll(MessageType t, const QJsonObject& p) { for (auto* s : peers) sendMessage(s, t, p); }

QJsonObject P2PNetwork::blockToJson(const Block& b) const {
    QJsonObject j; j["index"] = b.index; j["timestamp"] = QString::fromStdString(b.timestamp);
    j["prevHash"] = QString::fromStdString(b.prevHash); j["hash"] = QString::fromStdString(b.hash);
    j["merkleRoot"] = QString::fromStdString(b.merkleRoot); j["nonce"] = (qint64)b.nonce;
    QJsonArray arr; for (auto& tx : b.transactions) arr.append(txToJson(tx));
    j["transactions"] = arr;
    return j;
}
QJsonObject P2PNetwork::txToJson(const Transaction& tx) const {
    QJsonObject o; o["sender"] = QString::fromStdString(tx.sender); o["receiver"] = QString::fromStdString(tx.receiver);
    o["amount"] = tx.amount; o["isCoinbase"] = tx.isCoinbase; o["isNFT"] = tx.isNFT;
    o["isNFTTransfer"] = tx.isNFTTransfer; o["nftID"] = QString::fromStdString(tx.nftID);
    o["nftData"] = QString::fromStdString(tx.nftData); o["isComputeTask"] = tx.isComputeTask;
    o["taskID"] = QString::fromStdString(tx.taskID); o["taskInput"] = QString::fromStdString(tx.taskInput);
    o["taskResult"] = QString::fromStdString(tx.taskResult); o["submitter"] = QString::fromStdString(tx.submitter);
    o["publicKey"] = QString::fromStdString(tx.publicKey); o["signature"] = QString::fromStdString(tx.signature);
    return o;
}
Block P2PNetwork::blockFromJson(const QJsonObject& j) {
    std::vector<Transaction> txs;
    for (auto v : j["transactions"].toArray()) txs.push_back(txFromJson(v.toObject()));
    Block b(j["index"].toInt(), txs, j["prevHash"].toString().toStdString(), 0);
    b.timestamp = j["timestamp"].toString().toStdString(); b.hash = j["hash"].toString().toStdString();
    b.merkleRoot = j["merkleRoot"].toString().toStdString(); b.nonce = j["nonce"].toInteger();
    return b;
}
Transaction P2PNetwork::txFromJson(const QJsonObject& o) {
    Transaction tx(o["sender"].toString().toStdString(), o["receiver"].toString().toStdString(),
        o["amount"].toDouble(), o["publicKey"].toString().toStdString(), o["signature"].toString().toStdString());
    tx.isCoinbase = o["isCoinbase"].toBool(); tx.isNFT = o["isNFT"].toBool();
    tx.isNFTTransfer = o["isNFTTransfer"].toBool(); tx.isComputeTask = o["isComputeTask"].toBool();
    tx.nftID = o["nftID"].toString().toStdString(); tx.nftData = o["nftData"].toString().toStdString();
    tx.taskID = o["taskID"].toString().toStdString(); tx.taskInput = o["taskInput"].toString().toStdString();
    tx.taskResult = o["taskResult"].toString().toStdString(); tx.submitter = o["submitter"].toString().toStdString();
    return tx;
}
void P2PNetwork::broadcastForkChain(const std::vector<Block>& chain) {
    QJsonArray arr;
    for (auto& b : chain) arr.append(blockToJson(b));
    sendMessageToAll(MSG_FORK_CHAIN, { {"chain", arr} });
}