#include "traypopup.h"
#include "traycontroller.h"
#include "connectioninfobuilder.h"
#include "icons.h"
#include "nm/glib_compat.h"
#include "nm/nmeventpump.h"

#include <tdelocale.h>
#include <kiconloader.h>
#include <tdeglobalsettings.h>

#include <tqpainter.h>
#include <tqimage.h>
#include <tqlayout.h>
#include <tqframe.h>
#include <tqfont.h>
#include <tqcursor.h>
#include <tqapplication.h>
#include <tqdesktopwidget.h>
#include <tqevent.h>
#include <tqtimer.h>
#include <tqobjectlist.h>

namespace {

static const TQColor kPopupHoverColor(0x3D, 0xAE, 0xE9);
static const TQColor kPopupActiveGroupBgColor(0xC5, 0xEB, 0xFC);
static const TQColor kPopupActiveGroupHoverColor(0x9D, 0xD4, 0xF5);
static const TQColor kPopupBgColor(0xF5, 0xF5, 0xF5);
static const TQColor kPopupTitleBgColor(0xE8, 0xE8, 0xE8);
static const TQColor kPopupSeparatorColor(0xC0, 0xC0, 0xC0);
static const int kQuickPopupWidth = 280;
static const int kMainPopupWidth = kQuickPopupWidth;
static const int kPopupWidth = kQuickPopupWidth;
static const int kPopupInnerWidth = kQuickPopupWidth - 10;
static const int kPopupRowMargin = 4;
static const int kPopupRowSpacing = 6;
static const int kPopupMarkSize = TDEIcon::SizeSmall;
static const int kPopupIndent = 18; // fixed label text inset from left margin
static const int kPopupClosePollMs = 500;
static const int kPopupCloseOutsideTicks = 6;

static const TQColor kPopupSignalColor(100, 100, 100);
static const TQColor kPopupSignalDisabledColor(150, 150, 150);

enum WifiRowStyle {
    WifiRowNone = 0,
    WifiRowActive,
    WifiRowAvailable
};

static TQFont popupMenuFont(bool bold = false)
{
    TQFont font = TDEGlobalSettings::menuFont();
    if (bold)
        font.setBold(true);
    return font;
}

static TQFont popupMenuSmallFont(bool bold = false)
{
    TQFont font = popupMenuFont(bold);
    if (font.pointSize() > 0)
        font.setPointSize(font.pointSize() - 1);
    else if (font.pixelSize() > 0)
        font.setPixelSize(TQMAX(1, font.pixelSize() - 1));
    return font;
}

static int popupRowHeight()
{
    const TQFontMetrics fm(popupMenuFont());
    int h = fm.height() + 2 * kPopupRowMargin + 6;
    const int minForIcon = TDEIcon::SizeSmall + 2 * kPopupRowMargin;
    if (h < minForIcon)
        h = minForIcon;
    return h;
}

static TQPixmap greyedIcon(const TQPixmap &icon)
{
    if (icon.isNull())
        return icon;

    TQImage img = icon.convertToImage();
    if (img.isNull())
        return icon;
    if (img.depth() != 32)
        img = img.convertDepth(32);

    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            TQRgb px = img.pixel(x, y);
            const int alpha = tqAlpha(px);
            if (alpha == 0)
                continue;
            const int grey = (tqRed(px) + tqGreen(px) + tqBlue(px)) / 3;
            const int blend = (grey + 160) / 2;
            img.setPixel(x, y, tqRgba(blend, blend, blend, alpha * 55 / 100));
        }
    }
    return TQPixmap(img);
}

static void paintCheckMark(TQPainter &p, const TQRect &rect, bool enabled)
{
    TQPixmap checkPm = NmIcons::checkMarkPixmap();
    if (checkPm.isNull())
        return;

    const int x = rect.x() + rect.width() - kPopupRowMargin - kPopupMarkSize;
    const int y = rect.y() + (rect.height() - kPopupMarkSize) / 2;
    if (enabled)
        p.drawPixmap(x, y, checkPm);
    else
        p.drawPixmap(x, y, greyedIcon(checkPm));
}

static void paintTruncatedText(TQPainter &p, const TQFontMetrics &fm, int x, int y,
                              int maxWidth, const TQString &text)
{
    if (maxWidth <= 0)
        return;

    TQString drawText = text;
    if (fm.width(drawText) > maxWidth) {
        TQString ellipses("...");
        while (drawText.length() > 1
               && fm.width(drawText + ellipses) > maxWidth)
            drawText = drawText.left(drawText.length() - 1);
        drawText += ellipses;
    }
    p.drawText(x, y, drawText);
}

static void paintRow(TQPainter &p, const TQRect &rect, const TQPixmap &icon,
                     const TQString &text, const TQColor &normalBg, bool hovered,
                     bool enabled, int leftPad, int rightReserve, bool bold = false,
                     bool checked = false, int signalStrength = -1,
                     WifiRowStyle wifiStyle = WifiRowNone,
                     bool alignTextRight = false,
                     bool smallText = false,
                     const TQColor &hoverColor = kPopupHoverColor)
{
    const TQColor fill = (hovered && enabled) ? hoverColor : normalBg;
    p.fillRect(rect, fill);

    int textX = rect.x() + kPopupRowMargin + leftPad;
    if (!icon.isNull()) {
        const TQPixmap &drawIcon = enabled ? icon : greyedIcon(icon);
        const int iy = rect.y() + (rect.height() - drawIcon.height()) / 2;
        p.drawPixmap(textX, iy, drawIcon);
        textX += drawIcon.width() + kPopupRowSpacing;
    } else if (!alignTextRight && leftPad == 0) {
        textX += kPopupIndent;
    }

    const TQColor textColor = enabled ? TQColor(0, 0, 0) : TQColor(120, 120, 120);
    p.setPen(textColor);
    p.setFont(smallText ? popupMenuSmallFont(bold && enabled)
                        : popupMenuFont(bold && enabled));

    const TQFontMetrics fm = p.fontMetrics();
    const int textY = rect.y() + (rect.height() + fm.ascent() - fm.descent()) / 2;
    if (wifiStyle != WifiRowNone)
        rightReserve = kPopupRowMargin + kPopupMarkSize + kPopupRowSpacing;

    const int textW = rect.x() + rect.width() - textX - rightReserve;
    if (textW > 0) {
        if (wifiStyle != WifiRowNone) {
            const TQFontMetrics signalFm(popupMenuFont(false));
            const int signalColumnWidth = signalFm.width(TQString("100%"));
            const int nameMaxWidth = textW - signalColumnWidth - kPopupRowSpacing;
            const int signalX = textX + textW - signalColumnWidth;

            paintTruncatedText(p, fm, textX, textY, nameMaxWidth, text);

            if (signalStrength >= 0) {
                p.setPen(enabled ? kPopupSignalColor : kPopupSignalDisabledColor);
                p.setFont(popupMenuFont(false));
                const TQString signalText = TQString("%1%").arg(signalStrength);
                const int signalTextWidth = signalFm.width(signalText);
                const int signalDrawX = signalX + signalColumnWidth - signalTextWidth;
                p.drawText(signalDrawX, textY, signalText);
                p.setPen(textColor);
                if (bold && enabled)
                    p.setFont(popupMenuFont(true));
            }
        } else if (alignTextRight) {
            const int textWidth = fm.width(text);
            int drawX = textX + textW - textWidth;
            if (drawX < textX)
                drawX = textX;
            p.drawText(drawX, textY, text);
        } else {
            p.drawText(textX, textY, text);
        }
    }

    if (checked)
        paintCheckMark(p, rect, enabled);
}

