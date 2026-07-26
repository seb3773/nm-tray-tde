#ifndef CONNECTION_EDITOR_DIALOG_H
#define CONNECTION_EDITOR_DIALOG_H

#include <tqdialog.h>

class TQShowEvent;
class TDEListView;
class TQPushButton;
class NmClient;

class ConnectionEditorDialog : public TQDialog
{
    TQ_OBJECT

public:
    explicit ConnectionEditorDialog(NmClient *client, TQWidget *parent = 0);

protected:
    void showEvent(TQShowEvent *event);

private slots:
    void reload();
    void onNew();
    void onDelete();
    void onEdit();
    void onSelectionChanged();

private:
    TQString selectedConnectionPath() const;

    NmClient *m_client;
    TDEListView *m_list;
    TQPushButton *m_newBtn;
};

#endif
