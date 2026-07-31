// mainwindow.cpp — 最终完整版（所有功能集成）
#include "mainwindow.h"
#include <QApplication>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPalette>
#include <QSplitter>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QScrollArea>
#include <QInputDialog>
#include <QDebug>
#include <chrono>
#include <map>
#include <string>
#include <sstream>
#include <cstdlib>

// ==================== 构造函数 ====================
MainWindow::MainWindow(quint16 listenPort, QWidget* parent) : QMainWindow(parent),
myPort(listenPort),
aliceWallet(), bobWallet(), eveWallet(), youWallet()
{
    // ---------- 钱包映射 ----------
    nameToWallet["ALICE"] = &aliceWallet;
    nameToWallet["BOB"] = &bobWallet;
    nameToWallet["EVE"] = &eveWallet;
    nameToWallet["YOU"] = &youWallet;

    nameToAddr["ALICE"] = aliceWallet.getAddress();
    nameToAddr["BOB"] = bobWallet.getAddress();
    nameToAddr["EVE"] = eveWallet.getAddress();
    nameToAddr["YOU"] = youWallet.getAddress();
    for (auto& p : nameToAddr) addrToName[p.second] = p.first;

    // ---------- 创世块分配 ----------
    std::vector<std::pair<std::string, double>> genesisAlloc;
    genesisAlloc.push_back(std::make_pair(aliceWallet.getAddress(), 100.0));
    genesisAlloc.push_back(std::make_pair(bobWallet.getAddress(), 50.0));
    genesisAlloc.push_back(std::make_pair(eveWallet.getAddress(), 25.0));
    genesisAlloc.push_back(std::make_pair(youWallet.getAddress(), 200.0));
    bc.init(4, genesisAlloc);

    // ---------- P2P 网络 ----------
    p2p = new P2PNetwork(&bc, this);
    p2p->startServer(myPort);
    connect(p2p, &P2PNetwork::blockReceived, this, &MainWindow::onBlockReceived);
    connect(p2p, &P2PNetwork::transactionReceived, this, &MainWindow::onTransactionReceived);
    connect(p2p, &P2PNetwork::chainReceived, this, &MainWindow::onChainReceived);
    connect(p2p, &P2PNetwork::statusMessage, this, &MainWindow::onStatusMessage);

    // ---------- 窗口基本设置 ----------
    setWindowTitle(QString("Blockchain Simulator 2026 — PolyAVX (Port %1)").arg(myPort));
    resize(950, 680);

    // 暗色主题
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(20, 20, 20));
    darkPalette.setColor(QPalette::WindowText, QColor(220, 220, 220));
    darkPalette.setColor(QPalette::Base, QColor(35, 35, 35));
    darkPalette.setColor(QPalette::Text, QColor(220, 220, 220));
    darkPalette.setColor(QPalette::Button, QColor(50, 50, 50));
    darkPalette.setColor(QPalette::ButtonText, QColor(220, 220, 220));
    darkPalette.setColor(QPalette::Highlight, QColor(88, 166, 255));
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);
    QApplication::setPalette(darkPalette);

    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(splitter);

    // ==================== 左侧：可视化区块面板 ====================
    QFrame* leftCard = new QFrame;
    leftCard->setFrameShape(QFrame::StyledPanel);
    leftCard->setStyleSheet("QFrame { background-color: #1e1e1e; border-radius: 8px; padding: 4px; }");
    QVBoxLayout* leftLayout = new QVBoxLayout(leftCard);
    QLabel* titleLabel = new QLabel("BLOCKCHAIN LEDGER");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #58a6ff; padding: 4px;");
    leftLayout->addWidget(titleLabel);
    scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(false);
    scrollArea->setStyleSheet("QScrollArea { background-color: #141414; border: none; }");
    blockView = new BlockchainView(&bc);
    scrollArea->setWidget(blockView);
    leftLayout->addWidget(scrollArea);
    splitter->addWidget(leftCard);

    // ==================== 右侧：带滚动条的控制面板 ====================
    QScrollArea* rightScroll = new QScrollArea;
    rightScroll->setWidgetResizable(true);
    rightScroll->setStyleSheet("QScrollArea { background-color: #1e1e1e; border: none; }");
    rightScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QFrame* rightCard = new QFrame;
    rightCard->setFrameShape(QFrame::StyledPanel);
    rightCard->setStyleSheet("QFrame { background-color: #1e1e1e; border-radius: 8px; padding: 4px; }");
    QVBoxLayout* rightLayout = new QVBoxLayout(rightCard);
    rightLayout->setSpacing(12);

    QLabel* panelTitle = new QLabel("NEW TRANSACTION");
    panelTitle->setAlignment(Qt::AlignCenter);
    panelTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #58a6ff; margin-bottom: 8px;");
    rightLayout->addWidget(panelTitle);

    // 矿工地址
    minerAddressLabel = new QLabel(QString("Miner (YOU): %1...").arg(QString::fromStdString(youWallet.getAddress()).left(25)));
    minerAddressLabel->setStyleSheet("font-size: 9px; color: #8b949e;");
    rightLayout->addWidget(minerAddressLabel);

    // 输入框辅助 lambda
    auto addInput = [&](const char* label, QLineEdit*& input, const char* placeholder) {
        QLabel* lbl = new QLabel(label);
        lbl->setStyleSheet("font-size: 10px; color: #8b949e; letter-spacing: 1px;");
        rightLayout->addWidget(lbl);
        input = new QLineEdit;
        input->setPlaceholderText(placeholder);
        input->setStyleSheet(inputStyle());
        rightLayout->addWidget(input);
        };
    addInput("SENDER", senderInput, "ALICE / BOB / EVE / YOU");
    addInput("RECEIVER", receiverInput, "ALICE / BOB / EVE / YOU");
    addInput("AMOUNT", amountInput, "10.0");

    rightLayout->addSpacing(8);

    // 发送交易按钮
    QPushButton* sendBtn = new QPushButton("SEND TRANSACTION");
    sendBtn->setCursor(Qt::PointingHandCursor);
    sendBtn->setStyleSheet(
        "QPushButton { background-color: #238636; color: white; border: none;"
        "border-radius: 6px; padding: 12px 24px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background-color: #2ea043; }");
    connect(sendBtn, &QPushButton::clicked, this, &MainWindow::sendTransaction);
    rightLayout->addWidget(sendBtn);

    // 挖矿按钮
    mineBtn = new QPushButton("MINE NEW BLOCK");
    mineBtn->setCursor(Qt::PointingHandCursor);
    mineBtn->setStyleSheet(
        "QPushButton { background-color: #1f6feb; color: white; border: none;"
        "border-radius: 6px; padding: 12px 24px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background-color: #388bfd; }");
    connect(mineBtn, &QPushButton::clicked, this, &MainWindow::mineBlock);
    rightLayout->addWidget(mineBtn);

    // NFT 铸造
    mintBtn = new QPushButton("MINT NFT");
    mintBtn->setCursor(Qt::PointingHandCursor);
    mintBtn->setStyleSheet(
        "QPushButton { background-color: #bf5b00; color: white; border: none;"
        "border-radius: 6px; padding: 12px 24px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background-color: #d97a00; }");
    connect(mintBtn, &QPushButton::clicked, this, &MainWindow::mintNFT);
    rightLayout->addWidget(mintBtn);

    // NFT 转移
    transferNFTBtn = new QPushButton("TRANSFER NFT");
    transferNFTBtn->setCursor(Qt::PointingHandCursor);
    transferNFTBtn->setStyleSheet(
        "QPushButton { background-color: #8b5cf6; color: white; border: none;"
        "border-radius: 6px; padding: 12px 24px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background-color: #a78bfa; }");
    connect(transferNFTBtn, &QPushButton::clicked, this, &MainWindow::transferNFT);
    rightLayout->addWidget(transferNFTBtn);

    // P2P 连接
    connectBtn = new QPushButton("CONNECT TO PEER");
    connectBtn->setCursor(Qt::PointingHandCursor);
    connectBtn->setStyleSheet(
        "QPushButton { background-color: #6e40c9; color: white; border: none;"
        "border-radius: 6px; padding: 12px 24px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background-color: #8957e5; }");
    connect(connectBtn, &QPushButton::clicked, this, &MainWindow::connectToPeerDialog);
    rightLayout->addWidget(connectBtn);

    // P2P 状态
    p2pStatusLabel = new QLabel("P2P: Not connected to any peer");
    p2pStatusLabel->setStyleSheet("font-size: 9px; color: #8b949e; padding: 4px;");
    rightLayout->addWidget(p2pStatusLabel);
    forkInfoLabel = new QLabel("Forks: 0 | Main chain: 1 blocks");
    forkInfoLabel->setStyleSheet("font-size: 9px; color: #58a6ff; padding: 2px;");
    rightLayout->addWidget(forkInfoLabel);
    // 难度滑块
    rightLayout->addSpacing(16);
    QLabel* diffTitle = new QLabel("MINING DIFFICULTY");
    diffTitle->setAlignment(Qt::AlignCenter);
    diffTitle->setStyleSheet("font-size: 10px; color: #8b949e; letter-spacing: 1px;");
    rightLayout->addWidget(diffTitle);
    difficultyLabel = new QLabel("4");
    difficultyLabel->setAlignment(Qt::AlignCenter);
    difficultyLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #58a6ff;");
    rightLayout->addWidget(difficultyLabel);
    difficultySlider = new QSlider(Qt::Horizontal);
    difficultySlider->setRange(1, 20);
    difficultySlider->setValue(4);
    difficultySlider->setStyleSheet(
        "QSlider::groove:horizontal { height: 6px; background: #30363d; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #58a6ff; width: 18px; margin: -6px 0; border-radius: 9px; }");
    connect(difficultySlider, &QSlider::valueChanged, this, &MainWindow::onDifficultyChanged);
    rightLayout->addWidget(difficultySlider);

    // 质押面板
    rightLayout->addSpacing(16);
    QLabel* stakeTitle = new QLabel("STAKING (PoS)");
    stakeTitle->setAlignment(Qt::AlignCenter);
    stakeTitle->setStyleSheet("font-size: 10px; color: #8b949e; letter-spacing: 1px;");
    rightLayout->addWidget(stakeTitle);
    stakeInfoLabel = new QLabel("YOU Stake: 0 coins");
    stakeInfoLabel->setAlignment(Qt::AlignCenter);
    stakeInfoLabel->setStyleSheet("font-size: 11px; color: #3fb950;");
    rightLayout->addWidget(stakeInfoLabel);
    addInput("Amount to stake", stakeInput, "0.0");
    stakeBtn = new QPushButton("STAKE");
    stakeBtn->setStyleSheet(
        "QPushButton { background-color: #1f6feb; color: white; border: none;"
        "border-radius: 6px; padding: 8px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #388bfd; }");
    connect(stakeBtn, &QPushButton::clicked, this, &MainWindow::stakeCoins);
    rightLayout->addWidget(stakeBtn);
    unstakeBtn = new QPushButton("UNSTAKE ALL");
    unstakeBtn->setStyleSheet(
        "QPushButton { background-color: #da3633; color: white; border: none;"
        "border-radius: 6px; padding: 8px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #f85149; }");
    connect(unstakeBtn, &QPushButton::clicked, this, &MainWindow::unstakeCoins);
    rightLayout->addWidget(unstakeBtn);

    // 余额面板
    rightLayout->addSpacing(16);
    QLabel* balanceTitle = new QLabel("ACCOUNT BALANCES");
    balanceTitle->setAlignment(Qt::AlignCenter);
    balanceTitle->setStyleSheet("font-size: 10px; color: #8b949e; letter-spacing: 1px;");
    rightLayout->addWidget(balanceTitle);
    totalSupplyLabel = new QLabel;
    totalSupplyLabel->setAlignment(Qt::AlignCenter);
    totalSupplyLabel->setStyleSheet("font-size: 11px; color: #58a6ff; background-color: #141414;"
        "border: 1px solid #30363d; border-radius: 4px; padding: 4px;");
    rightLayout->addWidget(totalSupplyLabel);
    balanceList = new QListWidget;
    balanceList->setStyleSheet("QListWidget { background-color: #0d1117; border: 1px solid #30363d;"
        "border-radius: 4px; color: #c9d1d9; font-size: 12px; }");
    balanceList->setMaximumHeight(150);
    rightLayout->addWidget(balanceList);
    burnedLabel = new QLabel("Total Burned: 0 coins");
    burnedLabel->setAlignment(Qt::AlignCenter);
    burnedLabel->setStyleSheet("font-size: 10px; color: #f85149; margin-top: 8px;");
    rightLayout->addWidget(burnedLabel);

    // ZKP 任务面板
    rightLayout->addSpacing(16);
    QLabel* zkpTitle = new QLabel("ZKP COMPUTE TASKS");
    zkpTitle->setAlignment(Qt::AlignCenter);
    zkpTitle->setStyleSheet("font-size: 10px; color: #8b949e; letter-spacing: 1px;");
    rightLayout->addWidget(zkpTitle);
    zkpTaskList = new QListWidget;
    zkpTaskList->setStyleSheet("QListWidget { background-color: #0d1117; border: 1px solid #30363d;"
        "border-radius: 4px; color: #c9d1d9; font-size: 11px; }");
    zkpTaskList->setMaximumHeight(100);
    rightLayout->addWidget(zkpTaskList);
    zkpTaskBtn = new QPushButton("PUBLISH ZKP TASK");
    zkpTaskBtn->setStyleSheet(
        "QPushButton { background-color: #7c3aed; color: white; border: none;"
        "border-radius: 6px; padding: 10px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #a78bfa; }");
    connect(zkpTaskBtn, &QPushButton::clicked, this, &MainWindow::publishZKPTask);
    rightLayout->addWidget(zkpTaskBtn);

    rightLayout->addStretch();
    rightScroll->setWidget(rightCard);
    splitter->addWidget(rightScroll);
    splitter->setSizes({ 750, 200 });

    refreshDisplay();
}