class PopupRowWidget : public TQWidget
{
public:
    PopupRowWidget(NmTrayPopup *popup, bool isSavedNetworksRow, TQWidget *parent)
        : TQWidget(parent)
        , m_hovered(false)
        , m_popup(popup)
        , m_isSavedNetworksRow(isSavedNetworksRow)
    {
        setFixedHeight(popupRowHeight());
        setBackgroundMode(TQt::NoBackground);
    }

    void clearHover()
    {
        setRowHovered(false);
    }

    void setRowHovered(bool hovered)
    {
        if (m_hovered != hovered) {
            m_hovered = hovered;
            update();
        }
    }

    bool rowHovered() const { return m_hovered; }

protected:
    void enterEvent(TQEvent *)
    {
        if (!isEnabled())
            return;
        setRowHovered(true);
        update();
        if (m_popup)
            m_popup->onMainRowEntered(m_isSavedNetworksRow);
    }

    void leaveEvent(TQEvent *)
    {
        TQPoint localPos = mapFromGlobal(TQCursor::pos());
        if (rect().contains(localPos))
            return;
        setRowHovered(false);
        update();
    }

    void mousePressEvent(TQMouseEvent *e)
    {
        if (e->button() == TQt::RightButton)
            e->accept();
    }

    NmTrayPopup *m_popup;
    bool m_isSavedNetworksRow;
    bool m_hovered;
};

class PopupWifiActiveGroup : public TQWidget
{
public:
    PopupWifiActiveGroup(const TQPixmap &icon, const TQString &name, int signalStrength,
                         int disconnectActionId, bool isConnecting, NmTrayPopup *popup, TQWidget *parent,
                         const char *objName = "wifi_active")
        : TQWidget(parent)
        , m_icon(icon)
        , m_name(name)
        , m_signalStrength(signalStrength)
        , m_disconnectActionId(disconnectActionId)
        , m_isConnecting(isConnecting)
        , m_popup(popup)
        , m_hovered(false)
        , m_disconnectHovered(false)
    {
        const int rowH = popupRowHeight();
        setFixedHeight(rowH * 2);
        setBackgroundMode(TQt::NoBackground);
        setMouseTracking(true);
        if (popup)
            setMinimumWidth(popup->popupInnerWidth());
        else
            setMinimumWidth(kPopupInnerWidth);
        setName(objName);
    }

    void setSignalInfo(const TQPixmap &icon, const TQString &name, int signalStrength)
    {
        if (m_name == name && m_signalStrength == signalStrength)
            return;
        m_icon = icon;
        m_name = name;
        m_signalStrength = signalStrength;
        update();
    }

protected:
    void paintEvent(TQPaintEvent *)
    {
        TQPainter p(this);
        const int rowH = popupRowHeight();
        const int rightReserve = kPopupRowMargin + kPopupMarkSize + kPopupRowSpacing;

        paintRow(p, TQRect(0, 0, width(), rowH), m_icon, m_name,
                 kPopupActiveGroupBgColor, m_hovered, isEnabled(), 0, rightReserve,
                 true, true, m_signalStrength, WifiRowActive, false, false,
                 kPopupActiveGroupHoverColor);
                 
        TQString actionText;
        bool hasDisconnect = (m_disconnectActionId > 0);
        bool isClickable = false;
        
        if (m_isConnecting) {
            actionText = i18n("Connecting...");
        } else if (hasDisconnect) {
            actionText = i18n("Disconnect");
            isClickable = true;
        }

        paintRow(p, TQRect(0, rowH, width(), rowH), TQPixmap(),
                 actionText, kPopupActiveGroupBgColor, m_hovered, isEnabled(),
                 0, kPopupRowMargin, m_disconnectHovered, false, -1, WifiRowNone, isClickable,
                 isClickable, kPopupActiveGroupHoverColor);
    }

    void updateHoverState(const TQPoint &pos)
    {
        if (!isEnabled())
            return;

        const bool hovered = rect().contains(pos);
        bool disconnectHovered = false;
        if (!m_isConnecting && m_disconnectActionId > 0 && hovered && disconnectLabelRect().contains(pos))
            disconnectHovered = true;

        if (m_hovered == hovered && m_disconnectHovered == disconnectHovered)
            return;

        m_hovered = hovered;
        m_disconnectHovered = disconnectHovered;
        update();
    }

    void enterEvent(TQEvent *)
    {
        if (!isEnabled())
            return;
        updateHoverState(mapFromGlobal(TQCursor::pos()));
        if (m_popup)
            m_popup->onMainRowEntered(false);
    }

    void leaveEvent(TQEvent *)
    {
        TQPoint localPos = mapFromGlobal(TQCursor::pos());
        if (rect().contains(localPos))
            return;
        m_hovered = false;
        m_disconnectHovered = false;
        update();
    }

    void mouseMoveEvent(TQMouseEvent *e)
    {
        updateHoverState(e->pos());
        TQWidget::mouseMoveEvent(e);
    }

    void mousePressEvent(TQMouseEvent *e)
    {
        if (e->button() != TQt::LeftButton || !isEnabled() || !m_popup || m_isConnecting)
            return;
        if (!disconnectLabelRect().contains(e->pos()))
            return;
        e->accept();
        m_popup->triggerAction(m_disconnectActionId);
    }

private:
    TQRect disconnectLabelRect() const
    {
        const int rowH = popupRowHeight();
        const TQRect rowRect(0, rowH, width(), rowH);
        const TQString text = m_isConnecting ? i18n("Connecting...") : i18n("Disconnect");
        const int textX = rowRect.x() + kPopupRowMargin;
        const int rightReserve = kPopupRowMargin;
        const int textAreaW = rowRect.x() + rowRect.width() - textX - rightReserve;

        if (textAreaW <= 0)
            return TQRect();

        const TQFontMetrics fm(popupMenuSmallFont(m_disconnectHovered));
        const int textWidth = fm.width(text);
        int drawX = textX + textAreaW - textWidth;
        if (drawX < textX)
            drawX = textX;

        const int textY = rowRect.y() + (rowRect.height() + fm.ascent() - fm.descent()) / 2;
        return TQRect(drawX, textY - fm.ascent(), textWidth, fm.height());
    }

