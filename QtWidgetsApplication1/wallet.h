#ifndef WALLET_H
#define WALLET_H

#include <string>

class Wallet {
public:
    Wallet();
    ~Wallet();
    std::string getPublicKey() const;
    std::string getAddress() const;
    std::string signData(const std::string& data) const;
    static bool verifySignature(const std::string& publicKey,
        const std::string& data,
        const std::string& signature);

private:
    void* pkey;        // EVP_PKEY*
    std::string pubKeyHex;
    std::string address;
};

#endif