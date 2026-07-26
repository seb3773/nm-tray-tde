#include "nmtray.h"
#include "icons.h"

#include <tdeapplication.h>
#include <tdelocale.h>

#include <tqevent.h>
#include <tqpixmap.h>
#include <tqtooltip.h>

NmTray::NmTray()
    : KSystemTray(0, "nm-tray-tde")
    , m_iconName(NmIcons::notConnectedIcon())
    , m_connectFramesLoaded(false)
    , m_connectAnimActive(false)
    , m_connectAnimStage(0)
{
    setScaledContents(true);
    setCaption(i18n("Network Manager"));
    m_currentPixmap = NmIcons::trayPixmap(m_iconName);
    applyTrayPixmap(m_currentPixmap);
    m_toolTip = i18n("Network Manager");
    setStatusToolTip(m_toolTip);
    show();
}

void NmTray::loadConnectFrames()
{
    if (m_connectFramesLoaded)
        return;

    for (int i = 0; i < 7; ++i)
        m_connectFrames[i] = NmIcons::trayPixmap(NmIcons::connectingStageIcon(i));
    m_connectFramesLoaded = true;
}

void NmTray::applyTrayPixmap(const TQPixmap &pix)
{
    if (pix.isNull())
        return;

    m_currentPixmap = pix;
    setPixmap(m_currentPixmap);
    repaint(false);
}

void NmTray::setStatusIcon(const TQString &iconName)
{
    bool wasAnimating = m_connectAnimActive;
    if (m_connectAnimActive)
        m_connectAnimActive = false;

    if (iconName == m_iconName && !wasAnimating)
        return;

    m_iconName = iconName;
    applyTrayPixmap(NmIcons::trayPixmap(iconName));
}

void NmTray::setStatusToolTip(const TQString &tip)
{
    if (tip == m_toolTip)
        return;

    m_toolTip = tip;
    TQToolTip::remove(this);
    if (!m_toolTip.isEmpty())
        TQToolTip::add(this, m_toolTip);
}

void NmTray::startConnectingAnimation()
{
    m_connectFramesLoaded = false;
    loadConnectFrames();

    m_connectAnimActive = true;
    m_connectAnimStage = 0;
    m_iconName = NmIcons::connectingStageIcon(0);
    applyTrayPixmap(m_connectFrames[0]);
}

void NmTray::advanceConnectingFrame(int stage)
{
    if (!m_connectAnimActive)
        return;
    if (stage < 0)
        stage = 0;
    if (stage > 6)
        stage = 6;
    if (stage == m_connectAnimStage)
        return;

    loadConnectFrames();
    m_connectAnimStage = stage;
    m_iconName = NmIcons::connectingStageIcon(stage);
    applyTrayPixmap(m_connectFrames[stage]);
}

void NmTray::stopConnectingAnimation()
{
    m_connectAnimActive = false;
    m_connectAnimStage = 0;
}

void NmTray::mousePressEvent(TQMouseEvent *e)
{
    if (!rect().contains(e->pos())) {
        KSystemTray::mousePressEvent(e);
        return;
    }

    if (e->button() == TQt::LeftButton)
        emit leftClicked(e->globalPos());
    else if (e->button() == TQt::RightButton)
        emit rightClicked(e->globalPos());
    else
        KSystemTray::mousePressEvent(e);
}

#include "nmtray.moc"