    TQPixmap m_icon;
    TQString m_name;
    int m_signalStrength;
    int m_disconnectActionId;
    bool m_isConnecting;
    NmTrayPopup *m_popup;
    bool m_hovered;
    bool m_disconnectHovered;
};

class PopupActionRow : public PopupRowWidget
{
public:
    PopupActionRow(const TQPixmap &icon, const TQString &text, int actionId,
                   int leftPad, NmTrayPopup *popup, TQWidget *parent,
                   bool bold = false, int signalStrength = -1,
                   WifiRowStyle wifiStyle = WifiRowNone,
                   bool activeGroupStyle = false)
        : PopupRowWidget(popup, false, parent)
        , m_icon(icon)
        , m_text(text)
        , m_actionId(actionId)
        , m_leftPad(leftPad)
        , m_popup(popup)
        , m_checked(false)
        , m_bold(bold)
        , m_signalStrength(signalStrength)
        , m_wifiStyle(wifiStyle)
        , m_activeGroupStyle(activeGroupStyle)
    {
        if (popup)
            setMinimumWidth(popup->popupInnerWidth());
        else
            setMinimumWidth(kPopupInnerWidth);
        setName(TQString("act_%1").arg(actionId));
    }

    void setChecked(bool checked)
    {
        if (m_checked != checked) {
            m_checked = checked;
            update();
        }
    }

    void setText(const TQString &text)
    {
        if (m_text != text) {
            m_text = text;
            update();
        }
    }

    void setIcon(const TQPixmap &icon)
    {
        m_icon = icon;
        update();
    }

    void setSignalStrength(int signalStrength)
    {
        if (m_signalStrength == signalStrength)
            return;
        m_signalStrength = signalStrength;
        update();
    }

    int actionId() const { return m_actionId; }
    bool isChecked() const { return m_checked; }

protected:
    void paintEvent(TQPaintEvent *)
    {
        TQPainter p(this);
        const int rightReserve = (m_checked || m_wifiStyle != WifiRowNone)
            ? (kPopupRowMargin + kPopupMarkSize + kPopupRowSpacing) : kPopupRowMargin;
        const TQColor normalBg = m_activeGroupStyle ? kPopupActiveGroupBgColor : kPopupBgColor;
        const TQColor hoverColor = m_activeGroupStyle ? kPopupActiveGroupHoverColor
                                                      : kPopupHoverColor;
        paintRow(p, rect(), m_icon, m_text, normalBg, rowHovered(), isEnabled(),
                 m_leftPad, rightReserve, m_bold, m_checked, m_signalStrength,
                 m_wifiStyle, false, false, hoverColor);
    }

    void mousePressEvent(TQMouseEvent *e)
    {
        if (e->button() == TQt::LeftButton && isEnabled() && m_popup) {
            if (m_popup->isQuickToggleBlocked(m_actionId)) {
                e->accept();
                return;
            }
            e->accept();
            m_popup->triggerAction(m_actionId);
            return;
        }
        PopupRowWidget::mousePressEvent(e);
    }

private:
    TQPixmap m_icon;
    TQString m_text;
    int m_actionId;
    int m_leftPad;
    NmTrayPopup *m_popup;
    bool m_checked;
    bool m_bold;
    int m_signalStrength;
    WifiRowStyle m_wifiStyle;
    bool m_activeGroupStyle;
};

class PopupTitleRow : public PopupRowWidget
{
public:
    PopupTitleRow(const TQPixmap &icon, const TQString &text,
                  const TQPixmap &statusIcon,
                  NmTrayPopup *popup, TQWidget *parent)
        : PopupRowWidget(popup, false, parent)
        , m_icon(icon)
        , m_text(text)
        , m_statusIcon(statusIcon)
    {
        if (popup)
            setMinimumWidth(popup->popupInnerWidth());
        else
            setMinimumWidth(kPopupInnerWidth);
        setEnabled(false);
    }

protected:
    void paintEvent(TQPaintEvent *)
    {
        TQPainter p(this);
        const TQRect r = rect();
        p.fillRect(r, kPopupTitleBgColor);

        int x = r.x() + kPopupRowMargin;
        if (!m_icon.isNull()) {
            const int iy = r.y() + (r.height() - m_icon.height()) / 2;
            p.drawPixmap(x, iy, m_icon);
            x += m_icon.width() + kPopupRowSpacing;
        } else {
            x += kPopupIndent;
        }

        p.setPen(TQColor(0, 0, 0));
        p.setFont(popupMenuFont(true));
        const TQFontMetrics fm = p.fontMetrics();
        const int textY = r.y() + (r.height() + fm.ascent() - fm.descent()) / 2;
        p.drawText(x, textY, m_text);

        if (!m_statusIcon.isNull()) {
            const int sx = x + fm.width(m_text) + kPopupRowSpacing;
            const int sy = r.y() + (r.height() - m_statusIcon.height()) / 2;
            p.drawPixmap(sx, sy, m_statusIcon);
        }
    }

private:
    TQPixmap m_icon;
    TQString m_text;
    TQPixmap m_statusIcon;
};

class PopupStatusRow : public PopupRowWidget
{
public:
    PopupStatusRow(const TQString &text, int leftPad,
                   NmTrayPopup *popup, TQWidget *parent)
        : PopupRowWidget(popup, false, parent)
        , m_text(text)
        , m_leftPad(leftPad)
    {
        if (popup)
            setMinimumWidth(popup->popupInnerWidth());
        else
            setMinimumWidth(kPopupInnerWidth);
        setEnabled(false);
    }

protected:
    void paintEvent(TQPaintEvent *)
    {
        TQPainter p(this);
        paintRow(p, rect(), TQPixmap(), m_text, kPopupBgColor, false, false,
                 m_leftPad, kPopupRowMargin, false, false);
    }

private:
    TQString m_text;
    int m_leftPad;
};

class PopupSubmenuOpenRow : public PopupRowWidget
{
public:
    PopupSubmenuOpenRow(const TQPixmap &icon, const TQString &text,
                        NmTrayPopup *popup, TQWidget *parent)
        : PopupRowWidget(popup, true, parent)
        , m_icon(icon)
        , m_text(text)
        , m_popup(popup)
        , m_submenuAnchorHighlight(false)
    {
        if (popup)
            setMinimumWidth(popup->popupInnerWidth());
        else
            setMinimumWidth(kPopupInnerWidth);
    }

    void setSubmenuAnchorHighlight(bool highlighted)
    {
        if (m_submenuAnchorHighlight != highlighted) {
            m_submenuAnchorHighlight = highlighted;
            update();
        }
    }

protected:
    void paintEvent(TQPaintEvent *)
    {
        TQPainter p(this);
        paintRow(p, rect(), m_icon, m_text, kPopupBgColor,
                 m_hovered || m_submenuAnchorHighlight, true,
                 0, kPopupRowMargin, false, false);
    }

    void enterEvent(TQEvent *e)
    {
        PopupRowWidget::enterEvent(e);
        if (m_popup)
            m_popup->startSavedSubmenuHover(this);
    }

