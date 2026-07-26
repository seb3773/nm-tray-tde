#ifndef NM_ITEM_H
#define NM_ITEM_H

#include <tqstring.h>
#include <tqvaluelist.h>

struct NmItem
{
    enum Type {
        Active,
        Wifi,
        Saved,
        Device
    };

    Type type;
    TQString name;
    TQString uuid;
    TQString path;          // D-Bus object path (connection or AP)
    TQString devicePath;    // device for activation
    TQString specificObject; // AP path for Wi-Fi activation
    TQString iconName;
    int signalStrength;     // 0-100 for Wi-Fi
    bool secured;
    bool isActive;
    bool isActivating;
    bool hasSavedProfile;

    NmItem()
        : type(Wifi)
        , signalStrength(0)
        , secured(false)
        , isActive(false)
        , isActivating(false)
        , hasSavedProfile(false)
    {
    }
};

typedef TQValueList<NmItem> NmItemList;

#endif
