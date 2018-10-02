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

#if defined(Q_OS_MACOS)
    // Mac base font scaling is different than Linux and Windows
    // use ~ 96/72 = 1.333 factor for upscaling every font size
    // we must do this everywhere a font size is specified
    this->setFont(QFont(".SF NS Text", 13));
#endif
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
