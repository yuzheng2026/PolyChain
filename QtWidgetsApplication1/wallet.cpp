#include "wallet.h"

#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <openssl/param_build.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <openssl/sha.h>
#include <openssl/err.h>
#include <openssl/provider.h>

#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

// 辅助函数：EVP 计算消息摘要
static std::vector<unsigned char> evpDigest(const char* algorithm,
    const void* data, size_t dataLen) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_MD_CTX_new failed");

    const EVP_MD* md = EVP_get_digestbyname(algorithm);
    if (!md) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error(std::string("Unknown digest: ") + algorithm);
    }

    if (EVP_DigestInit_ex(ctx, md, nullptr) != 1 ||
        EVP_DigestUpdate(ctx, data, dataLen) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("Digest operation failed");
    }

    unsigned int len = 0;
    std::vector<unsigned char> hash(EVP_MAX_MD_SIZE);
    if (EVP_DigestFinal_ex(ctx, hash.data(), &len) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("Digest finalization failed");
    }

    hash.resize(len);
    EVP_MD_CTX_free(ctx);
    return hash;
}

Wallet::Wallet() {
    // 1. 生成密钥对
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    if (!ctx) throw std::runtime_error("EVP_PKEY_CTX_new_id failed");

    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("EVP_PKEY_keygen_init failed");
    }

    if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx, NID_secp256k1) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("set ec curve failed");
    }

    EVP_PKEY* key = nullptr;
    if (EVP_PKEY_generate(ctx, &key) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        throw std::runtime_error("EVP_PKEY_generate failed");
    }
    pkey = static_cast<void*>(key);
    EVP_PKEY_CTX_free(ctx);

    // 2. 提取原始公钥（64字节，未压缩）
    std::vector<unsigned char> rawPub(64);
    size_t len = 64;
    bool gotRaw = false;

    // 方法 A：尝试 EVP_PKEY_get_raw_public_key
    if (EVP_PKEY_get_raw_public_key(key, nullptr, &len) == 1 && len == 64) {
        if (EVP_PKEY_get_raw_public_key(key, rawPub.data(), &len) == 1 && len == 64) {
            gotRaw = true;
        }
    }

    // 方法 B：回退到手动提取点坐标
    if (!gotRaw) {
        BIGNUM* x = BN_new();
        BIGNUM* y = BN_new();
        if (EVP_PKEY_get_bn_param(key, OSSL_PKEY_PARAM_EC_PUB_X, &x) == 1 &&
            EVP_PKEY_get_bn_param(key, OSSL_PKEY_PARAM_EC_PUB_Y, &y) == 1) {
            char* xHex = BN_bn2hex(x);
            char* yHex = BN_bn2hex(y);
            std::string xStr(xHex), yStr(yHex);
            OPENSSL_free(xHex);
            OPENSSL_free(yHex);

            while (xStr.length() < 64) xStr = "0" + xStr;
            while (yStr.length() < 64) yStr = "0" + yStr;

            for (size_t i = 0; i < 32; ++i) {
                rawPub[i] = static_cast<unsigned char>(std::strtoul(xStr.substr(i * 2, 2).c_str(), nullptr, 16));
                rawPub[32 + i] = static_cast<unsigned char>(std::strtoul(yStr.substr(i * 2, 2).c_str(), nullptr, 16));
            }
            gotRaw = true;
        }
        BN_free(x);
        BN_free(y);
    }

    if (!gotRaw) throw std::runtime_error("Failed to extract public key");

    // 3. 生成未压缩十六进制公钥
    std::stringstream pubStream;
    pubStream << "04";
    for (unsigned char c : rawPub) {
        pubStream << std::hex << std::setfill('0') << std::setw(2) << (int)c;
    }
    pubKeyHex = pubStream.str();

    // 4. 双重 SHA256 地址
    std::vector<unsigned char> hash1 = evpDigest("SHA256", pubKeyHex.data(), pubKeyHex.size());
    std::vector<unsigned char> hash2 = evpDigest("SHA256", hash1.data(), hash1.size());

    std::stringstream addr;
    addr << "1";
    for (size_t i = 0; i < 20 && i < hash2.size(); ++i)
        addr << std::hex << std::setfill('0') << std::setw(2) << (int)hash2[i];
    address = addr.str();
}

Wallet::~Wallet() {
    if (pkey) {
        EVP_PKEY_free(static_cast<EVP_PKEY*>(pkey));
        pkey = nullptr;
    }
}

std::string Wallet::getPublicKey() const { return pubKeyHex; }
std::string Wallet::getAddress() const { return address; }

std::string Wallet::signData(const std::string& data) const {
    std::vector<unsigned char> hash = evpDigest("SHA256", data.data(), data.size());

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(static_cast<EVP_PKEY*>(pkey), nullptr);
    if (!ctx) return "";

    if (EVP_PKEY_sign_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return "";
    }

    size_t sigLen = 0;
    if (EVP_PKEY_sign(ctx, nullptr, &sigLen, hash.data(), hash.size()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return "";
    }

    std::vector<unsigned char> sig(sigLen);
    if (EVP_PKEY_sign(ctx, sig.data(), &sigLen, hash.data(), hash.size()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        return "";
    }
    EVP_PKEY_CTX_free(ctx);
    return std::string(reinterpret_cast<char*>(sig.data()), sigLen);
}

bool Wallet::verifySignature(const std::string& publicKeyHex,
    const std::string& data,
    const std::string& signature) {
    if (publicKeyHex.size() < 130 || publicKeyHex.substr(0, 2) != "04")
        return false;

    std::string rawHex = publicKeyHex.substr(2);
    std::vector<unsigned char> rawPub;
    for (size_t i = 0; i < rawHex.size(); i += 2)
        rawPub.push_back(static_cast<unsigned char>(std::strtoul(rawHex.substr(i, 2).c_str(), nullptr, 16)));

    EVP_PKEY* key = EVP_PKEY_new_raw_public_key(EVP_PKEY_EC, nullptr, rawPub.data(), rawPub.size());
    if (!key) return false;

    std::vector<unsigned char> hash = evpDigest("SHA256", data.data(), data.size());

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(key, nullptr);
    if (!ctx) {
        EVP_PKEY_free(key);
        return false;
    }

    int ret = 0;
    if (EVP_PKEY_verify_init(ctx) > 0) {
        ret = EVP_PKEY_verify(ctx,
            reinterpret_cast<const unsigned char*>(signature.c_str()),
            signature.size(),
            hash.data(), hash.size());
    }
    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(key);
    return ret == 1;
}