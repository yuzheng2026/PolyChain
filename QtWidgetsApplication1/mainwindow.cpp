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
#include <chrono>
#include <map>
#include <string>
#include <sstream>

// ==================== 构造函数 ====================
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent),
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
    p2p->startServer(12345);
    connect(p2p, &P2PNetwork::blockReceived, this, &MainWindow::onBlockReceived);

    // ---------- 窗口基本设置 ----------
    setWindowTitle("Blockchain Simulator 2026 — PolyAVX");
    resize(950, 680);

    // ---------- 全局暗色主题 ----------
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

    // ---------- 主分割器 ----------
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

    // 标题
    QLabel* panelTitle = new QLabel("NEW TRANSACTION");
    panelTitle->setAlignment(Qt::AlignCenter);
    panelTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #58a6ff; margin-bottom: 8px;");
    rightLayout->addWidget(panelTitle);

    // 矿工地址
    minerAddressLabel = new QLabel(QString("Miner (YOU): %1...").arg(QString::fromStdString(youWallet.getAddress()).left(25)));
    minerAddressLabel->setStyleSheet("font-size: 9px; color: #8b949e;");
    rightLayout->addWidget(minerAddressLabel);

    // 发送方
    QLabel* senderLabel = new QLabel("SENDER");
    senderLabel->setStyleSheet("font-size: 10px; color: #8b949e; letter-spacing: 1px;");
    rightLayout->addWidget(senderLabel);
    senderInput = new QLineEdit;
    senderInput->setPlaceholderText("ALICE / BOB / EVE / YOU");
    senderInput->setStyleSheet(inputStyle());
    rightLayout->addWidget(senderInput);

    // 接收方
    QLabel* recvLabel = new QLabel("RECEIVER");
    recvLabel->setStyleSheet("font-size: 10px; color: #8b949e; letter-spacing: 1px;");
    rightLayout->addWidget(recvLabel);
    receiverInput = new QLineEdit;
    receiverInput->setPlaceholderText("ALICE / BOB / EVE / YOU");
    receiverInput->setStyleSheet(inputStyle());
    rightLayout->addWidget(receiverInput);

    // 金额
    QLabel* amtLabel = new QLabel("AMOUNT");
    amtLabel->setStyleSheet("font-size: 10px; color: #8b949e; letter-spacing: 1px;");
    rightLayout->addWidget(amtLabel);
    amountInput = new QLineEdit;
    amountInput->setPlaceholderText("10.0");
    amountInput->setStyleSheet(inputStyle());
    rightLayout->addWidget(amountInput);

    rightLayout->addSpacing(8);

    // 发送交易按钮
    QPushButton* sendBtn = new QPushButton("SEND TRANSACTION");
    sendBtn->setCursor(Qt::PointingHandCursor);
    sendBtn->setStyleSheet(
        "QPushButton { background-color: #238636; color: white; border: none;"
        "border-radius: 6px; padding: 12px 24px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background-color: #2ea043; }"
        "QPushButton:pressed { background-color: #196c2e; }");
    connect(sendBtn, &QPushButton::clicked, this, &MainWindow::sendTransaction);
    rightLayout->addWidget(sendBtn);

    // 挖矿按钮
    rightLayout->addSpacing(8);
    mineBtn = new QPushButton("MINE NEW BLOCK");
    mineBtn->setCursor(Qt::PointingHandCursor);
    mineBtn->setStyleSheet(
        "QPushButton { background-color: #1f6feb; color: white; border: none;"
        "border-radius: 6px; padding: 12px 24px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background-color: #388bfd; }"
        "QPushButton:pressed { background-color: #1559b0; }");
    connect(mineBtn, &QPushButton::clicked, this, &MainWindow::mineBlock);
    rightLayout->addWidget(mineBtn);

    // NFT 铸造按钮
    rightLayout->addSpacing(8);
    mintBtn = new QPushButton("MINT NFT");
    mintBtn->setCursor(Qt::PointingHandCursor);
    mintBtn->setStyleSheet(
        "QPushButton { background-color: #bf5b00; color: white; border: none;"
        "border-radius: 6px; padding: 12px 24px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background-color: #d97a00; }"
        "QPushButton:pressed { background-color: #8a4200; }");
    connect(mintBtn, &QPushButton::clicked, this, &MainWindow::mintNFT);
    rightLayout->addWidget(mintBtn);

    // P2P 连接按钮
    rightLayout->addSpacing(8);
    connectBtn = new QPushButton("CONNECT TO PEER");
    connectBtn->setCursor(Qt::PointingHandCursor);
    connectBtn->setStyleSheet(
        "QPushButton { background-color: #6e40c9; color: white; border: none;"
        "border-radius: 6px; padding: 12px 24px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background-color: #8957e5; }"
        "QPushButton:pressed { background-color: #4a2d8a; }");
    connect(connectBtn, &QPushButton::clicked, this, &MainWindow::connectToPeerDialog);
    rightLayout->addWidget(connectBtn);

    // ---------- 难度调节 ----------
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

    // ---------- 质押面板 ----------
    rightLayout->addSpacing(16);
    QLabel* stakeTitle = new QLabel("STAKING (PoS)");
    stakeTitle->setAlignment(Qt::AlignCenter);
    stakeTitle->setStyleSheet("font-size: 10px; color: #8b949e; letter-spacing: 1px;");
    rightLayout->addWidget(stakeTitle);
    stakeInfoLabel = new QLabel("YOU Stake: 0 coins");
    stakeInfoLabel->setAlignment(Qt::AlignCenter);
    stakeInfoLabel->setStyleSheet("font-size: 11px; color: #3fb950;");
    rightLayout->addWidget(stakeInfoLabel);
    stakeInput = new QLineEdit;
    stakeInput->setPlaceholderText("Amount to stake");
    stakeInput->setStyleSheet(inputStyle());
    rightLayout->addWidget(stakeInput);
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

    // ---------- 余额面板 ----------
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

    rightLayout->addStretch();

    // 将右侧卡片放入滚动区域
    rightScroll->setWidget(rightCard);
    splitter->addWidget(rightScroll);
    splitter->setSizes({ 750, 200 });

    // ---------- 初始刷新 ----------
    refreshDisplay();
}

