#pragma once

#include <QDialog>
#include "ui_aboutdialog.h"

class aboutdialog : public QDialog
{
	Q_OBJECT

public:
	aboutdialog(QDialog *parent = Q_NULLPTR);
	~aboutdialog();

private slots:
	void showAboutQt(bool);

private:
	Ui::aboutdialog ui;
};
