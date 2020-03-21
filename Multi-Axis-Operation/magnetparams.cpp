#include "stdafx.h"
#include "magnetparams.h"

//---------------------------------------------------------------------------
MagnetParams::MagnetParams(QWidget *parent)
	: QDialog(parent)
{
	ui.setupUi(this);
	this->setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	readOnly = false;

#if defined(Q_OS_MACOS)
    // Mac base font scaling is different than Linux and Windows
    // use ~ 96/72 = 1.333 factor for upscaling every font size
    // we must do this everywhere a font size is specified
    this->setFont(QFont(".SF NS Text", 13));
#endif

	// restore geometry and settings
	QSettings settings;

	QString dpiStr = QString::number(QApplication::desktop()->screen()->logicalDpiX());
	restoreGeometry(settings.value("MagnetParams/Geometry/" + dpiStr).toByteArray());

	restore();
}

//---------------------------------------------------------------------------
MagnetParams::~MagnetParams()
{
	// save geometry
	QSettings settings;

	// save different geometry for different DPI screens
	QString dpiStr = QString::number(QApplication::desktop()->screen()->logicalDpiX());

	settings.setValue("MagnetParams/Geometry/" + dpiStr, saveGeometry());
}

//---------------------------------------------------------------------------
void MagnetParams::actionActivate(bool checked)
{
	x.activate = ui.xAxisGroupBox->isChecked();
	y.activate = ui.yAxisGroupBox->isChecked();
	z.activate = ui.zAxisGroupBox->isChecked();
}

//---------------------------------------------------------------------------
void MagnetParams::setFieldUnits(FieldUnits units)
{
	QString tempStr;

	if (units == KG)
	{
		tempStr = ui.magnitudeLimitLabel->text().replace("(T)", "(kG)");
		ui.magnitudeLimitLabel->setText(tempStr);

		tempStr = ui.xAxisCoilConstLabel->text().replace("(T", "(kG");
		ui.xAxisCoilConstLabel->setText(tempStr);

		tempStr = ui.yAxisCoilConstLabel->text().replace("(T", "(kG");
		ui.yAxisCoilConstLabel->setText(tempStr);

		tempStr = ui.zAxisCoilConstLabel->text().replace("(T", "(kG");
		ui.zAxisCoilConstLabel->setText(tempStr);
	}
	else if (units == TESLA)
	{
		tempStr = ui.magnitudeLimitLabel->text().replace("(kG)", "(T)");
		ui.magnitudeLimitLabel->setText(tempStr);

		tempStr = ui.xAxisCoilConstLabel->text().replace("(kG", "(T");
		ui.xAxisCoilConstLabel->setText(tempStr);

		tempStr = ui.yAxisCoilConstLabel->text().replace("(kG", "(T");
		ui.yAxisCoilConstLabel->setText(tempStr);

		tempStr = ui.zAxisCoilConstLabel->text().replace("(kG", "(T");
		ui.zAxisCoilConstLabel->setText(tempStr);
	}
}

//---------------------------------------------------------------------------
void MagnetParams::convertFieldValues(FieldUnits newUnits)
{
	if (newUnits == TESLA)	// convert from KG
	{
		magnitudeLimit /= 10.0;
		x.coilConst /= 10.0;
		y.coilConst /= 10.0;
		z.coilConst /= 10.0;
	}
	else if (newUnits == KG)
	{
		magnitudeLimit *= 10.0;
		x.coilConst *= 10.0;
		y.coilConst *= 10.0;
		z.coilConst *= 10.0;
	}

	// update the interface
	if (!ui.magnitudeLimitEdit->text().isEmpty())
		ui.magnitudeLimitEdit->setText(QString::number(magnitudeLimit, 'g', 10));

	if (x.activate && !ui.xAxisCoilConstValue->text().isEmpty())
		ui.xAxisCoilConstValue->setText(QString::number(x.coilConst, 'g', 10));

	if (y.activate && !ui.yAxisCoilConstValue->text().isEmpty())
		ui.yAxisCoilConstValue->setText(QString::number(y.coilConst, 'g', 10));

	if (z.activate && !ui.zAxisCoilConstValue->text().isEmpty())
		ui.zAxisCoilConstValue->setText(QString::number(z.coilConst, 'g', 10));
}

//---------------------------------------------------------------------------
void MagnetParams::showError(QString errMsg)
{
	QMessageBox msgBox;

	msgBox.setWindowTitle("Magnet Parameter Entry Error?");
	msgBox.setText(errMsg);
	msgBox.setStandardButtons(QMessageBox::Ok);
	msgBox.setDefaultButton(QMessageBox::Ok);
	msgBox.setIcon(QMessageBox::Critical);
	int ret = msgBox.exec();
}

