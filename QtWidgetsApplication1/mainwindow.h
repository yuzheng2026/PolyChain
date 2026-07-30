#pragma once
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QScrollArea>
#include <QListWidget>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QInputDialog>
#include "blockchain.h"
#include "blockchainview.h"
#include "wallet.h"
#include "p2pnetwork.h"
#include <map>
#include <string>
#include <utility>
#include <vector>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() {}

private slots:
    void sendTransaction();
    void onDifficultyChanged(int value);
    void mineBlock();
    void stakeCoins();
    void unstakeCoins();
    void mintNFT();                 // NFT 铸造
    void connectToPeerDialog();     // P2P 连接
    void onBlockReceived(const Block& block); // 收到远程区块

private:
    void refreshDisplay();
    QString inputStyle() const;
    std::string nameToAddress(const std::string& name) const;
    std::string addressToName(const std::string& addr) const;

    // UI
    QLineEdit* senderInput;
    QLineEdit* receiverInput;
    QLineEdit* amountInput;
    BlockchainView* blockView;
    QScrollArea* scrollArea;
    QListWidget* balanceList;
    QSlider* difficultySlider;
    QLabel* difficultyLabel;
    QLabel* totalSupplyLabel;
    QLabel* burnedLabel;
    QLineEdit* stakeInput;
    QLabel* stakeInfoLabel;
    QPushButton* stakeBtn;
    QPushButton* unstakeBtn;
    QPushButton* mineBtn;
    QPushButton* mintBtn;
    QPushButton* connectBtn;
    QLabel* minerAddressLabel;

    Blockchain      bc;
    Wallet          aliceWallet, bobWallet, eveWallet, youWallet;
    P2PNetwork* p2p;

    std::map<std::string, std::string> nameToAddr;
    std::map<std::string, std::string> addrToName;
    std::map<std::string, Wallet*> nameToWallet;
};

#endif