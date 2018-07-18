#include "stdafx.h"
#include "multiaxisoperation.h"
#include "conversions.h"


//---------------------------------------------------------------------------
// Contains methods related to the stdin/stdout parser thread.
// We need several custom SLOTS to which to connect between the main
// GUI thread and the parser thread.
// Broken out from multiaxisoperation.cpp for ease of editing.
//---------------------------------------------------------------------------

void MultiAxisOperation::system_connect(void)
{
	ui.actionConnect->setChecked(true);
	actionConnect();
}

//---------------------------------------------------------------------------
void MultiAxisOperation::system_disconnect(void)
{
	ui.actionConnect->setChecked(false);
	actionConnect();
}

//---------------------------------------------------------------------------
void MultiAxisOperation::load_settings(FILE *file, bool *success)
{
	loadFromFile(file);

	// sync the UI with the newly loaded values
	magnetParams->syncUI();
	magnetParams->save();

	updateWindowTitle();
	vectorSelectionChanged();
	polarSelectionChanged();

	fclose(file);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::save_settings(FILE *file, bool *success)
{
	saveToFile(file);

	fclose(file);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::set_units(int value)
{
	setFieldUnits((FieldUnits)value, true);
	convertFieldValues((FieldUnits)value, true);
	convertAlignmentFieldValues((FieldUnits)value);
	convertPolarFieldValues((FieldUnits)value);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::set_align1(double mag, double azimuth, double inclination)
{
	ui.alignMagValueSpinBox1->setValue(mag);
	ui.alignThetaSpinBox1->setValue(azimuth);
	ui.alignPhiSpinBox1->setValue(inclination);

	ui.makeAlignActiveButton1->setChecked(true);
	makeAlignVector1Active(true);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::set_align1_active(void)
{
	ui.makeAlignActiveButton1->setChecked(true);
	makeAlignVector1Active(true);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::get_align1(double *mag, double *azimuth, double *inclination)
{
	*mag = mag1ShadowVal;
	*azimuth = theta1ShadowVal;
	*inclination = phi1ShadowVal;
}

//---------------------------------------------------------------------------
void MultiAxisOperation::get_align1_cartesian(double *x, double *y, double *z)
{
	sphericalToCartesian(mag1ShadowVal, theta1ShadowVal, phi1ShadowVal, x, y, z);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::set_align2(double mag, double azimuth, double inclination)
{
	ui.alignMagValueSpinBox2->setValue(mag);
	ui.alignThetaSpinBox2->setValue(azimuth);
	ui.alignPhiSpinBox2->setValue(inclination);

	ui.makeAlignActiveButton2->setChecked(true);
	makeAlignVector2Active(true);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::set_align2_active(void)
{
	ui.makeAlignActiveButton2->setChecked(true);
	makeAlignVector2Active(true);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::get_align2(double *mag, double *azimuth, double *inclination)
{
	*mag = mag2ShadowVal;
	*azimuth = theta2ShadowVal;
	*inclination = phi2ShadowVal;
}

//---------------------------------------------------------------------------
void MultiAxisOperation::get_align2_cartesian(double *x, double *y, double *z)
{
	sphericalToCartesian(mag2ShadowVal, theta2ShadowVal, phi2ShadowVal, x, y, z);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::get_active(double *mag, double *azimuth, double *inclination)
{
	cartesianToSpherical(xTarget, yTarget, zTarget, mag, azimuth, inclination);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::get_active_cartesian(double *x, double *y, double *z)
{
	*x = xTarget;
	*y = yTarget;
	*z = zTarget;
}

//---------------------------------------------------------------------------
void MultiAxisOperation::get_field(double *mag, double *azimuth, double *inclination)
{
	*mag = magnitudeField;
	*azimuth = thetaAngle;
	*inclination = phiAngle;
}

//---------------------------------------------------------------------------
void MultiAxisOperation::get_field_cartesian(double *x, double *y, double *z)
{
	*x = xField;
	*y = yField;
	*z = zField;
}

//---------------------------------------------------------------------------
void MultiAxisOperation::get_plane(double *x, double *y, double *z)
{
	*x = crossResult.x();
	*y = crossResult.y();
	*z = crossResult.z();
}

//---------------------------------------------------------------------------
VectorError MultiAxisOperation::check_vector(double x, double y, double z)
{
	if (magnetParams->GetXAxisParams()->activate)
	{
		double checkValue = fabs(x / magnetParams->GetXAxisParams()->coilConst);

		if (checkValue > magnetParams->GetXAxisParams()->currentLimit)
			return EXCEEDS_X_RANGE;
	}
	else
	{
		// can't achieve the vector as there is no X-axis activated
		if (fabs(x) > 0.0)
			return INACTIVE_X_AXIS;
	}

	if (magnetParams->GetYAxisParams()->activate)
	{
		double checkValue = fabs(y / magnetParams->GetYAxisParams()->coilConst);

		if (checkValue > magnetParams->GetYAxisParams()->currentLimit)
			return EXCEEDS_Y_RANGE;
	}
	else
	{
		// can't achieve the vector as there is no Y-axis activated
		if (fabs(y) > 0.0)
			return INACTIVE_Y_AXIS;
	}

	if (magnetParams->GetZAxisParams()->activate)
	{
		double checkValue = fabs(z / magnetParams->GetZAxisParams()->coilConst);

		if (checkValue > magnetParams->GetZAxisParams()->currentLimit)
			return EXCEEDS_Z_RANGE;
	}
	else
	{
		// can't achieve the vector as there is no Z-axis activated
		if (fabs(z) > 0.0)
			return INACTIVE_Z_AXIS;
	}

	{
		// check magnitude
		double temp = sqrt(x * x + y * y + z * z);

		if (temp > magnetParams->getMagnitudeLimit())
			return EXCEEDS_MAGNITUDE_LIMIT;
	}

	return NO_VECTOR_ERROR;		// vector is good!
}

//---------------------------------------------------------------------------
void MultiAxisOperation::set_vector(double mag, double az, double inc, int time)
{
	int newRow = ui.vectorsTableWidget->rowCount();
	ui.vectorsTableWidget->insertRow(newRow);
	initNewRow(newRow);

	if (loadedCoordinates == SPHERICAL_COORDINATES)
	{
		// set the spherical coordinate values for new row
		ui.vectorsTableWidget->item(newRow, 0)->setText(QString::number(mag, 'g', 10));
		ui.vectorsTableWidget->item(newRow, 1)->setText(QString::number(az, 'g', 10));
		ui.vectorsTableWidget->item(newRow, 2)->setText(QString::number(inc, 'g', 10));
		ui.vectorsTableWidget->item(newRow, 3)->setText(QString::number(time));
	}
	else if (loadedCoordinates == CARTESIAN_COORDINATES)
	{
		double x, y, z;
		sphericalToCartesian(mag, az, inc, &x, &y, &z);

		// set the Cartesian coordinate values for new row
		ui.vectorsTableWidget->item(newRow, 0)->setText(QString::number(x, 'g', 10));
		ui.vectorsTableWidget->item(newRow, 1)->setText(QString::number(y, 'g', 10));
		ui.vectorsTableWidget->item(newRow, 2)->setText(QString::number(z, 'g', 10));
		ui.vectorsTableWidget->item(newRow, 3)->setText(QString::number(time));
	}

	// make present vector and goto
	ui.vectorsTableWidget->selectRow(newRow);
	goToSelectedVector();
}

//---------------------------------------------------------------------------
void MultiAxisOperation::set_vector_cartesian(double x, double y, double z, int time)
{
	int newRow = ui.vectorsTableWidget->rowCount();
	ui.vectorsTableWidget->insertRow(newRow);
	initNewRow(newRow);

	if (loadedCoordinates == SPHERICAL_COORDINATES)
	{
		double mag, az, inc;
		cartesianToSpherical(x, y, z, &mag, &az, &inc);

		// set the spherical coordinate values for new row
		ui.vectorsTableWidget->item(newRow, 0)->setText(QString::number(mag, 'g', 10));
		ui.vectorsTableWidget->item(newRow, 1)->setText(QString::number(az, 'g', 10));
		ui.vectorsTableWidget->item(newRow, 2)->setText(QString::number(inc, 'g', 10));
		ui.vectorsTableWidget->item(newRow, 3)->setText(QString::number(time));
	}
	else if (loadedCoordinates == CARTESIAN_COORDINATES)
	{
		// set the Cartesian coordinate values for new row
		ui.vectorsTableWidget->item(newRow, 0)->setText(QString::number(x, 'g', 10));
		ui.vectorsTableWidget->item(newRow, 1)->setText(QString::number(y, 'g', 10));
		ui.vectorsTableWidget->item(newRow, 2)->setText(QString::number(z, 'g', 10));
		ui.vectorsTableWidget->item(newRow, 3)->setText(QString::number(time));
	}

	// make present vector and goto
	ui.vectorsTableWidget->selectRow(newRow);
	goToSelectedVector();
}

//---------------------------------------------------------------------------
bool MultiAxisOperation::vec_table_row_in_range(int tableRow)
{
	if (tableRow >= 0 && tableRow < ui.vectorsTableWidget->rowCount())
		return true;

	return false;
}

//---------------------------------------------------------------------------
VectorError MultiAxisOperation::check_vector_table(int tableRow)
{
	double coord1, coord2, coord3;
	double x, y, z;
	bool ok, error = false;
	double temp;

	// get vector values and check for numerical conversion
	temp = ui.vectorsTableWidget->item(tableRow, 0)->text().toDouble(&ok);
	if (ok)
		coord1 = temp;
	else
		error = true;	// error

	temp = ui.vectorsTableWidget->item(tableRow, 1)->text().toDouble(&ok);
	if (ok)
		coord2 = temp;
	else
		error = true;	// error

	temp = ui.vectorsTableWidget->item(tableRow, 2)->text().toDouble(&ok);
	if (ok)
		coord3 = temp;
	else
		error = true;	// error

	if (!error)
	{
		if (loadedCoordinates == CARTESIAN_COORDINATES)
		{
			x = coord1;
			y = coord2;
			z = coord3;
		}
		else if (loadedCoordinates == SPHERICAL_COORDINATES)
		{
			// check input values for errors
			if (coord1 < 0.0)	// magnitude cannot be negative
			{
				return NEGATIVE_MAGNITUDE;
			}
			if (coord3 < 0.0 || coord3 > 180.0)	// angle from Z-axis must be >= 0 and <= 180 degrees
			{
				return INCLINATION_OUT_OF_RANGE;
			}

			sphericalToCartesian(coord1, coord2, coord3, &x, &y, &z);
		}

		// check vector
		return check_vector(x, y, z);
	}
	else
	{
		return NON_NUMERICAL_ENTRY;
	}

	return NO_VECTOR_ERROR;
}

//---------------------------------------------------------------------------
void MultiAxisOperation::goto_vector(int tableRow)
{
	if (tableRow >= 0 && tableRow < ui.vectorsTableWidget->rowCount())
	{
		ui.vectorsTableWidget->selectRow(tableRow);
		goToSelectedVector();
	}
	else
	{
		; // tableRow out of range (should already be checked)
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::set_polar(double mag, double angle, int time)
{
	int newRow = ui.polarTableWidget->rowCount();
	ui.polarTableWidget->insertRow(newRow);
	initNewPolarRow(newRow);

	// set the polar coordinate values for new row
	ui.polarTableWidget->item(newRow, 0)->setText(QString::number(mag, 'g', 10));
	ui.polarTableWidget->item(newRow, 1)->setText(QString::number(angle, 'g', 10));
	ui.polarTableWidget->item(newRow, 2)->setText(QString::number(time));

	// make present vector and goto
	ui.polarTableWidget->selectRow(newRow);
	goToSelectedPolarVector();
}

//---------------------------------------------------------------------------
void MultiAxisOperation::goto_polar(int tableRow)
{
	if (tableRow >= 0 && tableRow < ui.polarTableWidget->rowCount())
	{
		ui.polarTableWidget->selectRow(tableRow);
		goToSelectedPolarVector();
	}
	else
	{
		; // tableRow out of range
	}
}

//---------------------------------------------------------------------------
bool MultiAxisOperation::polar_table_row_in_range(int tableRow)
{
	if (tableRow >= 0 && tableRow < ui.polarTableWidget->rowCount())
		return true;

	return false;
}

//---------------------------------------------------------------------------
VectorError MultiAxisOperation::check_polar_table(int tableRow)
{
	double coord1, coord2;
	bool ok, error = false;
	double temp;

	// get vector values and check for numerical conversion
	temp = ui.polarTableWidget->item(tableRow, 0)->text().toDouble(&ok);
	if (ok)
	{
		coord1 = temp;

		if (coord1 < 0.0)	// magnitude cannot be negative
		{
			error = true;
			return NEGATIVE_MAGNITUDE;
		}
	}
	else
		error = true;	// error

	temp = ui.polarTableWidget->item(tableRow, 1)->text().toDouble(&ok);
	if (ok)
		coord2 = temp;
	else
		error = true;	// error

	if (!error)
	{
		// get polar vector in magnet axes coordinates
		QVector3D vector;

		polarToCartesian(coord1, coord2, &vector);
		vector *= coord1;	// multiply by magnitude

		// check vector
		return check_vector(vector.x(), vector.y(), vector.z());
	}
	else
	{
		return NON_NUMERICAL_ENTRY;
	}

	return NO_VECTOR_ERROR;
}

//---------------------------------------------------------------------------
void MultiAxisOperation::set_persistence(bool persistent)
{
	if (switchInstalled)
	{
		if (persistent)
		{
			ui.actionPersistentMode->setChecked(true);

			if (switchHeatingTimer->isActive())
				switchHeatingTimer->stop();
		}
		else
		{
			ui.actionPersistentMode->setChecked(false);

			if (switchCoolingTimer->isActive())
				switchCoolingTimer->stop();
		}

		actionPersistentMode();
	}
}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------