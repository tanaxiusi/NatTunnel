#include "MultiLineInputDialog.h"

MultiLineInputDialog::MultiLineInputDialog(QWidget *parent)
	: QDialog(parent)
{
	ui.setupUi(this);
	connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
	connect(ui.buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
}

MultiLineInputDialog::~MultiLineInputDialog()
{

}

QString MultiLineInputDialog::getText(QWidget *parent, const QString &title, const QString &label, const QString &text,
	bool *ok, Qt::WindowFlags flags)
{
	MultiLineInputDialog dlg;
	dlg.setWindowTitle(title);
	dlg.ui.label->setText(label);
	dlg.ui.plainTextEdit->setPlainText(text);
	dlg.setWindowFlags(flags);
	if (dlg.exec() == QDialog::Accepted)
	{
		if (ok)
			*ok = true;
		return dlg.ui.plainTextEdit->toPlainText();
	}
	else
	{
		if (ok)
			*ok = false;
		return QString();
	}
}
