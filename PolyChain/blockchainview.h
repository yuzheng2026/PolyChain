#ifndef BLOCKCHAINVIEW_H
#define BLOCKCHAINVIEW_H

#include <QWidget>
#include "blockchain.h"

class BlockchainView : public QWidget {
    Q_OBJECT
public:
    explicit BlockchainView(Blockchain* bc, QWidget* parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    Blockchain* m_bc;
    static const int BLOCK_WIDTH = 720;
    static const int BLOCK_HEIGHT = 220;
    static const int VERTICAL_SPACING = 60;
};

#endif