    void leaveEvent(TQEvent *e)
    {
        PopupRowWidget::leaveEvent(e);
        if (m_popup)
            m_popup->cancelSavedSubmenuHover(this);
    }

    void mousePressEvent(TQMouseEvent *e)
    {
        if (e->button() != TQt::LeftButton && e->button() != TQt::RightButton)
            return;
        if (!m_popup)
            return;
        int anchorY = mapToGlobal(TQPoint(width() / 2, height() / 2)).y();
        m_popup->openSavedSubmenu(anchorY, true, this);
        m_popup->cancelSavedSubmenuHover(this);
    }

private:
    TQPixmap m_icon;
    TQString m_text;
    NmTrayPopup *m_popup;
    bool m_submenuAnchorHighlight;
};

class PopupSeparatorRow : public TQWidget
{
public:
    PopupSeparatorRow(NmTrayPopup *popup, TQWidget *parent)
        : TQWidget(parent)
        , m_popup(popup)
    {
        setFixedHeight(5);
        setBackgroundMode(TQt::NoBackground);
    }

protected:
    void paintEvent(TQPaintEvent *)
    {
        TQPainter p(this);
        p.fillRect(rect(), kPopupBgColor);
        const int y = rect().height() / 2;
        p.setPen(kPopupSeparatorColor);
        p.drawLine(kPopupRowMargin, y, rect().width() - kPopupRowMargin, y);
    }

    void enterEvent(TQEvent *)
    {
        if (m_popup)
            m_popup->onMainRowEntered(false);
    }

    void mousePressEvent(TQMouseEvent *e)
    {
        if (e->button() == TQt::RightButton)
            e->accept();
    }

private:
    NmTrayPopup *m_popup;
};

static TQWidget *makeSeparator(NmTrayPopup *popup, TQWidget *parent)
{
    return new PopupSeparatorRow(popup, parent);
}

static void setSavedNetworksAnchorHighlight(TQWidget *row, bool highlighted)
{
    PopupSubmenuOpenRow *subRow = dynamic_cast<PopupSubmenuOpenRow *>(row);
    if (subRow)
        subRow->setSubmenuAnchorHighlight(highlighted);
}

static void setPopupContentBackground(TQWidget *content)
{
    content->setPaletteBackgroundColor(kPopupBgColor);
    content->setBackgroundMode(TQt::PaletteBackground);
}

} // namespace

NmTrayPopup::NmTrayPopup(TrayController *controller, PopupKind kind, TQWidget *parent,
                         NmTrayPopup *parentPopup)
    : TQWidget(parent, "NmTrayPopup",
               WStyle_Customize | WStyle_NoBorder | WStyle_StaysOnTop
               | WType_Popup | WX11BypassWM)
    , m_controller(controller)
    , m_kind(kind)
    , m_parentPopup(parentPopup)
    , m_subPopup(0)
    , m_layout(0)
    , m_content(0)
    , m_closeTimer(0)
    , m_submenuHoverTimer(0)
    , m_savedSubmenuRow(0)
    , m_savedSubmenuAnchorRow(0)
    , m_outsideTicks(0)
    , m_bgColor(kPopupBgColor)
    , m_hasAnchor(false)
    , m_placedBelow(false)
{
    setFocusPolicy(StrongFocus);
    setPaletteBackgroundColor(m_bgColor);
    setBackgroundMode(TQt::PaletteBackground);

    m_layout = new TQVBoxLayout(this, 5, 0);
    m_layout->setMargin(5);

    m_content = new TQWidget(this);
    setPopupContentBackground(m_content);
    m_layout->addWidget(m_content);

    m_closeTimer = new TQTimer(this);
    connect(m_closeTimer, TQT_SIGNAL(timeout()), this, TQT_SLOT(onCloseTimer()));

    m_submenuHoverTimer = new TQTimer(this);
    connect(m_submenuHoverTimer, TQT_SIGNAL(timeout()),
            this, TQT_SLOT(onSavedSubmenuHoverTimer()));

    setMinimumWidth(popupWidth());
    setMaximumWidth(popupWidth());
}

int NmTrayPopup::popupWidth() const
{
    return m_kind == MainPopup ? kMainPopupWidth : kQuickPopupWidth;
}

int NmTrayPopup::popupInnerWidth() const
{
    return popupWidth() - 10;
}

NmTrayPopup::~NmTrayPopup()
{
    closeSubPopup();
}

bool NmTrayPopup::isOpen() const
{
    return isVisible();
}

TQPixmap NmTrayPopup::loadMenuIcon(const TQString &name) const
{
    return NmIcons::menuPixmap(name);
}

void NmTrayPopup::addSeparator(TQWidget *content)
{
    TQVBoxLayout *layout = content
        ? (TQVBoxLayout *) content->layout() : 0;
    if (!layout)
        return;
    layout->addWidget(makeSeparator(this, content));
}

int NmTrayPopup::measurePopupHeight(TQWidget *content) const
{
    int contentH = 0;

    if (content && content->layout()) {
        content->layout()->activate();
        contentH = content->layout()->minimumSize().height();
    }

    if (contentH <= 0 && content)
        contentH = content->minimumSizeHint().height();

    /* Outer layout margin is 5 on each side. */
    int h = contentH + 10;
    if (h < 80)
        h = 80;
    return h;
}

void NmTrayPopup::swapContent(TQWidget *next)
{
    if (!next || !m_layout)
        return;

    TQWidget *old = m_content;
    const int h = measurePopupHeight(next);

    if (old)
        m_layout->remove(old);

    m_content = next;
    setPopupContentBackground(m_content);
    m_layout->addWidget(m_content);
    m_content->show();

    setMinimumWidth(popupWidth());
    setMaximumWidth(popupWidth());
    
    if (isVisible() && m_hasAnchor) {
        int px = m_anchor.x() - popupWidth() / 2;
        int py = m_placedBelow
            ? (m_anchor.y() + 10)
            : (m_anchor.y() - h - 10);
            
        TQDesktopWidget *desktop = TQApplication::desktop();
        int screen = desktop->screenNumber(m_anchor);
        TQRect area = desktop->availableGeometry(screen);

        if (px < 10)
            px = 10;
        if (px + popupWidth() > area.right() - 10)
            px = area.right() - popupWidth() - 10;

        setGeometry(px, py, popupWidth(), h);
    } else {
        resize(popupWidth(), h);
    }

    if (old) {
        old->hide();
        TQTimer::singleShot(1000, old, SLOT(deleteLater()));
    }
}

void NmTrayPopup::rebuild()
{
    if (m_kind == MainPopup)
        closeSubPopup();

    /* Double-buffer: build into a hidden widget, measure, then swap once. */
    TQWidget *next = new TQWidget(this);
    setPopupContentBackground(next);
    next->hide();
    next->setFixedWidth(popupInnerWidth());

    switch (m_kind) {
    case MainPopup:
        fillMain(next);
        break;
    case SavedSubPopup:
        fillSavedSubmenu(next);
        break;
    default:
        fillQuick(next);
        break;
    }

    swapContent(next);
}

