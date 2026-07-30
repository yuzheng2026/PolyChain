#include "p2pnetwork.h"

P2PNetwork::P2PNetwork(Blockchain* bc, QObject* parent)
    : QObject(parent), server(nullptr), blockchain(bc) {
}

void P2PNetwork::startServer(quint16 port) {
    server = new QTcpServer(this);
    connect(server, &QTcpServer::newConnection, this, &P2PNetwork::onNewConnection);
    server->listen(QHostAddress::Any, port);
}

void P2PNetwork::connectToPeer(const QString& host, quint16 port) {
    QTcpSocket* socket = new QTcpSocket(this);
    socket->connectToHost(host, port);
    if (socket->waitForConnected(3000)) {
        peers.append(socket);
        connect(socket, &QTcpSocket::readyRead, this, &P2PNetwork::onReadyRead);
        emit peerConnected(host + ":" + QString::number(port));
    }
}

void P2PNetwork::broadcastBlock(const Block& block) {
    QJsonObject json;
    json["index"] = block.index;
    json["timestamp"] = QString::fromStdString(block.timestamp);
    json["prevHash"] = QString::fromStdString(block.prevHash);
    json["hash"] = QString::fromStdString(block.hash);
    json["merkleRoot"] = QString::fromStdString(block.merkleRoot);
    json["nonce"] = (qint64)block.nonce;

    QJsonArray txArray;
    for (const auto& tx : block.transactions) {
        QJsonObject txObj;
        txObj["sender"] = QString::fromStdString(tx.sender);
        txObj["receiver"] = QString::fromStdString(tx.receiver);
        txObj["amount"] = tx.amount;
        txObj["isCoinbase"] = tx.isCoinbase;
        txObj["isNFT"] = tx.isNFT;
        txObj["nftID"] = QString::fromStdString(tx.nftID);
        txObj["nftData"] = QString::fromStdString(tx.nftData);
        txObj["publicKey"] = QString::fromStdString(tx.publicKey);
        txObj["signature"] = QString::fromStdString(tx.signature);
        txArray.append(txObj);
    }
    json["transactions"] = txArray;

    QJsonDocument doc(json);
    QByteArray data = doc.toJson();

    for (auto* peer : peers) {
        peer->write(data);
    }
}

void P2PNetwork::onNewConnection() {
    while (QTcpSocket* socket = server->nextPendingConnection()) {
        peers.append(socket);
        connect(socket, &QTcpSocket::readyRead, this, &P2PNetwork::onReadyRead);
    }
}

void P2PNetwork::onReadyRead() {
    QTcpSocket* sender = qobject_cast<QTcpSocket*>(QObject::sender());
    if (!sender) return;
    QByteArray data = sender->readAll();
    processMessage(sender, data);
}

void P2PNetwork::processMessage(QTcpSocket* sender, const QByteArray& data) {
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return;
    QJsonObject json = doc.object();
    Block block = blockFromJson(json);
    emit blockReceived(block);
}

Block P2PNetwork::blockFromJson(const QJsonObject& json) {
    std::vector<Transaction> txs;
    QJsonArray txArray = json["transactions"].toArray();
    for (const auto& val : txArray) {
        QJsonObject txObj = val.toObject();
        if (txObj["isCoinbase"].toBool()) {
            txs.push_back(Transaction(
                txObj["receiver"].toString().toStdString(),
                txObj["amount"].toDouble()));
        }
        else if (txObj["isNFT"].toBool()) {
            txs.push_back(Transaction(
                txObj["receiver"].toString().toStdString(),
                txObj["nftID"].toString().toStdString(),
                txObj["nftData"].toString().toStdString()));
        }
        else {
            Transaction tx(
                txObj["sender"].toString().toStdString(),
                txObj["receiver"].toString().toStdString(),
                txObj["amount"].toDouble(),
                txObj["publicKey"].toString().toStdString(),
                txObj["signature"].toString().toStdString());
            txs.push_back(tx);
        }
    }
    return Block(json["index"].toInt(), txs,
        json["prevHash"].toString().toStdString(),
        0);
}