// ==================== 交易发送 ====================
void MainWindow::sendTransaction() {
    QString s = senderInput->text().trimmed();
    QString r = receiverInput->text().trimmed();
    bool ok;
    double a = amountInput->text().toDouble(&ok);
    if (s.isEmpty() || r.isEmpty() || !ok || a <= 0) {
        QMessageBox::warning(this, "Invalid Input", "Please fill in valid sender, receiver, and amount.");
        return;
    }
    std::string senderName = s.toStdString();
    std::string recvName = r.toStdString();
    if (nameToAddr.find(senderName) == nameToAddr.end() || nameToAddr.find(recvName) == nameToAddr.end()) {
        QMessageBox::warning(this, "Unknown Sender/Receiver", "Must be ALICE, BOB, EVE, or YOU.");
        return;
    }

    Transaction tx(nameToAddr[senderName], nameToAddr[recvName], a);
    Wallet* senderWallet = nameToWallet[senderName];
    tx.publicKey = senderWallet->getPublicKey();
    tx.signature = senderWallet->signData(tx.toData());

    std::vector<Transaction> txs;
    txs.push_back(tx);

    auto start = std::chrono::steady_clock::now();
    bool success = bc.addBlock(txs, youWallet.getAddress());
    auto end = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end - start).count();

    if (!success) {
        QMessageBox::warning(this, "Insufficient Balance", QString("%1 does not have enough balance.").arg(s));
        return;
    }
    p2p->broadcastBlock(bc.chain.back());
    refreshDisplay();
    senderInput->clear(); receiverInput->clear(); amountInput->clear();

    double gasFee = a * Blockchain::GAS_FEE_RATE;
    QMessageBox::information(this, "Block Added",
        QString("Block mined!\nDifficulty: %1\nTime: %2 ms\nNonce: %3\nGAS Fee: %4 (80%% burned, 20%% to miner)")
        .arg(bc.difficulty).arg(elapsed, 0, 'f', 1).arg(bc.chain.back().nonce).arg(gasFee, 0, 'f', 4));
}