//---------------------------------------------------------------------------
void MagnetParams::setReadOnly(void)
{
	QList<QLineEdit*> l_lineEdits = this->findChildren<QLineEdit*>();

	foreach(QLineEdit* l_lineEdit, l_lineEdits)
	{
		l_lineEdit->setReadOnly(true);
	}

	ui.xAxisGroupBox->setCheckable(false);
	ui.xAxisGroupBox->setEnabled(x.activate);

	ui.yAxisGroupBox->setCheckable(false);
	ui.yAxisGroupBox->setEnabled(y.activate);

	ui.zAxisGroupBox->setCheckable(false);
	ui.zAxisGroupBox->setEnabled(z.activate);

	ui.xAxisSwitchInstalledButton->setEnabled(false);
	ui.yAxisSwitchInstalledButton->setEnabled(false);
	ui.zAxisSwitchInstalledButton->setEnabled(false);

	this->setWindowTitle("Magnet Parameters and Limits (Read Only)");
	readOnly = true;
}

//---------------------------------------------------------------------------
void MagnetParams::clearReadOnly(void)
{
	QList<QLineEdit*> l_lineEdits = this->findChildren<QLineEdit*>();

	foreach(QLineEdit* l_lineEdit, l_lineEdits)
	{
		l_lineEdit->setReadOnly(false);
	}

	ui.xAxisSwitchInstalledButton->setEnabled(true);
	ui.yAxisSwitchInstalledButton->setEnabled(true);
	ui.zAxisSwitchInstalledButton->setEnabled(true);

	ui.xAxisGroupBox->setCheckable(true);
	ui.xAxisGroupBox->setChecked(x.activate);

	ui.yAxisGroupBox->setCheckable(true);
	ui.yAxisGroupBox->setChecked(y.activate);

	ui.zAxisGroupBox->setCheckable(true);
	ui.zAxisGroupBox->setChecked(z.activate);

	this->setWindowTitle("Magnet Parameters and Limits");
	readOnly = false;
}

//---------------------------------------------------------------------------
// If any active axis has a switch, return true
bool MagnetParams::switchInstalled(void)
{
	if (x.activate && x.switchInstalled)
		return true;

	if (y.activate && y.switchInstalled)
		return true;

	if (z.activate && z.switchInstalled)
		return true;

	return false;
}

//---------------------------------------------------------------------------
void MagnetParams::xAxisSwitchUiConfig(bool checked, bool updateUI)
{
	if (checked)
	{
		if (updateUI)
			ui.xAxisSwitchInstalledButton->setChecked(true);

		ui.xAxisSwitchCurrentLabel->setEnabled(true);
		ui.xAxisSwitchCurrentValue->setEnabled(true);
		ui.xAxisSwitchHeatTimeLabel->setEnabled(true);
		ui.xAxisSwitchHeatTimeValue->setEnabled(true);
		ui.xAxisSwitchCoolTimeLabel->setEnabled(true);
		ui.xAxisSwitchCoolTimeValue->setEnabled(true);
	}
	else
	{
		if (updateUI)
			ui.xAxisSwitchInstalledButton->setChecked(false);

		ui.xAxisSwitchCurrentLabel->setEnabled(false);
		ui.xAxisSwitchCurrentValue->setEnabled(false);
		ui.xAxisSwitchCurrentValue->clear();
		ui.xAxisSwitchHeatTimeLabel->setEnabled(false);
		ui.xAxisSwitchHeatTimeValue->setEnabled(false);
		ui.xAxisSwitchHeatTimeValue->clear();
		ui.xAxisSwitchCoolTimeLabel->setEnabled(false);
		ui.xAxisSwitchCoolTimeValue->setEnabled(false);
		ui.xAxisSwitchCoolTimeValue->clear();
	}
}

//---------------------------------------------------------------------------
void MagnetParams::yAxisSwitchUiConfig(bool checked, bool updateUI)
{
	if (checked)
	{
		if (updateUI)
			ui.yAxisSwitchInstalledButton->setChecked(true);

		ui.yAxisSwitchCurrentLabel->setEnabled(true);
		ui.yAxisSwitchCurrentValue->setEnabled(true);
		ui.yAxisSwitchHeatTimeLabel->setEnabled(true);
		ui.yAxisSwitchHeatTimeValue->setEnabled(true);
		ui.yAxisSwitchCoolTimeLabel->setEnabled(true);
		ui.yAxisSwitchCoolTimeValue->setEnabled(true);
	}
	else
	{
		if (updateUI)
			ui.yAxisSwitchInstalledButton->setChecked(false);

		ui.yAxisSwitchCurrentLabel->setEnabled(false);
		ui.yAxisSwitchCurrentValue->setEnabled(false);
		ui.yAxisSwitchCurrentValue->clear();
		ui.yAxisSwitchHeatTimeLabel->setEnabled(false);
		ui.yAxisSwitchHeatTimeValue->setEnabled(false);
		ui.yAxisSwitchHeatTimeValue->clear();
		ui.yAxisSwitchCoolTimeLabel->setEnabled(false);
		ui.yAxisSwitchCoolTimeValue->setEnabled(false);
		ui.yAxisSwitchCoolTimeValue->clear();
	}
}

