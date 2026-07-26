#ifndef NM_TRAY_POPUP_H
#define NM_TRAY_POPUP_H

#include <tqwidget.h>
#include <tqcolor.h>
#include <tqpoint.h>

class TQVBoxLayout;
class TQTimer;
class TrayController;

class NmTrayPopup : public TQWidget
{
    TQ_OBJECT

public:
    enum PopupKind {
        MainPopup,
        QuickPopup,
        SavedSubPopup
    };

    explicit NmTrayPopup(TrayController *controller, PopupKind kind,
                         TQWidget *parent = 0, NmTrayPopup *parentPopup = 0);
    ~NmTrayPopup();

    void rebuild();
    void showNear(const TQPoint &anchor);
    bool isOpen() const;

    void triggerAction(int id);
    void openSavedSubmenu(int anchorGlobalY, bool toggle = true,
                          TQWidget *anchorRow = 0);
    void closeSubPopup();
    bool isSavedSubmenuOpen() const;
    void startSavedSubmenuHover(TQWidget *row);
    void cancelSavedSubmenuHover(TQWidget *row);
    void onMainRowEntered(bool isSavedNetworksRow);
    int popupWidth() const;
    int popupInnerWidth() const;
    void refreshQuickScanRow();
    void refreshQuickToggles();
    void refreshWifiSignalLevels();
    bool nextQuickToggleState(int actionId) const;
    void setQuickToggleChecked(int actionId, bool checked);
    bool isQuickToggleBlocked(int actionId) const;

signals:
    void popupClosed();

protected:
    void paintEvent(TQPaintEvent *event);
    void focusOutEvent(TQFocusEvent *event);
    void mousePressEvent(TQMouseEvent *event);
    void showEvent(TQShowEvent *event);
    void hideEvent(TQHideEvent *event);

private slots:
    void onCloseTimer();
    void onRowAction(int id);
    void onSavedSubmenuHoverTimer();

private:
    bool isRelatedWidget(TQWidget *widget) const;
    bool isPointerOverSavedSubmenuAnchor() const;

    void fillMain(TQWidget *content);
    void fillSavedSubmenu(TQWidget *content);
    void fillQuick(TQWidget *content);
    void addSeparator(TQWidget *content);
    void swapContent(TQWidget *next);
    int measurePopupHeight(TQWidget *content) const;
    void applyGeometry();
    void placeAtStoredAnchor();
    void clampMove(int &px, int &py) const;
    TQPixmap loadMenuIcon(const TQString &name) const;

    TrayController *m_controller;
    PopupKind m_kind;
    NmTrayPopup *m_parentPopup;
    NmTrayPopup *m_subPopup;
    TQVBoxLayout *m_layout;
    TQWidget *m_content;
    TQTimer *m_closeTimer;
    TQTimer *m_submenuHoverTimer;
    TQWidget *m_savedSubmenuRow;
    TQWidget *m_savedSubmenuAnchorRow;
    int m_outsideTicks;
    TQColor m_bgColor;
    TQPoint m_anchor;
    bool m_hasAnchor;
    bool m_placedBelow;
};

#endif