void NmTrayPopup::fillMain(TQWidget *content)
{
    if (!m_controller || !content)
        return;

    m_controller->clearMenuItems();

    TQVBoxLayout *layout = new TQVBoxLayout(content, 0, 0);
    layout->setMargin(0);

    if (!m_controller->client()->isNmRunning()) {
        layout->addWidget(new PopupTitleRow(
            loadMenuIcon(NmIcons::notConnectedIcon()),
            i18n("NetworkManager is not running"),
            TQPixmap(), this, content));
        return;
    }

    const bool networkingEnabled = m_controller->networkingEnabledForUi();
    const bool wirelessEnabled = m_controller->wirelessEnabledForUi();
    const bool hasActiveWired = m_controller->data()->hasActiveWired();

    const NmItemList &wired = m_controller->data()->wiredSavedItems();
    if (m_controller->data()->hasWiredDevice()) {
        const bool wiredEnabled = m_controller->wiredEnabledForUi();
        const bool wiredToggling = m_controller->isWiredToggling();
        TQPixmap wiredStatus;
        if (!networkingEnabled || !wiredEnabled || wiredToggling
            || (!m_controller->data()->isWiredCablePlugged() && !hasActiveWired))
            wiredStatus = loadMenuIcon(NmIcons::unpluggedIcon());

        layout->addWidget(new PopupTitleRow(
            loadMenuIcon(NmIcons::wiredIcon()),
            i18n("Wired Network"),
            wiredStatus, this, content));

        if (!networkingEnabled) {
            layout->addWidget(new PopupStatusRow(
                i18n("Networking is disabled"), 0, this, content));
        } else if (wiredToggling) {
            layout->addWidget(new PopupStatusRow(
                m_controller->wiredToggleTarget()
                    ? i18n("Activating wired...")
                    : i18n("Deactivating wired..."),
                0, this, content));
        } else if (!wiredEnabled) {
            layout->addWidget(new PopupStatusRow(
                i18n("Wired is disabled"), 0, this, content));
        } else if (!m_controller->data()->isWiredCablePlugged() && !hasActiveWired) {
            layout->addWidget(new PopupStatusRow(
                i18n("Cable disconnected"), 0, this, content));
        } else {
            if (hasActiveWired) {
                NmItem active = m_controller->data()->activeWiredItem();
                m_controller->registerMenuItem(TrayController::IdActiveWired, active);
                layout->addWidget(new PopupWifiActiveGroup(
                    loadMenuIcon(active.iconName), active.name,
                    -1, -1,
                    active.isActivating, this, content, "wired_active"));
            }

            for (uint i = 0; i < wired.size(); ++i) {
                if (wired[i].isActive)
                    continue;
                int id = TrayController::IdWiredSavedBase + (int) i;
                m_controller->registerMenuItem(id, wired[i]);
                layout->addWidget(new PopupActionRow(
                    loadMenuIcon(wired[i].iconName), wired[i].name,
                    id, 0, this, content));
            }
        }
        addSeparator(content);
    }

    if (m_controller->data()->hasWifiDevice()) {
        TQPixmap wifiStatus;
        const bool wifiToggling = m_controller->isWifiToggling();
        if (!networkingEnabled || !wirelessEnabled || wifiToggling)
            wifiStatus = loadMenuIcon(NmIcons::wifiOffIcon());

        layout->addWidget(new PopupTitleRow(
            loadMenuIcon(NmIcons::wirelessIcon()),
            i18n("Wireless Networks"),
            wifiStatus, this, content));

        if (m_controller->isNetworkingToggling()) {
            layout->addWidget(new PopupStatusRow(
                m_controller->networkingToggleTarget()
                    ? i18n("Activating networking...")
                    : i18n("Deactivating networking..."),
                0, this, content));
        } else if (!networkingEnabled) {
            layout->addWidget(new PopupStatusRow(
                i18n("Networking is disabled"), 0, this, content));
        } else if (wifiToggling) {
            layout->addWidget(new PopupStatusRow(
                m_controller->wifiToggleTarget()
                    ? i18n("Activating wifi...")
                    : i18n("Deactivating wifi..."),
                0, this, content));
        } else if (!wirelessEnabled) {
            layout->addWidget(new PopupStatusRow(
                i18n("Wi-Fi is disabled"), 0, this, content));
        } else {
            if (m_controller->data()->hasActiveWifi()) {
                NmItem active = m_controller->data()->activeWifiItem();
                m_controller->registerMenuItem(TrayController::IdActiveWifi, active);
                layout->addWidget(new PopupWifiActiveGroup(
                    loadMenuIcon(active.iconName), active.name,
                    active.signalStrength, TrayController::IdDisconnectWifi,
                    false, this, content));
            }

            const NmItemList &available = m_controller->data()->wifiAvailableItems();
            const NmItemList &more = m_controller->data()->wifiSavedOutOfRangeItems();

            for (uint i = 0; i < available.size(); ++i) {
                int id = TrayController::IdWifiAvailableBase + (int) i;
                m_controller->registerMenuItem(id, available[i]);
                if (available[i].isActivating) {
                    layout->addWidget(new PopupWifiActiveGroup(
                        loadMenuIcon(available[i].iconName), available[i].name,
                        available[i].signalStrength, 0,
                        true, this, content));
                } else {
                    layout->addWidget(new PopupActionRow(
                        loadMenuIcon(available[i].iconName), available[i].name,
                        id, 0, this, content, false, available[i].signalStrength,
                        WifiRowAvailable));
                }
            }

            if (available.isEmpty() && more.isEmpty()
                && !m_controller->data()->hasActiveWifi()) {
                layout->addWidget(new PopupStatusRow(
                    i18n("No wireless networks in range"), 0, this, content));
            }

            if (!more.isEmpty()) {
                layout->addWidget(new PopupSubmenuOpenRow(
                    TQPixmap(), i18n("Saved networks..."),
                    this, content));
            }
        }
        addSeparator(content);
    }

    const NmItemList &vpn = m_controller->data()->vpnSavedItems();
    if (!vpn.isEmpty()) {
        layout->addWidget(new PopupTitleRow(
            loadMenuIcon(NmIcons::vpnActiveIcon()),
            i18n("VPN Connections"),
            TQPixmap(), this, content));

        for (uint i = 0; i < vpn.size(); ++i) {
            int id = TrayController::IdVpnBase + (int) i;
            m_controller->registerMenuItem(id, vpn[i]);
            PopupActionRow *row = new PopupActionRow(
                loadMenuIcon(vpn[i].iconName), vpn[i].name,
                id, 0, this, content, vpn[i].isActive);
            if (vpn[i].isActive)
                row->setChecked(true);
            layout->addWidget(row);
        }
        addSeparator(content);
    }

    if (m_controller->data()->hasWifiDevice()) {
        layout->addWidget(new PopupActionRow(
            TQPixmap(), i18n("Connect to Hidden Wi-Fi Network..."),
            TrayController::IdHiddenWifi, 0, this, content));

        PopupActionRow *createRow = new PopupActionRow(
            TQPixmap(), i18n("Create New Wi-Fi Network..."),
            TrayController::IdCreateWifi, 0, this, content);
        createRow->setEnabled(m_controller->data()->canCreateWifiHotspot());
        layout->addWidget(createRow);
    }
}