//---------------------------------------------------------------------------
void MagnetParams::zAxisSwitchUiConfig(bool checked, bool updateUI)
{
	if (checked)
	{
		if (updateUI)
			ui.zAxisSwitchInstalledButton->setChecked(true);

		ui.zAxisSwitchCurrentLabel->setEnabled(true);
		ui.zAxisSwitchCurrentValue->setEnabled(true);
		ui.zAxisSwitchHeatTimeLabel->setEnabled(true);
		ui.zAxisSwitchHeatTimeValue->setEnabled(true);
		ui.zAxisSwitchCoolTimeLabel->setEnabled(true);
		ui.zAxisSwitchCoolTimeValue->setEnabled(true);
	}
	else
	{
		if (updateUI)
			ui.zAxisSwitchInstalledButton->setChecked(false);

		ui.zAxisSwitchCurrentLabel->setEnabled(false);
		ui.zAxisSwitchCurrentValue->setEnabled(false);
		ui.zAxisSwitchCurrentValue->clear();
		ui.zAxisSwitchHeatTimeLabel->setEnabled(false);
		ui.zAxisSwitchHeatTimeValue->setEnabled(false);
		ui.zAxisSwitchHeatTimeValue->clear();
		ui.zAxisSwitchCoolTimeLabel->setEnabled(false);
		ui.zAxisSwitchCoolTimeValue->setEnabled(false);
		ui.zAxisSwitchCoolTimeValue->clear();
	}
}

//---------------------------------------------------------------------------
void MagnetParams::switchConfigClicked(bool checked)
{
	if (qobject_cast<QCheckBox*>(sender()) == ui.xAxisSwitchInstalledButton)
		xAxisSwitchUiConfig(checked, false);
	else if (qobject_cast<QCheckBox*>(sender()) == ui.yAxisSwitchInstalledButton)
		yAxisSwitchUiConfig(checked, false);
	else if (qobject_cast<QCheckBox*>(sender()) == ui.zAxisSwitchInstalledButton)
		zAxisSwitchUiConfig(checked, false);
}