// ==================== 挖矿 ====================
void MainWindow::mineBlock() {
    std::vector<Transaction> emptyTxs;
    auto start = std::chrono::steady_clock::now();
    bool success = bc.addBlock(emptyTxs, youWallet.getAddress());
    auto end = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
    if (!success) {
        QMessageBox::warning(this, "Mining Failed", "Failed to mine a new block.");
        return;
    }
    p2p->broadcastBlock(bc.chain.back());
    refreshDisplay();
    QMessageBox::information(this, "Mining Complete",
        QString("Block successfully mined!\nDifficulty: %1\nTime: %2 ms\nNonce: %3\nReward: %4 coins -> YOU")
        .arg(bc.difficulty).arg(elapsed, 0, 'f', 1).arg(bc.chain.back().nonce).arg(Blockchain::MINING_REWARD));
}

// ==================== NFT 铸造 ====================
void MainWindow::mintNFT() {
    bool ok;
    QString nftId = QInputDialog::getText(this, "Mint NFT", "NFT ID:", QLineEdit::Normal, "", &ok);
    if (!ok || nftId.isEmpty()) return;
    QString nftData = QInputDialog::getText(this, "Mint NFT", "NFT Data (e.g. IPFS hash):", QLineEdit::Normal, "", &ok);
    if (!ok) return;

    Transaction tx(youWallet.getAddress(), nftId.toStdString(), nftData.toStdString());
    tx.publicKey = youWallet.getPublicKey();
    tx.signature = youWallet.signData(tx.toData());
    std::vector<Transaction> txs; txs.push_back(tx);

    bool success = bc.addBlock(txs, youWallet.getAddress());
    if (!success) {
        QMessageBox::warning(this, "Error", "NFT already exists or failed.");
        return;
    }
    p2p->broadcastBlock(bc.chain.back());
    refreshDisplay();
    QMessageBox::information(this, "NFT Minted", QString("NFT '%1' minted to YOU.\nData: %2").arg(nftId, nftData));
}

