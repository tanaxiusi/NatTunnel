#pragma once

#include <QDialog>
#include "ui_MultiLineInputDialog.h"

class MultiLineInputDialog : public QDialog
{
	Q_OBJECT

public:
	MultiLineInputDialog(QWidget *parent = 0);
	~MultiLineInputDialog();

	static QString getText(QWidget *parent, const QString &title, const QString &label,
		const QString &text = QString(), bool *ok = NULL, Qt::WindowFlags flags = Qt::WindowFlags());

private:
	Ui::MultiLineInputDialog ui;
};