//---------------------------------------------------------------------------
void MagnetParams::dialogButtonClicked(QAbstractButton * button)
{
	if (readOnly)
	{
		emit reject();
	}
	else
	{
		if (qobject_cast<QPushButton*>(button) == ui.dialogButtonBox->button(QDialogButtonBox::Ok))
		{
			bool ok;
			double temp;

			// copy data from dialog on accept (OK button pressed)
			magnetID = ui.magnetIDEdit->text();

			temp = ui.magnitudeLimitEdit->text().toDouble(&ok);
			if (ok)
				magnitudeLimit = temp;
			else
			{
				showError("Invalid Magnitude Limit value, please check.");	// error
				ui.magnitudeLimitEdit->setFocus();
				return;
			}

			// X axis data
			if (x.activate = ui.xAxisGroupBox->isChecked())
			{
				x.ipAddress = ui.xAxisIPValue->text();

				temp = ui.xAxisCurrentLimitValue->text().toDouble(&ok);
				if (ok)
					x.currentLimit = temp;
				else
				{
					showError("Invalid X-axis Current Limit value, please check.");	// error
					ui.xAxisCurrentLimitValue->setFocus();
					return;
				}

				temp = ui.xAxisVoltageLimitValue->text().toDouble(&ok);
				if (ok)
					x.voltageLimit = temp;
				else
				{
					showError("Invalid X-axis Voltage Limit value, please check.");	// error
					ui.xAxisVoltageLimitValue->setFocus();
					return;
				}

				temp = ui.xAxisMaxRampRateValue->text().toDouble(&ok);
				if (ok)
					x.maxRampRate = temp;
				else
				{
					showError("Invalid X-axis Max Ramp Rate value, please check.");	// error
					ui.xAxisMaxRampRateValue->setFocus();
					return;
				}

				temp = ui.xAxisCoilConstValue->text().toDouble(&ok);
				if (ok)
					x.coilConst = temp;
				else
				{
					showError("Invalid X-axis Coil Constant value, please check.");	// error
					ui.xAxisCoilConstValue->setFocus();
					return;
				}

				temp = ui.xAxisInductanceValue->text().toDouble(&ok);
				if (ok)
					x.inductance = temp;
				else
				{
					showError("Invalid X-axis Inductance value, please check.");	// error
					ui.xAxisInductanceValue->setFocus();
					return;
				}

				if (ui.xAxisSwitchInstalledButton->isChecked())
					x.switchInstalled = true;
				else
					x.switchInstalled = false;

				if (x.switchInstalled)
				{
					temp = ui.xAxisSwitchCurrentValue->text().toDouble(&ok);
					if (ok)
						x.switchHeaterCurrent = temp;
					else
					{
						showError("Invalid X-axis Switch Heater Current value, please check.");	// error
						ui.xAxisSwitchCurrentValue->setFocus();
						return;
					}

					temp = ui.xAxisSwitchHeatTimeValue->text().toDouble(&ok);
					if (ok)
						x.switchHeatingTime = (int)temp;
					else
					{
						showError("Invalid X-axis Switch Heating Time value, please check.");	// error
						ui.xAxisSwitchHeatTimeValue->setFocus();
						return;
					}

					temp = ui.xAxisSwitchCoolTimeValue->text().toDouble(&ok);
					if (ok)
						x.switchCoolingTime = (int)temp;
					else
					{
						showError("Invalid X-axis Switch Cooling Time value, please check.");	// error
						ui.xAxisSwitchCoolTimeValue->setFocus();
						return;
					}
				}
			}

			// Y axis data
			if (y.activate = ui.yAxisGroupBox->isChecked())
			{
				y.ipAddress = ui.yAxisIPValue->text();

				temp = ui.yAxisCurrentLimitValue->text().toDouble(&ok);
				if (ok)
					y.currentLimit = temp;
				else
				{
					showError("Invalid Y-axis Current Limit value, please check.");	// error
					ui.yAxisCurrentLimitValue->setFocus();
					return;
				}

				temp = ui.yAxisVoltageLimitValue->text().toDouble(&ok);
				if (ok)
					y.voltageLimit = temp;
				else
				{
					showError("Invalid Y-axis Voltage Limit value, please check.");	// error
					ui.yAxisVoltageLimitValue->setFocus();
					return;
				}

				temp = ui.yAxisMaxRampRateValue->text().toDouble(&ok);
				if (ok)
					y.maxRampRate = temp;
				else
				{
					showError("Invalid Y-axis Max Ramp Rate value, please check.");	// error
					ui.yAxisMaxRampRateValue->setFocus();
					return;
				}

				temp = ui.yAxisCoilConstValue->text().toDouble(&ok);
				if (ok)
					y.coilConst = temp;
				else
				{
					showError("Invalid Y-axis Coil Constant value, please check.");	// error
					ui.yAxisCoilConstValue->setFocus();
					return;
				}

				temp = ui.yAxisInductanceValue->text().toDouble(&ok);
				if (ok)
					y.inductance = temp;
				else
				{
					showError("Invalid Y-axis Inductance value, please check.");	// error
					ui.yAxisInductanceValue->setFocus();
					return;
				}

				if (ui.yAxisSwitchInstalledButton->isChecked())
					y.switchInstalled = true;
				else
					y.switchInstalled = false;

				if (y.switchInstalled)
				{
					temp = ui.yAxisSwitchCurrentValue->text().toDouble(&ok);
					if (ok)
						y.switchHeaterCurrent = temp;
					else
					{
						showError("Invalid Y-axis Switch Heater Current value, please check.");	// error
						ui.yAxisSwitchCurrentValue->setFocus();
						return;
					}

					temp = ui.yAxisSwitchHeatTimeValue->text().toDouble(&ok);
					if (ok)
						y.switchHeatingTime = (int)temp;
					else
					{
						showError("Invalid Y-axis Switch Heating Time value, please check.");	// error
						ui.yAxisSwitchHeatTimeValue->setFocus();
						return;
					}

					temp = ui.yAxisSwitchCoolTimeValue->text().toDouble(&ok);
					if (ok)
						y.switchCoolingTime = (int)temp;
					else
					{
						showError("Invalid Y-axis Switch Cooling Time value, please check.");	// error
						ui.yAxisSwitchCoolTimeValue->setFocus();
						return;
					}
				}
			}

			// Z axis data
			if (z.activate = ui.zAxisGroupBox->isChecked())
			{
				z.ipAddress = ui.zAxisIPValue->text();

				temp = ui.zAxisCurrentLimitValue->text().toDouble(&ok);
				if (ok)
					z.currentLimit = temp;
				else
				{
					showError("Invalid Z-axis Current Limit value, please check.");	// error
					ui.zAxisCurrentLimitValue->setFocus();
					return;
				}

				temp = ui.zAxisVoltageLimitValue->text().toDouble(&ok);
				if (ok)
					z.voltageLimit = temp;
				else
				{
					showError("Invalid Z-axis Voltage Limit value, please check.");	// error
					ui.zAxisVoltageLimitValue->setFocus();
					return;
				}

				temp = ui.zAxisMaxRampRateValue->text().toDouble(&ok);
				if (ok)
					z.maxRampRate = temp;
				else
				{
					showError("Invalid Z-axis Max Ramp Rate value, please check.");	// error
					ui.zAxisMaxRampRateValue->setFocus();
					return;
				}

				temp = ui.zAxisCoilConstValue->text().toDouble(&ok);
				if (ok)
					z.coilConst = temp;
				else
				{
					showError("Invalid Z-axis Coil Constant value, please check.");	// error
					ui.zAxisCoilConstValue->setFocus();
					return;
				}

				temp = ui.zAxisInductanceValue->text().toDouble(&ok);
				if (ok)
					z.inductance = temp;
				else
				{
					showError("Invalid Z-axis Inductance value, please check.");	// error
					ui.zAxisInductanceValue->setFocus();
					return;
				}

				if (ui.zAxisSwitchInstalledButton->isChecked())
					z.switchInstalled = true;
				else
					z.switchInstalled = false;

				if (z.switchInstalled)
				{
					temp = ui.zAxisSwitchCurrentValue->text().toDouble(&ok);
					if (ok)
						z.switchHeaterCurrent = temp;
					else
					{
						showError("Invalid Z-axis Switch Heater Current value, please check.");	// error
						ui.zAxisSwitchCurrentValue->setFocus();
						return;
					}

					temp = ui.zAxisSwitchHeatTimeValue->text().toDouble(&ok);
					if (ok)
						z.switchHeatingTime = (int)temp;
					else
					{
						showError("Invalid Z-axis Switch Heating Time value, please check.");	// error
						ui.zAxisSwitchHeatTimeValue->setFocus();
						return;
					}

					temp = ui.zAxisSwitchCoolTimeValue->text().toDouble(&ok);
					if (ok)
						z.switchCoolingTime = (int)temp;
					else
					{
						showError("Invalid Z-axis Switch Cooling Time value, please check.");	// error
						ui.zAxisSwitchCoolTimeValue->setFocus();
						return;
					}
				}
			}

			// everything is good in the dialog, save, accept and close
			save();
			emit accept();
		}
		else
		{
			// cancel button pressed, discard data
			emit reject();
		}
	}
}