// ==================== NFT 转移 ====================
void MainWindow::transferNFT() {
    bool ok;
    QString nftId = QInputDialog::getText(this, "Transfer NFT", "NFT ID to transfer:", QLineEdit::Normal, "", &ok);
    if (!ok || nftId.isEmpty()) return;
    QString recipient = QInputDialog::getText(this, "Transfer NFT", "Recipient (ALICE/BOB/EVE/YOU):", QLineEdit::Normal, "", &ok);
    if (!ok || recipient.isEmpty()) return;

    std::string nftIdStr = nftId.toStdString();
    std::string recvName = recipient.toStdString();

    auto it = bc.nftOwners.find(nftIdStr);
    if (it == bc.nftOwners.end()) {
        QMessageBox::warning(this, "Error", QString("NFT '%1' does not exist.").arg(nftId));
        return;
    }
    if (it->second != youWallet.getAddress()) {
        QMessageBox::warning(this, "Error", "You don't own this NFT.");
        return;
    }
    if (nameToAddr.find(recvName) == nameToAddr.end()) {
        QMessageBox::warning(this, "Error", "Unknown recipient. Must be ALICE, BOB, EVE, or YOU.");
        return;
    }

    Transaction tx(youWallet.getAddress(), nameToAddr[recvName], nftIdStr, true);
    tx.publicKey = youWallet.getPublicKey();
    tx.signature = youWallet.signData(tx.toData());
    std::vector<Transaction> txs; txs.push_back(tx);

    bool success = bc.addBlock(txs, youWallet.getAddress());
    if (!success) {
        QMessageBox::warning(this, "Error", "Failed to transfer NFT.");
        return;
    }
    p2p->broadcastBlock(bc.chain.back());
    refreshDisplay();
    QMessageBox::information(this, "NFT Transferred",
        QString("NFT '%1' transferred to %2.").arg(nftId, recipient));
}

