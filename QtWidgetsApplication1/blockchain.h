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
#include "poly_avx.hpp"

// 安全截取字符串
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

// 交易结构体
struct Transaction {
    std::string sender, receiver;
    double amount;
    std::string publicKey, signature;
    bool isCoinbase, isNFT, isNFTTransfer, isComputeTask;
    std::string nftID, nftData;
    std::string taskID, taskInput, taskResult, submitter;

    Transaction(const std::string& s, const std::string& r, double a,
        const std::string& pk = "", const std::string& sig = "")
        : sender(s), receiver(r), amount(a), publicKey(pk), signature(sig),
        isCoinbase(false), isNFT(false), isNFTTransfer(false), isComputeTask(false) {
    }

    Transaction(const std::string& miner, double reward)
        : sender("COINBASE"), receiver(miner), amount(reward),
        isCoinbase(true), isNFT(false), isNFTTransfer(false), isComputeTask(false) {
    }

    Transaction(const std::string& creator, const std::string& nftId, const std::string& data)
        : sender("SYSTEM"), receiver(creator), amount(0.0),
        isCoinbase(false), isNFT(true), isNFTTransfer(false), isComputeTask(false),
        nftID(nftId), nftData(data) {
    }

    Transaction(const std::string& from, const std::string& to,
        const std::string& nftId, bool isTransfer)
        : sender(from), receiver(to), amount(0.0),
        isCoinbase(false), isNFT(true), isNFTTransfer(true), isComputeTask(false),
        nftID(nftId) {
    }

    Transaction(const std::string& taskId, const std::string& input,
        const std::string& result, const std::string& miner)
        : sender(miner), receiver(""), amount(0.0),
        isCoinbase(false), isNFT(false), isNFTTransfer(false), isComputeTask(true),
        taskID(taskId), taskInput(input), taskResult(result), submitter(miner) {
    }

    std::string toString() const {
        if (isCoinbase) return "COINBASE -> " + safeTruncate(receiver) + " : " + std::to_string(amount) + " (reward)";
        if (isNFT && isNFTTransfer) return "TRANSFER NFT [" + safeTruncate(nftID, 6) + "] from " + safeTruncate(sender) + " to " + safeTruncate(receiver);
        if (isNFT) return "MINT NFT [" + safeTruncate(nftID, 6) + "] to " + safeTruncate(receiver);
        if (isComputeTask) return "ZKP TASK [" + safeTruncate(taskID, 8) + "] solved by " + safeTruncate(submitter);
        return safeTruncate(sender) + " -> " + safeTruncate(receiver) + " : " + std::to_string(amount);
    }

    std::string toData() const {
        std::ostringstream oss;
        oss << sender << receiver << amount;
        if (isNFT) oss << nftID << nftData;
        if (isComputeTask) oss << taskID << taskInput << taskResult;
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
    std::string timestamp, prevHash, hash, merkleRoot;
    std::vector<Transaction> transactions;
    uint64_t nonce;

    Block(int idx, const std::vector<Transaction>& txs, const std::string& prev, int difficulty = 4)
        : index(idx), transactions(txs), prevHash(prev), nonce(0) {
        std::time_t now = std::time(0); char buf[26]; ctime_s(buf, sizeof(buf), &now);
        timestamp = buf; if (!timestamp.empty() && timestamp.back() == '\n') timestamp.pop_back();
        merkleRoot = computeMerkleRoot(transactions);
        hash = mine(difficulty);
    }

    Block(int idx, const Transaction& genesisTx, int difficulty = 4)
        : index(idx), prevHash("0"), nonce(0) {
        transactions.push_back(genesisTx);
        std::time_t now = std::time(0); char buf[26]; ctime_s(buf, sizeof(buf), &now);
        timestamp = buf; if (!timestamp.empty() && timestamp.back() == '\n') timestamp.pop_back();
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
            std::stringstream ss; ss << std::hex << curr.substr(0, 16); ss >> hashNum;
            if (nonce > MAX_NONCE) return sha256("BLOCK_MINE_FAILED_" + std::to_string(nonce));
        } while (hashNum >= target);
        return curr;
    }
};