//---------------------------------------------------------------------------
// saves the dialog state to the registry
//---------------------------------------------------------------------------
void MagnetParams::save(void)
{
	QSettings settings;

	settings.setValue("MagnetParams/HaveSaved", true);
	settings.setValue("MagnetParams/MagnetID", ui.magnetIDEdit->text());
	settings.setValue("MagnetParams/MagnitudeLimit", ui.magnitudeLimitEdit->text());

	// X-axis settings
	settings.setValue("MagnetParams/X/Enabled", ui.xAxisGroupBox->isChecked());

	if (ui.xAxisGroupBox->isChecked())
	{
		settings.setValue("MagnetParams/X/IPAddr", ui.xAxisIPValue->text());
		settings.setValue("MagnetParams/X/CurrentLimit", ui.xAxisCurrentLimitValue->text());
		settings.setValue("MagnetParams/X/VoltageLimit", ui.xAxisVoltageLimitValue->text());
		settings.setValue("MagnetParams/X/MaxRampRate", ui.xAxisMaxRampRateValue->text());
		settings.setValue("MagnetParams/X/CoilConst", ui.xAxisCoilConstValue->text());
		settings.setValue("MagnetParams/X/Inductance", ui.xAxisInductanceValue->text());
		settings.setValue("MagnetParams/X/SwitchInstalled", ui.xAxisSwitchInstalledButton->isChecked());

		if (ui.xAxisSwitchInstalledButton->isChecked())
		{
			settings.setValue("MagnetParams/X/SwitchHeaterCurr", ui.xAxisSwitchCurrentValue->text());
			settings.setValue("MagnetParams/X/SwitchHeatTime", ui.xAxisSwitchHeatTimeValue->text());
			settings.setValue("MagnetParams/X/SwitchCoolTime", ui.xAxisSwitchCoolTimeValue->text());
		}
	}

	// Y-axis settings
	settings.setValue("MagnetParams/Y/Enabled", ui.yAxisGroupBox->isChecked());

	if (ui.yAxisGroupBox->isChecked())
	{
		settings.setValue("MagnetParams/Y/IPAddr", ui.yAxisIPValue->text());
		settings.setValue("MagnetParams/Y/CurrentLimit", ui.yAxisCurrentLimitValue->text());
		settings.setValue("MagnetParams/Y/VoltageLimit", ui.yAxisVoltageLimitValue->text());
		settings.setValue("MagnetParams/Y/MaxRampRate", ui.yAxisMaxRampRateValue->text());
		settings.setValue("MagnetParams/Y/CoilConst", ui.yAxisCoilConstValue->text());
		settings.setValue("MagnetParams/Y/Inductance", ui.yAxisInductanceValue->text());
		settings.setValue("MagnetParams/Y/SwitchInstalled", ui.yAxisSwitchInstalledButton->isChecked());

		if (ui.yAxisSwitchInstalledButton->isChecked())
		{
			settings.setValue("MagnetParams/Y/SwitchHeaterCurr", ui.yAxisSwitchCurrentValue->text());
			settings.setValue("MagnetParams/Y/SwitchHeatTime", ui.yAxisSwitchHeatTimeValue->text());
			settings.setValue("MagnetParams/Y/SwitchCoolTime", ui.yAxisSwitchCoolTimeValue->text());
		}
	}

	// Z-axis settings
	settings.setValue("MagnetParams/Z/Enabled", ui.zAxisGroupBox->isChecked());

	if (ui.zAxisGroupBox->isChecked())
	{
		settings.setValue("MagnetParams/Z/IPAddr", ui.zAxisIPValue->text());
		settings.setValue("MagnetParams/Z/CurrentLimit", ui.zAxisCurrentLimitValue->text());
		settings.setValue("MagnetParams/Z/VoltageLimit", ui.zAxisVoltageLimitValue->text());
		settings.setValue("MagnetParams/Z/MaxRampRate", ui.zAxisMaxRampRateValue->text());
		settings.setValue("MagnetParams/Z/CoilConst", ui.zAxisCoilConstValue->text());
		settings.setValue("MagnetParams/Z/Inductance", ui.zAxisInductanceValue->text());
		settings.setValue("MagnetParams/Z/SwitchInstalled", ui.zAxisSwitchInstalledButton->isChecked());

		if (ui.zAxisSwitchInstalledButton->isChecked())
		{
			settings.setValue("MagnetParams/Z/SwitchHeaterCurr", ui.zAxisSwitchCurrentValue->text());
			settings.setValue("MagnetParams/Z/SwitchHeatTime", ui.zAxisSwitchHeatTimeValue->text());
			settings.setValue("MagnetParams/Z/SwitchCoolTime", ui.zAxisSwitchCoolTimeValue->text());
		}
	}
}