// ==================== P2P 连接 ====================
void MainWindow::connectToPeerDialog() {
    bool ok;
    QString host = QInputDialog::getText(this, "Connect to Peer", "Host:", QLineEdit::Normal, "127.0.0.1", &ok);
    if (!ok) return;
    int port = QInputDialog::getInt(this, "Connect to Peer", "Port:", 12346, 1, 65535, 1, &ok);
    if (!ok) return;
    p2p->connectToPeer(host, static_cast<quint16>(port));
}

void MainWindow::onBlockReceived(const Block& block) {
    if (block.index > bc.chain.back().index) {
        QMessageBox::information(this, "New Block",
            QString("Received block %1 from peer.\nHash: %2").arg(block.index).arg(QString::fromStdString(block.hash).left(20)));
    }
}

void MainWindow::onTransactionReceived(const Transaction& tx) {
    QMessageBox::information(this, "New Transaction", QString("Received tx: %1").arg(QString::fromStdString(tx.toString())));
}

void MainWindow::onChainReceived(const std::vector<Block>& chain) {
    if (chain.size() > bc.chain.size()) {
        QMessageBox::information(this, "Chain Synced", QString("Received longer chain with %1 blocks.").arg(chain.size()));
    }
}

void MainWindow::onStatusMessage(const QString& msg) {
    p2pStatusLabel->setText("P2P: " + msg);
}

// ==================== 难度调节 ====================
void MainWindow::onDifficultyChanged(int value) {
    difficultyLabel->setText(QString::number(value));
    bc.setDifficulty(value);
}

// ==================== 质押 / 取消质押 ====================
void MainWindow::stakeCoins() {
    bool ok;
    double amount = stakeInput->text().toDouble(&ok);
    if (!ok || amount <= 0) {
        QMessageBox::warning(this, "Invalid Input", "Please enter a valid amount to stake.");
        return;
    }
    if (!bc.stake(youWallet.getAddress(), amount)) {
        QMessageBox::warning(this, "Insufficient Balance", "YOU don't have enough coins to stake.");
        return;
    }
    refreshDisplay();
    stakeInput->clear();
    QMessageBox::information(this, "Staking", QString("Successfully staked %1 coins.").arg(amount));
}

