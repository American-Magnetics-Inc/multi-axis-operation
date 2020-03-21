#include "stdafx.h"
#include "multiaxisoperation.h"
#include "conversions.h"

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
#include <unistd.h>
#endif

//---------------------------------------------------------------------------
// Local constants and static variables.
//---------------------------------------------------------------------------
enum PolarAutostepStates
{
	POLAR_TABLE_RAMPING_TO_NEXT_VECTOR = 0,
	POLAR_TABLE_SETTLING_AT_VECTOR,
	POLAR_TABLE_COOLING_SWITCH,
	POLAR_TABLE_HOLDING,
	POLAR_TABLE_HEATING_SWITCH,
	POLAR_TABLE_NEXT_VECTOR
};

const double RAD_TO_DEG = 180.0 / M_PI;
static int settlingTime = 0;
PolarAutostepStates polarAutostepState;	// state machine for autostep
static bool tableIsLoading = false;

//---------------------------------------------------------------------------
// Contains methods related to the Rotation in Alignment Plane tab view.
// Broken out from multiaxisoperation.cpp for ease of editing.
//---------------------------------------------------------------------------
void MultiAxisOperation::restorePolarTab(QSettings *settings)
{
	// restore any persistent values
	ui.executePolarCheckBox->setChecked(settings->value("PolarTable/EnableExecution", false).toBool());
	ui.polarAppLocationEdit->setText(settings->value("PolarTable/AppPath", "").toString());
	ui.polarAppArgsEdit->setText(settings->value("PolarTable/AppArgs", "").toString());
	ui.polarPythonPathEdit->setText(settings->value("PolarTable/PythonPath", "").toString());
	ui.polarAppStartEdit->setText(settings->value("PolarTable/ExecutionTime", "").toString());
	ui.polarPythonCheckBox->setChecked(settings->value("PolarTable/PythonScript", false).toBool());

	// init app/script execution
	polarAppCheckBoxChanged(0);

	// make connections
	connect(ui.executePolarCheckBox, SIGNAL(stateChanged(int)), this, SLOT(polarAppCheckBoxChanged(int)));
	connect(ui.polarPythonCheckBox, SIGNAL(stateChanged(int)), this, SLOT(polarPythonCheckBoxChanged(int)));
	connect(ui.polarAppLocationButton, SIGNAL(clicked()), this, SLOT(browseForPolarAppPath()));
	connect(ui.polarPythonLocationButton, SIGNAL(clicked()), this, SLOT(browseForPolarPythonPath()));
	connect(ui.executePolarNowButton, SIGNAL(clicked()), this, SLOT(executePolarNowClick()));
	connect(ui.polarTableWidget, SIGNAL(itemChanged(QTableWidgetItem*)), this, SLOT(polarTableItemChanged(QTableWidgetItem*)));

	setPolarTableHeader();
}

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

    *conversion *= magnitude;	// multiply by magnitude
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

    *conversion *= magnitude;	// multiply by magnitude
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionLoad_Polar_Table(void)
{
	QSettings settings;
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

        if (inFile != nullptr)
		{
			QTextStream in(inFile);

			tableIsLoading = true;

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

		// clear any prior data
		polarTableClear();

		// clear status text
		statusMisc->clear();

		// if ramping, set system to PAUSE
		actionPause();

		// now read in data
		if (magnetParams->switchInstalled())
		{
			ui.polarTableWidget->loadFromFile(polarFileName, 1);

			// set persistence if switched
			if (optionsDialog->enterPersistence())
				setPolarTablePersistence(true);
			else
				setPolarTablePersistence(false);
		}
		else
			ui.polarTableWidget->loadFromFile(polarFileName, 1);

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
		tableIsLoading = false;
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
		ui.polarTableWidget->horizontalHeaderItem(0)->setText("Magnitude (kG)");
	}
	else
	{
		ui.polarTableWidget->horizontalHeaderItem(0)->setText("Magnitude (T)");
	}

	// set hold time format
	if (magnetParams->switchInstalled())
		ui.polarTableWidget->horizontalHeaderItem(2)->setText("Enter Persistence?/\nHold Time (sec)");
	else
		ui.polarTableWidget->horizontalHeaderItem(2)->setText("Hold Time (sec)");
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
void MultiAxisOperation::recalculateRemainingPolarTime(void)
{
	if (autostepPolarTimer->isActive())
		calculatePolarRemainingTime(presentPolar + 1, autostepEndIndexPolar);
	else
		polarRangeChanged();
}

//---------------------------------------------------------------------------
void MultiAxisOperation::polarTableItemChanged(QTableWidgetItem *item)
{
	// recalculate time after change and check for errors
	if (!tableIsLoading)
		recalculateRemainingPolarTime();
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
		ui.polarPswitchToggleToolButton->setEnabled(false);
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
		{
			ui.polarTableClearToolButton->setEnabled(true);

			if ((switchInstalled = magnetParams->switchInstalled()))
				ui.polarPswitchToggleToolButton->setEnabled(true);
		}
		else
		{
			ui.polarTableClearToolButton->setEnabled(false);
			ui.polarPswitchToggleToolButton->setEnabled(false);
		}
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

        if (cell == nullptr)
			ui.polarTableWidget->setItem(newRow, i, cell = new QTableWidgetItem(""));
		else
			cell->setText("");

		if (i == 2 && magnetParams->switchInstalled())
			cell->setCheckState(Qt::Unchecked);

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
				lastTargetMsg.clear();
				setStatusMsg("");
				presentPolar = lastPolar = -1;
			}
			else if (row < presentPolar)
			{
				// selection shift up by one
				presentPolar--;
				lastPolar = presentPolar;
				lastTargetMsg = "Target Vector : Polar Table #" + QString::number(presentPolar + 1);
				setStatusMsg(lastTargetMsg);
			}
		}
		else
		{
			if (row <= presentPolar)
			{
				// selection shifted down by one
				presentPolar++;
				lastPolar = presentPolar;
				lastTargetMsg = "Target Vector : Polar Table #" + QString::number(presentPolar + 1);
				setStatusMsg(lastTargetMsg);
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
	lastTargetMsg.clear();
	setStatusMsg("");
	polarSelectionChanged();
}

//---------------------------------------------------------------------------
// Set persistence for all entries in table.
void MultiAxisOperation::setPolarTablePersistence(bool state)
{
	int numRows = ui.polarTableWidget->rowCount();

	if (numRows)
	{
		for (int i = numRows; i > 0; i--)
		{
			QTableWidgetItem *cell = ui.polarTableWidget->item(i - 1, 2);

			if (cell)
			{
				if (state)
					cell->setCheckState(Qt::Checked);
				else
					cell->setCheckState(Qt::Unchecked);
			}
		}
	}
}

//---------------------------------------------------------------------------
// Toggles persistence for all entries in table.
void MultiAxisOperation::polarTableTogglePersistence(void)
{
	int numRows = ui.polarTableWidget->rowCount();

	if (numRows)
	{
		for (int i = numRows; i > 0; i--)
		{
			QTableWidgetItem *cell = ui.polarTableWidget->item(i - 1, 2);

			if (cell)
			{
				if (cell->checkState() == Qt::Checked)
					cell->setCheckState(Qt::Unchecked);
				else
					cell->setCheckState(Qt::Checked);
			}
		}
	}
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
			goToPolarVector(presentPolar, true);
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
				goToPolarVector(presentPolar, true);
			}
		}
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::goToPolarVector(int polarIndex, bool makeTarget)
{
	double coord1, coord2;
	bool ok, error = false;
	double temp;

	// get vector values and check for numerical conversion
	temp = ui.polarTableWidget->item(polarIndex, 0)->text().toDouble(&ok);
	if (ok)
	{
		coord1 = temp;

		if (coord1 < 0.0)	// magnitude cannot be negative
		{
			error = true;
			vectorError = NEGATIVE_MAGNITUDE;
			showErrorString("Polar Vector #" + QString::number(polarIndex + 1) + " has negative magnitude");	// error annunciation
			abortPolarAutostep("Polar Auto-Stepping aborted due to an error with Polar Vector #" + QString::number(polarIndex + 1));
			return;
		}
	}
	else
		error = true;	// error

	temp = ui.polarTableWidget->item(polarIndex, 1)->text().toDouble(&ok);
	if (ok)
		coord2 = temp;
	else
		error = true;	// error

	if (!error)
	{
		// get polar vector in magnet axes coordinates
		QVector3D vector;

		polarToCartesian(coord1, coord2, &vector);

		// attempt to go to vector
		if ((vectorError = checkNextVector(vector.x(), vector.y(), vector.z(), "Polar Table #" + QString::number(polarIndex + 1))) == NO_VECTOR_ERROR)
		{
			if (makeTarget)
			{
				sendNextVector(vector.x(), vector.y(), vector.z());
				targetSource = POLAR_TABLE;
				lastPolar = polarIndex;

				if (autostepPolarTimer->isActive())
				{
					lastTargetMsg = "Auto-Stepping : Polar Table #" + QString::number(polarIndex + 1);
					setStatusMsg(lastTargetMsg);
				}
				else
				{
					lastTargetMsg = "Target Vector : Polar Table #" + QString::number(polarIndex + 1);
					setStatusMsg(lastTargetMsg);
				}
			}
		}
		else
		{
			abortPolarAutostep("Polar Auto-Stepping aborted due to an error with Polar Vector #" + QString::number(polarIndex + 1));
		}
	}
	else
	{
		vectorError = NON_NUMERICAL_ENTRY;
		showErrorString("Polar Table #" + QString::number(polarIndex + 1) + " has non-numerical entry");	// error annunciation
		abortPolarAutostep("Polar Auto-Stepping aborted due to an error with Polar Vector #" + QString::number(polarIndex + 1));
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::polarRangeChanged(void)
{
	if (connected)
	{
		autostepStartIndexPolar = ui.startIndexEditPolar->text().toInt();
		autostepEndIndexPolar = ui.endIndexEditPolar->text().toInt();

		if (autostepStartIndexPolar < 1 || autostepStartIndexPolar > ui.polarTableWidget->rowCount())
			return;
		else if (autostepEndIndexPolar <= autostepStartIndexPolar || autostepEndIndexPolar > ui.polarTableWidget->rowCount())
			return;
		else
			calculatePolarRemainingTime(autostepStartIndexPolar, autostepEndIndexPolar);

		// display total autostep time
		displayPolarRemainingTime();
	}
	else
	{
		ui.polarRemainingTimeValue->clear();
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::calculatePolarRemainingTime(int startIndex, int endIndex)
{
	polarRemainingTime = 0;
	double lastX = xField, lastY = yField, lastZ = zField;	// start from present field values

	if (startIndex < autostepStartIndexPolar || endIndex > autostepEndIndexPolar)	// out of range
		return;

	// calculate total remaining time
	for (int i = startIndex - 1; i < endIndex; i++)
	{
		double coord1, coord2;
		double rampX, rampY, rampZ;	// unused in this context

		goToPolarVector(i, false);	// check vector for errors

		if (!vectorError)
		{
			// get vector values
			coord1 = ui.polarTableWidget->item(i, 0)->text().toDouble();
			coord2 = ui.polarTableWidget->item(i, 1)->text().toDouble();

			// get polar vector in magnet axes coordinates
			QVector3D vector;

			polarToCartesian(coord1, coord2, &vector);

			// calculate ramping time
			polarRemainingTime += calculateRampingTime(vector.x(), vector.y(), vector.z(), lastX, lastY, lastZ, rampX, rampY, rampZ);

			// save as start for next step
			lastX = vector.x();
			lastY = vector.y();
			lastZ = vector.z();

			// add any hold time
			bool ok;
			double temp = 0;

			temp = ui.polarTableWidget->item(i, 2)->text().toDouble(&ok);
			if (ok)
                polarRemainingTime += static_cast<int>(temp);

			if (magnetParams->switchInstalled())
			{
				// transition switch at this step?
				if (ui.polarTableWidget->item(i, 2)->checkState() == Qt::Checked)
				{
					// add time required to cool and reheat switch, plus settling time
					polarRemainingTime += longestCoolingTime + longestHeatingTime + optionsDialog->settlingTime();
				}
			}
		}
		else
			break;	// break on any polar vector error
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::displayPolarRemainingTime(void)
{
	int hours, minutes, seconds, remainder;

	hours = polarRemainingTime / 3600;
	remainder = polarRemainingTime % 3600;
	minutes = remainder / 60;
	seconds = remainder % 60;

	QString timeStr = QString("%1:%2:%3").arg(hours, 2, 10, QChar('0')).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
	ui.polarRemainingTimeValue->setText(timeStr);
}


//---------------------------------------------------------------------------
void MultiAxisOperation::startPolarAutostep(void)
{
	if (connected)
	{
		if (systemState < SYSTEM_QUENCH)
		{
			// deactivate any alignment vectors
			deactivateAlignmentVectors();

			autostepStartIndexPolar = ui.startIndexEditPolar->text().toInt();
			autostepEndIndexPolar = ui.endIndexEditPolar->text().toInt();
			polarRangeChanged();

			if (vectorError == NO_VECTOR_ERROR)
			{
				if (autostepStartIndexPolar < 1 || autostepStartIndexPolar > ui.polarTableWidget->rowCount())
				{
					showErrorString("Starting Polar Index is out of range!");
				}
				else if (autostepEndIndexPolar <= autostepStartIndexPolar || autostepEndIndexPolar > ui.polarTableWidget->rowCount())
				{
					showErrorString("Ending Polar Index is out of range!");
				}
				else if (ui.executePolarCheckBox->isChecked() && !QFile::exists(ui.polarAppLocationEdit->text()))
				{
					showErrorString("App/script does not exist at specified path!");
				}
				else if (ui.executePolarCheckBox->isChecked() && !checkPolarExecutionTime())
				{
					showErrorString("App/script execution time is not a positive, non-zero integer!");
				}
				else if (ui.executePolarCheckBox->isChecked() && ui.polarPythonCheckBox->isChecked() && !QFile::exists(ui.polarPythonPathEdit->text()))
				{
					showErrorString("Python not found at specified path!");
				}
				else if (magnetParams->switchInstalled() && ui.actionPersistentMode->isChecked())
				{
					showErrorString("Cannot ramp to target current or field in persistent mode (i.e. cooled switch)!");
					lastStatusString.clear();
					QApplication::beep();
				}
				else
				{
					ui.startIndexEditPolar->setEnabled(false);
					ui.endIndexEditPolar->setEnabled(false);
					ui.polarRemainingTimeLabel->setEnabled(true);
					ui.polarRemainingTimeValue->setEnabled(true);

					elapsedHoldTimerTicksPolar = 0;
					autostepPolarTimer->start();

					ui.autostepStartButtonPolar->setEnabled(false);
					ui.autostartStopButtonPolar->setEnabled(true);
					ui.manualPolarControlGroupBox->setEnabled(false);
					ui.actionLoad_Polar_Table->setEnabled(false);

					ui.manualVectorControlGroupBox->setEnabled(false);
					ui.autoStepGroupBox->setEnabled(false);
					ui.actionLoad_Vector_Table->setEnabled(false);

					if ((switchInstalled = magnetParams->switchInstalled()))
						ui.actionPersistentMode->setEnabled(false);

					// begin with first vector
					presentPolar = autostepStartIndexPolar - 1;

					// highlight row in table
					ui.polarTableWidget->selectRow(presentPolar);
					magnetState = RAMPING;
					systemState = SYSTEM_RAMPING;
					polarSelectionChanged(); // lockout row changes

					goToPolarVector(presentPolar, true);
					settlingTime = 0;
					suspendPolarAutostepFlag = false;
					polarAutostepState = POLAR_TABLE_RAMPING_TO_NEXT_VECTOR;
				}
			}
		}
		else
		{
			showErrorString("Cannot start autostep mode in present system state");
			lastStatusString.clear();
			QApplication::beep();
		}
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::abortPolarAutostep(QString errorMessage)
{
	if (autostepPolarTimer->isActive())	// first checks for active autostep sequence
	{
		autostepPolarTimer->stop();
		while (errorStatusIsActive)	// show any error condition first
		{
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
            usleep(100000);
#else
			Sleep(100);
#endif
		}
		lastTargetMsg.clear();
		setStatusMsg(errorMessage);
		enablePolarTableControls();
		polarRangeChanged();
		haveExecuted = false;
		suspendPolarAutostepFlag = false;
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::stopPolarAutostep(void)
{
	if (autostepPolarTimer->isActive())
	{
		autostepPolarTimer->stop();
		lastTargetMsg.clear();
		setStatusMsg("Polar Auto-Stepping aborted via Stop button");
		enablePolarTableControls();
		polarRangeChanged();
		haveExecuted = false;
		suspendPolarAutostepFlag = false;
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::suspendPolarAutostep(void)
{
	settlingTime = 0;
	suspendPolarAutostepFlag = true;
	setStatusMsg("Auto-Stepping of Polar Table suspended... Press Ramp to resume");
}

//---------------------------------------------------------------------------
void MultiAxisOperation::resumePolarAutostep(void)
{
	suspendPolarAutostepFlag = false;
	setStatusMsg("Auto-Stepping of Polar Table resumed");
}

//---------------------------------------------------------------------------
void MultiAxisOperation::autostepPolarTimerTick(void)
{
	if (!suspendPolarAutostepFlag)
	{
		if (polarRemainingTime)
		{
			polarRemainingTime--;	// decrement by one second
			displayPolarRemainingTime();
		}

		//////////////////////////////////////
		// POLAR_TABLE_RAMPING_TO_NEXT_VECTOR
		//////////////////////////////////////
		if (polarAutostepState == POLAR_TABLE_RAMPING_TO_NEXT_VECTOR)
		{
			if (magnetState == HOLDING)	// waiting for HOLDING state
			{
				// if a switch is installed, check to see if we want to enter persistent mode
				if (magnetParams->switchInstalled())
				{
					if (ui.polarTableWidget->item(presentPolar, 2)->checkState() == Qt::Checked)
					{
						// enter settling time
						polarAutostepState = POLAR_TABLE_SETTLING_AT_VECTOR;
					}
					else
					{
						polarAutostepState = POLAR_TABLE_HOLDING;	// no switch transition preferred, enter HOLD time
					}
				}
				else
				{
					polarAutostepState = POLAR_TABLE_HOLDING;	// no switch, enter HOLD time
				}
			}
		}

		//////////////////////////////////////
		// POLAR_TABLE_SETTLING_AT_VECTOR
		//////////////////////////////////////
		else if (polarAutostepState == POLAR_TABLE_SETTLING_AT_VECTOR)
		{
			settlingTime++;

			// wait for settling time and magnetVoltage to decay
			if (settlingTime >= optionsDialog->settlingTime() /* && fabs(model430.magnetVoltage) < 0.01 */)
			{
				/////////////////////
				// enter persistence
				/////////////////////
				polarAutostepState = POLAR_TABLE_COOLING_SWITCH;
				ui.actionPersistentMode->setChecked(true);
				actionPersistentMode();

				lastStatusString = "Entering persistence, wait for cooling cycle to complete...";
				setStatusMsg(lastStatusString);

				settlingTime = 0; // reset for next time
			}
			else
			{
				int settlingRemaining = optionsDialog->settlingTime() - settlingTime;

				if (settlingTime > 0)
					lastStatusString = "Waiting for settling time of " + QString::number(settlingRemaining) + " sec before entering persistent mode...";
				else
					lastStatusString = "Waiting for magnet voltage to decay to zero before entering persistent mode...";

				setStatusMsg(lastStatusString);
			}
		}

		//////////////////////////////////////
		// POLAR_TABLE_COOLING_SWITCH
		//////////////////////////////////////
		else if (polarAutostepState == POLAR_TABLE_COOLING_SWITCH)
		{
			if (switchCoolingTimer->isActive() == false)
			{
				polarAutostepState = POLAR_TABLE_HOLDING;
			}
		}

		//////////////////////////////////////
		// POLAR_TABLE_HOLDING
		//////////////////////////////////////
		else if (polarAutostepState == POLAR_TABLE_HOLDING)
		{
			if (magnetState == HOLDING || (magnetParams->switchInstalled() && magnetState == PAUSED))
			{
				elapsedHoldTimerTicksPolar++;

				// check time
				if (ui.polarTableWidget->item(presentPolar, 2) != nullptr)
				{
					bool ok;
					double temp;

					// get time
					temp = ui.polarTableWidget->item(presentPolar, 2)->text().toDouble(&ok);

					if (ok)	// time is a number
					{
						// check for external execution
						if (ui.executePolarCheckBox->isChecked())
						{
							int executionTime = ui.polarAppStartEdit->text().toInt();	// we already verified the time is proper format

							if ((elapsedHoldTimerTicksPolar >= (static_cast<int>(temp) - executionTime)) && !haveExecuted)
							{
								haveExecuted = true;
								executePolarApp();
							}
						}

						// has HOLD time expired?
						if (elapsedHoldTimerTicksPolar >= static_cast<int>(temp))
						{
							// reset for next HOLD time
							elapsedHoldTimerTicksPolar = 0;

							// first check to see if we need to exit persistence
							if (magnetParams->switchInstalled())
							{
								if (ui.polarTableWidget->item(presentPolar, 2)->checkState() == Qt::Checked || ui.actionPersistentMode->isChecked())
								{
									if (ui.actionPersistentMode->isChecked())	// heater is OFF, persistent
									{
										////////////////////
										// exit persistence
										////////////////////
										ui.actionPersistentMode->setChecked(false);
										actionPersistentMode();

										lastStatusString = "Exiting persistence, wait for heating cycle to complete...";
										setStatusMsg(lastStatusString);

										polarAutostepState = POLAR_TABLE_HEATING_SWITCH;
									}
								}
								else // no switch transition needed, move to next vector
								{
									polarAutostepState = POLAR_TABLE_NEXT_VECTOR;
								}
							}
							else // no switch, move to next vector
							{
								polarAutostepState = POLAR_TABLE_NEXT_VECTOR;
							}
						}
						else // HOLD time continues
						{
							if (!errorStatusIsActive)
							{
								// update the HOLDING countdown
								QString tempStr = statusMisc->text();
								int index = tempStr.indexOf('(');
								if (index >= 1)
									tempStr.truncate(index - 1);

								QString timeStr = " (" + QString::number(temp - elapsedHoldTimerTicksPolar) + " sec of Hold Time remaining)";
								setStatusMsg(tempStr + timeStr);
							}
						}
					}
					else
					{
						autostepPolarTimer->stop();
						lastTargetMsg.clear();
						setStatusMsg("Polar Auto-Stepping aborted due to non-integer dwell time on line #" + QString::number(presentPolar + 1));
						enablePolarTableControls();
						haveExecuted = false;
					}
				}
				else
				{
					autostepPolarTimer->stop();
					lastTargetMsg.clear();
					setStatusMsg("Polar Auto-Stepping aborted due to unknown dwell time on line #" + QString::number(presentPolar + 1));
					enablePolarTableControls();
					haveExecuted = false;
				}
			}
			else
			{
				if (!errorStatusIsActive)
				{
					// remove any erroneous countdown text
					QString tempStr = statusMisc->text();
					int index = tempStr.indexOf('(');
					if (index >= 1)
						tempStr.truncate(index - 1);

					setStatusMsg(tempStr);
				}
			}
		}

		//////////////////////////////////////
		// POLAR_TABLE_HEATING_SWITCH
		//////////////////////////////////////
		else if (polarAutostepState == POLAR_TABLE_HEATING_SWITCH)
		{
		if (matchMagnetCurrentTimer->isActive() == false && switchHeatingTimer->isActive() == false)
				polarAutostepState = POLAR_TABLE_NEXT_VECTOR;
		}

		//////////////////////////////////////
		// POLAR_TABLE_NEXT_VECTOR
		//////////////////////////////////////
		else if (polarAutostepState == POLAR_TABLE_NEXT_VECTOR)
		{
			if (presentPolar + 1 < autostepEndIndexPolar)
			{
				// highlight row in table
				presentPolar++;
				ui.polarTableWidget->selectRow(presentPolar);
				magnetState = RAMPING;
				systemState = SYSTEM_RAMPING;

				// update remaining time (remember presentPolar is zero-based)
				calculatePolarRemainingTime(presentPolar + 1, autostepEndIndexPolar);

				///////////////////////////////////////////////
				// go to next polar vector!
				///////////////////////////////////////////////
				goToPolarVector(presentPolar, true);
				haveExecuted = false;
				polarAutostepState = POLAR_TABLE_RAMPING_TO_NEXT_VECTOR;
			}
			else
			{
				/////////////////////////////////////////////////////
				// successfully completed polar table auto-stepping
				/////////////////////////////////////////////////////
				lastTargetMsg = "Polar Auto-Step Completed @ Polar Vector #" + QString::number(presentPolar + 1);
				setStatusMsg(lastTargetMsg);
				autostepPolarTimer->stop();
				enablePolarTableControls();
				haveExecuted = false;
				suspendPolarAutostepFlag = false;
			}
		}
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::enablePolarTableControls(void)
{
	ui.startIndexEditPolar->setEnabled(true);
	ui.endIndexEditPolar->setEnabled(true);
	ui.polarRemainingTimeLabel->setEnabled(false);
	ui.polarRemainingTimeValue->setEnabled(false);
	ui.autostepStartButtonPolar->setEnabled(true);
	ui.autostartStopButtonPolar->setEnabled(false);
	ui.manualPolarControlGroupBox->setEnabled(true);
	ui.actionLoad_Polar_Table->setEnabled(true);

	polarSelectionChanged();

	ui.manualVectorControlGroupBox->setEnabled(true);
	ui.autoStepGroupBox->setEnabled(true);
	ui.actionLoad_Vector_Table->setEnabled(true);
	if ((switchInstalled = magnetParams->switchInstalled()))
		ui.actionPersistentMode->setEnabled(true);

	if (ui.actionPersistentMode->isChecked())	// oops! heater is OFF, persistent
	{
		// don't allow target changes in persistent mode
		ui.manualPolarControlGroupBox->setEnabled(false);
		ui.autoStepGroupBoxPolar->setEnabled(false);
		ui.manualVectorControlGroupBox->setEnabled(false);
		ui.autoStepGroupBox->setEnabled(false);
	}
}

//---------------------------------------------------------------------------
// App/Script management and processing support
//---------------------------------------------------------------------------
void MultiAxisOperation::browseForPolarAppPath(void)
{
	QString exeFileName;
	QSettings settings;
	QString extension = NULL;

	lastPolarAppFilePath = settings.value("LastPolarAppFilePath").toString();

	// what type of file was last selected?
	QStringList parts = lastAppFilePath.split(".");
	QString lastBit = parts.at(parts.size() - 1);

	if (lastBit == "py")
		extension = "Script Files (*.py)";
#if defined(Q_OS_MAC)
	else if (lastBit == "app")
		extension = "Applications(*.app)";
	else
		extension = "All Files (*.*)";
#endif

#if defined(Q_OS_LINUX)
	exeFileName = QFileDialog::getOpenFileName(this, "Choose App/Script File", lastPolarAppFilePath, "All Files (*);;Script Files (*.py)", &extension);
#elif defined(Q_OS_MACOS)
	exeFileName = QFileDialog::getOpenFileName(this, "Choose App/Script File", lastPolarAppFilePath, "Applications (*.app);;Script Files (*.py);;All Files (*)", &extension);
#else
	exeFileName = QFileDialog::getOpenFileName(this, "Choose App/Script File", lastPolarAppFilePath, "Applications (*.exe);;Script Files (*.py)", &extension);
#endif

	if (!exeFileName.isEmpty())
	{
		lastPolarAppFilePath = exeFileName;
		ui.polarAppLocationEdit->setText(exeFileName);

		// save path
		settings.setValue("LastPolarAppFilePath", lastPolarAppFilePath);
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::browseForPolarPythonPath(void)
{
	QString pythonFileName;
	QSettings settings;

	lastPolarPythonPath = settings.value("LastPolarPythonPath").toString();

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
	pythonFileName = QFileDialog::getOpenFileName(this, "Choose Python Executable", lastPolarPythonPath, "Python Executable (*)");
#else
	pythonFileName = QFileDialog::getOpenFileName(this, "Choose Python Executable", lastPolarPythonPath, "Python Executable (*.exe)");
#endif

	if (!pythonFileName.isEmpty())
	{
		lastPolarPythonPath = pythonFileName;
		ui.polarPythonPathEdit->setText(pythonFileName);

		// save path
		settings.setValue("LastPolarPythonPath", lastPolarPythonPath);
	}
}

//---------------------------------------------------------------------------
bool MultiAxisOperation::checkPolarExecutionTime(void)
{
	bool ok = false;

	int temp = ui.polarAppStartEdit->text().toInt(&ok);

	if (ok)
	{
		// must be >= 0
		if (temp < 0)
			ok = false;
	}

	return ok;
}

//---------------------------------------------------------------------------
void MultiAxisOperation::executePolarNowClick(void)
{
	if (ui.executePolarCheckBox->isChecked() && !QFile::exists(ui.polarAppLocationEdit->text()))
	{
		showErrorString("App/script does not exist at specified path!");
	}
	else if (ui.executePolarCheckBox->isChecked() && !checkPolarExecutionTime())
	{
		showErrorString("App/script execution time is not a positive, non-zero integer!");
	}
	else if (ui.executePolarCheckBox->isChecked() && ui.polarPythonCheckBox->isChecked() && !QFile::exists(ui.polarPythonPathEdit->text()))
	{
		showErrorString("Python not found at specified path!");
	}
	else // all good! start!
	{
		executePolarApp();
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::executePolarApp(void)
{
	QString program = ui.polarAppLocationEdit->text();
	QString args = ui.polarAppArgsEdit->text();
	QStringList arguments = args.split(" ", QString::SkipEmptyParts);

	// if a python script, use python path for executable
	if (ui.polarPythonCheckBox->isChecked())
	{
		program = ui.polarPythonPathEdit->text();
		arguments.insert(0, ui.polarAppLocationEdit->text());
	}

	// launch detached background process
	QProcess *process = new QProcess(this);
	process->setProgram(program);
	process->setArguments(arguments);
	process->startDetached();
	process->deleteLater();
}

//---------------------------------------------------------------------------
void MultiAxisOperation::polarAppCheckBoxChanged(int state)
{
	if (ui.executePolarCheckBox->isChecked())
	{
		ui.polarAppFrame->setVisible(true);
		ui.executePolarNowButton->setVisible(true);
	}
	else
	{
		ui.polarAppFrame->setVisible(false);
		ui.executePolarNowButton->setVisible(false);
	}

	polarPythonCheckBoxChanged(0);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::polarPythonCheckBoxChanged(int state)
{
	if (ui.polarPythonCheckBox->isChecked())
	{
		ui.polarPythonPathLabel->setVisible(true);
		ui.polarPythonPathEdit->setVisible(true);
		ui.polarPythonLocationButton->setVisible(true);
	}
	else
	{
		ui.polarPythonPathLabel->setVisible(false);
		ui.polarPythonPathEdit->setVisible(false);
		ui.polarPythonLocationButton->setVisible(false);
	}
}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------