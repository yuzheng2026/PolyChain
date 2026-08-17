#ifndef P2PNETWORK_H
#define P2PNETWORK_H

#include <QtNetwork/QAbstractSocket>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>    
#include <QObject>
#include <QList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "blockchain.h"

enum MessageType {
    MSG_NEW_BLOCK,
    MSG_NEW_TX,
    MSG_REQUEST_CHAIN,
    MSG_RESPONSE_CHAIN,
    MSG_GET_PEERS,
    MSG_FORK_CHAIN
};

class P2PNetwork : public QObject {
    Q_OBJECT
public:
    explicit P2PNetwork(Blockchain* bc, QObject* parent = nullptr);
    ~P2PNetwork();

    void startServer(quint16 port);
    void connectToPeer(const QString& host, quint16 port);
    void broadcastBlock(const Block& block);
    void broadcastTransaction(const Transaction& tx);
    void broadcastForkChain(const std::vector<Block>& chain);
    void requestFullChain(QTcpSocket* target);

signals:
    void blockReceived(const Block& block);
    void transactionReceived(const Transaction& tx);
    void chainReceived(const std::vector<Block>& chain);
    void peerConnected(const QString& peerInfo);
    void statusMessage(const QString& msg);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void processMessage(QTcpSocket* sender, const QByteArray& data);
    void sendMessage(QTcpSocket* receiver, MessageType type, const QJsonObject& payload);
    void sendMessageToAll(MessageType type, const QJsonObject& payload);

    Block blockFromJson(const QJsonObject& json);
    Transaction txFromJson(const QJsonObject& json);
    QJsonObject blockToJson(const Block& block) const;
    QJsonObject txToJson(const Transaction& tx) const;

    QTcpServer* server;
    QList<QTcpSocket*> peers;
    Blockchain* blockchain;
};

#endif