// ZKP 任务结构
struct ZKPTask {
    std::string taskID, functionName, inputCoeffs, expectedResult, proverAddress, proof;
    int terms;
};

// 区块链
class Blockchain {
public:
    std::vector<Block> chain;
    int difficulty;
    std::map<std::string, double> balances, stakes;
    double burnedCoins;
    std::map<std::string, std::string> nftOwners;
    std::vector<ZKPTask> zkpTasks;

    static constexpr double MINING_REWARD = 10.0, STAKING_REWARD_RATE = 0.01, GAS_FEE_RATE = 0.005, ZKP_REWARD = 15.0;

    Blockchain() : difficulty(4), burnedCoins(0.0) {}

    void init(int diff, const std::vector<std::pair<std::string, double>>& genesisAlloc) {
        difficulty = diff; chain.clear(); balances.clear(); stakes.clear(); burnedCoins = 0.0; nftOwners.clear();
        std::vector<Transaction> genesisTxs;
        for (const auto& p : genesisAlloc) genesisTxs.push_back(Transaction("SYSTEM", p.first, p.second));
        chain.push_back(Block(0, genesisTxs, "0", difficulty));
        mainChainTip = chain.back().hash;
        for (const auto& tx : genesisTxs) balances[tx.receiver] += tx.amount;
    }

    bool stake(const std::string& addr, double amt) {
        if (balances[addr] < amt) return false;
        balances[addr] -= amt; stakes[addr] += amt; return true;
    }
    bool unstake(const std::string& addr, double amt) {
        if (stakes[addr] < amt) return false;
        stakes[addr] -= amt; balances[addr] += amt; return true;
    }

    static bool verifySignature(const std::string& pk, const std::string& data, const std::string& sig);

    bool addBlock(const std::vector<Transaction>& txs, const std::string& miner) {
        std::vector<Transaction> processed;
        for (auto tx : txs) {
            if (tx.isCoinbase) { processed.push_back(tx); continue; }
            if (!tx.isNFT && !tx.isComputeTask && !tx.signature.empty())
                if (!verifySignature(tx.publicKey, tx.toData(), tx.signature)) continue;
            if (tx.isNFT) {
                if (tx.isNFTTransfer) { if (nftOwners[tx.nftID] == tx.sender) nftOwners[tx.nftID] = tx.receiver; }
                else { if (nftOwners.find(tx.nftID) == nftOwners.end()) nftOwners[tx.nftID] = tx.receiver; }
                processed.push_back(tx); continue;
            }
            if (tx.isComputeTask) { processed.push_back(tx); continue; }
            if (balances[tx.sender] < tx.amount) continue;
            double gas = tx.amount * GAS_FEE_RATE;
            double burn = gas * 0.8, minerShare = gas * 0.2;
            balances[tx.sender] -= (tx.amount + gas); balances[tx.receiver] += tx.amount;
            burnedCoins += burn; balances[miner] += minerShare;
            processed.push_back(tx);
        }
        double totalStaked = 0; for (const auto& p : stakes) totalStaked += p.second;
        if (totalStaked > 0) for (auto& p : stakes) balances[p.first] += p.second * STAKING_REWARD_RATE;
        processed.insert(processed.begin(), Transaction(miner, MINING_REWARD));
        balances[miner] += MINING_REWARD;
        chain.push_back(Block(chain.back().index + 1, processed, chain.back().hash, difficulty));
        return true;
    }

    void setDifficulty(int d) { difficulty = d; }
    // 分叉管理：保存所有已知的链（每条链的末端区块哈希 → 完整链）
    std::map<std::string, std::vector<Block>> forks;
    std::string mainChainTip;  // 当前主链末端区块的哈希

    // 处理接收到的区块（可能形成分叉）
    bool processBlock(const Block& block);

    // 切换主链
    bool switchToChain(const std::string& tipHash);

    // 获取当前主链
    std::vector<Block>& getMainChain() { return chain; }