// ==================== 发送交易（含签名） ====================
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
    if (nameToAddr.find(senderName) == nameToAddr.end() ||
        nameToAddr.find(recvName) == nameToAddr.end()) {
        QMessageBox::warning(this, "Unknown Sender/Receiver", "Must be ALICE, BOB, EVE, or YOU.");
        return;
    }

    Transaction tx(nameToAddr[senderName], nameToAddr[recvName], a);

    // 用发送方钱包签名
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
        QMessageBox::warning(this, "Insufficient Balance",
            QString("%1 does not have enough balance.").arg(s));
        return;
    }

    // 广播新区块
    p2p->broadcastBlock(bc.chain.back());
    refreshDisplay();

    senderInput->clear();
    receiverInput->clear();
    amountInput->clear();

    double gasFee = a * Blockchain::GAS_FEE_RATE;
    QString msg = QString("Block mined!\nDifficulty: %1\nTime: %2 ms\nNonce: %3\n"
        "GAS Fee: %4 (80% burned, 20% to miner)")
        .arg(bc.difficulty)
        .arg(elapsed, 0, 'f', 1)
        .arg(bc.chain.back().nonce)
        .arg(gasFee, 0, 'f', 4);
    QMessageBox::information(this, "Block Added", msg);
}

// ==================== 单独挖矿 ====================
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

    QString msg = QString("Block successfully mined!\nDifficulty: %1\nTime: %2 ms\n"
        "Nonce: %3\nReward: %4 coins -> YOU")
        .arg(bc.difficulty)
        .arg(elapsed, 0, 'f', 1)
        .arg(bc.chain.back().nonce)
        .arg(Blockchain::MINING_REWARD);
    QMessageBox::information(this, "Mining Complete", msg);
}

