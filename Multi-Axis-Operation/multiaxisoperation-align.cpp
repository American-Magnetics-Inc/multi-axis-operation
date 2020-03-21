#include "stdafx.h"
#include "multiaxisoperation.h"
#include "conversions.h"

const double SCALING_FACTOR = 1.0e7;
QPalette groupBoxPalette;


//---------------------------------------------------------------------------
// Contains methods related to the Sample Alignment tab view.
// Broken out from multiaxisoperation.cpp for ease of editing.
//---------------------------------------------------------------------------
void MultiAxisOperation::alignmentTabInitState(void)
{
	// init variables
	mag1ShadowVal = 0;
	theta1Dial = 0;
	theta1ShadowVal = 0;
	phi1Dial = 0;
	phi1ShadowVal = 0;
	mag2ShadowVal = 0;
	theta2Dial = 0;
	theta2ShadowVal = 0;
	phi2Dial = 0;
	phi2ShadowVal = 0;

	// restore alignment vector last settings
	QSettings settings;

	ui.alignMagValueSpinBox1->setValue(settings.value("Alignment/Align1Magnitude").toDouble());
	ui.alignThetaSpinBox1->setValue(settings.value("Alignment/Align1Theta").toDouble());
	ui.alignPhiSpinBox1->setValue(settings.value("Alignment/Align1Phi").toDouble());
	ui.alignMagValueSpinBox2->setValue(settings.value("Alignment/Align2Magnitude").toDouble());
	ui.alignThetaSpinBox2->setValue(settings.value("Alignment/Align2Theta").toDouble());
	ui.alignPhiSpinBox2->setValue(settings.value("Alignment/Align2Phi").toDouble());

	groupBoxPalette = ui.alignGroupBox1->palette();
	ui.alignGroupBox1->setAutoFillBackground(true);
	ui.alignGroupBox2->setAutoFillBackground(true);

	// set shadow values
	mag1ShadowVal = ui.alignMagValueSpinBox1->value();
	theta1ShadowVal = ui.alignThetaSpinBox1->value();
	phi1ShadowVal = ui.alignPhiSpinBox1->value();
	mag2ShadowVal = ui.alignMagValueSpinBox2->value();
	theta2ShadowVal = ui.alignThetaSpinBox2->value();
	phi2ShadowVal = ui.alignPhiSpinBox2->value();

	// init normal vector
	recalculateRotationVector();
}

//---------------------------------------------------------------------------
void MultiAxisOperation::alignmentTabSaveState(void)
{
	// save alignment vector last settings
	QSettings settings;

	settings.setValue("Alignment/Align1Magnitude", ui.alignMagValueSpinBox1->value());
	settings.setValue("Alignment/Align1Theta", ui.alignThetaSpinBox1->value());
	settings.setValue("Alignment/Align1Phi", ui.alignPhiSpinBox1->value());
	settings.setValue("Alignment/Align2Magnitude", ui.alignMagValueSpinBox2->value());
	settings.setValue("Alignment/Align2Theta", ui.alignThetaSpinBox2->value());
	settings.setValue("Alignment/Align2Phi", ui.alignPhiSpinBox2->value());
}

//---------------------------------------------------------------------------
void MultiAxisOperation::alignmentTabLoadFromStream(QTextStream *stream)
{
	ui.alignMagValueSpinBox1->setValue(stream->readLine().toDouble());
	ui.alignThetaSpinBox1->setValue(stream->readLine().toDouble());
	ui.alignPhiSpinBox1->setValue(stream->readLine().toDouble());
	ui.alignMagValueSpinBox2->setValue(stream->readLine().toDouble());
	ui.alignThetaSpinBox2->setValue(stream->readLine().toDouble());
	ui.alignPhiSpinBox2->setValue(stream->readLine().toDouble());

	// set shadow values
	mag1ShadowVal = ui.alignMagValueSpinBox1->value();
	theta1ShadowVal = ui.alignThetaSpinBox1->value();
	phi1ShadowVal = ui.alignPhiSpinBox1->value();
	mag2ShadowVal = ui.alignMagValueSpinBox2->value();
	theta2ShadowVal = ui.alignThetaSpinBox2->value();
	phi2ShadowVal = ui.alignPhiSpinBox2->value();

	// re-initialize interface
	makeAlignVector1Active(false);
	makeAlignVector2Active(false);

	recalculateRotationVector();
}