//---------------------------------------------------------------------------
// restores the dialog state from the registry
//---------------------------------------------------------------------------
void MagnetParams::restore(void)
{
	QSettings settings;
	QString tempStr;

	if (settings.value("MagnetParams/HaveSaved").toBool())
	{
		tempStr = settings.value("MagnetParams/MagnetID").toString();
		ui.magnetIDEdit->setText(tempStr);
		magnetID = tempStr;

		tempStr = settings.value("MagnetParams/MagnitudeLimit").toString();
		ui.magnitudeLimitEdit->setText(tempStr);
		magnitudeLimit = tempStr.toDouble();

		// restore X axis params
		if (x.activate = settings.value("MagnetParams/X/Enabled").toBool())
		{
			tempStr = settings.value("MagnetParams/X/IPAddr").toString();
			ui.xAxisIPValue->setText(tempStr);
			x.ipAddress = tempStr;

			tempStr = settings.value("MagnetParams/X/CurrentLimit").toString();
			ui.xAxisCurrentLimitValue->setText(tempStr);
			x.currentLimit = tempStr.toDouble();

			tempStr = settings.value("MagnetParams/X/VoltageLimit").toString();
			ui.xAxisVoltageLimitValue->setText(tempStr);
			x.voltageLimit = tempStr.toDouble();

			tempStr = settings.value("MagnetParams/X/MaxRampRate").toString();
			ui.xAxisMaxRampRateValue->setText(tempStr);
			x.maxRampRate = tempStr.toDouble();

			tempStr = settings.value("MagnetParams/X/CoilConst").toString();
			ui.xAxisCoilConstValue->setText(tempStr);
			x.coilConst = tempStr.toDouble();

			tempStr = settings.value("MagnetParams/X/Inductance").toString();
			ui.xAxisInductanceValue->setText(tempStr);
			x.inductance = tempStr.toDouble();

			if (settings.value("MagnetParams/X/SwitchInstalled").toBool())
			{
				x.switchInstalled = true;
				xAxisSwitchUiConfig(true, true);

				tempStr = settings.value("MagnetParams/X/SwitchHeaterCurr").toString();
				ui.xAxisSwitchCurrentValue->setText(tempStr);
				x.switchHeaterCurrent = tempStr.toDouble();

				tempStr = settings.value("MagnetParams/X/SwitchHeatTime").toString();
				ui.xAxisSwitchHeatTimeValue->setText(tempStr);
				x.switchHeatingTime = (int)(tempStr.toDouble());

				tempStr = settings.value("MagnetParams/X/SwitchCoolTime").toString();
				ui.xAxisSwitchCoolTimeValue->setText(tempStr);
				x.switchCoolingTime = (int)(tempStr.toDouble());
			}
			else
			{
				x.switchInstalled = false;
				xAxisSwitchUiConfig(false, true);
				x.switchHeaterCurrent = 0;
				x.switchHeatingTime = 0;
				x.switchCoolingTime = 0;
			}
		}
		else
		{
			ui.xAxisGroupBox->setChecked(false);
			x.ipAddress = "";
			x.currentLimit = 0;
			x.voltageLimit = 0;
			x.maxRampRate = 0;
			x.coilConst = 0;
			x.inductance = 0;
		}

		// restore Y axis params
		if (y.activate = settings.value("MagnetParams/Y/Enabled").toBool())
		{
			tempStr = settings.value("MagnetParams/Y/IPAddr").toString();
			ui.yAxisIPValue->setText(tempStr);
			y.ipAddress = tempStr;

			tempStr = settings.value("MagnetParams/Y/CurrentLimit").toString();
			ui.yAxisCurrentLimitValue->setText(tempStr);
			y.currentLimit = tempStr.toDouble();

			tempStr = settings.value("MagnetParams/Y/VoltageLimit").toString();
			ui.yAxisVoltageLimitValue->setText(tempStr);
			y.voltageLimit = tempStr.toDouble();

			tempStr = settings.value("MagnetParams/Y/MaxRampRate").toString();
			ui.yAxisMaxRampRateValue->setText(tempStr);
			y.maxRampRate = tempStr.toDouble();

			tempStr = settings.value("MagnetParams/Y/CoilConst").toString();
			ui.yAxisCoilConstValue->setText(tempStr);
			y.coilConst = tempStr.toDouble();

			tempStr = settings.value("MagnetParams/Y/Inductance").toString();
			ui.yAxisInductanceValue->setText(tempStr);
			y.inductance = tempStr.toDouble();

			if (settings.value("MagnetParams/Y/SwitchInstalled").toBool())
			{
				y.switchInstalled = true;
				yAxisSwitchUiConfig(true, true);

				tempStr = settings.value("MagnetParams/Y/SwitchHeaterCurr").toString();
				ui.yAxisSwitchCurrentValue->setText(tempStr);
				y.switchHeaterCurrent = tempStr.toDouble();

				tempStr = settings.value("MagnetParams/Y/SwitchHeatTime").toString();
				ui.yAxisSwitchHeatTimeValue->setText(tempStr);
				y.switchHeatingTime = (int)(tempStr.toDouble());

				tempStr = settings.value("MagnetParams/Y/SwitchCoolTime").toString();
				ui.yAxisSwitchCoolTimeValue->setText(tempStr);
				y.switchCoolingTime = (int)(tempStr.toDouble());
			}
			else
			{
				y.switchInstalled = false;
				yAxisSwitchUiConfig(false, true);
				y.switchHeaterCurrent = 0;
				y.switchHeatingTime = 0;
				y.switchCoolingTime = 0;
			}
		}
		else
		{
			ui.yAxisGroupBox->setChecked(false);
			y.ipAddress = "";
			y.currentLimit = 0;
			y.voltageLimit = 0;
			y.maxRampRate = 0;
			y.coilConst = 0;
			y.inductance = 0;
		}

		// restore Z axis params
		if (z.activate = settings.value("MagnetParams/Z/Enabled").toBool())
		{
			tempStr = settings.value("MagnetParams/Z/IPAddr").toString();
			ui.zAxisIPValue->setText(tempStr);
			z.ipAddress = tempStr;

			tempStr = settings.value("MagnetParams/Z/CurrentLimit").toString();
			ui.zAxisCurrentLimitValue->setText(tempStr);
			z.currentLimit = tempStr.toDouble();

			tempStr = settings.value("MagnetParams/Z/VoltageLimit").toString();
			ui.zAxisVoltageLimitValue->setText(tempStr);
			z.voltageLimit = tempStr.toDouble();

			tempStr = settings.value("MagnetParams/Z/MaxRampRate").toString();
			ui.zAxisMaxRampRateValue->setText(tempStr);
			z.maxRampRate = tempStr.toDouble();

			tempStr = settings.value("MagnetParams/Z/CoilConst").toString();
			ui.zAxisCoilConstValue->setText(tempStr);
			z.coilConst = tempStr.toDouble();

			tempStr = settings.value("MagnetParams/Z/Inductance").toString();
			ui.zAxisInductanceValue->setText(tempStr);
			z.inductance = tempStr.toDouble();

			if (settings.value("MagnetParams/Z/SwitchInstalled").toBool())
			{
				z.switchInstalled = true;
				zAxisSwitchUiConfig(true, true);

				tempStr = settings.value("MagnetParams/Z/SwitchHeaterCurr").toString();
				ui.zAxisSwitchCurrentValue->setText(tempStr);
				z.switchHeaterCurrent = tempStr.toDouble();

				tempStr = settings.value("MagnetParams/Z/SwitchHeatTime").toString();
				ui.zAxisSwitchHeatTimeValue->setText(tempStr);
				z.switchHeatingTime = (int)(tempStr.toDouble());

				tempStr = settings.value("MagnetParams/Z/SwitchCoolTime").toString();
				ui.zAxisSwitchCoolTimeValue->setText(tempStr);
				z.switchCoolingTime = (int)(tempStr.toDouble());
			}
			else
			{
				z.switchInstalled = false;
				zAxisSwitchUiConfig(false, true);
				z.switchHeaterCurrent = 0;
				z.switchHeatingTime = 0;
				z.switchCoolingTime = 0;
			}
		}
		else
		{
			ui.zAxisGroupBox->setChecked(false);
			z.ipAddress = "";
			z.currentLimit = 0;
			z.voltageLimit = 0;
			z.maxRampRate = 0;
			z.coilConst = 0;
			z.inductance = 0;
		}
	}
	else
	{
		magnetID = "";
		magnitudeLimit = 0;
		x.ipAddress = "";
		x.currentLimit = 0;
		x.voltageLimit = 0;
		x.maxRampRate = 0;
		x.coilConst = 0;
		x.inductance = 0;
		x.switchInstalled = false;
		x.switchHeaterCurrent = 0;
		x.switchHeatingTime = 0;
		x.switchCoolingTime = 0;
		y.ipAddress = "";
		y.currentLimit = 0;
		y.voltageLimit = 0;
		y.maxRampRate = 0;
		y.coilConst = 0;
		y.inductance = 0;
		y.switchInstalled = false;
		y.switchHeaterCurrent = 0;
		y.switchHeatingTime = 0;
		y.switchCoolingTime = 0;
		z.ipAddress = "";
		z.currentLimit = 0;
		z.voltageLimit = 0;
		z.maxRampRate = 0;
		z.coilConst = 0;
		z.inductance = 0;
		z.switchInstalled = false;
		z.switchHeaterCurrent = 0;
		z.switchHeatingTime = 0;
		z.switchCoolingTime = 0;
	}
}