void NmTrayPopup::fillSavedSubmenu(TQWidget *content)
{
    if (!m_controller || !content)
        return;

    m_controller->clearMenuItems();

    TQVBoxLayout *layout = new TQVBoxLayout(content, 0, 0);
    layout->setMargin(0);

    const NmItemList &more = m_controller->data()->wifiSavedOutOfRangeItems();
    for (uint i = 0; i < more.size(); ++i) {
        int id = TrayController::IdWifiMoreBase + (int) i;
        m_controller->registerMenuItem(id, more[i]);
        PopupActionRow *row = new PopupActionRow(
            loadMenuIcon(more[i].iconName), more[i].name,
            id, 0, this, content, more[i].isActive);
        if (more[i].isActive)
            row->setChecked(true);
        layout->addWidget(row);
    }
}

void NmTrayPopup::fillQuick(TQWidget *content)
{
    if (!m_controller || !content)
        return;

    m_controller->clearMenuItems();

    TQVBoxLayout *layout = new TQVBoxLayout(content, 0, 0);
    layout->setMargin(0);

    PopupActionRow *netRow = new PopupActionRow(
        TQPixmap(), i18n("Networking"),
        TrayController::IdToggleNetworking, 0, this, content);
    const bool netToggling = m_controller->isNetworkingToggling();
    const bool netEnabled = m_controller->networkingEnabledForUi();
    netRow->setChecked(netToggling
        ? m_controller->networkingToggleTarget()
        : m_controller->client()->networkingEnabled());
    layout->addWidget(netRow);

    addSeparator(content);

    PopupActionRow *wiredRow = new PopupActionRow(
        TQPixmap(), i18n("Wired"),
        TrayController::IdToggleWired, 0, this, content);
    const bool wiredToggling = m_controller->isWiredToggling();
    wiredRow->setChecked(netEnabled && (wiredToggling
        ? m_controller->wiredToggleTarget()
        : m_controller->data()->wiredEnabled()));
    wiredRow->setEnabled(netEnabled);
    layout->addWidget(wiredRow);

    if (m_controller->data()->hasWifiDevice()) {
        PopupActionRow *wifiRow = new PopupActionRow(
            TQPixmap(), i18n("Wi-Fi"),
            TrayController::IdToggleWireless, 0, this, content);
        const bool wifiToggling = m_controller->isWifiToggling();
        wifiRow->setChecked(netEnabled && (wifiToggling
            ? m_controller->wifiToggleTarget()
            : m_controller->client()->wirelessEnabled()));
        wifiRow->setEnabled(netEnabled && m_controller->client()->wirelessHardwareEnabled());
        layout->addWidget(wifiRow);
    }

    PopupActionRow *notifRow = new PopupActionRow(
        TQPixmap(), i18n("Notifications"),
        TrayController::IdToggleNotifications, 0, this, content);
    notifRow->setChecked(m_controller->notifier()->enabled());
    layout->addWidget(notifRow);

    addSeparator(content);

    if (m_controller->data()->hasWifiDevice()
        && m_controller->networkingEnabledForUi()
        && m_controller->wirelessEnabledForUi()
        && m_controller->client()->wirelessHardwareEnabled()) {
        PopupActionRow *scanRow = new PopupActionRow(
            TQPixmap(),
            m_controller->data()->isWifiScanning()
                ? i18n("Scanning wifi networks...")
                : i18n("Wifi - request scan"),
            TrayController::IdRequestWifiScan, 0, this, content);
        scanRow->setEnabled(!m_controller->data()->isWifiScanning() && !m_controller->data()->isTrayConnecting());
        layout->addWidget(scanRow);
    }

    PopupActionRow *infoRow = new PopupActionRow(
        TQPixmap(), i18n("Connection Information..."),
        TrayController::IdConnectionInfo, 0, this, content);
    NMClient *nmClient = m_controller->client()->nmClient();
    infoRow->setEnabled(hasActiveConnectionInfo(nmClient));
    layout->addWidget(infoRow);

    layout->addWidget(new PopupActionRow(
        TQPixmap(), i18n("Edit Connections..."),
        TrayController::IdEditConnections, 0, this, content));

    addSeparator(content);

    layout->addWidget(new PopupActionRow(
        TQPixmap(), i18n("Quit"),
        TrayController::IdQuit, 0, this, content));
}

static bool isQuickKeepOpenAction(int actionId)
{
    bool keepOpen = false;
    if (actionId == TrayController::IdToggleNetworking
        || actionId == TrayController::IdToggleWireless
        || actionId == TrayController::IdToggleWired
        || actionId == TrayController::IdToggleNotifications
        || actionId == TrayController::IdRequestWifiScan) {
        keepOpen = true;
    }
    return keepOpen;
}

bool NmTrayPopup::nextQuickToggleState(int actionId) const
{
    if (m_kind != QuickPopup || !m_content)
        return false;

    const TQObjectList *children = m_content->children();
    if (!children)
        return false;

    TQObjectListIterator it(*children);
    for (; it.current(); ++it) {
        PopupActionRow *row = dynamic_cast<PopupActionRow *>(it.current());
        if (!row || row->actionId() != actionId)
            continue;
        return !row->isChecked();
    }

    return false;
}

void NmTrayPopup::setQuickToggleChecked(int actionId, bool checked)
{
    if (m_kind != QuickPopup || !m_content)
        return;

    const TQObjectList *children = m_content->children();
    if (!children)
        return;

    TQObjectListIterator it(*children);
    for (; it.current(); ++it) {
        PopupActionRow *row = dynamic_cast<PopupActionRow *>(it.current());
        if (!row || row->actionId() != actionId)
            continue;
        row->setChecked(checked);
        return;
    }
}

bool NmTrayPopup::isQuickToggleBlocked(int actionId) const
{
    return m_controller && m_controller->isQuickToggleBlocked(actionId);
}