//---------------------------------------------------------------------------
void MultiAxisOperation::alignmentTabSaveToStream(QTextStream *stream)
{
	*stream << ui.alignMagValueSpinBox1->value() << "\n";
	*stream << ui.alignThetaSpinBox1->value() << "\n";
	*stream << ui.alignPhiSpinBox1->value() << "\n";
	*stream << ui.alignMagValueSpinBox2->value() << "\n";
	*stream << ui.alignThetaSpinBox2->value() << "\n";
	*stream << ui.alignPhiSpinBox2->value() << "\n";
}

//---------------------------------------------------------------------------
void MultiAxisOperation::convertAlignmentFieldValues(FieldUnits newUnits)
{
	double temp1 = ui.alignMagValueSpinBox1->value();
	double temp2 = ui.alignMagValueSpinBox2->value();

	if (newUnits == TESLA)	// convert from KG
	{
		temp1 /= 10.0;
		temp2 /= 10.0;
	}
	else if (newUnits == KG) // convert from T
	{
		temp1 *= 10.0;
		temp2 *= 10.0;
	}

	// update the interface
	ui.alignMagValueSpinBox1->setValue(temp1);
	ui.alignMagValueSpinBox2->setValue(temp2);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::deactivateAlignmentVectors(void)
{
	if (ui.makeAlignActiveButton1->isChecked())
	{
		ui.makeAlignActiveButton1->setChecked(false);
		makeAlignVector1Active(false);
	}
	if (ui.makeAlignActiveButton2->isChecked())
	{
		ui.makeAlignActiveButton2->setChecked(false);
		makeAlignVector2Active(false);
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::alignmentTabConnect(void)
{
	if (!ui.actionPersistentMode->isChecked())
	{
		ui.makeAlignActiveButton1->setChecked(false);
		ui.makeAlignActiveButton1->setEnabled(true);
		ui.makeAlignActiveButton2->setChecked(false);
		ui.makeAlignActiveButton2->setEnabled(true);
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::alignmentTabDisconnect(void)
{
	ui.makeAlignActiveButton1->setChecked(false);
	ui.makeAlignActiveButton1->setEnabled(false);
	ui.makeAlignActiveButton2->setChecked(false);
	ui.makeAlignActiveButton2->setEnabled(false);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::recalculateRotationVector(void)
// if active status is cleared, then recalculate the rotation vector
{
	bool error = false;
	QVector3D v1, v2;
	double x, y, z;

	error = checkAlignVector1();

	if (!error)
	{
		sphericalToCartesian(mag1ShadowVal, theta1ShadowVal, phi1ShadowVal, &x, &y, &z);
		v1.setX(x);
		v1.setY(y);
		v1.setZ(z);

		error = checkAlignVector2();

		if (!error)
		{
			sphericalToCartesian(mag2ShadowVal, theta2ShadowVal, phi2ShadowVal, &x, &y, &z);
			v2.setX(x);
			v2.setY(y);
			v2.setZ(z);

			setNormalUnitVector(&v1, &v2);
		}
	}
}

//---------------------------------------------------------------------------
// Alignment Vector #1 methods
//---------------------------------------------------------------------------
void MultiAxisOperation::alignVector1LockPressed(bool checked)
{
	if (checked)
	{
		ui.alignMagValueSpinBox1->setEnabled(true);
		ui.alignThetaSpinBox1->setEnabled(true);
		ui.alignThetaDial1->setEnabled(true);
		ui.alignPhiSpinBox1->setEnabled(true);
		ui.alignPhiDial1->setEnabled(true);
	}
	else
	{
		ui.alignMagValueSpinBox1->setEnabled(false);
		ui.alignThetaSpinBox1->setEnabled(false);
		ui.alignThetaDial1->setEnabled(false);
		ui.alignPhiSpinBox1->setEnabled(false);
		ui.alignPhiDial1->setEnabled(false);
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::makeAlignVector1Active(bool checked)
{
	if (ui.makeAlignActiveButton2->isChecked())
	{
		ui.makeAlignActiveButton2->setChecked(false);
		ui.alignGroupBox2->setPalette(groupBoxPalette);
#if defined(Q_OS_MACOS)
        ui.alignGroupBox1->repaint();
#endif
	}

	if (ui.makeAlignActiveButton1->isChecked())
	{
		// cancel any active auto-step
		if (autostepTimer->isActive())
			abortAutostep("");
		if (autostepPolarTimer->isActive())
			abortPolarAutostep("");

		sendAlignVector1();
		ui.alignGroupBox1->setPalette(QPalette(Qt::white, QRgb(0xD6DBE9)));
#if defined(Q_OS_MACOS)
        ui.alignGroupBox1->repaint();
#endif
	}
	else
	{
		statusMisc->clear();
		ui.alignGroupBox1->setPalette(groupBoxPalette);

		recalculateRotationVector();
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::alignVector1ValueChanged(double value)
{
	if (ui.makeAlignActiveButton1->isChecked())
		sendAlignVector1();
	else
		recalculateRotationVector();

	if (qobject_cast<QDoubleSpinBox *>(sender()) == ui.alignMagValueSpinBox1)
		mag1ShadowVal = ui.alignMagValueSpinBox1->value();

	if (qobject_cast<QDoubleSpinBox *>(sender()) == ui.alignThetaSpinBox1)
		theta1ShadowVal = ui.alignThetaSpinBox1->value();

	if (qobject_cast<QDoubleSpinBox *>(sender()) == ui.alignPhiSpinBox1)
		phi1ShadowVal = ui.alignPhiSpinBox1->value();
}

//---------------------------------------------------------------------------
void MultiAxisOperation::alignVector1ThetaDialChanged(int value)
{
	int delta = value - theta1Dial;

	// check for a rollover, if so determine direction
	if (abs(delta) > ui.alignThetaDial1->maximum() / 2)
	{
		if (delta > 0)
			delta = -(theta1Dial + ui.alignThetaDial1->maximum() - value);
		else
			delta = ui.alignThetaDial1->maximum() - theta1Dial + value;
	}

	// square value for velocity sensitive
	if (delta > 0)
		delta *= delta;
	else
		delta = -(delta * delta);

	// calculate the delta floating point change
	theta1ShadowVal += (double)delta / SCALING_FACTOR;
	ui.alignThetaSpinBox1->setValue(theta1ShadowVal);

	// save state
	theta1Dial = value;
}

//---------------------------------------------------------------------------
void MultiAxisOperation::alignVector1PhiDialChanged(int value)
{
	int delta = value - phi1Dial;

	// check for a rollover, if so determine direction
	if (abs(delta) > ui.alignPhiDial1->maximum() / 2)
	{
		if (delta > 0)
			delta = -(phi1Dial + ui.alignPhiDial1->maximum() - value);
		else
			delta = ui.alignPhiDial1->maximum() - phi1Dial + value;
	}

	// square value for velocity sensitive
	if (delta > 0)
		delta *= delta;
	else
		delta = -(delta * delta);

	// calculate the delta floating point change
	phi1ShadowVal += (double)delta / SCALING_FACTOR;
	ui.alignPhiSpinBox1->setValue(phi1ShadowVal);

	// save state
	phi1Dial = value;
}

//---------------------------------------------------------------------------
// return true if error
bool MultiAxisOperation::checkAlignVector1(void)
{
	bool ok, error = false;
	double temp;

	// get vector value and check for numerical conversion
	temp = ui.alignMagValueSpinBox1->text().toDouble(&ok);
	if (!ok)
	{
		error = true;	// error
	}
	else if (temp < 0.0)	// magnitude cannot be negative
	{
		showErrorString("Alignment Vector #1: Magnitude cannot be a negative value");	// error annunciation
		error = true;
		return error;
	}
	else
		mag1ShadowVal = temp;

	temp = ui.alignThetaSpinBox1->text().toDouble(&ok);
	if (!ok)
		error = true;	// error
	else
		theta1ShadowVal = temp;

	temp = ui.alignPhiSpinBox1->text().toDouble(&ok);
	if (!ok)
	{
		error = true;	// error
	}
	else if (temp < 0.0 || temp > 180.0)	// angle from Z-axis must be >= 0 and <= 180 degrees
	{
		showErrorString("Alignment Vector #1: Angle from Z-axis must be from 0 to 180 degrees");	// error annunciation
		error = true;
		return error;
	}
	else
		phi1ShadowVal = temp;

	if (error)
	{
		showErrorString("Alignment Vector #1 has non-numerical entry");	// error annunciation
	}

	return error;
}

//---------------------------------------------------------------------------
void MultiAxisOperation::sendAlignVector1(void)
{
	if (!checkAlignVector1())
	{
		double x, y, z;

		sphericalToCartesian(mag1ShadowVal, theta1ShadowVal, phi1ShadowVal, &x, &y, &z);

		// attempt to go to vector
		if ((vectorError = checkNextVector(x, y, z, "Alignment Vector #1")) == NO_VECTOR_ERROR)
		{
			sendNextVector(x, y, z);
			targetSource = ALIGN_TAB;
			lastTargetMsg = "Target Vector : Alignment Vector #1";
			setStatusMsg(lastTargetMsg);
		}
	}
}

//---------------------------------------------------------------------------
// Alignment Vector #2 methods
//---------------------------------------------------------------------------
void MultiAxisOperation::alignVector2LockPressed(bool checked)
{
	if (checked)
	{
		ui.alignMagValueSpinBox2->setEnabled(true);
		ui.alignThetaSpinBox2->setEnabled(true);
		ui.alignThetaDial2->setEnabled(true);
		ui.alignPhiSpinBox2->setEnabled(true);
		ui.alignPhiDial2->setEnabled(true);
	}
	else
	{
		ui.alignMagValueSpinBox2->setEnabled(false);
		ui.alignThetaSpinBox2->setEnabled(false);
		ui.alignThetaDial2->setEnabled(false);
		ui.alignPhiSpinBox2->setEnabled(false);
		ui.alignPhiDial2->setEnabled(false);
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::makeAlignVector2Active(bool checked)
{
	if (ui.makeAlignActiveButton1->isChecked())
	{
		ui.makeAlignActiveButton1->setChecked(false);
		ui.alignGroupBox1->setPalette(groupBoxPalette);
#if defined(Q_OS_MACOS)
        ui.alignGroupBox1->repaint();
#endif
	}

	if (ui.makeAlignActiveButton2->isChecked())
	{
		// cancel any active auto-step
		if (autostepTimer->isActive())
			abortAutostep("");
		if (autostepPolarTimer->isActive())
			abortPolarAutostep("");

		sendAlignVector2();
		ui.alignGroupBox2->setPalette(QPalette(Qt::white, QRgb(0xD6DBE9)));
#if defined(Q_OS_MACOS)
        ui.alignGroupBox1->repaint();
#endif
	}
	else
	{
		statusMisc->clear();
		ui.alignGroupBox2->setPalette(groupBoxPalette);

		recalculateRotationVector();
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::alignVector2ValueChanged(double value)
{
	if (ui.makeAlignActiveButton2->isChecked())
		sendAlignVector2();
	else
		recalculateRotationVector();

	if (qobject_cast<QDoubleSpinBox *>(sender()) == ui.alignMagValueSpinBox2)
		mag2ShadowVal = ui.alignMagValueSpinBox2->value();

	if (qobject_cast<QDoubleSpinBox *>(sender()) == ui.alignThetaSpinBox2)
		theta2ShadowVal = ui.alignThetaSpinBox2->value();

	if (qobject_cast<QDoubleSpinBox *>(sender()) == ui.alignPhiSpinBox2)
		phi2ShadowVal = ui.alignPhiSpinBox2->value();
}

//---------------------------------------------------------------------------
void MultiAxisOperation::alignVector2ThetaDialChanged(int value)
{
	int delta = value - theta2Dial;

	// check for a rollover, if so determine direction
	if (abs(delta) > ui.alignThetaDial2->maximum() / 2)
	{
		if (delta > 0)
			delta = -(theta2Dial + ui.alignThetaDial2->maximum() - value);
		else
			delta = ui.alignThetaDial2->maximum() - theta2Dial + value;
	}

	// square value for velocity sensitive
	if (delta > 0)
		delta *= delta;
	else
		delta = -(delta * delta);

	// calculate the delta floating point change
	theta2ShadowVal += (double)delta / SCALING_FACTOR;
	ui.alignThetaSpinBox2->setValue(theta2ShadowVal);

	// save state
	theta2Dial = value;
}

//---------------------------------------------------------------------------
void MultiAxisOperation::alignVector2PhiDialChanged(int value)
{
	int delta = value - phi2Dial;

	// check for a rollover, if so determine direction
	if (abs(delta) > ui.alignPhiDial2->maximum() / 2)
	{
		if (delta > 0)
			delta = -(phi2Dial + ui.alignPhiDial2->maximum() - value);
		else
			delta = ui.alignPhiDial2->maximum() - phi2Dial + value;
	}

	// square value for velocity sensitive
	if (delta > 0)
		delta *= delta;
	else
		delta = -(delta * delta);

	// calculate the delta floating point change
	phi2ShadowVal += (double)delta / SCALING_FACTOR;
	ui.alignPhiSpinBox2->setValue(phi2ShadowVal);

	// save state
	phi2Dial = value;
}

//---------------------------------------------------------------------------
// return true if error
bool MultiAxisOperation::checkAlignVector2(void)
{
	bool ok, error = false;
	double temp;

	// get vector value and check for numerical conversion
	temp = ui.alignMagValueSpinBox2->text().toDouble(&ok);
	if (!ok)
	{
		error = true;	// error
	}
	else if (temp < 0.0)	// magnitude cannot be negative
	{
		showErrorString("Alignment Vector #2: Magnitude cannot be a negative value");	// error annunciation
		error = true;
		return error;
	}
	else
		mag2ShadowVal = temp;

	temp = ui.alignThetaSpinBox2->text().toDouble(&ok);
	if (!ok)
		error = true;	// error
	else
		theta2ShadowVal = temp;

	temp = ui.alignPhiSpinBox2->text().toDouble(&ok);
	if (!ok)
	{
		error = true;	// error
	}
	else if (temp < 0.0 || temp > 180.0)	// angle from Z-axis must be >= 0 and <= 180 degrees
	{
		showErrorString("Alignment Vector #2: Angle from Z-axis must be from 0 to 180 degrees");	// error annunciation
		error = true;
		return error;
	}
	else
		phi2ShadowVal = temp;

	if (error)
	{
		showErrorString("Alignment Vector #2 has non-numerical entry");	// error annunciation
	}

	return error;
}

//---------------------------------------------------------------------------
void MultiAxisOperation::sendAlignVector2(void)
{
	if (!checkAlignVector2())
	{
		double x, y, z;

		sphericalToCartesian(mag2ShadowVal, theta2ShadowVal, phi2ShadowVal, &x, &y, &z);

		// attempt to go to vector
		if ((vectorError = checkNextVector(x, y, z, "Alignment Vector #2")) == NO_VECTOR_ERROR)
		{
			sendNextVector(x, y, z);
			targetSource = ALIGN_TAB;
			lastTargetMsg = "Target Vector : Alignment Vector #2";
			setStatusMsg(lastTargetMsg);
		}
	}
}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------