    // 初始化后设置主链
    void setMainChain() {
        if (!chain.empty()) mainChainTip = chain.back().hash;
    }
    std::string generateZKPProof(const std::string& tid, const std::string& result) { return sha256(tid + result + "SALT"); }
    bool verifyZKPProof(const ZKPTask& t) { return t.proof == sha256(t.taskID + t.expectedResult + "SALT"); }

    std::string executeComputeTask(const std::string& func, const std::string& coeffs, int terms) {
        using namespace poly_avx;
        std::vector<double> c; std::stringstream ss(coeffs); double v; while (ss >> v) c.push_back(v);
        PolyD A(c); PolyD res;
        if (func == "sin") { PolyD A0 = A - PolyD(A[0]); res = poly_sin(A0, terms); }
        else if (func == "cos") { PolyD A0 = A - PolyD(A[0]); res = poly_cos(A0, terms); }
        else if (func == "exp") res = A.exp(terms);
        else if (func == "log") res = A.log(terms);
        else return "";
        std::ostringstream rs; for (int i = 0; i < terms; ++i) { if (i) rs << " "; rs << res[i]; }
        return rs.str();
    }

    ZKPTask createZKPTask(const std::string& func, const std::string& coeffs, int terms, const std::string& prover) {
        ZKPTask t; t.taskID = sha256(func + coeffs + std::to_string(terms) + std::to_string(chain.size())).substr(0, 16);
        t.functionName = func; t.inputCoeffs = coeffs; t.terms = terms;
        t.expectedResult = executeComputeTask(func, coeffs, terms);
        t.proverAddress = prover; t.proof = generateZKPProof(t.taskID, t.expectedResult);
        zkpTasks.push_back(t); return t;
    }

    bool addZKPBlock(const ZKPTask& t, const std::string& miner) {
        if (!verifyZKPProof(t)) return false;
        Transaction reward(miner, ZKP_REWARD);
        std::vector<Transaction> txs; txs.push_back(reward);
        return addBlock(txs, miner);
    }
    
};

constexpr double Blockchain::MINING_REWARD, Blockchain::STAKING_REWARD_RATE, Blockchain::GAS_FEE_RATE, Blockchain::ZKP_REWARD;

#include "wallet.h"
inline bool Blockchain::verifySignature(const std::string& pk, const std::string& data, const std::string& sig) {
    return Wallet::verifySignature(pk, data, sig);
}
// ===== processBlock 实现 =====
inline bool Blockchain::processBlock(const Block& block) {
    // 1. 检查区块是否已存在
    for (auto& f : forks) {
        for (auto& b : f.second) if (b.hash == block.hash) return true;
    }
    for (auto& b : chain) if (b.hash == block.hash) return true;

    // 2. 找到父链
    std::vector<Block>* parentChain = nullptr;
    int parentIndex = -1;

    for (int i = 0; i < (int)chain.size(); ++i) {
        if (chain[i].hash == block.prevHash) {
            parentChain = &chain;
            parentIndex = i;
            break;
        }
    }
    if (!parentChain) {
        for (auto& f : forks) {
            auto& fc = f.second;
            for (int i = 0; i < (int)fc.size(); ++i) {
                if (fc[i].hash == block.prevHash) {
                    parentChain = &fc;
                    parentIndex = i;
                    break;
                }
            }
            if (parentChain) break;
        }
    }
    if (!parentChain) return false; // 孤立区块，暂存（可扩展）

    // 3. 构建分叉链
    std::vector<Block> newFork(parentChain->begin(), parentChain->begin() + parentIndex + 1);
    newFork.push_back(block);
    forks[block.hash] = newFork;

    // 4. 如果分叉比主链长，切换
    if (newFork.size() > chain.size()) {
        chain = newFork;
        mainChainTip = block.hash;
    }
    return true;
}

// ===== switchToChain 实现 =====
inline bool Blockchain::switchToChain(const std::string& tipHash) {
    auto it = forks.find(tipHash);
    if (it == forks.end()) return false;
    if (it->second.size() <= chain.size()) return false;
    chain = it->second;
    mainChainTip = tipHash;
    return true;
}
#endif