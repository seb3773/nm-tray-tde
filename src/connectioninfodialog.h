#ifndef CONNECTION_INFO_DIALOG_H
#define CONNECTION_INFO_DIALOG_H

#include <tqdialog.h>

class TQShowEvent;
class TQMultiLineEdit;
class NmClient;

class ConnectionInfoDialog : public TQDialog
{
    TQ_OBJECT

public:
    explicit ConnectionInfoDialog(NmClient *client, TQWidget *parent = 0);

protected:
    void showEvent(TQShowEvent *event);

private slots:
    void onRefresh();

private:
    void reload();

    NmClient *m_client;
    TQMultiLineEdit *m_text;
};

#endif
