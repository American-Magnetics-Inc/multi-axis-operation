#include "stdafx.h"
#include "aboutdialog.h"

//---------------------------------------------------------------------------
aboutdialog::aboutdialog(QDialog *parent)
	: QDialog(parent)
{
	ui.setupUi(this);
	connect(ui.aboutQtButton, SIGNAL(clicked(bool)), this, SLOT(showAboutQt(bool)));
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	ui.license->setTextInteractionFlags(Qt::TextBrowserInteraction);
	ui.license->setOpenExternalLinks(true);
	ui.appname->setTextInteractionFlags(Qt::TextBrowserInteraction);
	ui.appname->setOpenExternalLinks(true);
}

//---------------------------------------------------------------------------
aboutdialog::~aboutdialog()
{
}

//---------------------------------------------------------------------------
void aboutdialog::showAboutQt(bool)
{
	qApp->aboutQt();
}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------