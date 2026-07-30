#pragma once
#ifndef P2PNETWORK_H
#define P2PNETWORK_H

#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>
#include <QObject>
#include <QList>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "blockchain.h"

class P2PNetwork : public QObject {
    Q_OBJECT
public:
    explicit P2PNetwork(Blockchain* bc, QObject* parent = nullptr);
    void startServer(quint16 port);
    void connectToPeer(const QString& host, quint16 port);
    void broadcastBlock(const Block& block);

signals:
    void blockReceived(const Block& block);
    void peerConnected(const QString& peerInfo);

private slots:
    void onNewConnection();
    void onReadyRead();

private:
    void processMessage(QTcpSocket* sender, const QByteArray& data);
    Block blockFromJson(const QJsonObject& json);

    QTcpServer* server;
    QList<QTcpSocket*> peers;
    Blockchain* blockchain;
};

#endif