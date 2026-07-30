#include "blockchainview.h"
#include <QPainter>
#include <QFont>
#include <QPen>
#include <QScrollBar>

BlockchainView::BlockchainView(Blockchain* bc, QWidget* parent)
    : QWidget(parent), m_bc(bc) {
    setMinimumSize(BLOCK_WIDTH + 40, 200);
}

QSize BlockchainView::sizeHint() const {
    int h = static_cast<int>((BLOCK_HEIGHT + VERTICAL_SPACING) * m_bc->chain.size() + 80);
    return QSize(BLOCK_WIDTH + 40, h);
}

void BlockchainView::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(20, 20, 20));

    int y = 20;
    QFont titleFont("Arial", 12, QFont::Bold);
    QFont textFont("Arial", 9);
    QFont hashFont("Courier New", 6);

    for (size_t i = 0; i < m_bc->chain.size(); ++i) {
        const Block& block = m_bc->chain[i];

        if (i > 0) {
            int centerX = BLOCK_WIDTH / 2 + 10;
            int prevBottomY = y - VERTICAL_SPACING / 2;
            int currTopY = y - 10;
            painter.setPen(QPen(QColor(88, 166, 255), 2));
            painter.drawLine(centerX, prevBottomY, centerX, currTopY);
            painter.drawLine(centerX - 5, currTopY - 5, centerX, currTopY);
            painter.drawLine(centerX + 5, currTopY - 5, centerX, currTopY);
        }

        QRect card(10, y, BLOCK_WIDTH, BLOCK_HEIGHT);
        painter.setBrush(QColor(30, 30, 40));
        painter.setPen(QPen(QColor(88, 166, 255), 2));
        painter.drawRoundedRect(card, 10, 10);

        int textX = card.left() + 15;
        int textY = card.top() + 25;
        int lineHeight = 18;

        painter.setPen(QColor(88, 166, 255));
        painter.setFont(titleFont);
        painter.drawText(textX, textY, QString("Block #%1").arg(block.index));
        textY += lineHeight + 5;

        painter.setPen(QColor(200, 200, 200));
        painter.setFont(textFont);
        painter.drawText(textX, textY, QString("Time: %1").arg(QString::fromStdString(block.timestamp)));
        textY += lineHeight;

        painter.setPen(QColor(150, 150, 150));
        painter.setFont(hashFont);
        painter.drawText(textX, textY, QString("Prev:   %1").arg(QString::fromStdString(block.prevHash)));
        textY += lineHeight - 4;
        painter.drawText(textX, textY, QString("Hash:   %1").arg(QString::fromStdString(block.hash)));
        textY += lineHeight - 4;
        painter.drawText(textX, textY, QString("Merkle: %1").arg(QString::fromStdString(block.merkleRoot)));
        textY += lineHeight - 4;

        painter.setPen(QColor(180, 180, 180));
        painter.setFont(textFont);
        painter.drawText(textX, textY, QString("Nonce: %1  |  Tx count: %2")
            .arg(block.nonce)
            .arg(block.transactions.size()));
        textY += lineHeight + 4;

        for (const auto& tx : block.transactions) {
            painter.drawText(textX, textY, QString::fromStdString(tx.toString()));
            textY += lineHeight;
        }

        y += BLOCK_HEIGHT + VERTICAL_SPACING;
    }
}