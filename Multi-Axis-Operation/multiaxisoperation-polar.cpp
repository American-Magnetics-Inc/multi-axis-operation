#include "stdafx.h"
#include "multiaxisoperation.h"
#include "conversions.h" 

const double RAD_TO_DEG = 180.0 / M_PI;

//---------------------------------------------------------------------------
// Contains methods related to the Rotation in Alignment Plane tab view.
// Broken out from multiaxisoperation.cpp for ease of editing.
//---------------------------------------------------------------------------

void MultiAxisOperation::setNormalUnitVector(QVector3D *v1, QVector3D *v2)
{
	crossResult = QVector3D::crossProduct(*v1, *v2);
	crossResult.normalize();

	// reference vector for rotation in plane is alignment vector #1
	QVector3D referenceVector;
	referenceVector.setX(v1->x());
	referenceVector.setY(v1->y());
	referenceVector.setZ(v1->z());
	referenceVector.normalize();

	referenceQuaternion.setVector(referenceVector);
	referenceQuaternion.setScalar(0.0);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::polarToCartesian(double magnitude, double angle, QVector3D *conversion)
{
	double angleRad = angle / RAD_TO_DEG;

	// specify the rotation quaternion based on the crossResult
	QVector3D rotationVector;
	rotationVector.setX(sin(angleRad / 2.0) * crossResult.x());
	rotationVector.setY(sin(angleRad / 2.0) * crossResult.y());
	rotationVector.setZ(sin(angleRad / 2.0) * crossResult.z());

	rotationQuaternion.setVector(rotationVector);
	rotationQuaternion.setScalar(cos(angleRad / 2.0));

	// calculate polar rotation in sample alignment plane
	QQuaternion result = (rotationQuaternion * referenceQuaternion) * rotationQuaternion.conjugate();
	
	// load conversion vector
	conversion->setX(result.x());
	conversion->setY(result.y());
	conversion->setZ(result.z());
}

//---------------------------------------------------------------------------
// alternative (faster?) form; also use as a check
void MultiAxisOperation::altPolarToCartesian(double magnitude, double angle, QVector3D *conversion)
{
	double angleRad = angle / RAD_TO_DEG;

	QVector3D u(sin(angleRad / 2.0) * crossResult.x(), sin(angleRad / 2.0) * crossResult.y(), sin(angleRad / 2.0) * crossResult.z());
	QVector3D v(referenceQuaternion.x(), referenceQuaternion.y(), referenceQuaternion.z());

	// Scalar part of the quaternion
	double s = cos(angleRad / 2.0);

	// Do the math
	QVector3D vprime = 2.0 * QVector3D::dotProduct(u, v) * u + (s * s - QVector3D::dotProduct(u, u)) * v + 2.0 * s * QVector3D::crossProduct(u, v);

	// load conversion vector
	conversion->setX(vprime.x());
	conversion->setY(vprime.y());
	conversion->setZ(vprime.z());
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionLoad_Polar_Table(void)
{
	QSettings settings;
	int skipCnt = 0;
	lastPolarLoadPath = settings.value("LastPolarFilePath").toString();
	bool convertFieldUnits = false;

	if (autostepPolarTimer->isActive())
		return;	// no load during autostepping

	polarFileName = QFileDialog::getOpenFileName(this, "Choose Polar File", lastPolarLoadPath, "Polar Definition Files (*.txt *.log *.csv)");

	if (!polarFileName.isEmpty())
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);

		lastPolarLoadPath = polarFileName;

		FILE *inFile;
		inFile = fopen(polarFileName.toLocal8Bit(), "r");

		if (inFile != NULL)
		{
			QTextStream in(inFile);

			// now scan for units in header
			QString str = in.readLine();

			// convert to all caps and remove all leading/trailing whitespace
			str = str.toUpper();
			str = str.trimmed();

			if (str.contains("(KG)") || str.contains("(KILOGAUSS)"))
			{
				if (fieldUnits == TESLA)
					convertFieldUnits = true;
			}
			else if (str.contains("(T)") || str.contains("(TESLA)"))
			{
				if (fieldUnits == KG)
					convertFieldUnits = true;
			}

			fclose(inFile);
		}

		// clear status text
		statusMisc->clear();

		// if ramping, set system to PAUSE
		actionPause();

		// now read in data
		ui.polarTableWidget->loadFromFile(polarFileName, false, 1);

		// save path
		settings.setValue("LastPolarFilePath", lastPolarLoadPath);

		presentPolar = -1;	// no selection

		// set headings as appropriate
		setPolarTableHeader();

		if (convertFieldUnits)
		{
			convertPolarFieldValues(fieldUnits);
		}

		QApplication::restoreOverrideCursor();
		polarSelectionChanged();
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::convertPolarFieldValues(FieldUnits newUnits)
{
	// loop through polar table and convert magnitude only
	int numRows = ui.polarTableWidget->rowCount();

	for (int i = 0; i < numRows; i++)
	{
		QTableWidgetItem *cell = ui.polarTableWidget->item(i, 0);
		QString cellStr = cell->text();

		if (!cellStr.isEmpty())
		{
			// try to convert to number
			bool ok;
			double value = cellStr.toDouble(&ok);

			if (ok)
			{
				if (newUnits == TESLA)	// convert from KG
					value /= 10.0;
				else if (newUnits == KG)
					value *= 10.0;

				cell->setText(QString::number(value, 'g', 10));
			}
		}
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::setPolarTableHeader(void)
{
	if (fieldUnits == KG)
	{
		ui.polarTableWidget->horizontalHeaderItem(0)->setText("Mag (kG)");
	}
	else
	{
		ui.polarTableWidget->horizontalHeaderItem(0)->setText("Mag (T)");
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionSave_Polar_Table(void)
{
	QSettings settings;
	lastPolarSavePath = settings.value("LastPolarSavePath").toString();

	savePolarFileName = QFileDialog::getSaveFileName(this, "Save Polar File", lastPolarSavePath, "Run Test Files (*.txt *.log *.csv)");

	if (!savePolarFileName.isEmpty())
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);

		lastPolarSavePath = savePolarFileName;

		// save table contents
		ui.polarTableWidget->saveToFile(savePolarFileName);

		// save path
		settings.setValue("LastPolarSavePath", lastPolarSavePath);

		QApplication::restoreOverrideCursor();
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::polarSelectionChanged(void)
{
	if (autostepPolarTimer->isActive())
	{
		// no table mods during autostep!
		ui.polarAddRowAboveToolButton->setEnabled(false);
		ui.polarAddRowBelowToolButton->setEnabled(false);
		ui.polarRemoveRowToolButton->setEnabled(false);
		ui.polarTableClearToolButton->setEnabled(false);
	}
	else
	{
		// any selected vectors?
		QList<QTableWidgetItem *> list = ui.polarTableWidget->selectedItems();

		if (list.count())
		{
			ui.polarAddRowAboveToolButton->setEnabled(true);
			ui.polarAddRowBelowToolButton->setEnabled(true);
			ui.polarRemoveRowToolButton->setEnabled(true);
		}
		else
		{
			if (ui.polarTableWidget->rowCount())
			{
				ui.polarAddRowAboveToolButton->setEnabled(false);
				ui.polarAddRowBelowToolButton->setEnabled(false);
			}
			else
			{
				ui.polarAddRowAboveToolButton->setEnabled(true);
				ui.polarAddRowBelowToolButton->setEnabled(true);
			}

			ui.polarRemoveRowToolButton->setEnabled(false);
		}

		if (ui.polarTableWidget->rowCount())
			ui.polarTableClearToolButton->setEnabled(true);
		else
			ui.polarTableClearToolButton->setEnabled(false);
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::polarTableAddRowAbove(void)
{
	int newRow = -1;

	// find selected vector
	QList<QTableWidgetItem *> list = ui.polarTableWidget->selectedItems();

	if (list.count())
	{
		int currentRow = list[0]->row();
		ui.polarTableWidget->insertRow(currentRow);
		newRow = currentRow;
	}
	else
	{
		if (ui.polarTableWidget->rowCount() == 0)
		{
			ui.polarTableWidget->insertRow(0);
			newRow = 0;
		}
	}

	if (newRow > -1)
	{
		initNewPolarRow(newRow);
		updatePresentPolar(newRow, false);
		polarSelectionChanged();
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::polarTableAddRowBelow(void)
{
	int newRow = -1;

	// find selected vector
	QList<QTableWidgetItem *> list = ui.polarTableWidget->selectedItems();

	if (list.count())
	{
		int currentRow = list[0]->row();
		ui.polarTableWidget->insertRow(currentRow + 1);
		newRow = currentRow + 1;
	}
	else
	{
		if (ui.polarTableWidget->rowCount() == 0)
		{
			ui.polarTableWidget->insertRow(0);
			newRow = 0;
		}
	}

	if (newRow > -1)
	{
		initNewPolarRow(newRow);
		updatePresentPolar(newRow, false);
		polarSelectionChanged();
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::initNewPolarRow(int newRow)
{
	int numColumns = ui.polarTableWidget->columnCount();

	// initialize cells
	for (int i = 0; i < numColumns; ++i)
	{
		QTableWidgetItem *cell = ui.polarTableWidget->item(newRow, i);

		if (cell == NULL)
			ui.polarTableWidget->setItem(newRow, i, cell = new QTableWidgetItem(""));
		else
			cell->setText("");

		cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::updatePresentPolar(int row, bool removed)
{
	if (targetSource == POLAR_TABLE)
	{
		if (removed)
		{
			if (presentPolar == row)
			{
				setStatusMsg("");
				presentPolar = lastPolar = -1;
			}
			else if (row < presentPolar)
			{
				// selection shift up by one
				presentPolar--;
				lastPolar = presentPolar;
				setStatusMsg("Target Vector : Polar Table #" + QString::number(presentPolar + 1));
			}
		}
		else
		{
			if (row <= presentPolar)
			{
				// selection shifted down by one
				presentPolar++;
				lastPolar = presentPolar;
				setStatusMsg("Target Vector : Polar Table #" + QString::number(presentPolar + 1));
			}
		}
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::polarTableRemoveRow(void)
{
	// find selected vector
	QList<QTableWidgetItem *> list = ui.polarTableWidget->selectedItems();

	if (list.count())
	{
		int currentRow = list[0]->row();
		ui.polarTableWidget->removeRow(currentRow);
		updatePresentPolar(currentRow, true);
		polarSelectionChanged();
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::polarTableClear(void)
{
	int numRows = ui.polarTableWidget->rowCount();

	if (numRows)
	{
		for (int i = numRows; i > 0; i--)
			ui.polarTableWidget->removeRow(i - 1);
	}

	presentPolar = lastPolar = -1;
	setStatusMsg("");
	polarSelectionChanged();
}

//---------------------------------------------------------------------------
void MultiAxisOperation::goToSelectedPolarVector(void)
{
	if (connected)
	{
		// deactivate any alignment vectors
		deactivateAlignmentVectors();

		// find selected vector
		QList<QTableWidgetItem *> list = ui.polarTableWidget->selectedItems();

		if (list.count())
		{
			presentPolar = list[0]->row();
			magnetState = RAMPING;
			systemState = SYSTEM_RAMPING;
			goToPolarVector();
		}
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::goToNextPolarVector(void)
{
	if (connected)
	{
		// deactivate any alignment vectors
		deactivateAlignmentVectors();

		// find selected vector
		QList<QTableWidgetItem *> list = ui.polarTableWidget->selectedItems();

		if (list.count())
		{
			presentPolar = list[0]->row();

			// is next vector a valid request?
			if (((presentPolar + 1) >= 1) && ((presentPolar + 1) < ui.polarTableWidget->rowCount()))
			{
				presentPolar++;	// choose next vector

				// highlight row in table
				ui.polarTableWidget->selectRow(presentPolar);

				magnetState = RAMPING;
				systemState = SYSTEM_RAMPING;
				goToPolarVector();
			}
		}
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::goToPolarVector(void)
{
	double coord1, coord2;
	bool ok, error = false;
	double temp;

	// get vector values and check for numerical conversion
	temp = ui.polarTableWidget->item(presentPolar, 0)->text().toDouble(&ok);
	if (ok)
	{
		coord1 = temp;

		if (coord1 < 0.0)	// magnitude cannot be negative
		{
			error = true;
			vectorError = NEGATIVE_MAGNITUDE;
			showErrorString("Polar Vector #" + QString::number(presentPolar + 1) + " has negative magnitude");	// error annunciation
			abortPolarAutostep();
			return;
		}
	}
	else
		error = true;	// error

	temp = ui.polarTableWidget->item(presentPolar, 1)->text().toDouble(&ok);
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

		// attempt to go to vector
		if ((vectorError = checkNextVector(vector.x(), vector.y(), vector.z(), "Polar Table #" + QString::number(presentPolar + 1))) == NO_VECTOR_ERROR)
		{
			sendNextVector(vector.x(), vector.y(), vector.z());
			targetSource = POLAR_TABLE;
			lastPolar = presentPolar;

			if (autostepPolarTimer->isActive())
			{
				setStatusMsg("Auto-Stepping : Polar Table #" + QString::number(presentPolar + 1));
			}
			else
			{
				setStatusMsg("Target Vector : Polar Table #" + QString::number(presentPolar + 1));
			}
		}
		else
		{
			abortPolarAutostep();
		}
	}
	else
	{
		vectorError = NON_NUMERICAL_ENTRY;
		showErrorString("Polar Table #" + QString::number(presentPolar + 1) + " has non-numerical entry");	// error annunciation
		abortPolarAutostep();
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::startPolarAutostep(void)
{
	if (connected)
	{
		// deactivate any alignment vectors
		deactivateAlignmentVectors();

		autostepStartIndexPolar = ui.startIndexEditPolar->text().toInt();
		autostepEndIndexPolar = ui.endIndexEditPolar->text().toInt();

		if (autostepStartIndexPolar < 1 || autostepStartIndexPolar > ui.polarTableWidget->rowCount())
		{
			showErrorString("Starting Polar Index is out of range!");
			return;
		}

		if (autostepEndIndexPolar <= autostepStartIndexPolar || autostepEndIndexPolar > ui.polarTableWidget->rowCount())
		{
			showErrorString("Ending Polar Index is out of range!");
			return;
		}

		ui.startIndexEditPolar->setEnabled(false);
		ui.endIndexEditPolar->setEnabled(false);

		elapsedTimerTicksPolar = 0;
		autostepPolarTimer->start();

		ui.autostepStartButtonPolar->setEnabled(false);
		ui.autostartStopButtonPolar->setEnabled(true);
		ui.manualPolarControlGroupBox->setEnabled(false);
		ui.actionLoad_Polar_Table->setEnabled(false);

		ui.manualVectorControlGroupBox->setEnabled(false);
		ui.autoStepGroupBox->setEnabled(false);
		ui.actionLoad_Vector_Table->setEnabled(false);

		if (switchInstalled)
			ui.actionPersistentMode->setEnabled(false);

		// begin with first vector
		presentPolar = autostepStartIndexPolar - 1;

		// highlight row in table
		ui.polarTableWidget->selectRow(presentPolar);
		magnetState = RAMPING;
		systemState = SYSTEM_RAMPING;
		polarSelectionChanged(); // lockout row changes

		goToPolarVector();
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::abortPolarAutostep(void)
{
	if (autostepPolarTimer->isActive())	// first checks for active autostep sequence
	{
		autostepPolarTimer->stop();
		while (errorStatusIsActive)	// show any error condition first
		{
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
			Sleep(100);
		}
		setStatusMsg("Polar Auto-Stepping aborted due to an error with Polar Vector #" + QString::number(presentPolar + 1));
		ui.startIndexEditPolar->setEnabled(true);
		ui.endIndexEditPolar->setEnabled(true);
		ui.autostepStartButtonPolar->setEnabled(true);
		ui.autostartStopButtonPolar->setEnabled(false);
		ui.manualPolarControlGroupBox->setEnabled(true);
		ui.actionLoad_Polar_Table->setEnabled(true);

		ui.manualVectorControlGroupBox->setEnabled(true);
		ui.autoStepGroupBox->setEnabled(true);
		ui.actionLoad_Vector_Table->setEnabled(true);
		if (switchInstalled)
			ui.actionPersistentMode->setEnabled(true);

		polarSelectionChanged();
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::stopPolarAutostep(void)
{
	if (autostepPolarTimer->isActive())
	{
		autostepPolarTimer->stop();
		setStatusMsg("Polar Auto-Stepping aborted via Stop button");
		ui.startIndexEditPolar->setEnabled(true);
		ui.endIndexEditPolar->setEnabled(true);
		ui.autostepStartButtonPolar->setEnabled(true);
		ui.autostartStopButtonPolar->setEnabled(false);
		ui.manualPolarControlGroupBox->setEnabled(true);
		ui.actionLoad_Polar_Table->setEnabled(true);

		ui.manualVectorControlGroupBox->setEnabled(true);
		ui.autoStepGroupBox->setEnabled(true);
		ui.actionLoad_Vector_Table->setEnabled(true);
		if (switchInstalled)
			ui.actionPersistentMode->setEnabled(true);

		polarSelectionChanged();
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::autostepPolarTimerTick(void)
{
	if (magnetState == HOLDING)
	{
		elapsedTimerTicksPolar++;

		bool ok;
		double temp;

		// get time
		temp = ui.polarTableWidget->item(presentPolar, 2)->text().toDouble(&ok);

		if (ok)
		{
			if (elapsedTimerTicksPolar >= (int)temp)
			{
				elapsedTimerTicksPolar = 0;

				if (presentPolar + 1 < autostepEndIndexPolar)
				{
					// highlight row in table
					presentPolar++;
					ui.polarTableWidget->selectRow(presentPolar);
					magnetState = RAMPING;
					systemState = SYSTEM_RAMPING;

					// next vector!
					goToPolarVector();
				}
				else
				{
					// successfully completed polar table auto-stepping
					setStatusMsg("Polar Auto-Step Completed @ Polar Vector #" + QString::number(presentPolar + 1));
					autostepPolarTimer->stop();
					ui.startIndexEditPolar->setEnabled(true);
					ui.endIndexEditPolar->setEnabled(true);
					ui.autostepStartButtonPolar->setEnabled(true);
					ui.autostartStopButtonPolar->setEnabled(false);
					ui.manualPolarControlGroupBox->setEnabled(true);
					polarSelectionChanged();

					ui.manualVectorControlGroupBox->setEnabled(true);
					ui.autoStepGroupBox->setEnabled(true);
					ui.actionLoad_Vector_Table->setEnabled(true);
					if (switchInstalled)
						ui.actionPersistentMode->setEnabled(true);
				}
			}
			else
			{
				// update the HOLDING countdown
				QString tempStr = statusMisc->text();
				int index = tempStr.indexOf('(');
				if (index >= 1)
					tempStr.truncate(index - 1);

				QString timeStr = " (" + QString::number(temp - elapsedTimerTicksPolar) + " sec remaining)";
				setStatusMsg(tempStr + timeStr);
			}
		}
		else
		{
			autostepPolarTimer->stop();
			setStatusMsg("Polar Auto-Stepping aborted due to unknown dwell time on line #" + QString::number(presentPolar + 1));
			ui.startIndexEditPolar->setEnabled(true);
			ui.endIndexEditPolar->setEnabled(true);
			ui.autostepStartButtonPolar->setEnabled(true);
			ui.autostartStopButtonPolar->setEnabled(false);
			ui.manualPolarControlGroupBox->setEnabled(true);
			polarSelectionChanged();

			ui.manualVectorControlGroupBox->setEnabled(true);
			ui.autoStepGroupBox->setEnabled(true);
			ui.actionLoad_Vector_Table->setEnabled(true);
			if (switchInstalled)
				ui.actionPersistentMode->setEnabled(true);
		}
	}
	else
	{
		elapsedTimerTicksPolar = 0;

		// remove any erroneous countdown text
		QString tempStr = statusMisc->text();
		int index = tempStr.indexOf('(');
		if (index >= 1)
			tempStr.truncate(index - 1);

		setStatusMsg(tempStr);
	}
}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------