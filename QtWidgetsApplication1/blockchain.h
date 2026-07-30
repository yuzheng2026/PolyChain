#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <string>
#include <vector>
#include <map>
#include <utility>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstdint>
#include <openssl/evp.h>

// 安全字符串截取
inline std::string safeTruncate(const std::string& s, size_t len = 8) {
    if (s.size() <= len * 2) return s;
    return s.substr(0, len) + ".." + s.substr(s.size() - len);
}

// SHA-256 (EVP)
inline std::string sha256(const std::string& data) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, data.c_str(), data.size());
    EVP_DigestFinal_ex(ctx, hash, &hashLen);
    EVP_MD_CTX_free(ctx);
    std::ostringstream oss;
    for (unsigned int i = 0; i < hashLen; ++i)
        oss << std::hex << std::setfill('0') << std::setw(2) << (int)hash[i];
    return oss.str();
}

// 交易
struct Transaction {
    std::string sender;
    std::string receiver;
    double amount;
    std::string publicKey;
    std::string signature;
    bool isCoinbase;
    bool isNFT;             // 是否为 NFT 交易
    std::string nftID;      // NFT 唯一标识符
    std::string nftData;    // NFT 元数据

    // 普通转账交易
    Transaction(const std::string& s, const std::string& r, double a,
        const std::string& pubKey = "", const std::string& sig = "")
        : sender(s), receiver(r), amount(a), publicKey(pubKey), signature(sig),
        isCoinbase(false), isNFT(false) {
    }

    // Coinbase 交易（矿工奖励）
    Transaction(const std::string& miner, double reward)
        : sender("COINBASE"), receiver(miner), amount(reward),
        publicKey(""), signature(""), isCoinbase(true), isNFT(false) {
    }

    // NFT 铸造交易
    Transaction(const std::string& creator, const std::string& nftId, const std::string& data)
        : sender("SYSTEM"), receiver(creator), amount(0.0),
        publicKey(""), signature(""),
        isCoinbase(false), isNFT(true), nftID(nftId), nftData(data) {
    }

    std::string toString() const {
        if (isCoinbase)
            return "COINBASE -> " + safeTruncate(receiver) + " : " + std::to_string(amount) + " (reward)";
        if (isNFT)
            return "MINT NFT [" + safeTruncate(nftID, 6) + "] to " + safeTruncate(receiver) + " (" + safeTruncate(nftData, 12) + ")";
        return safeTruncate(sender) + " -> " + safeTruncate(receiver) + " : " + std::to_string(amount);
    }

    std::string toData() const {
        std::ostringstream oss;
        oss << sender << receiver << amount;
        if (isNFT) oss << nftID << nftData;
        return oss.str();
    }
};

// 梅克尔根
inline std::string computeMerkleRoot(const std::vector<Transaction>& txs) {
    if (txs.empty()) return sha256("empty");
    std::vector<std::string> leaves;
    for (const auto& tx : txs) leaves.push_back(tx.toData());
    while (leaves.size() > 1) {
        std::vector<std::string> next;
        if (leaves.size() % 2 == 1) leaves.push_back(leaves.back());
        for (size_t i = 0; i < leaves.size(); i += 2)
            next.push_back(sha256(leaves[i] + leaves[i + 1]));
        leaves = next;
    }
    return leaves[0];
}

// 区块
class Block {
public:
    int index;
    std::string timestamp;
    std::string prevHash;
    std::string hash;
    std::string merkleRoot;
    std::vector<Transaction> transactions;
    uint64_t nonce;

    Block(int idx, const std::vector<Transaction>& txs, const std::string& prev,
        int difficulty = 4)
        : index(idx), transactions(txs), prevHash(prev), nonce(0) {
        std::time_t now = std::time(0);
        char buf[26];
        ctime_s(buf, sizeof(buf), &now);
        timestamp = buf;
        if (!timestamp.empty() && timestamp.back() == '\n') timestamp.pop_back();
        merkleRoot = computeMerkleRoot(transactions);
        hash = mine(difficulty);
    }

    Block(int idx, const Transaction& genesisTx, int difficulty = 4)
        : index(idx), prevHash("0"), nonce(0) {
        transactions.push_back(genesisTx);
        std::time_t now = std::time(0);
        char buf[26];
        ctime_s(buf, sizeof(buf), &now);
        timestamp = buf;
        if (!timestamp.empty() && timestamp.back() == '\n') timestamp.pop_back();
        merkleRoot = computeMerkleRoot(transactions);
        hash = mine(difficulty);
    }

    std::string blockHeader() const {
        std::ostringstream oss;
        oss << index << timestamp << merkleRoot << prevHash << nonce;
        return oss.str();
    }

    std::string mine(int difficulty) {
        const uint64_t target = UINT64_MAX >> difficulty;
        uint64_t hashNum = 0;
        const uint64_t MAX_NONCE = 20000000;
        std::string curr;
        nonce = 0;
        do {
            nonce++;
            curr = sha256(blockHeader());
            std::stringstream ss;
            ss << std::hex << curr.substr(0, 16);
            ss >> hashNum;
            if (nonce > MAX_NONCE)
                return sha256("BLOCK_MINE_FAILED_" + std::to_string(nonce));
        } while (hashNum >= target);
        return curr;
    }
};