//---------------------------------------------------------------------------
// syncs the UI with parameters loaded from a SAV file
//---------------------------------------------------------------------------
void MagnetParams::syncUI(void)
{
	QString tempStr;

	ui.magnetIDEdit->setText(magnetID);
	ui.magnitudeLimitEdit->setText(tempStr.setNum(magnitudeLimit));

	// update UI for loaded settings
	if (x.switchInstalled)
		xAxisSwitchUiConfig(true, true);
	else
		xAxisSwitchUiConfig(false, true);

	if (y.switchInstalled)
		yAxisSwitchUiConfig(true, true);
	else
		yAxisSwitchUiConfig(false, true);

	if (z.switchInstalled)
		zAxisSwitchUiConfig(true, true);
	else
		zAxisSwitchUiConfig(false, true);

	// sync X axis UI
	if (x.activate)
	{
		ui.xAxisGroupBox->setChecked(true);
		ui.xAxisIPValue->setText(x.ipAddress);
		ui.xAxisCurrentLimitValue->setText(tempStr.setNum(x.currentLimit));
		ui.xAxisVoltageLimitValue->setText(tempStr.setNum(x.voltageLimit));
		ui.xAxisMaxRampRateValue->setText(tempStr.setNum(x.maxRampRate));
		ui.xAxisCoilConstValue->setText(tempStr.setNum(x.coilConst));
		ui.xAxisInductanceValue->setText(tempStr.setNum(x.inductance));

		if (x.switchInstalled)
		{
			ui.xAxisSwitchCurrentValue->setText(tempStr.setNum(x.switchHeaterCurrent));
			ui.xAxisSwitchHeatTimeValue->setText(tempStr.setNum(x.switchHeatingTime));
			ui.xAxisSwitchCoolTimeValue->setText(tempStr.setNum(x.switchCoolingTime));
		}
	}
	else
	{
		ui.xAxisGroupBox->setChecked(false);
		ui.xAxisIPValue->clear();
		ui.xAxisCurrentLimitValue->clear();
		ui.xAxisVoltageLimitValue->clear();
		ui.xAxisMaxRampRateValue->clear();
		ui.xAxisCoilConstValue->clear();
		ui.xAxisInductanceValue->clear();
	}

	// sync Y axis UI
	if (y.activate)
	{
		ui.yAxisGroupBox->setChecked(true);
		ui.yAxisIPValue->setText(y.ipAddress);
		ui.yAxisCurrentLimitValue->setText(tempStr.setNum(y.currentLimit));
		ui.yAxisVoltageLimitValue->setText(tempStr.setNum(y.voltageLimit));
		ui.yAxisMaxRampRateValue->setText(tempStr.setNum(y.maxRampRate));
		ui.yAxisCoilConstValue->setText(tempStr.setNum(y.coilConst));
		ui.yAxisInductanceValue->setText(tempStr.setNum(y.inductance));


		if (y.switchInstalled)
		{
			ui.yAxisSwitchCurrentValue->setText(tempStr.setNum(y.switchHeaterCurrent));
			ui.yAxisSwitchHeatTimeValue->setText(tempStr.setNum(y.switchHeatingTime));
			ui.yAxisSwitchCoolTimeValue->setText(tempStr.setNum(y.switchCoolingTime));
		}
	}
	else
	{
		ui.yAxisGroupBox->setChecked(false);
		ui.yAxisIPValue->clear();
		ui.yAxisCurrentLimitValue->clear();
		ui.yAxisVoltageLimitValue->clear();
		ui.yAxisMaxRampRateValue->clear();
		ui.yAxisCoilConstValue->clear();
		ui.yAxisInductanceValue->clear();
	}

	// sync Z axis UI
	if (z.activate)
	{
		ui.zAxisGroupBox->setChecked(true);
		ui.zAxisIPValue->setText(z.ipAddress);
		ui.zAxisCurrentLimitValue->setText(tempStr.setNum(z.currentLimit));
		ui.zAxisVoltageLimitValue->setText(tempStr.setNum(z.voltageLimit));
		ui.zAxisMaxRampRateValue->setText(tempStr.setNum(z.maxRampRate));
		ui.zAxisCoilConstValue->setText(tempStr.setNum(z.coilConst));
		ui.zAxisInductanceValue->setText(tempStr.setNum(z.inductance));

		if (z.switchInstalled)
		{
			ui.zAxisSwitchCurrentValue->setText(tempStr.setNum(z.switchHeaterCurrent));
			ui.zAxisSwitchHeatTimeValue->setText(tempStr.setNum(z.switchHeatingTime));
			ui.zAxisSwitchCoolTimeValue->setText(tempStr.setNum(z.switchCoolingTime));
		}
	}
	else
	{
		ui.zAxisGroupBox->setChecked(false);
		ui.zAxisIPValue->clear();
		ui.zAxisCurrentLimitValue->clear();
		ui.zAxisVoltageLimitValue->clear();
		ui.zAxisMaxRampRateValue->clear();
		ui.zAxisCoilConstValue->clear();
		ui.zAxisInductanceValue->clear();
	}
}

//---------------------------------------------------------------------------
