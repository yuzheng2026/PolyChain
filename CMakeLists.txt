cmake_minimum_required(VERSION 3.21)
project(BlockchainSimulator VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用 Qt 的 MOC、UIC、RCC 自动处理
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
find_package(OpenSSL REQUIRED)

# 添加所有源文件（根据你的实际文件名调整）
add_executable(BlockchainSimulator
    main.cpp
    mainwindow.cpp
    p2pnetwork.cpp
    blockchainview.cpp
    wallet.cpp
    cpu_dispatch.cpp
    # 如果有资源文件，添加如下：
    # resources.qrc
)

# 添加包含目录（如果需要）
target_include_directories(BlockchainSimulator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接 Qt 和 OpenSSL
target_link_libraries(BlockchainSimulator PRIVATE
    Qt6::Widgets
    Qt6::Network
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Windows 下设置控制台子系统（如果 GUI 应用不需要控制台，可改为 WIN32）
if(WIN32)
    set_target_properties(BlockchainSimulator PROPERTIES
        WIN32_EXECUTABLE FALSE   # 若希望双击无控制台窗口，可改为 TRUE
    )
endif()
