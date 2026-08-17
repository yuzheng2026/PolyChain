# PolyChain – 集成 PolyAVX 的区块链模拟器

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![GitHub stars](https://img.shields.io/github/stars/yuzheng2026/QtWidgetsApplication1)](https://github.com/yuzheng2026/QtWidgetsApplication1/stargazers)
[![GitHub issues](https://img.shields.io/github/issues/yuzheng2026/QtWidgetsApplication1)](https://github.com/yuzheng2026/QtWidgetsApplication1/issues)
[![GitHub forks](https://img.shields.io/github/forks/yuzheng2026/QtWidgetsApplication1)](https://github.com/yuzheng2026/QtWidgetsApplication1/network/members)
[![CI](https://github.com/yuzheng2026/QtWidgetsApplication1/actions/workflows/ci.yml/badge.svg)](https://github.com/yuzheng2026/QtWidgetsApplication1/actions/workflows/ci.yml)

**English Version**: [README.md](README.md)

一个基于 **Qt 6** 和 **C++17** 的功能丰富的区块链模拟器，展示了区块链的核心概念，并集成了 **PolyAVX** 高性能多项式库，用于实现“有用工作量证明（PoUW）”任务。本项目使用 [PolyAVX](https://github.com/yuzheng2026/PolyAVX) 作为高性能多项式计算引擎。

本项目面向教学目的，也可作为探索高级共识机制、数字资产和点对点网络的起点。

## 功能特性

- **钱包系统**
  - 使用 OpenSSL EVP 生成 secp256k1 密钥对（不使用已弃用 API）。
  - 使用双重 SHA‑256 推导地址。
  - 使用 ECDSA 进行交易签名和验证。

- **共识机制**
  - **工作量证明（PoW）：** 基于 SHA‑256 的经典哈希挖矿，难度可调。
  - **权益证明（PoS）：** 质押代币以获取区块奖励；包含模拟双重签名时的**罚没**机制。
  - **有用工作量证明（ZKP 任务）：** 使用 **PolyAVX** 求解科学计算任务（如多项式级数展开）。

- **数字资产（NFT）**
  - 铸造带元数据的唯一 NFT。
  - 在钱包地址之间转移 NFT。

- **点对点网络**
  - 通过 TCP 进行多节点连接。
  - 区块和交易广播。
  - 完整链同步。
  - 分叉检测和最长链切换。

- **经济模型**
  - 交易手续费，80% 销毁（类似 EIP‑1559）。
  - 挖矿奖励和质押奖励。
  - 总供应量追踪。

- **可视化**
  - 图形化区块卡片，显示完整哈希详情。
  - 实时余额和质押显示。
  - 难度滑块和状态指示器。

## 架构

项目结构如下：
```text
QtWidgetsApplication1/
├── blockchain.h          # 核心区块链数据结构和逻辑
├── wallet.h / .cpp       # 使用 OpenSSL EVP 的钱包实现
├── p2pnetwork.h / .cpp   # TCP 网络、消息处理和链同步
├── blockchainview.h / .cpp # 用于绘制区块链的自定义控件
├── mainwindow.h / .cpp   # 主 GUI 和所有用户交互
├── main.cpp              # 应用程序入口
└── poly_avx.hpp          # PolyAVX 库（用于 ZKP 任务）
```

## 依赖

- **Qt 6**（Widgets、Network）– 已在 Qt 6.11.1（msvc2022_64）上测试
- **OpenSSL 3.x**（EVP API）
- **PolyAVX** – 你的高性能多项式库
- **Visual Studio 2022/2026**，使用 MSVC v143/v145 工具集（二进制兼容）

## 构建说明

### 1. 准备环境
- 安装 Qt 6（MSVC 2022 64 位）和 Qt Network 模块。
- 使用 vcpkg 安装 OpenSSL：
  ```powershell
  vcpkg install openssl --triplet x64-windows
  vcpkg integrate install
  ```
- 确保 `poly_avx.hpp` 和 `cpu_dispatch.cpp` 在项目目录中。

### 2. 配置项目
- 在 Visual Studio 中打开解决方案。
- 将活动配置设置为 **Release x64**。
- 在项目属性中：
  - **C/C++ → 常规 → 附加包含目录** 添加：
    - `C:\Qt\6.11.1\msvc2022_64\include`
    - `C:\Qt\6.11.1\msvc2022_64\include\QtWidgets`
    - `C:\Qt\6.11.1\msvc2022_64\include\QtNetwork`
    - `D:\vcpkg\installed\x64-windows\include`
  - **链接器 → 常规 → 附加库目录** 添加：
    - `C:\Qt\6.11.1\msvc2022_64\lib`
    - `D:\vcpkg\installed\x64-windows\lib`
  - **链接器 → 输入 → 附加依赖项** 添加：
    - `Qt6Widgets.lib`、`Qt6Core.lib`、`Qt6Gui.lib`、`Qt6Network.lib`
    - `libssl.lib`、`libcrypto.lib`
  - **C/C++ → 代码生成 → 运行库** 设置为 **多线程 DLL (/MD)**（Release）。
  - **C/C++ → 语言 → C++ 语言标准** 设置为 **ISO C++17**。

### 3. 编译并运行
- 编译解决方案。
- 运行单个节点：
  ```powershell
  .\x64\Release\QtWidgetsApplication1.exe 12345
  ```
  （参数指定监听端口，默认为 12345。）

## 运行多个节点（P2P 测试）

你可以在同一台机器上运行两个实例来模拟点对点网络。

1. 启动第一个节点：
   ```powershell
   .\QtWidgetsApplication1.exe 12345
   ```

2. 在新的 PowerShell 窗口中启动第二个节点：
   ```powershell
   .\QtWidgetsApplication1.exe 12346
   ```

3. 在第二个节点中，点击 **CONNECT TO PEER** 并输入：
   - Host：`127.0.0.1`
   - Port：`12345`

节点将同步它们的链，任何一个节点挖出的新区块都会广播给另一个节点。

## 使用功能

- **发送交易：** 输入 `ALICE`、`BOB`、`EVE` 或 `YOU` 作为发送方/接收方和金额。
- **挖矿：** 点击 `MINE NEW BLOCK` 解决 SHA‑256 PoW 难题。
- **质押：** 输入金额并点击 `STAKE`；YOU 将在每个新区块获得奖励。
- **取消质押：** 点击 `UNSTAKE ALL` 取回质押。
- **铸造 NFT：** 点击 `MINT NFT`，提供 ID 和数据。
- **转移 NFT：** 点击 `TRANSFER NFT`，指定 NFT ID 和接收方。
- **模拟双重签名：** 点击 `SIMULATE DOUBLE SIGN` 罚没 YOU 50% 的质押。
- **发布 ZKP 任务：** 点击 `PUBLISH ZKP TASK` 生成科学计算任务（使用 PolyAVX 进行 sin/cos/exp/log 级数展开）并挖出一个包含该任务的区块。

## 已知警告

- 由于第三方依赖混合了不同的 CRT 库，可能会出现 `LNK4098`。它不影响功能。
- Qt 自身的弃用警告（例如 `compressEvent`）可以通过在预处理器定义中定义 `QT_NO_DEPRECATED_WARNINGS` 来抑制。

## 许可证

本项目根据 **GNU General Public License v3.0 (GPLv3)** 授权。  
详情见 `LICENSE` 文件。

## 致谢

- **DeepSeek AI** 在代码生成、调试和设计方面提供了帮助。
- **PolyAVX** 项目提供了用于 ZKP 任务的高性能多项式引擎。
- **Qt** 和 **OpenSSL** 社区提供了基础库。

---

*© 2026 yuzheng2026. 用 ❤️ 和无数个调试之夜构建。*
