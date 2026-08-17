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

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    quint16 myPort;
    explicit MainWindow(quint16 listenPort = 12345, QWidget* parent = nullptr);
    ~MainWindow() {}

private slots:
    void simulateDoubleSign();
    void sendTransaction();
    void onDifficultyChanged(int value);
    void mineBlock();
    void stakeCoins();
    void unstakeCoins();
    void mintNFT();
    void transferNFT();
    void connectToPeerDialog();
    void publishZKPTask();
    void onBlockReceived(const Block& block);
    void onTransactionReceived(const Transaction& tx);
    void onChainReceived(const std::vector<Block>& chain);
    void onStatusMessage(const QString& msg);

private:
    void refreshDisplay();
    QString inputStyle() const;
    std::string nameToAddress(const std::string& name) const;
    std::string addressToName(const std::string& addr) const;

    QLineEdit* senderInput, * receiverInput, * amountInput, * stakeInput;
    BlockchainView* blockView;
    QScrollArea* scrollArea;
    QListWidget* balanceList, * zkpTaskList;
    QSlider* difficultySlider;
    QLabel* difficultyLabel, * totalSupplyLabel, * burnedLabel, * stakeInfoLabel;
    QLabel* minerAddressLabel, * p2pStatusLabel;
    QLabel* forkInfoLabel;
    QPushButton* stakeBtn, * unstakeBtn, * mineBtn, * mintBtn, * transferNFTBtn;
    QPushButton* connectBtn, * zkpTaskBtn;
    QPushButton* slashBtn;   // 模拟双重签名按钮
    Blockchain      bc;
    Wallet          aliceWallet, bobWallet, eveWallet, youWallet;
    P2PNetwork* p2p;

    std::map<std::string, std::string> nameToAddr, addrToName;
    std::map<std::string, Wallet*> nameToWallet;
};

#endif