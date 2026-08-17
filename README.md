# PolyChain – Blockchain Simulator with PolyAVX Integration

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![GitHub stars](https://img.shields.io/github/stars/yuzheng2026/QtWidgetsApplication1)](https://github.com/yuzheng2026/QtWidgetsApplication1/stargazers)
[![GitHub issues](https://img.shields.io/github/issues/yuzheng2026/QtWidgetsApplication1)](https://github.com/yuzheng2026/QtWidgetsApplication1/issues)
[![GitHub forks](https://img.shields.io/github/forks/yuzheng2026/QtWidgetsApplication1)](https://github.com/yuzheng2026/QtWidgetsApplication1/network/members)
[![CI](https://github.com/yuzheng2026/QtWidgetsApplication1/actions/workflows/ci.yml/badge.svg)](https://github.com/yuzheng2026/QtWidgetsApplication1/actions/workflows/ci.yml)

**中文版**：[README_CN.md](README_CN.md)

A feature‑rich blockchain simulator built with **Qt 6** and **C++17** that demonstrates core blockchain concepts while integrating the **PolyAVX** high‑performance polynomial library for Proof‑of‑Useful‑Work (PoUW) tasks.This project is powered by [PolyAVX](https://github.com/yuzheng2026/PolyAVX), a high‑performance polynomial library.

This project is designed for educational purposes and as a foundation for exploring advanced consensus mechanisms, digital assets, and peer‑to‑peer networking.

## Features

- **Wallet System**  
  - secp256k1 key pair generation using OpenSSL EVP (no deprecated APIs).
  - Address derivation using double SHA‑256.
  - Transaction signing and verification with ECDSA.

- **Consensus Mechanisms**
  - **Proof‑of‑Work (PoW):** Classic SHA‑256 hash mining with adjustable difficulty.
  - **Proof‑of‑Stake (PoS):** Stake coins to earn block rewards; includes **slashing** for double‑signing simulation.
  - **Proof‑of‑Useful‑Work (ZKP Tasks):** Solve scientific computing tasks (e.g., polynomial series expansion) using **PolyAVX**.

- **Digital Assets (NFT)**
  - Mint unique NFTs with metadata.
  - Transfer NFTs between wallet addresses.

- **Peer‑to‑Peer Network**
  - Multi‑node connection over TCP.
  - Block and transaction broadcasting.
  - Full chain synchronization.
  - Fork detection and longest‑chain switching.

- **Economics**
  - Transaction fees with 80% burn (EIP‑1559 style).
  - Mining rewards and staking rewards.
  - Total supply tracking.

- **Visualization**
  - Graphical block cards with complete hash details.
  - Real‑time balance and stake displays.
  - Difficulty slider and status indicators.

## Architecture

The project is structured as follows:

```text
PolyChain/
├── blockchain.h          # Core blockchain data structures and logic
├── wallet.h / .cpp       # Wallet implementation using OpenSSL EVP
├── p2pnetwork.h / .cpp   # TCP networking, message handling, and chain sync
├── blockchainview.h / .cpp # Custom widget for drawing the blockchain
├── mainwindow.h / .cpp   # Main GUI and all user interactions
├── main.cpp              # Application entry point
└── poly_avx.hpp          # PolyAVX library (imported for ZKP tasks)
```
## Dependencies

- **Qt 6** (Widgets, Network) – tested with Qt 6.11.1 (msvc2022_64)
- **OpenSSL 3.x** (EVP API)
- **PolyAVX** – your high‑performance polynomial library
- **Visual Studio 2022/2026** with MSVC v143/v145 toolset (binary compatible)

## Build Instructions

### 1. Prepare the environment
- Install Qt 6 with MSVC 2022 64‑bit and Qt Network module.
- Install OpenSSL via vcpkg:
  ```powershell
  vcpkg install openssl --triplet x64-windows
  vcpkg integrate install
  ```
- Ensure `poly_avx.hpp` and `cpu_dispatch.cpp` are in the project directory.

### 2. Configure the project
- Open the solution in Visual Studio.
- Set active configuration to **Release x64**.
- In Project Properties:
  - **C/C++ → General → Additional Include Directories** add:
    - `C:\Qt\6.11.1\msvc2022_64\include`
    - `C:\Qt\6.11.1\msvc2022_64\include\QtWidgets`
    - `C:\Qt\6.11.1\msvc2022_64\include\QtNetwork`
    - `D:\vcpkg\installed\x64-windows\include`
  - **Linker → General → Additional Library Directories** add:
    - `C:\Qt\6.11.1\msvc2022_64\lib`
    - `D:\vcpkg\installed\x64-windows\lib`
  - **Linker → Input → Additional Dependencies** add:
    - `Qt6Widgets.lib`, `Qt6Core.lib`, `Qt6Gui.lib`, `Qt6Network.lib`
    - `libssl.lib`, `libcrypto.lib`
  - **C/C++ → Code Generation → Runtime Library** set to **Multi‑threaded DLL (/MD)** for Release.
  - **C/C++ → Language → C++ Language Standard** set to **ISO C++17**.

### 3. Build and run
- Build the solution.
- To run a single node:
  ```powershell
  .\x64\Release\QtWidgetsApplication1.exe 12345
  ```
  (The argument specifies the listening port; default is 12345.)

## Running Multiple Nodes (P2P Testing)

You can run two instances on the same machine to simulate a peer‑to‑peer network.

1. Start the first node:
   ```powershell
   .\QtWidgetsApplication1.exe 12345
   ```

2. Start the second node in a new PowerShell window:
   ```powershell
   .\QtWidgetsApplication1.exe 12346
   ```

3. In the second node, click **CONNECT TO PEER** and enter:
   - Host: `127.0.0.1`
   - Port: `12345`

The nodes will synchronize their chains, and any new block mined on one node will be broadcast to the other.

## Using the Features

- **Send Transaction:** Enter `ALICE`, `BOB`, `EVE`, or `YOU` as sender/receiver and an amount.
- **Mine:** Click `MINE NEW BLOCK` to solve a SHA‑256 PoW puzzle.
- **Stake:** Enter an amount and click `STAKE`; YOU earn rewards on every new block.
- **Unstake:** Click `UNSTAKE ALL` to withdraw your stake.
- **Mint NFT:** Click `MINT NFT`, provide an ID and data.
- **Transfer NFT:** Click `TRANSFER NFT`, specify the NFT ID and recipient.
- **Simulate Double Sign:** Click `SIMULATE DOUBLE SIGN` to slash 50% of YOUR stake.
- **Publish ZKP Task:** Click `PUBLISH ZKP TASK` to generate a scientific computing task (sin/cos/exp/log series expansion using PolyAVX) and mine a block with it.

## Known Warnings

- `LNK4098` may appear due to mixed CRT libraries from third‑party dependencies. It does not affect functionality.
- Qt’s own deprecation warnings (e.g., `compressEvent`) can be suppressed by defining `QT_NO_DEPRECATED_WARNINGS` in preprocessor definitions.

## License

This project is licensed under the **GNU General Public License v3.0 (GPLv3)**.  
See the `LICENSE` file for details.

## Acknowledgements

- **DeepSeek AI** for assistance in code generation, debugging, and design.
- **PolyAVX** project for providing the high‑performance polynomial engine used in ZKP tasks.
- **Qt** and **OpenSSL** communities for the foundational libraries.

---

*© 2026 yuzheng2026. Built with ❤️ and an unreasonable amount of debugging.*
