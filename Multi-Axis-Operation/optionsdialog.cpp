#include "stdafx.h"
#include "optionsdialog.h"

//---------------------------------------------------------------------------
OptionsDialog::OptionsDialog(QWidget *parent)
	: QDialog(parent)
{
	ui.setupUi(this);

	restoreSettings();
	connect(ui.buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(buttonClicked(QAbstractButton*)));
	connect(ui.magnetDAQLocationButton, SIGNAL(clicked()), this, SLOT(findMagnetDAQ()));
}

//---------------------------------------------------------------------------
OptionsDialog::~OptionsDialog()
{
}

//---------------------------------------------------------------------------
void OptionsDialog::restoreSettings(void)
{
	QSettings settings;

	m_enterPersistence = settings.value("Options/EnterPersistence", true).toBool();

	if (m_enterPersistence)
		ui.importPersistenceCheckBox->setChecked(true);
	else
		ui.importPersistenceCheckBox->setChecked(false);

	m_settlingTime = settings.value("Options/SettlingTime", 20).toInt();
	ui.settlingTimeEdit->setText(QString::number(m_settlingTime));

#if defined(Q_OS_WIN)
	QString exepath = "C:/Program Files/American Magnetics, Inc/Magnet-DAQ/Magnet-DAQ.exe";
#elif defined(Q_OS_MAC)
	QString exepath = "/Applications/Magnet-DAQ.app/Contents/MacOS/Magnet-DAQ";
#else
	QString exepath = "/usr/lib/magnet-daq/Magnet-DAQ";
#endif

	m_magnetDAQLocation = settings.value("Options/MagnetDAQLocation", exepath).toString();
	ui.magnetDAQLocationEdit->setText(m_magnetDAQLocation);

	m_magnetDAQMinimized = settings.value("Options/MagnetDAQMinimzed", true).toBool();

	if (m_magnetDAQMinimized)
		ui.minimizedCheckBox->setChecked(true);
	else
		ui.minimizedCheckBox->setChecked(false);

	m_disableAutoStability = settings.value("Options/DisableAutoStability", false).toBool();

	if (m_disableAutoStability)
		ui.autoModeDisableCheckBox->setChecked(true);
	else
		ui.autoModeDisableCheckBox->setChecked(false);
}

//---------------------------------------------------------------------------
void OptionsDialog::saveSettings(void)
{
	QSettings settings;

	settings.setValue("Options/EnterPersistence", m_enterPersistence);
	settings.setValue("Options/SettlingTime", m_settlingTime);
	settings.setValue("Options/MagnetDAQLocation", m_magnetDAQLocation);
	settings.setValue("Options/MagnetDAQMinimzed", m_magnetDAQMinimized);
	settings.setValue("Options/DisableAutoStability", m_disableAutoStability);
}

//---------------------------------------------------------------------------
bool OptionsDialog::readSettingsFromDialog(void)
{
	int checkValue;
	QString checkStr;
	bool ok;

	// read enter persistence
	m_enterPersistence = ui.importPersistenceCheckBox->isChecked();

	// check settling time
	checkValue = ui.settlingTimeEdit->text().toInt(&ok);
	if (ok && checkValue >= 20 && checkValue <= 999)
		m_settlingTime = checkValue;
	else
	{
		showError("Invalid Settling Time value, please check.");	// error
		ui.settlingTimeEdit->setFocus();
		return false;
	}

	// check that magnetDAQ location exists
	checkStr = ui.magnetDAQLocationEdit->text();
	if (!(QFileInfo::exists(checkStr)))
	{
		showError("Invalid Magnet-DAQ executable or app bundle location, please check.");	// error
		ui.magnetDAQLocationEdit->setFocus();
		return false;
	}

	// read minimized preference
	m_magnetDAQMinimized = ui.minimizedCheckBox->isChecked();

	// read AUTO Stability Mode override
	m_disableAutoStability = ui.autoModeDisableCheckBox->isChecked();

	saveSettings();

	return true;	// all settings good!
}

//---------------------------------------------------------------------------
void OptionsDialog::showError(QString errMsg)
{
	QMessageBox msgBox;

	msgBox.setWindowTitle("Parameter Entry Error?");
	msgBox.setText(errMsg);
	msgBox.setStandardButtons(QMessageBox::Ok);
	msgBox.setDefaultButton(QMessageBox::Ok);
	msgBox.setIcon(QMessageBox::Critical);
	int ret = msgBox.exec();
}

//---------------------------------------------------------------------------
void OptionsDialog::buttonClicked(QAbstractButton* whichButton)
{
	QDialogButtonBox::StandardButton stdButton = ui.buttonBox->standardButton(whichButton);
	switch (stdButton)
	{
	case QDialogButtonBox::Ok:
		if (readSettingsFromDialog())
			emit accept();

		break;

	case QDialogButtonBox::Cancel:
		restoreSettings();
		emit reject();

		break;
	}
}

//---------------------------------------------------------------------------
void OptionsDialog::findMagnetDAQ(void)
{
	QString location;
	QSettings settings;

#if defined(Q_OS_LINUX)
	location = QFileDialog::getOpenFileName(this, "Find Magnet-DAQ Executable", m_magnetDAQLocation, "Executable (*)");
#elif defined(Q_OS_MACOS)
	location = QFileDialog::getOpenFileName(this, "Find Magnet-DAQ Application", m_magnetDAQLocation, "Application (*)");
#else
	location = QFileDialog::getOpenFileName(this, "Find Magnet-DAQ Executable", m_magnetDAQLocation, "Executable (*.exe)");
#endif

	if (!location.isEmpty())
	{
		m_magnetDAQLocation = location;
		ui.magnetDAQLocationEdit->setText(m_magnetDAQLocation);

		// save path
		settings.setValue("Options/MagnetDAQLocation", m_magnetDAQLocation);
	}
}

//---------------------------------------------------------------------------