void NmTrayPopup::refreshQuickToggles()
{
    if (m_kind != QuickPopup || !m_controller || !m_content)
        return;

    const TQObjectList *children = m_content->children();
    if (!children)
        return;

    TQObjectListIterator it(*children);
    for (; it.current(); ++it) {
        PopupActionRow *row = dynamic_cast<PopupActionRow *>(it.current());
        if (!row)
            continue;

        switch (row->actionId()) {
        case TrayController::IdToggleNetworking:
            if (m_controller->isNetworkingToggling()) {
                row->setChecked(m_controller->networkingToggleTarget());
            } else {
                row->setChecked(m_controller->client()->networkingEnabled());
            }
            break;
        case TrayController::IdToggleWireless:
            if (m_controller->isWifiToggling()) {
                row->setChecked(m_controller->wifiToggleTarget());
            } else {
                row->setChecked(m_controller->client()->wirelessEnabled());
            }
            row->setEnabled(m_controller->client()->wirelessHardwareEnabled());
            break;
        case TrayController::IdToggleNotifications:
            row->setChecked(m_controller->notifier()->enabled());
            break;
        default:
            break;
        }
    }
}

void NmTrayPopup::refreshQuickScanRow()
{
    if (m_kind != QuickPopup || !m_controller || !m_content)
        return;

    const TQObjectList *children = m_content->children();
    if (!children)
        return;

    const bool scanning = m_controller->data()->isWifiScanning();
    TQObjectListIterator it(*children);
    for (; it.current(); ++it) {
        PopupActionRow *row = dynamic_cast<PopupActionRow *>(it.current());
        if (!row || row->actionId() != TrayController::IdRequestWifiScan)
            continue;

        row->setText(scanning
            ? i18n("Scanning wifi networks...")
            : i18n("Wifi - request scan"));
        row->setEnabled(!scanning && !m_controller->data()->isTrayConnecting());
        row->update();
        return;
    }
}

void NmTrayPopup::refreshWifiSignalLevels()
{
    if (m_kind != MainPopup || !m_controller || !m_content)
        return;

    const TQObjectList *children = m_content->children();
    if (!children)
        return;

    NmData *data = m_controller->data();
    const NmItemList &available = data->wifiAvailableItems();

    TQObjectListIterator it(*children);
    for (; it.current(); ++it) {
        TQObject *obj = it.current();

        if (data->hasActiveWifi()
            && obj->name() && TQString(obj->name()) == TQString("wifi_active")) {
            PopupWifiActiveGroup *active =
                dynamic_cast<PopupWifiActiveGroup *>(obj);
            if (active) {
                NmItem item = data->activeWifiItem();
                active->setSignalInfo(loadMenuIcon(item.iconName),
                                      item.name, item.signalStrength);
                m_controller->registerMenuItem(TrayController::IdActiveWifi, item);
            }
            continue;
        }

        PopupActionRow *row = dynamic_cast<PopupActionRow *>(obj);
        if (!row)
            continue;

        const int id = row->actionId();
        if (id < TrayController::IdWifiAvailableBase
            || id >= TrayController::IdWifiMoreBase)
            continue;

        const int idx = id - TrayController::IdWifiAvailableBase;
        if (idx < 0 || (uint) idx >= available.size())
            continue;

        row->setIcon(loadMenuIcon(available[idx].iconName));
        row->setSignalStrength(available[idx].signalStrength);
        m_controller->registerMenuItem(id, available[idx]);
    }
}

void NmTrayPopup::applyGeometry()
{
    if (m_content && m_content->layout())
        m_content->layout()->activate();

    setMinimumWidth(popupWidth());
    setMaximumWidth(popupWidth());
    resize(popupWidth(), measurePopupHeight(m_content));

    if (isVisible() && m_hasAnchor)
        placeAtStoredAnchor();
}

void NmTrayPopup::placeAtStoredAnchor()
{
    if (!m_hasAnchor)
        return;

    int px = m_anchor.x() - width() / 2;
    int py = m_placedBelow
        ? (m_anchor.y() + 10)
        : (m_anchor.y() - height() - 10);

    /* Horizontal only — do not pull the tray-facing edge away from the icon. */
    TQDesktopWidget *desktop = TQApplication::desktop();
    int screen = desktop->screenNumber(m_anchor);
    TQRect area = desktop->availableGeometry(screen);

    if (px < area.x() + 10)
        px = area.x() + 10;
    if (px + width() > area.right() - 10)
        px = area.right() - width() - 10;

    /* If the menu is taller than the screen, keep the tray edge and clip the far side. */
    if (!m_placedBelow && py < area.y() + 10)
        py = area.y() + 10;
    if (m_placedBelow && py + height() > area.bottom() - 10)
        py = area.bottom() - height() - 10;

    move(px, py);
}

void NmTrayPopup::clampMove(int &px, int &py) const
{
    TQDesktopWidget *desktop = TQApplication::desktop();
    int screen = desktop->screenNumber(TQPoint(px, py));
    TQRect area = desktop->availableGeometry(screen);

    if (px < area.x() + 10)
        px = area.x() + 10;
    if (px + width() > area.right() - 10)
        px = area.right() - width() - 10;
    if (py + height() > area.bottom() - 10)
        py = area.bottom() - height() - 10;
    if (py < area.y() + 10)
        py = area.y() + 10;
}

void NmTrayPopup::onMainRowEntered(bool isSavedNetworksRow)
{
    if (m_kind != MainPopup)
        return;

    if (!isSavedNetworksRow) {
        cancelSavedSubmenuHover(0);
        closeSubPopup();
    }
}

void NmTrayPopup::showNear(const TQPoint &anchor)
{
    if (m_kind == MainPopup)
        closeSubPopup();

    rebuild();

    m_anchor = anchor;
    m_hasAnchor = true;
    m_placedBelow = false;

    /* Same as before: bottom of popup = tray icon Y - 10 (menu above tray). */
    int px = anchor.x() - width() / 2;
    int py = anchor.y() - height() - 10;
    if (py < 0) {
        py = anchor.y() + 10;
        m_placedBelow = true;
    }

    if (px < 10)
        px = 10;
    if (px + width() > TQApplication::desktop()->width() - 10)
        px = TQApplication::desktop()->width() - width() - 10;

    move(px, py);
    show();
    raise();
    setFocus();
}

void NmTrayPopup::startSavedSubmenuHover(TQWidget *row)
{
    if (m_kind != MainPopup || !row)
        return;

    m_savedSubmenuRow = row;
    if (m_submenuHoverTimer)
        m_submenuHoverTimer->start(200, true);
}

void NmTrayPopup::cancelSavedSubmenuHover(TQWidget *row)
{
    if (row && m_savedSubmenuRow != row)
        return;

    m_savedSubmenuRow = 0;
    if (m_submenuHoverTimer)
        m_submenuHoverTimer->stop();
}

void NmTrayPopup::onSavedSubmenuHoverTimer()
{
    if (m_kind != MainPopup || !m_savedSubmenuRow || !isVisible())
        return;

    TQPoint localPos = m_savedSubmenuRow->mapFromGlobal(TQCursor::pos());
    if (!m_savedSubmenuRow->rect().contains(localPos))
        return;

    int anchorY = m_savedSubmenuRow->mapToGlobal(
        TQPoint(m_savedSubmenuRow->width() / 2, m_savedSubmenuRow->height() / 2)).y();
    openSavedSubmenu(anchorY, false);
}

