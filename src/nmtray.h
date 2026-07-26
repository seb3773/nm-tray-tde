#ifndef NM_TRAY_H
#define NM_TRAY_H

#include <ksystemtray.h>
#include <tqpixmap.h>

class TQMouseEvent;

class NmTray : public KSystemTray
{
    TQ_OBJECT

public:
    NmTray();
    void setStatusIcon(const TQString &iconName);
    void setStatusToolTip(const TQString &tip);
    void startConnectingAnimation();
    void advanceConnectingFrame(int stage);
    void stopConnectingAnimation();

signals:
    void leftClicked(const TQPoint &globalPos);
    void rightClicked(const TQPoint &globalPos);

protected:
    virtual void mousePressEvent(TQMouseEvent *e);

private:
    void loadConnectFrames();
    void applyTrayPixmap(const TQPixmap &pix);

    TQString m_iconName;
    TQString m_toolTip;
    TQPixmap m_currentPixmap;
    TQPixmap m_connectFrames[7];
    bool m_connectFramesLoaded;
    bool m_connectAnimActive;
    int m_connectAnimStage;
};

#endif
