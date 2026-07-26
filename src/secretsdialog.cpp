#include "secretsdialog.h"
#include "connectioneditorutil.h"

#include <tdelocale.h>
#include <tqlabel.h>
#include <tqlineedit.h>
#include <tqpushbutton.h>
#include <tqlayout.h>
#include <tqevent.h>

#include <tqdialog.h>

class SecretsDialog : public TQDialog
{
public:
    SecretsDialog(const SecretsDialogRequest &request, TQWidget *parent = 0)
        : TQDialog(parent, "secrets_dialog", true)
        , m_identityEdit(0)
        , m_secretEdit(0)
    {
        setCaption(request.title.isEmpty()
            ? i18n("Network Authentication") : request.title);

        TQVBoxLayout *root = new TQVBoxLayout(this, 12, 6);

        if (!request.message.isEmpty()) {
            TQLabel *msg = new TQLabel(request.message, this);
            msg->setAlignment(TQt::WordBreak | TQt::AlignAuto);
            root->addWidget(msg);
        }

        if (request.showIdentity) {
            TQHBoxLayout *idRow = new TQHBoxLayout();
            idRow->setSpacing(6);
            TQString idLabel = request.identityLabel.isEmpty()
                ? i18n("Username:") : request.identityLabel;
            idRow->addWidget(new TQLabel(idLabel, this));
            m_identityEdit = new TQLineEdit(this);
            m_identityEdit->setText(request.identityPrefill);
            idRow->addWidget(m_identityEdit, 1);
            root->addLayout(idRow);
        }

        TQHBoxLayout *secretRow = new TQHBoxLayout();
        secretRow->setSpacing(6);
        TQString secLabel = request.secretLabel.isEmpty()
            ? i18n("Password:") : request.secretLabel;
        secretRow->addWidget(new TQLabel(secLabel, this));
        m_secretEdit = new TQLineEdit(this);
        m_secretEdit->setEchoMode(TQLineEdit::Password);
        secretRow->addWidget(m_secretEdit, 1);
        root->addLayout(secretRow);

        TQHBoxLayout *buttons = new TQHBoxLayout();
        buttons->setSpacing(6);
        buttons->addStretch(1);
        TQPushButton *cancelBtn = new TQPushButton(i18n("Cancel"), this);
        TQPushButton *okBtn = new TQPushButton(i18n("Connect"), this);
        okBtn->setDefault(true);
        buttons->addWidget(cancelBtn);
        buttons->addWidget(okBtn);
        root->addLayout(buttons);

        connect(okBtn, TQT_SIGNAL(clicked()), this, TQT_SLOT(accept()));
        connect(cancelBtn, TQT_SIGNAL(clicked()), this, TQT_SLOT(reject()));

        if (request.showIdentity && m_identityEdit)
            m_identityEdit->setFocus();
        else
            m_secretEdit->setFocus();

        resize(460, request.showIdentity ? 180 : 140);
    }

    TQString identity() const
    {
        return m_identityEdit ? m_identityEdit->text().stripWhiteSpace() : TQString::null;
    }

    TQString secret() const
    {
        return m_secretEdit ? m_secretEdit->text() : TQString::null;
    }

protected:
    void showEvent(TQShowEvent *event)
    {
        TQDialog::showEvent(event);
        centerEditorDialog(this);
    }

private:
    TQLineEdit *m_identityEdit;
    TQLineEdit *m_secretEdit;
};

bool runSecretsDialog(const SecretsDialogRequest &request,
                      SecretsDialogResult *result)
{
    if (!result)
        return false;

    result->accepted = false;
    result->identity = TQString::null;
    result->secret = TQString::null;

    SecretsDialog dlg(request);
    if (dlg.exec() != TQDialog::Accepted)
        return false;

    result->identity = dlg.identity();
    result->secret = dlg.secret();
    if (result->secret.isEmpty())
        return false;

    result->accepted = true;
    return true;
}