void NmTrayPopup::openSavedSubmenu(int anchorGlobalY, bool toggle, TQWidget *anchorRow)
{
    if (!m_controller || m_kind != MainPopup || !isVisible())
        return;

    if (m_subPopup && m_subPopup->isVisible()) {
        if (toggle)
            closeSubPopup();
        return;
    }

    m_savedSubmenuAnchorRow = anchorRow ? anchorRow : m_savedSubmenuRow;

    m_subPopup = new NmTrayPopup(m_controller, SavedSubPopup, 0, this);
    m_subPopup->rebuild();

    int sx = x() + width() - 4;
    int sy = anchorGlobalY - m_subPopup->height() / 2;

    if (sx + m_subPopup->width() > TQApplication::desktop()->width() - 10)
        sx = x() - m_subPopup->width() + 4;

    m_subPopup->clampMove(sx, sy);
    m_subPopup->move(sx, sy);
    m_subPopup->show();
    m_subPopup->raise();
    m_subPopup->setFocus();
    setSavedNetworksAnchorHighlight(m_savedSubmenuAnchorRow, true);
}

void NmTrayPopup::closeSubPopup()
{
    if (!m_subPopup)
        return;
    setSavedNetworksAnchorHighlight(m_savedSubmenuAnchorRow, false);
    m_subPopup->hide();
    TQTimer::singleShot(1000, m_subPopup, SLOT(deleteLater()));
    m_subPopup = 0;
    m_savedSubmenuAnchorRow = 0;
}

bool NmTrayPopup::isSavedSubmenuOpen() const
{
    return m_subPopup && m_subPopup->isOpen();
}

bool NmTrayPopup::isPointerOverSavedSubmenuAnchor() const
{
    if (m_kind != MainPopup || !m_savedSubmenuAnchorRow || !m_savedSubmenuAnchorRow->isVisible())
        return false;

    const TQPoint localPos = m_savedSubmenuAnchorRow->mapFromGlobal(TQCursor::pos());
    return m_savedSubmenuAnchorRow->rect().contains(localPos);
}

void NmTrayPopup::triggerAction(int id)
{
    TrayController *controller = m_controller;
    NmTrayPopup *main = m_parentPopup;
    bool isConnectAction = (id >= TrayController::IdWifiAvailableBase && id < TrayController::IdVpnBase);
    const bool keepOpen = (m_kind == QuickPopup && isQuickKeepOpenAction(id)) || isConnectAction;

    if (controller)
        controller->triggerMenuAction(id);

    if (m_kind == SavedSubPopup && main) {
        /* This will hide and delete `this` (the sub-popup). */
        main->closeSubPopup();
        if (!keepOpen)
            main->hide();
        return;
    }

    closeSubPopup();
    
    if (!keepOpen) {
        hide();
    } else if (m_kind == QuickPopup) {
        NmEventPump::pump();
        if (id != TrayController::IdToggleNetworking
            && id != TrayController::IdToggleWireless)
            refreshQuickToggles();
        refreshQuickScanRow();
    }
}

void NmTrayPopup::paintEvent(TQPaintEvent *event)
{
    TQPainter p(this);
    p.fillRect(rect(), m_bgColor);
    (void) event;
}

void NmTrayPopup::showEvent(TQShowEvent *event)
{
    TQWidget::showEvent(event);
    m_outsideTicks = 0;
    if (m_closeTimer)
        m_closeTimer->start(kPopupClosePollMs);
}

void NmTrayPopup::hideEvent(TQHideEvent *event)
{
    if (m_closeTimer)
        m_closeTimer->stop();
    cancelSavedSubmenuHover(0);
    if (m_kind == MainPopup)
        closeSubPopup();
    m_hasAnchor = false;
    TQWidget::hideEvent(event);
    if (m_kind != SavedSubPopup)
        emit popupClosed();
}

bool NmTrayPopup::isRelatedWidget(TQWidget *widget) const
{
    while (widget) {
        if (widget == this)
            return true;
        if (m_subPopup && widget == m_subPopup)
            return true;
        if (m_parentPopup && widget == m_parentPopup)
            return true;
        widget = widget->parentWidget();
    }
    return false;
}

void NmTrayPopup::mousePressEvent(TQMouseEvent *event)
{
    if (event->button() == TQt::RightButton) {
        event->accept();
        setFocus();
        return;
    }

    if (event->button() == TQt::LeftButton && m_content && m_controller) {
        TQPoint contentLocal = m_content->mapFromGlobal(event->globalPos());
        if (!m_content->rect().contains(contentLocal)) {
            hide();
            event->accept();
            return;
        }

        TQWidget *w = m_content->childAt(contentLocal);
        while (w && w != m_content) {
            TQString name = w->name();
            if (name.startsWith("act_")) {
                bool ok = false;
                int id = name.mid(4).toInt(&ok);
                if (ok) {
                    event->accept();
                    triggerAction(id);
                    return;
                }
            }
            w = w->parentWidget();
        }
    }

    TQWidget::mousePressEvent(event);
}

void NmTrayPopup::focusOutEvent(TQFocusEvent *event)
{
    TQWidget *fw = tqApp->focusWidget();
    if (isRelatedWidget(fw)) {
        TQWidget::focusOutEvent(event);
        return;
    }
    if (fw && fw->inherits("KSystemTray")) {
        TQWidget::focusOutEvent(event);
        return;
    }

    const TQPoint globalPos = TQCursor::pos();
    TQPoint localPos = mapFromGlobal(globalPos);
    if (rect().contains(localPos)) {
        TQWidget::focusOutEvent(event);
        setFocus();
        return;
    }
    if (m_subPopup && m_subPopup->isVisible()) {
        TQPoint subLocal = m_subPopup->mapFromGlobal(globalPos);
        if (m_subPopup->rect().contains(subLocal)) {
            TQWidget::focusOutEvent(event);
            m_subPopup->setFocus();
            return;
        }
    }

    TQWidget::focusOutEvent(event);
}

void NmTrayPopup::onCloseTimer()
{
    const TQPoint globalPos = TQCursor::pos();
    TQPoint localPos = mapFromGlobal(globalPos);
    bool inside = rect().contains(localPos);

    if (!inside && m_subPopup && m_subPopup->isVisible()) {
        TQPoint subLocal = m_subPopup->mapFromGlobal(globalPos);
        inside = m_subPopup->rect().contains(subLocal);
    }

    if (!inside && m_kind == SavedSubPopup && m_parentPopup
        && m_parentPopup->isPointerOverSavedSubmenuAnchor()) {
        inside = true;
    }

    if (!inside) {
        m_outsideTicks++;
        if (m_outsideTicks >= kPopupCloseOutsideTicks) {
            if (m_closeTimer)
                m_closeTimer->stop();
            hide();
        }
    } else {
        m_outsideTicks = 0;
    }
}

void NmTrayPopup::onRowAction(int id)
{
    triggerAction(id);
}

#include "traypopup.moc"