// ==================== NFT 铸造 ====================
void MainWindow::mintNFT() {
    bool ok;
    QString nftId = QInputDialog::getText(this, "Mint NFT", "NFT ID:", QLineEdit::Normal, "", &ok);
    if (!ok || nftId.isEmpty()) return;
    QString nftData = QInputDialog::getText(this, "Mint NFT", "NFT Data (e.g., IPFS hash):", QLineEdit::Normal, "", &ok);
    if (!ok) return;

    Transaction tx(youWallet.getAddress(), nftId.toStdString(), nftData.toStdString());
    std::vector<Transaction> txs;
    txs.push_back(tx);

    bool success = bc.addBlock(txs, youWallet.getAddress());
    if (!success) {
        QMessageBox::warning(this, "Error", "NFT already exists or failed.");
        return;
    }

    p2p->broadcastBlock(bc.chain.back());
    refreshDisplay();

    QMessageBox::information(this, "NFT Minted",
        QString("NFT '%1' minted to YOU.\nData: %2").arg(nftId, nftData));
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
            QString("Received block %1 from peer.\nHash: %2")
            .arg(block.index)
            .arg(QString::fromStdString(block.hash).left(20)));
        // 实际项目中应验证区块并加入链
    }
}

// ==================== 难度回调 ====================
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
    std::string youAddr = youWallet.getAddress();
    if (!bc.stake(youAddr, amount)) {
        QMessageBox::warning(this, "Insufficient Balance", "YOU don't have enough coins to stake.");
        return;
    }
    refreshDisplay();
    stakeInput->clear();
    QMessageBox::information(this, "Staking",
        QString("Successfully staked %1 coins.\nYOU will earn rewards on each new block.").arg(amount));
}

void MainWindow::unstakeCoins() {
    std::string youAddr = youWallet.getAddress();
    double stakedAmount = bc.stakes[youAddr];
    if (stakedAmount <= 0) {
        QMessageBox::information(this, "No Stake", "YOU have no staked coins.");
        return;
    }
    bc.unstake(youAddr, stakedAmount);
    refreshDisplay();
    QMessageBox::information(this, "Unstaking",
        QString("Successfully unstaked %1 coins.").arg(stakedAmount));
}

// ==================== 刷新界面 ====================
void MainWindow::refreshDisplay() {
    // 左侧区块画布
    blockView->updateGeometry();
    blockView->resize(blockView->sizeHint());
    blockView->update();

    // 右侧余额列表
    balanceList->clear();
    for (const auto& pair : bc.balances) {
        const std::string& addr = pair.first;
        double bal = pair.second;
        std::string name = addressToName(addr);
        if (name.empty()) name = addr.substr(0, 12) + "...";
        balanceList->addItem(QString("%1 : %2").arg(QString::fromStdString(name)).arg(bal, 0, 'f', 2));
    }

    // NFT 列表（显示前 5 个）
    for (const auto& nft : bc.nftOwners) {
        if (balanceList->count() < 20) { // 避免太拥挤
            balanceList->addItem(QString("NFT[%1] → %2")
                .arg(QString::fromStdString(nft.first).left(10))
                .arg(QString::fromStdString(addressToName(nft.second))));
        }
    }

    // 质押信息
    std::string youAddr = youWallet.getAddress();
    double myStake = bc.stakes[youAddr];
    stakeInfoLabel->setText(QString("YOU Stake: %1 coins").arg(myStake, 0, 'f', 2));

    // 销毁量
    burnedLabel->setText(QString("Total Burned: %1 coins").arg(bc.burnedCoins, 0, 'f', 4));

    // 总供应量（余额 + 质押 + 已销毁）
    double totalSupply = 0.0;
    for (const auto& p : bc.balances) totalSupply += p.second;
    for (const auto& p : bc.stakes)   totalSupply += p.second;
    totalSupply += bc.burnedCoins;
    totalSupplyLabel->setText(QString("Total Supply: %1 coins").arg(totalSupply, 0, 'f', 2));
}

// ==================== 输入框样式 ====================
QString MainWindow::inputStyle() const {
    return "QLineEdit { background-color: #0d1117; border: 1px solid #30363d; border-radius: 6px;"
        "padding: 10px 12px; font-size: 13px; color: #c9d1d9; }"
        "QLineEdit:focus { border-color: #58a6ff; }";
}

// ==================== 名字 ↔ 地址转换 ====================
std::string MainWindow::nameToAddress(const std::string& name) const {
    auto it = nameToAddr.find(name);
    return (it != nameToAddr.end()) ? it->second : "";
}

std::string MainWindow::addressToName(const std::string& addr) const {
    auto it = addrToName.find(addr);
    return (it != addrToName.end()) ? it->second : "";
}