// 区块链
class Blockchain {
public:
    std::vector<Block> chain;
    int difficulty;
    std::map<std::string, double> balances;
    std::map<std::string, double> stakes;
    double burnedCoins;

    static constexpr double MINING_REWARD = 10.0;
    static constexpr double STAKING_REWARD_RATE = 0.01;
    static constexpr double GAS_FEE_RATE = 0.005;

    // NFT 所有权映射
    std::map<std::string, std::string> nftOwners;  // nftID -> 当前所有者地址

    // 默认构造函数
    Blockchain() : difficulty(4), burnedCoins(0.0) {}

    // 初始化
    void init(int diff, const std::vector<std::pair<std::string, double>>& genesisAlloc) {
        difficulty = diff;
        chain.clear();
        balances.clear();
        stakes.clear();
        burnedCoins = 0.0;
        nftOwners.clear();

        std::vector<Transaction> genesisTxs;
        for (size_t i = 0; i < genesisAlloc.size(); ++i) {
            const std::string& addr = genesisAlloc[i].first;
            double amt = genesisAlloc[i].second;
            genesisTxs.push_back(Transaction("SYSTEM", addr, amt));
        }
        chain.push_back(Block(0, genesisTxs, "0", difficulty));
        for (size_t i = 0; i < genesisTxs.size(); ++i) {
            balances[genesisTxs[i].receiver] += genesisTxs[i].amount;
        }
    }

    bool stake(const std::string& address, double amount) {
        if (balances[address] < amount) return false;
        balances[address] -= amount;
        stakes[address] += amount;
        return true;
    }

    bool unstake(const std::string& address, double amount) {
        if (stakes[address] < amount) return false;
        stakes[address] -= amount;
        balances[address] += amount;
        return true;
    }

    // 签名验证函数前向声明（实现在 wallet.h 中，此处仅声明）
    static bool verifySignature(const std::string& publicKey,
        const std::string& data,
        const std::string& signature);

    bool addBlock(const std::vector<Transaction>& txs,
        const std::string& minerAddress) {
        std::vector<Transaction> processedTxs;
        for (size_t i = 0; i < txs.size(); ++i) {
            Transaction tx = txs[i];
            if (tx.isCoinbase) {
                processedTxs.push_back(tx);
                continue;
            }

            // 签名验证（跳过 coinbase 和 NFT 铸造交易）
            if (!tx.isNFT && !tx.signature.empty()) {
                if (!verifySignature(tx.publicKey, tx.toData(), tx.signature)) {
                    // 签名无效，拒绝此交易
                    continue;
                }
            }

            // NFT 交易处理
            if (tx.isNFT) {
                if (nftOwners.find(tx.nftID) != nftOwners.end()) continue; // 已存在
                nftOwners[tx.nftID] = tx.receiver;
                processedTxs.push_back(tx);
                continue;
            }

            // 普通转账：检查余额
            if (balances[tx.sender] < tx.amount) continue;

            double gasFee = tx.amount * GAS_FEE_RATE;
            double burnAmount = gasFee * 0.8;
            double minerShare = gasFee * 0.2;

            balances[tx.sender] -= (tx.amount + gasFee);
            balances[tx.receiver] += tx.amount;
            burnedCoins += burnAmount;
            balances[minerAddress] += minerShare;

            processedTxs.push_back(tx);
        }

        // 分配质押奖励
        double totalStaked = 0.0;
        for (std::map<std::string, double>::const_iterator it = stakes.begin();
            it != stakes.end(); ++it)
            totalStaked += it->second;
        if (totalStaked > 0) {
            for (std::map<std::string, double>::iterator it = stakes.begin();
                it != stakes.end(); ++it) {
                double reward = it->second * STAKING_REWARD_RATE;
                balances[it->first] += reward;
            }
        }

        // Coinbase 交易
        Transaction coinbase(minerAddress, MINING_REWARD);
        balances[minerAddress] += MINING_REWARD;

        std::vector<Transaction> finalTxs;
        finalTxs.push_back(coinbase);
        for (size_t i = 0; i < processedTxs.size(); ++i)
            finalTxs.push_back(processedTxs[i]);

        chain.push_back(Block(chain.back().index + 1, finalTxs,
            chain.back().hash, difficulty));
        return true;
    }

    // NFT 转移（普通转账中若涉及 nftID 则调用）
    bool transferNFT(const std::string& from, const std::string& to, const std::string& nftID) {
        if (nftOwners[nftID] != from) return false;
        nftOwners[nftID] = to;
        return true;
    }

    void setDifficulty(int diff) { difficulty = diff; }
};

constexpr double Blockchain::MINING_REWARD;
constexpr double Blockchain::STAKING_REWARD_RATE;
constexpr double Blockchain::GAS_FEE_RATE;

// 静态签名验证函数（桥接到 Wallet）
#include "wallet.h"
inline bool Blockchain::verifySignature(const std::string& publicKey,
    const std::string& data,
    const std::string& signature) {
    return Wallet::verifySignature(publicKey, data, signature);
}

#endif // BLOCKCHAIN_H