void MainWindow::unstakeCoins() {
    double stakedAmount = bc.stakes[youWallet.getAddress()];
    if (stakedAmount <= 0) {
        QMessageBox::information(this, "No Stake", "YOU have no staked coins.");
        return;
    }
    bc.unstake(youWallet.getAddress(), stakedAmount);
    refreshDisplay();
    QMessageBox::information(this, "Unstaking", QString("Successfully unstaked %1 coins.").arg(stakedAmount));
}

// ==================== ZKP 任务发布 ====================
void MainWindow::publishZKPTask() {
    QStringList functions;
    functions << "sin" << "cos" << "exp" << "log";
    QString funcName = functions[rand() % functions.size()];

    int degree = 2 + rand() % 3;
    std::ostringstream coeffStream;
    for (int i = 0; i <= degree; ++i) {
        if (i > 0) coeffStream << " ";
        coeffStream << (1 + rand() % 5) << "." << (rand() % 10);
    }
    std::string inputCoeffs = coeffStream.str();
    int terms = 5 + rand() % 6;

    ZKPTask task = bc.createZKPTask(funcName.toStdString(), inputCoeffs, terms, youWallet.getAddress());
    bool success = bc.addZKPBlock(task, youWallet.getAddress());
    if (!success) {
        QMessageBox::warning(this, "ZKP Failed", "Failed to verify ZKP proof.");
        return;
    }
    p2p->broadcastBlock(bc.chain.back());
    refreshDisplay();

    zkpTaskList->clear();
    for (const auto& t : bc.zkpTasks) {
        zkpTaskList->addItem(QString("[%1] %2(%3, %4 terms)")
            .arg(QString::fromStdString(t.taskID).left(8))
            .arg(QString::fromStdString(t.functionName))
            .arg(QString::fromStdString(t.inputCoeffs))
            .arg(t.terms));
    }

    QMessageBox::information(this, "ZKP Task Completed",
        QString("Function: %1\nInput: %2\nTerms: %3\nResult: %4...")
        .arg(funcName).arg(QString::fromStdString(inputCoeffs)).arg(terms)
        .arg(QString::fromStdString(task.expectedResult).left(40)));
}

// ==================== 刷新显示 ====================
void MainWindow::refreshDisplay() {
    blockView->updateGeometry();
    blockView->resize(blockView->sizeHint());
    blockView->update();

    balanceList->clear();
    for (const auto& p : bc.balances) {
        std::string name = addressToName(p.first);
        if (name.empty()) name = p.first.substr(0, 12) + "...";
        balanceList->addItem(QString("%1 : %2").arg(QString::fromStdString(name)).arg(p.second, 0, 'f', 2));
    }
    for (const auto& nft : bc.nftOwners) {
        balanceList->addItem(QString("NFT[%1] → %2")
            .arg(QString::fromStdString(nft.first).left(10))
            .arg(QString::fromStdString(addressToName(nft.second))));
    }

    double myStake = bc.stakes[youWallet.getAddress()];
    stakeInfoLabel->setText(QString("YOU Stake: %1 coins").arg(myStake, 0, 'f', 2));
    burnedLabel->setText(QString("Total Burned: %1 coins").arg(bc.burnedCoins, 0, 'f', 4));

    double totalSupply = 0.0;
    for (const auto& p : bc.balances) totalSupply += p.second;
    for (const auto& p : bc.stakes) totalSupply += p.second;
    totalSupply += bc.burnedCoins;
    totalSupplyLabel->setText(QString("Total Supply: %1 coins").arg(totalSupply, 0, 'f', 2));
    int forkCount = bc.forks.size();
    forkInfoLabel->setText(QString("Forks: %1 | Main chain: %2 blocks").arg(forkCount).arg(bc.chain.size()));
}

// ==================== 辅助函数 ====================
QString MainWindow::inputStyle() const {
    return "QLineEdit { background-color: #0d1117; border: 1px solid #30363d; border-radius: 6px;"
        "padding: 10px 12px; font-size: 13px; color: #c9d1d9; }"
        "QLineEdit:focus { border-color: #58a6ff; }";
}

std::string MainWindow::nameToAddress(const std::string& name) const {
    auto it = nameToAddr.find(name);
    return it != nameToAddr.end() ? it->second : "";
}

std::string MainWindow::addressToName(const std::string& addr) const {
    auto it = addrToName.find(addr);
    return it != addrToName.end() ? it->second : "";
}