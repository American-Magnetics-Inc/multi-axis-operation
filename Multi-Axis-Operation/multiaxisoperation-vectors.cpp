#include "stdafx.h"
#include "multiaxisoperation.h"
#include "conversions.h"

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
#include <unistd.h>
// on Linux and macOS we include the Qxlsx source in the ./header and ./source folders
#include "header/xlsxdocument.h"
#else
// right now keep using the library form of QtXlsxWriter on Windows due to issues with
// linking Qxlsx to private API, and using pre-compiled headers in VS2017
#include <xlsxdocument.h>
#endif

//---------------------------------------------------------------------------
// Local constants and static variables.
//---------------------------------------------------------------------------
enum VectorAutostepStates
{
	VECTOR_TABLE_RAMPING_TO_NEXT_VECTOR = 0,
	VECTOR_TABLE_SETTLING_AT_VECTOR,
	VECTOR_TABLE_COOLING_SWITCH,
	VECTOR_TABLE_HOLDING,
	VECTOR_TABLE_HEATING_SWITCH,
	VECTOR_TABLE_NEXT_VECTOR
};

static int settlingTime = 0;
VectorAutostepStates vectorAutostepState;	// state machine for autostep
static bool tableIsLoading = false;

//---------------------------------------------------------------------------
// Contains methods related to the Vector Table tab view.
// Broken out from multiaxisoperation.cpp for ease of editing.
//---------------------------------------------------------------------------
void MultiAxisOperation::restoreVectorTab(QSettings *settings)
{
	// restore any persistent values
	ui.executeCheckBox->setChecked(settings->value("VectorTable/EnableExecution", false).toBool());
	ui.appLocationEdit->setText(settings->value("VectorTable/AppPath", "").toString());
	ui.appArgsEdit->setText(settings->value("VectorTable/AppArgs", "").toString());
	ui.pythonPathEdit->setText(settings->value("VectorTable/PythonPath", "").toString());
	ui.appStartEdit->setText(settings->value("VectorTable/ExecutionTime", "").toString());
	ui.pythonCheckBox->setChecked(settings->value("VectorTable/PythonScript", false).toBool());

	// init app/script execution
	appCheckBoxChanged(0);

	// make connections
	connect(ui.executeCheckBox, SIGNAL(stateChanged(int)), this, SLOT(appCheckBoxChanged(int)));
	connect(ui.pythonCheckBox, SIGNAL(stateChanged(int)), this, SLOT(pythonCheckBoxChanged(int)));
	connect(ui.appLocationButton, SIGNAL(clicked()), this, SLOT(browseForAppPath()));
	connect(ui.pythonLocationButton, SIGNAL(clicked()), this, SLOT(browseForPythonPath()));
	connect(ui.executeNowButton, SIGNAL(clicked()), this, SLOT(executeNowClick()));
	connect(ui.vectorsTableWidget, SIGNAL(itemChanged(QTableWidgetItem*)), this, SLOT(vectorTableItemChanged(QTableWidgetItem*)));

	setTableHeader();
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionLoad_Vector_Table(void)
{
	QSettings settings;
	int skipCnt = 0;
	lastVectorsLoadPath = settings.value("LastVectorFilePath").toString();
	bool convertFieldUnits = false;

	if (autostepTimer->isActive())
		return;	// no load during auto-stepping

	vectorsFileName = QFileDialog::getOpenFileName(this, "Choose Vector File", lastVectorsLoadPath, "Vector Definition Files (*.txt *.log *.csv)");

	if (!vectorsFileName.isEmpty())
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);

		lastVectorsLoadPath = vectorsFileName;

		FILE *inFile;
		inFile = fopen(vectorsFileName.toLocal8Bit(), "r");

        if (inFile != nullptr)
		{
			QTextStream in(inFile);

			tableIsLoading = true;

			// read first line for coordinate system selection
			QString str = in.readLine();

			// convert to all caps and remove all leading/trailing whitespace
			str = str.toUpper();
			str = str.trimmed();

			if (str.contains("CARTESIAN"))
			{
				loadedCoordinates = CARTESIAN_COORDINATES;
				skipCnt = 2;
			}
			else if (str.contains("SPHERICAL, ISO") || str.contains("SPHERICAL,ISO"))
			{
				loadedCoordinates = SPHERICAL_COORDINATES;
				convention = ISO;
				skipCnt = 2;
			}
			else if (str.contains("SPHERICAL, MATHEMATICAL") || str.contains("SPHERICAL,MATHEMATICAL"))
			{
				loadedCoordinates = SPHERICAL_COORDINATES;
				convention = MATHEMATICAL;
				skipCnt = 2;
			}
			else if (str.contains("SPHERICAL"))
			{
				loadedCoordinates = SPHERICAL_COORDINATES;
				convention = MATHEMATICAL;
				skipCnt = 2;
			}
			else
			{
				loadedCoordinates = SPHERICAL_COORDINATES;
				convention = MATHEMATICAL;
				skipCnt = 1;
			}

			// now scan for units in header
			if (skipCnt > 1)
				str = in.readLine();

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
		vectorTableClear();

		// clear status text
		statusMisc->clear();

		// if ramping, set system to PAUSE
		actionPause();

		// now read in data
		if (magnetParams->switchInstalled())
		{
			ui.vectorsTableWidget->loadFromFile(vectorsFileName, skipCnt);

			// set persistence if switched
			if (optionsDialog->enterPersistence())
				setVectorTablePersistence(true);
			else
				setVectorTablePersistence(false);
		}
		else
			ui.vectorsTableWidget->loadFromFile(vectorsFileName, skipCnt);

		// save path
		settings.setValue("LastVectorFilePath", lastVectorsLoadPath);

		presentVector = -1;	// no selection
		//ui.vectorsTableWidget->selectRow(presentVector);

		// set alignment on "Pass/Fail" column
		ui.vectorsTableWidget->setColumnAlignment(4, (Qt::AlignCenter | Qt::AlignVCenter));

		// set headings as appropriate
		setTableHeader();

		if (convertFieldUnits)
		{
			convertFieldValues(fieldUnits, false);
		}

		// set the spherical coordinate convention since it may have changed
		if (loadedCoordinates == SPHERICAL_COORDINATES)
			setSphericalConvention(convention, true);

		QApplication::restoreOverrideCursor();
		vectorSelectionChanged();
		tableIsLoading = false;
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::setTableHeader(void)
{
	if (loadedCoordinates == SPHERICAL_COORDINATES)
	{
		if (fieldUnits == KG)
		{
			ui.vectorsTableWidget->horizontalHeaderItem(0)->setText("Magnitude (kG)");
		}
		else
		{
			ui.vectorsTableWidget->horizontalHeaderItem(0)->setText("Magnitude (T)");
		}

		if (convention == MATHEMATICAL)
		{
			ui.vectorsTableWidget->horizontalHeaderItem(1)->setText(thetaStr);
			ui.vectorsTableWidget->horizontalHeaderItem(2)->setText(phiStr);
		}
		else
		{
			ui.vectorsTableWidget->horizontalHeaderItem(1)->setText(phiStr);
			ui.vectorsTableWidget->horizontalHeaderItem(2)->setText(thetaStr);
		}
	}
	else if (loadedCoordinates == CARTESIAN_COORDINATES)
	{
		if (fieldUnits == KG)
		{
			ui.vectorsTableWidget->horizontalHeaderItem(0)->setText("X (kG)");
			ui.vectorsTableWidget->horizontalHeaderItem(1)->setText("Y (kG)");
			ui.vectorsTableWidget->horizontalHeaderItem(2)->setText("Z (kG)");
		}
		else
		{
			ui.vectorsTableWidget->horizontalHeaderItem(0)->setText("X (T)");
			ui.vectorsTableWidget->horizontalHeaderItem(1)->setText("Y (T)");
			ui.vectorsTableWidget->horizontalHeaderItem(2)->setText("Z (T)");
		}
	}

	// set quench current header if present
	if (ui.vectorsTableWidget->columnCount() > 5)
	{
		if (ui.vectorsTableWidget->columnCount() >= 8)
		{
			ui.vectorsTableWidget->setHorizontalHeaderItem(5, new QTableWidgetItem("X Quench (A)"));
			ui.vectorsTableWidget->horizontalHeaderItem(5)->font().setBold(true);
			ui.vectorsTableWidget->setHorizontalHeaderItem(6, new QTableWidgetItem("Y Quench (A)"));
			ui.vectorsTableWidget->horizontalHeaderItem(6)->font().setBold(true);
			ui.vectorsTableWidget->setHorizontalHeaderItem(7, new QTableWidgetItem("Z Quench (A)"));
			ui.vectorsTableWidget->horizontalHeaderItem(7)->font().setBold(true);
		}
	}

	// set hold time format
	if (magnetParams->switchInstalled())
		ui.vectorsTableWidget->horizontalHeaderItem(3)->setText("Enter Persistence?/\nHold Time (sec)");
	else
		ui.vectorsTableWidget->horizontalHeaderItem(3)->setText("Hold Time (sec)");
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionSave_Vector_Table(void)
{
	QSettings settings;
	lastVectorsSavePath = settings.value("LastVectorSavePath").toString();

	saveVectorsFileName = QFileDialog::getSaveFileName(this, "Save Vector File", lastVectorsSavePath, "Run Test Files (*.txt *.log *.csv)");

	if (!saveVectorsFileName.isEmpty())
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);

		lastVectorsSavePath = saveVectorsFileName;

		FILE *outFile;
		outFile = fopen(saveVectorsFileName.toLocal8Bit(), "w");

		// save coordinate system and convention designation
        if (outFile != nullptr)
		{
			QTextStream out(outFile);
			QString tempStr;

			if (loadedCoordinates == SPHERICAL_COORDINATES)
			{
				tempStr = "SPHERICAL,";
				if (convention == ISO)
					tempStr += "ISO";
				else
					tempStr += "MATHEMATICAL";
			}
			else if (loadedCoordinates == CARTESIAN_COORDINATES)
			{
				tempStr = "CARTESIAN";
			}

			out << tempStr << "\n";

			Qt::flush(out);
			fclose(outFile);
		}

		// save table contents
		ui.vectorsTableWidget->saveToFile(saveVectorsFileName, true);

		// save path
		settings.setValue("LastVectorSavePath", lastVectorsSavePath);

		QApplication::restoreOverrideCursor();
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::recalculateRemainingTime(void)
{
	if (autostepTimer->isActive())
		calculateAutostepRemainingTime(presentVector + 1, autostepEndIndex);
	else
		autostepRangeChanged();
}

//---------------------------------------------------------------------------
void MultiAxisOperation::vectorTableItemChanged(QTableWidgetItem *item)
{
	// recalculate time after change and check for errors
	if (!tableIsLoading)
		recalculateRemainingTime();
}

//---------------------------------------------------------------------------
void MultiAxisOperation::vectorSelectionChanged(void)
{
	if (autostepTimer->isActive())
	{
		// no table mods during autostep!
		ui.vectorAddRowAboveToolButton->setEnabled(false);
		ui.vectorAddRowBelowToolButton->setEnabled(false);
		ui.vectorRemoveRowToolButton->setEnabled(false);
		ui.vectorTableClearToolButton->setEnabled(false);
		ui.pswitchToggleToolButton->setEnabled(false);
	}
	else
	{
		// any selected vectors?
		QList<QTableWidgetItem *> list = ui.vectorsTableWidget->selectedItems();

		if (list.count())
		{
			ui.vectorAddRowAboveToolButton->setEnabled(true);
			ui.vectorAddRowBelowToolButton->setEnabled(true);
			ui.vectorRemoveRowToolButton->setEnabled(true);
		}
		else
		{
			if (ui.vectorsTableWidget->rowCount())
			{
				ui.vectorAddRowAboveToolButton->setEnabled(false);
				ui.vectorAddRowBelowToolButton->setEnabled(false);
			}
			else
			{
				ui.vectorAddRowAboveToolButton->setEnabled(true);
				ui.vectorAddRowBelowToolButton->setEnabled(true);
			}

			ui.vectorRemoveRowToolButton->setEnabled(false);
		}

		if (ui.vectorsTableWidget->rowCount())
		{
			ui.vectorTableClearToolButton->setEnabled(true);

			if ((switchInstalled = magnetParams->switchInstalled()))
				ui.pswitchToggleToolButton->setEnabled(true);
		}
		else
		{
			ui.vectorTableClearToolButton->setEnabled(false);
			ui.pswitchToggleToolButton->setEnabled(false);
		}
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::vectorTableAddRowAbove(void)
{
	int newRow = -1;
	tableIsLoading = true;

	// find selected vector
	QList<QTableWidgetItem *> list = ui.vectorsTableWidget->selectedItems();

	if (list.count())
	{
		int currentRow = list[0]->row();
		ui.vectorsTableWidget->insertRow(currentRow);
		newRow = currentRow;
	}
	else
	{
		if (ui.vectorsTableWidget->rowCount() == 0)
		{
			ui.vectorsTableWidget->insertRow(0);
			newRow = 0;
		}
	}

	if (newRow > -1)
	{
		initNewRow(newRow);
		updatePresentVector(newRow, false);

		tableIsLoading = false;
		vectorSelectionChanged();
	}

	tableIsLoading = false;
}

//---------------------------------------------------------------------------
void MultiAxisOperation::vectorTableAddRowBelow(void)
{
	int newRow = -1;
	tableIsLoading = true;

	// find selected vector
	QList<QTableWidgetItem *> list = ui.vectorsTableWidget->selectedItems();

	if (list.count())
	{
		int currentRow = list[0]->row();
		ui.vectorsTableWidget->insertRow(currentRow + 1);
		newRow = currentRow + 1;
	}
	else
	{
		if (ui.vectorsTableWidget->rowCount() == 0)
		{
			ui.vectorsTableWidget->insertRow(0);
			newRow = 0;
		}
	}

	if (newRow > -1)
	{
		initNewRow(newRow);
		updatePresentVector(newRow, false);

		tableIsLoading = false;
		vectorSelectionChanged();
	}

	tableIsLoading = false;
}

//---------------------------------------------------------------------------
void MultiAxisOperation::initNewRow(int newRow)
{
	int numColumns = ui.vectorsTableWidget->columnCount();

	// initialize cells
	for (int i = 0; i < numColumns; ++i)
	{
		QTableWidgetItem *cell = ui.vectorsTableWidget->item(newRow, i);

        if (cell == nullptr)
			ui.vectorsTableWidget->setItem(newRow, i, cell = new QTableWidgetItem(""));
		else
			cell->setText("");

		if (i == 3 && magnetParams->switchInstalled())
			cell->setCheckState(Qt::Unchecked);

		if (i == (numColumns - 1))
			cell->setTextAlignment(Qt::AlignCenter);
		else
			cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::updatePresentVector(int row, bool removed)
{
	if (targetSource == VECTOR_TABLE)
	{
		if (removed)
		{
			if (presentVector == row)
			{
				lastTargetMsg.clear();
				setStatusMsg("");
				presentVector = lastVector = -1;
			}
			else if (row < presentVector)
			{
				// selection shift up by one
				presentVector--;
				lastVector = presentVector;
				lastTargetMsg = "Target Vector : Vector Table #" + QString::number(presentVector + 1);
				setStatusMsg(lastTargetMsg);
			}
		}
		else
		{
			if (row <= presentVector)
			{
				// selection shifted down by one
				presentVector++;
				lastVector = presentVector;
				lastTargetMsg = "Target Vector : Vector Table #" + QString::number(presentVector + 1);
				setStatusMsg(lastTargetMsg);
			}
		}
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::vectorTableRemoveRow(void)
{
	// find selected vector
	QList<QTableWidgetItem *> list = ui.vectorsTableWidget->selectedItems();

	if (list.count())
	{
		int currentRow = list[0]->row();
		ui.vectorsTableWidget->removeRow(currentRow);
		updatePresentVector(currentRow, true);
		vectorSelectionChanged();
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::vectorTableClear(void)
{
	int numRows = ui.vectorsTableWidget->rowCount();

	if (numRows)
	{
		for (int i = numRows; i > 0; i--)
			ui.vectorsTableWidget->removeRow(i - 1);
	}

	presentVector = lastVector = -1;
	lastTargetMsg.clear();
	setStatusMsg("");
	vectorSelectionChanged();
}

//---------------------------------------------------------------------------
// Set persistence for all entries in table.
void MultiAxisOperation::setVectorTablePersistence(bool state)
{
	int numRows = ui.vectorsTableWidget->rowCount();

	if (numRows)
	{
		for (int i = numRows; i > 0; i--)
		{
			QTableWidgetItem *cell = ui.vectorsTableWidget->item(i - 1, 3);

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
void MultiAxisOperation::vectorTableTogglePersistence(void)
{
	int numRows = ui.vectorsTableWidget->rowCount();

	if (numRows)
	{
		for (int i = numRows; i > 0; i--)
		{
			QTableWidgetItem *cell = ui.vectorsTableWidget->item(i - 1, 3);

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
void MultiAxisOperation::actionGenerate_Excel_Report(void)
{
	QSettings settings;
	lastReportPath = settings.value("LastReportPath").toString();

	reportFileName = QFileDialog::getSaveFileName(this, "Save Excel Report File", lastReportPath, "Excel Report Files (*.xlsx)");

	if (!reportFileName.isEmpty())
		saveReport(reportFileName);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::saveReport(QString reportFileName)
{
	if (!reportFileName.isEmpty())
	{
		QSettings settings;
		QString tempStr;
		QXlsx::Document xlsx;

		// save filename path
		settings.setValue("LastReportPath", reportFileName);

		// set document properties
		xlsx.setDocumentProperty("title", "Multi-Axis Magnet " + magnetParams->getMagnetID() + " Test Report");
		xlsx.setDocumentProperty("subject", "Magnet ID " + magnetParams->getMagnetID());
		xlsx.setDocumentProperty("company", "American Magnetics, Inc.");
		xlsx.setDocumentProperty("description", "Test Results");

		QXlsx::Format alignRightFormat;
		QXlsx::Format alignCenterFormat;
		QXlsx::Format boldAlignRightFormat;
		QXlsx::Format boldAlignCenterFormat;
		alignRightFormat.setHorizontalAlignment(QXlsx::Format::AlignRight);
		alignCenterFormat.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
		boldAlignRightFormat.setFontBold(true);
		boldAlignRightFormat.setHorizontalAlignment(QXlsx::Format::AlignRight);
		boldAlignCenterFormat.setFontBold(true);
		boldAlignCenterFormat.setHorizontalAlignment(QXlsx::Format::AlignHCenter);

		xlsx.deleteSheet("Sheet1");
		xlsx.addSheet("Parameters and Limits");
		xlsx.setColumnWidth(1, 30);
		xlsx.setColumnWidth(2, 4, 15);

		// write magnet ID
		xlsx.write("A1", "Magnet ID", boldAlignRightFormat);
		xlsx.write("B1", magnetParams->getMagnetID(), alignRightFormat);

		// write magnitude limit
		tempStr = "Magnitude Limit (";
		if (fieldUnits == KG)
			tempStr += "kG)";
		else
			tempStr += "T)";
		xlsx.write("A2", tempStr, boldAlignRightFormat);
		xlsx.write("B2", magnetParams->getMagnitudeLimit());

		// write magnet axes settings

		// write header
		xlsx.write("A4", "Parameter", boldAlignCenterFormat);
		xlsx.write("B4", "X-Axis", boldAlignCenterFormat);
		xlsx.write("C4", "Y-Axis", boldAlignCenterFormat);
		xlsx.write("D4", "Z-Axis", boldAlignCenterFormat);

		// write first column labels
		xlsx.write("A5", "IP Addr", alignCenterFormat);
		xlsx.write("A6", "Current Limit (A)", alignCenterFormat);
		xlsx.write("A7", "Voltage Limit (A)", alignCenterFormat);
		xlsx.write("A8", "Max Ramp Rate (A/s)", alignCenterFormat);
		tempStr = "Coil Constant (";
		if (fieldUnits == KG)
			tempStr += "kG/A)";
		else
			tempStr += "T/A)";
		xlsx.write("A9", tempStr, alignCenterFormat);
		xlsx.write("A10", "Inductance (H)", alignCenterFormat);
		xlsx.write("A11", "Switch Installed", alignCenterFormat);
		xlsx.write("A12", "Switch Heater Current (mA)", alignCenterFormat);
		xlsx.write("A13", "Switch Cooling Time (s)", alignCenterFormat);
		xlsx.write("A14", "Switch Heating Time (s)", alignCenterFormat);


		// write X parameters
		if (magnetParams->GetXAxisParams()->activate)
		{
			xlsx.write("B5", magnetParams->GetXAxisParams()->ipAddress, alignCenterFormat);
			xlsx.write("B6", magnetParams->GetXAxisParams()->currentLimit, alignRightFormat);
			xlsx.write("B7", magnetParams->GetXAxisParams()->voltageLimit, alignRightFormat);
			xlsx.write("B8", magnetParams->GetXAxisParams()->maxRampRate, alignRightFormat);
			xlsx.write("B9", magnetParams->GetXAxisParams()->coilConst, alignRightFormat);
			xlsx.write("B10", magnetParams->GetXAxisParams()->inductance, alignRightFormat);
			xlsx.write("B11", magnetParams->GetXAxisParams()->switchInstalled, alignRightFormat);

			if (magnetParams->GetXAxisParams()->switchInstalled)
			{
				xlsx.write("B12", magnetParams->GetXAxisParams()->switchHeaterCurrent, alignRightFormat);
				xlsx.write("B13", magnetParams->GetXAxisParams()->switchCoolingTime, alignRightFormat);
				xlsx.write("B14", magnetParams->GetXAxisParams()->switchHeatingTime, alignRightFormat);
			}
		}
		else
			xlsx.write("B5", "N/A", alignCenterFormat);

		// write Y parameters
		if (magnetParams->GetYAxisParams()->activate)
		{
			xlsx.write("C5", magnetParams->GetYAxisParams()->ipAddress, alignCenterFormat);
			xlsx.write("C6", magnetParams->GetYAxisParams()->currentLimit, alignRightFormat);
			xlsx.write("C7", magnetParams->GetYAxisParams()->voltageLimit, alignRightFormat);
			xlsx.write("C8", magnetParams->GetYAxisParams()->maxRampRate, alignRightFormat);
			xlsx.write("C9", magnetParams->GetYAxisParams()->coilConst, alignRightFormat);
			xlsx.write("C10", magnetParams->GetYAxisParams()->inductance, alignRightFormat);
			xlsx.write("C11", magnetParams->GetYAxisParams()->switchInstalled, alignRightFormat);

			if (magnetParams->GetYAxisParams()->switchInstalled)
			{
				xlsx.write("C12", magnetParams->GetYAxisParams()->switchHeaterCurrent, alignRightFormat);
				xlsx.write("C13", magnetParams->GetYAxisParams()->switchCoolingTime, alignRightFormat);
				xlsx.write("C14", magnetParams->GetYAxisParams()->switchHeatingTime, alignRightFormat);
			}
		}
		else
			xlsx.write("C5", "N/A", alignCenterFormat);

		// write Y parameters
		if (magnetParams->GetZAxisParams()->activate)
		{
			xlsx.write("D5", magnetParams->GetZAxisParams()->ipAddress, alignCenterFormat);
			xlsx.write("D6", magnetParams->GetZAxisParams()->currentLimit, alignRightFormat);
			xlsx.write("D7", magnetParams->GetZAxisParams()->voltageLimit, alignRightFormat);
			xlsx.write("D8", magnetParams->GetZAxisParams()->maxRampRate, alignRightFormat);
			xlsx.write("D9", magnetParams->GetZAxisParams()->coilConst, alignRightFormat);
			xlsx.write("D10", magnetParams->GetZAxisParams()->inductance, alignRightFormat);
			xlsx.write("D11", magnetParams->GetZAxisParams()->switchInstalled, alignRightFormat);

			if (magnetParams->GetZAxisParams()->switchInstalled)
			{
				xlsx.write("D12", magnetParams->GetZAxisParams()->switchHeaterCurrent, alignRightFormat);
				xlsx.write("D13", magnetParams->GetZAxisParams()->switchCoolingTime, alignRightFormat);
				xlsx.write("D14", magnetParams->GetZAxisParams()->switchHeatingTime, alignRightFormat);
			}
		}
		else
			xlsx.write("D5", "N/A", alignCenterFormat);

		// create vector table output sheet
		xlsx.addSheet("Vector Table Results");
		xlsx.setColumnWidth(1, 8, 15);

		// output date/time
		xlsx.write("A1", "Date", boldAlignRightFormat);
		xlsx.write("B1", QDate::currentDate().toString());
		xlsx.write("A2", "Time", boldAlignRightFormat);
		xlsx.write("B2", QTime::currentTime().toString());

		// output headers
		xlsx.write("F3", "Quench");
		xlsx.mergeCells("F3:H3", boldAlignCenterFormat);

		for (int i = 0; i < ui.vectorsTableWidget->columnCount(); i++)
		{
			xlsx.write(4, i + 1, ui.vectorsTableWidget->horizontalHeaderItem(i)->text(), boldAlignCenterFormat);
		}

		xlsx.write("F4", "X-Axis", boldAlignCenterFormat);
		xlsx.write("G4", "Y-Axis", boldAlignCenterFormat);
		xlsx.write("H4", "Z-Axis", boldAlignCenterFormat);

		// output vector data
		for (int i = 0; i < ui.vectorsTableWidget->rowCount(); i++)
		{
			for (int j = 0; j < ui.vectorsTableWidget->columnCount(); j++)
			{
				QTableWidgetItem *item;

				if ((item = ui.vectorsTableWidget->item(i, j)))
				{
					if (item->text() == "Pass" || item->text() == "Fail" || item->text().isEmpty())
					{
						xlsx.write(i + 1 + 4, j + 1, ui.vectorsTableWidget->item(i, j)->text(), alignCenterFormat);
					}
					else
					{
						if (((switchInstalled = magnetParams->switchInstalled())) && j == 3)
						{
							if (item->checkState() == Qt::Checked)
								xlsx.write(i + 1 + 4, j + 1, "Yes / " + ui.vectorsTableWidget->item(i, j)->text(), alignRightFormat);
							else
								xlsx.write(i + 1 + 4, j + 1, "No / " + ui.vectorsTableWidget->item(i, j)->text(), alignRightFormat);
						}
						else
							xlsx.write(i + 1 + 4, j + 1, ui.vectorsTableWidget->item(i, j)->text().toDouble(), alignRightFormat);
					}
				}
			}
		}

		xlsx.saveAs(reportFileName);
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::goToSelectedVector(void)
{
	if (connected)
	{
		// deactivate any alignment vectors
		deactivateAlignmentVectors();

		// find selected vector
		QList<QTableWidgetItem *> list = ui.vectorsTableWidget->selectedItems();

		if (list.count())
		{
			presentVector = list[0]->row();
			magnetState = RAMPING;
			systemState = SYSTEM_RAMPING;
			goToVector(presentVector, true);
		}
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::goToNextVector(void)
{
	if (connected)
	{
		// deactivate any alignment vectors
		deactivateAlignmentVectors();

		// find selected vector
		QList<QTableWidgetItem *> list = ui.vectorsTableWidget->selectedItems();

		if (list.count())
		{
			presentVector = list[0]->row();

			// is next vector a valid request?
			if (((presentVector + 1) >= 1) && ((presentVector + 1) < ui.vectorsTableWidget->rowCount()))
			{
				presentVector++;	// choose next vector

				// highlight row in table
				ui.vectorsTableWidget->selectRow(presentVector);

				magnetState = RAMPING;
				systemState = SYSTEM_RAMPING;
				goToVector(presentVector, true);
			}
		}
	}
}

//---------------------------------------------------------------------------
// Argument vectorIndex is referenced from a start of 0
//---------------------------------------------------------------------------
void MultiAxisOperation::goToVector(int vectorIndex, bool makeTarget)
{
	double coord1, coord2, coord3;
	double x, y, z;
	bool ok, error = false;
	double temp;

	// get vector values and check for numerical conversion
	temp = ui.vectorsTableWidget->item(vectorIndex, 0)->text().toDouble(&ok);
	if (ok)
		coord1 = temp;
	else
		error = true;	// error

	temp = ui.vectorsTableWidget->item(vectorIndex, 1)->text().toDouble(&ok);
	if (ok)
		coord2 = temp;
	else
		error = true;	// error

	temp = ui.vectorsTableWidget->item(vectorIndex, 2)->text().toDouble(&ok);
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
				vectorError = NEGATIVE_MAGNITUDE;
				showErrorString("Vector #" + QString::number(vectorIndex + 1) + " : Magnitude of vector cannot be a negative value");	// error annunciation
				abortAutostep("Auto-Stepping aborted due to an error with Vector #" + QString::number(vectorIndex + 1));
				return;
			}
			if (coord3 < 0.0 || coord3 > 180.0)	// angle from Z-axis must be >= 0 and <= 180 degrees
			{
				vectorError = INCLINATION_OUT_OF_RANGE;
				showErrorString("Vector #" + QString::number(vectorIndex + 1) + " : Angle from Z-axis must be from 0 to 180 degrees");	// error annunciation
				abortAutostep("Auto-Stepping aborted due to an error with Vector #" + QString::number(vectorIndex + 1));
				return;
			}

			sphericalToCartesian(coord1, coord2, coord3, &x, &y, &z);
		}

		// attempt to go to vector
		if ((vectorError = checkNextVector(x, y, z, "Vector #" + QString::number(vectorIndex + 1))) == NO_VECTOR_ERROR)
		{
			if (makeTarget)
			{
				sendNextVector(x, y, z);	// send down to connected Model 430's
				targetSource = VECTOR_TABLE;
				lastVector = vectorIndex;

				if (autostepTimer->isActive())
				{
					lastTargetMsg = "Auto-Stepping : Vector Table #" + QString::number(vectorIndex + 1);
					setStatusMsg(lastTargetMsg);
				}
				else
				{
					lastTargetMsg = "Target Vector : Vector Table #" + QString::number(vectorIndex + 1);
					setStatusMsg(lastTargetMsg);
				}
			}
		}
		else
		{
			abortAutostep("Auto-Stepping aborted due to an error with Vector #" + QString::number(vectorIndex + 1));
		}
	}
	else
	{
		vectorError = NON_NUMERICAL_ENTRY;
		showErrorString("Vector #" + QString::number(vectorIndex + 1) + " has non-numerical entry");	// error annunciation
		abortAutostep("Auto-Stepping aborted due to an error with Vector #" + QString::number(vectorIndex + 1));
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::autostepRangeChanged(void)
{
	if (connected)
	{
		autostepStartIndex = ui.startIndexEdit->text().toInt();
		autostepEndIndex = ui.endIndexEdit->text().toInt();

		if (autostepStartIndex < 1 || autostepStartIndex > ui.vectorsTableWidget->rowCount())
			return;
		else if (autostepEndIndex <= autostepStartIndex || autostepEndIndex > ui.vectorsTableWidget->rowCount())
			return;
		else
			calculateAutostepRemainingTime(autostepStartIndex, autostepEndIndex);

		// display total autostep time
		displayAutostepRemainingTime();
	}
	else
	{
		ui.autostepRemainingTimeValue->clear();
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::calculateAutostepRemainingTime(int startIndex, int endIndex)
{
	autostepRemainingTime = 0;
	double lastX = xField, lastY = yField, lastZ = zField;	// start from present field values

	if (startIndex < autostepStartIndex || endIndex > autostepEndIndex)	// out of range
		return;

	// calculate total remaining time
	for (int i = startIndex - 1; i < endIndex; i++)
	{
		double coord1, coord2, coord3;
		double x, y, z;
		double rampX, rampY, rampZ;	// unused in this context

		goToVector(i, false);	// check vector for errors

		if (!vectorError)
		{
			// get vector values
			coord1 = ui.vectorsTableWidget->item(i, 0)->text().toDouble();
			coord2 = ui.vectorsTableWidget->item(i, 1)->text().toDouble();
			coord3 = ui.vectorsTableWidget->item(i, 2)->text().toDouble();

			if (loadedCoordinates == CARTESIAN_COORDINATES)
			{
				x = coord1;
				y = coord2;
				z = coord3;
			}
			else if (loadedCoordinates == SPHERICAL_COORDINATES)
			{
				sphericalToCartesian(coord1, coord2, coord3, &x, &y, &z);
			}

			// calculate ramping time
			autostepRemainingTime += calculateRampingTime(x, y, z, lastX, lastY, lastZ, rampX, rampY, rampZ);

			// save as start for next step
			lastX = x;
			lastY = y;
			lastZ = z;

			// add any hold time
			bool ok;
			double temp = 0;

			temp = ui.vectorsTableWidget->item(i, 3)->text().toDouble(&ok);
			if (ok)
                autostepRemainingTime += static_cast<int>(temp);

			if (magnetParams->switchInstalled())
			{
				// transition switch at this step?
				if (ui.vectorsTableWidget->item(i, 3)->checkState() == Qt::Checked)
				{
					// add time required to cool and reheat switch, plus settling time
					autostepRemainingTime += longestCoolingTime + longestHeatingTime + optionsDialog->settlingTime();
				}
			}
		}
		else
			break;	// break on any vector error
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::displayAutostepRemainingTime(void)
{
	int hours, minutes, seconds, remainder;

	hours = autostepRemainingTime / 3600;
	remainder = autostepRemainingTime % 3600;
	minutes = remainder / 60;
	seconds = remainder % 60;

	QString timeStr = QString("%1:%2:%3").arg(hours, 2, 10, QChar('0')).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
	ui.autostepRemainingTimeValue->setText(timeStr);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::startAutostep(void)
{
	if (connected)
	{
		// deactivate any alignment vectors
		deactivateAlignmentVectors();

		if (systemState < SYSTEM_QUENCH)
		{
			autostepStartIndex = ui.startIndexEdit->text().toInt();
			autostepEndIndex = ui.endIndexEdit->text().toInt();
			autostepRangeChanged();

			if (vectorError == NO_VECTOR_ERROR)
			{
				if (autostepStartIndex < 1 || autostepStartIndex > ui.vectorsTableWidget->rowCount())
				{
					showErrorString("Starting Index is out of range!");
				}
				else if (autostepEndIndex <= autostepStartIndex || autostepEndIndex > ui.vectorsTableWidget->rowCount())
				{
					showErrorString("Ending Index is out of range!");
				}
				else if (ui.executeCheckBox->isChecked() && !QFile::exists(ui.appLocationEdit->text()))
				{
					showErrorString("App/script does not exist at specified path!");
				}
				else if (ui.executeCheckBox->isChecked() && !checkExecutionTime())
				{
					showErrorString("App/script execution time is not a positive, non-zero integer!");
				}
				else if (ui.executeCheckBox->isChecked() && ui.pythonCheckBox->isChecked() && !QFile::exists(ui.pythonPathEdit->text()))
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
					ui.startIndexEdit->setEnabled(false);
					ui.endIndexEdit->setEnabled(false);
					ui.autostepRemainingTimeLabel->setEnabled(true);
					ui.autostepRemainingTimeValue->setEnabled(true);

					elapsedHoldTimerTicks = 0;
					autostepTimer->start();
					ui.autoStepGroupBox->setTitle("Auto-Stepping (Active)");

					ui.autostepStartButton->setEnabled(false);
					ui.autostartStopButton->setEnabled(true);
					ui.appFrame->setEnabled(false);
					ui.executeCheckBox->setEnabled(false);
					ui.executeNowButton->setEnabled(false);
					ui.manualVectorControlGroupBox->setEnabled(false);
					ui.actionLoad_Vector_Table->setEnabled(false);

					ui.manualPolarControlGroupBox->setEnabled(false);
					ui.autoStepGroupBoxPolar->setEnabled(false);
					ui.actionLoad_Polar_Table->setEnabled(false);
					if ((switchInstalled = magnetParams->switchInstalled()))
						ui.actionPersistentMode->setEnabled(false);

					// begin with first vector
					presentVector = autostepStartIndex - 1;

					// highlight row in table
					ui.vectorsTableWidget->selectRow(presentVector);
					magnetState = RAMPING;
					systemState = SYSTEM_RAMPING;
					vectorSelectionChanged(); // lockout row changes
					haveAutosavedReport = false;

					goToVector(presentVector, true);
					settlingTime = 0;
					suspendAutostepFlag = false;
					vectorAutostepState = VECTOR_TABLE_RAMPING_TO_NEXT_VECTOR;
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
void MultiAxisOperation::abortAutostep(QString errorString)
{
	if (autostepTimer->isActive())	// first checks for active autostep sequence
	{
		autostepTimer->stop();
		ui.autoStepGroupBox->setTitle("Auto-Stepping");
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
		setStatusMsg(errorString);
		autostepRangeChanged();
		enableVectorTableControls();
		haveExecuted = false;
		suspendAutostepFlag = false;
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::stopAutostep(void)
{
	if (autostepTimer->isActive())
	{
		autostepTimer->stop();
		ui.autoStepGroupBox->setTitle("Auto-Stepping");
		lastTargetMsg.clear();
		setStatusMsg("Auto-Stepping aborted via Stop button");
		enableVectorTableControls();
		autostepRangeChanged();
		haveExecuted = false;
		suspendAutostepFlag = false;
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::suspendAutostep(void)
{
	settlingTime = 0;
	suspendAutostepFlag = true;
	setStatusMsg("Auto-Stepping of Vector Table suspended... Press Ramp to resume");
}

//---------------------------------------------------------------------------
void MultiAxisOperation::resumeAutostep(void)
{
	suspendAutostepFlag = false;
	setStatusMsg("Auto-Stepping of Vector Table resumed");
}

//---------------------------------------------------------------------------
void MultiAxisOperation::autostepTimerTick(void)
{
	if (!suspendAutostepFlag)
	{
		if (autostepRemainingTime)
		{
			autostepRemainingTime--;	// decrement by one second
			displayAutostepRemainingTime();
		}

		//////////////////////////////////////
		// VECTOR_TABLE_RAMPING_TO_NEXT_VECTOR
		//////////////////////////////////////
		if (vectorAutostepState == VECTOR_TABLE_RAMPING_TO_NEXT_VECTOR)
		{
			if (magnetState == HOLDING)	// waiting for HOLDING state
			{
				// if a switch is installed, check to see if we want to enter persistent mode
				if (magnetParams->switchInstalled())
				{
					if (ui.vectorsTableWidget->item(presentVector, 3)->checkState() == Qt::Checked)
					{
						// enter settling time
						vectorAutostepState = VECTOR_TABLE_SETTLING_AT_VECTOR;
					}
					else
					{
						vectorAutostepState = VECTOR_TABLE_HOLDING;	// no switch transition preferred, enter HOLD time
					}
				}
				else
				{
					vectorAutostepState = VECTOR_TABLE_HOLDING;	// no switch, enter HOLD time
				}
			}
		}

		//////////////////////////////////////
		// VECTOR_TABLE_SETTLING_AT_VECTOR
		//////////////////////////////////////
		else if (vectorAutostepState == VECTOR_TABLE_SETTLING_AT_VECTOR)
		{
			settlingTime++;

			// wait for settling time and magnetVoltage to decay
			if (settlingTime >= optionsDialog->settlingTime() /* && fabs(model430.magnetVoltage) < 0.01 */)
			{
				/////////////////////
				// enter persistence
				/////////////////////
				vectorAutostepState = VECTOR_TABLE_COOLING_SWITCH;
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
		// VECTOR_TABLE_COOLING_SWITCH
		//////////////////////////////////////
		else if (vectorAutostepState == VECTOR_TABLE_COOLING_SWITCH)
		{
			if (switchCoolingTimer->isActive() == false)
			{
				vectorAutostepState = VECTOR_TABLE_HOLDING;
			}
		}

		//////////////////////////////////////
		// VECTOR_TABLE_HOLDING
		//////////////////////////////////////
		else if (vectorAutostepState == VECTOR_TABLE_HOLDING)
		{
			if (magnetState == HOLDING || (magnetParams->switchInstalled() && magnetState == PAUSED))
			{
				elapsedHoldTimerTicks++;

				// check time
				if (ui.vectorsTableWidget->item(presentVector, 3) != nullptr)
				{
					bool ok;
					double temp;

					// get time
					temp = ui.vectorsTableWidget->item(presentVector, 3)->text().toDouble(&ok);

					if (ok)	// time is a number
					{
						// check for external execution
						if (ui.executeCheckBox->isChecked())
						{
							int executionTime = ui.appStartEdit->text().toInt();	// we already verified the time is proper format

							if ((elapsedHoldTimerTicks >= (static_cast<int>(temp) - executionTime)) && !haveExecuted)
							{
								haveExecuted = true;
								executeApp();
							}
						}

						// has HOLD time expired?
						if (elapsedHoldTimerTicks >= static_cast<int>(temp))  // if true, hold time has expired
						{
							// reset for next HOLD time
							elapsedHoldTimerTicks = 0;

							// first check to see if we need to exit persistence
							if (magnetParams->switchInstalled())
							{
								if (ui.vectorsTableWidget->item(presentVector, 3)->checkState() == Qt::Checked || ui.actionPersistentMode->isChecked())
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

										vectorAutostepState = VECTOR_TABLE_HEATING_SWITCH;
									}
								}
								else // no switch transition needed, move to next vector
								{
									vectorAutostepState = VECTOR_TABLE_NEXT_VECTOR;
								}
							}
							else // no switch, move to next vector
							{
								vectorAutostepState = VECTOR_TABLE_NEXT_VECTOR;
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

								QString timeStr = " (" + QString::number(temp - elapsedHoldTimerTicks) + " sec of Hold Time remaining)";
								setStatusMsg(tempStr + timeStr);
							}
						}
					}
					else
					{
						autostepTimer->stop();
						ui.autoStepGroupBox->setTitle("Auto-Stepping");
						lastTargetMsg.clear();
						setStatusMsg("Auto-Stepping aborted due to non-integer dwell time on line #" + QString::number(presentVector + 1));
						enableVectorTableControls();
						haveExecuted = false;
					}
				}
				else
				{
					autostepTimer->stop();
					ui.autoStepGroupBox->setTitle("Auto-Stepping");
					lastTargetMsg.clear();
					setStatusMsg("Auto-Stepping aborted due to unknown dwell time on line #" + QString::number(presentVector + 1));
					enableVectorTableControls();
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
		// VECTOR_TABLE_HEATING_SWITCH
		//////////////////////////////////////
		else if (vectorAutostepState == VECTOR_TABLE_HEATING_SWITCH)
		{
			if (matchMagnetCurrentTimer->isActive() == false && switchHeatingTimer->isActive() == false)
				vectorAutostepState = VECTOR_TABLE_NEXT_VECTOR;
		}

		//////////////////////////////////////
		// VECTOR_TABLE_NEXT_VECTOR
		//////////////////////////////////////
		else if (vectorAutostepState == VECTOR_TABLE_NEXT_VECTOR)
		{
			if (presentVector + 1 < autostepEndIndex)
			{
				// highlight row in table
				presentVector++;
				ui.vectorsTableWidget->selectRow(presentVector);
				magnetState = RAMPING;
				systemState = SYSTEM_RAMPING;

				// update remaining time (remember presentVector is zero-based)
				calculateAutostepRemainingTime(presentVector + 1, autostepEndIndex);

				///////////////////////////////////////////////
				// go to next vector!
				///////////////////////////////////////////////
				goToVector(presentVector, true);
				haveExecuted = false;
				vectorAutostepState = VECTOR_TABLE_RAMPING_TO_NEXT_VECTOR;
			}
			else
			{
				/////////////////////////////////////////////////////
				// successfully completed vector table auto-stepping
				/////////////////////////////////////////////////////
				lastTargetMsg = "Auto-Step Completed @ Vector #" + QString::number(presentVector + 1);
				setStatusMsg(lastTargetMsg);
				autostepTimer->stop();
				ui.autoStepGroupBox->setTitle("Auto-Stepping");
				enableVectorTableControls();
				doAutosaveReport();
				haveExecuted = false;
				suspendAutostepFlag = false;
			}
		}
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::doAutosaveReport(void)
{
	// auto report generation?
	if (ui.actionAutosave_Report->isChecked() && !haveAutosavedReport)
	{
		// guess appropriate filename using lastVectorsLoadPath
		if (!lastVectorsLoadPath.isEmpty())
		{
			if (QFileInfo::exists(lastVectorsLoadPath))
			{
				QFileInfo file(lastVectorsLoadPath);
				QString filepath = file.absolutePath();
				QString filename = file.baseName();	// no extension

				if (!filepath.isEmpty())
				{
					QString reportName = filepath + "/" + filename + ".xlsx";

					int i = 1;
					while (QFileInfo::exists(reportName))
					{
						reportName = filepath + "/" + filename + "." + QString::number(i) + ".xlsx";
						i++;
					}

					// filename constructed, now autosave the report
					saveReport(reportName);

					QFileInfo reportFile(reportName);

					if (reportFile.exists())
					{
						setStatusMsg(statusMisc->text() + " : Saved as " + reportFile.fileName());
						haveAutosavedReport = true;
					}
				}
			}
		}
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::enableVectorTableControls(void)
{
	ui.startIndexEdit->setEnabled(true);
	ui.endIndexEdit->setEnabled(true);
	ui.autostepRemainingTimeLabel->setEnabled(false);
	ui.autostepRemainingTimeValue->setEnabled(false);
	ui.autostepStartButton->setEnabled(true);
	ui.autostartStopButton->setEnabled(false);
	ui.appFrame->setEnabled(true);
	ui.executeCheckBox->setEnabled(true);
	ui.executeNowButton->setEnabled(true);
	ui.manualVectorControlGroupBox->setEnabled(true);
	ui.actionLoad_Vector_Table->setEnabled(true);

	vectorSelectionChanged();

	ui.manualPolarControlGroupBox->setEnabled(true);
	ui.autoStepGroupBoxPolar->setEnabled(true);
	ui.actionLoad_Polar_Table->setEnabled(true);
	if ((switchInstalled = magnetParams->switchInstalled()))
		ui.actionPersistentMode->setEnabled(true);

	if (ui.actionPersistentMode->isChecked())	// oops! heater is OFF, persistent
	{
		// don't allow target changes in persistent mode
		ui.manualVectorControlGroupBox->setEnabled(false);
		ui.autoStepGroupBox->setEnabled(false);
		ui.manualPolarControlGroupBox->setEnabled(false);
		ui.autoStepGroupBoxPolar->setEnabled(false);
	}
}


//---------------------------------------------------------------------------
// App/Script management and processing support
//---------------------------------------------------------------------------
void MultiAxisOperation::browseForAppPath(void)
{
	QString exeFileName;
	QSettings settings;
	QString extension = NULL;

	lastAppFilePath = settings.value("LastAppFilePath").toString();

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
	exeFileName = QFileDialog::getOpenFileName(this, "Choose App/Script File", lastAppFilePath, "All Files (*);;Script Files (*.py)", &extension);
#elif defined(Q_OS_MACOS)
	exeFileName = QFileDialog::getOpenFileName(this, "Choose App/Script File", lastAppFilePath, "Applications (*.app);;Script Files (*.py);;All Files (*)", &extension);
#else
	exeFileName = QFileDialog::getOpenFileName(this, "Choose App/Script File", lastAppFilePath, "Applications (*.exe);;Script Files (*.py)", &extension);
#endif

	if (!exeFileName.isEmpty())
	{
		lastAppFilePath = exeFileName;
		ui.appLocationEdit->setText(exeFileName);

		// save path
		settings.setValue("LastAppFilePath", lastAppFilePath);
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::browseForPythonPath(void)
{
	QString pythonFileName;
	QSettings settings;

	lastPythonPath = settings.value("LastPythonPath").toString();

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
	pythonFileName = QFileDialog::getOpenFileName(this, "Choose Python Executable", lastPythonPath, "Python Executable (*)");
#else
	pythonFileName = QFileDialog::getOpenFileName(this, "Choose Python Executable", lastPythonPath, "Python Executable (*.exe)");
#endif

	if (!pythonFileName.isEmpty())
	{
		lastPythonPath = pythonFileName;
		ui.pythonPathEdit->setText(pythonFileName);

		// save path
		settings.setValue("LastPythonPath", lastPythonPath);
	}
}

//---------------------------------------------------------------------------
bool MultiAxisOperation::checkExecutionTime(void)
{
	bool ok = false;

	int temp = ui.appStartEdit->text().toInt(&ok);

	if (ok)
	{
		// must be >= 0
		if (temp < 0)
			ok = false;
	}

	return ok;
}

//---------------------------------------------------------------------------
void MultiAxisOperation::executeNowClick(void)
{
	if (ui.executeCheckBox->isChecked() && !QFile::exists(ui.appLocationEdit->text()))
	{
		showErrorString("App/script does not exist at specified path!");
	}
	else if (ui.executeCheckBox->isChecked() && !checkExecutionTime())
	{
		showErrorString("App/script execution time is not a positive, non-zero integer!");
	}
	else if (ui.executeCheckBox->isChecked() && ui.pythonCheckBox->isChecked() && !QFile::exists(ui.pythonPathEdit->text()))
	{
		showErrorString("Python not found at specified path!");
	}
	else // all good! start!
	{
		statusMisc->setText(lastStatusString);
		executeApp();
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::executeApp(void)
{
	int precision = 6;
	QString program = ui.appLocationEdit->text();
	QString args = ui.appArgsEdit->text();
	QStringList arguments = args.split(" ", Qt::SkipEmptyParts);

	if (fieldUnits == TESLA)
		precision = 7;

	// loop through argument list and replace "special" variables with present value
	for (int i = 0; i < arguments.count(); i++)
	{
		QString testString = arguments[i].toUpper();

		if (testString == "%MAGNITUDE%" || testString == "$MAGNITUDE" || testString == "%MAG%" || testString == "$MAG")
			arguments[i] = QString::number(avoidSignedZeroOutput(magnitudeField, precision), 'f', precision);
		else if (testString == "%AZIMUTH%" || testString == "$AZIMUTH" || testString == "%AZ%" || testString == "$AZ")
			arguments[i] = QString::number(avoidSignedZeroOutput(thetaAngle, 4), 'f', 4);
		else if (testString == "%INCLINATION%" || testString == "$INCLINATION" || testString == "%INC%" || testString == "$INC")
			arguments[i] = QString::number(avoidSignedZeroOutput(phiAngle, 4), 'f', 4);
		else if (testString == "%FIELDX%" || testString == "$FIELDX")
			arguments[i] = QString::number(avoidSignedZeroOutput(xField, precision), 'f', precision);
		else if (testString == "%FIELDY%" || testString == "$FIELDY")
			arguments[i] = QString::number(avoidSignedZeroOutput(yField, precision), 'f', precision);
		else if (testString == "%FIELDZ%" || testString == "$FIELDZ")
			arguments[i] = QString::number(avoidSignedZeroOutput(zField, precision), 'f', precision);

		if (testString.contains("TARG"))
		{
			// prep for substituting a TARGET value in the field
			double x, y, z, mag, az, inc;

			this->get_active_cartesian(&x, &y, &z);
			this->get_active(&mag, &az, &inc);

			if (testString == "%TARG:MAG%" || testString == "$TARG:MAG")
				arguments[i] = QString::number(avoidSignedZeroOutput(mag, precision), 'f', precision);
			else if (testString == "%TARG:AZ%" || testString == "$TARG:AZ")
				arguments[i] = QString::number(avoidSignedZeroOutput(az, 4), 'f', 4);
			else if (testString == "%TARG:INC%" || testString == "$TARG:INC")
				arguments[i] = QString::number(avoidSignedZeroOutput(inc, 4), 'f', 4);
			else if (testString == "%TARG:X%" || testString == "$TARG:X")
				arguments[i] = QString::number(avoidSignedZeroOutput(x, precision), 'f', precision);
			else if (testString == "%TARG:Y%" || testString == "$TARG:Y")
				arguments[i] = QString::number(avoidSignedZeroOutput(y, precision), 'f', precision);
			else if (testString == "%TARG:Z%" || testString == "$TARG:Z")
				arguments[i] = QString::number(avoidSignedZeroOutput(z, precision), 'f', precision);
		}
	}

	// if a python script, use python path for executable
	if (ui.pythonCheckBox->isChecked())
	{
		program = ui.pythonPathEdit->text();
		arguments.insert(0, ui.appLocationEdit->text());
	}

	// launch detached background process
	process = new QProcess(this);
	process->setProgram(program);
	process->setArguments(arguments);

	connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
		[=](int exitCode, QProcess::ExitStatus exitStatus) { finishedApp(exitCode, exitStatus); });

	// disable Execute Now and Start buttons
	ui.executeNowButton->setEnabled(false);
	ui.autostepStartButton->setEnabled(false);
	ui.executePolarNowButton->setEnabled(false);
	ui.autostepStartButtonPolar->setEnabled(false);

	process->start(program, arguments);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::finishedApp(int exitCode, QProcess::ExitStatus exitStatus)
{
	if (!connected)
		return;

	QString output = process->readAllStandardOutput();

	// strip cr/lf
	output = output.simplified();

	process->deleteLater();
	process = nullptr;

	// check for error state, and if error stop autostepping and show error
	if (exitCode)
	{
		if (autostepTimer->isActive())	// first checks for active autostep sequence
		{
			autostepTimer->stop();
			ui.autoStepGroupBox->setTitle("Auto-Stepping");
			pinErrorString("Auto-stepping aborted: " + output, true);
			enableVectorTableControls();
			autostepRangeChanged();
			haveExecuted = false;
		}
		else
			pinErrorString(output, true);
	}

	if (!autostepTimer->isActive())	// re-enable Execute Now buttons
	{
		ui.executeNowButton->setEnabled(true);
		ui.autostepStartButton->setEnabled(true);
		ui.executePolarNowButton->setEnabled(true);
		ui.autostepStartButtonPolar->setEnabled(true);
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::appCheckBoxChanged(int state)
{
	if (ui.executeCheckBox->isChecked())
	{
		ui.appFrame->setVisible(true);
		ui.executeNowButton->setVisible(true);
	}
	else
	{
		ui.appFrame->setVisible(false);
		ui.executeNowButton->setVisible(false);
	}

	pythonCheckBoxChanged(0);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::pythonCheckBoxChanged(int state)
{
	if (ui.pythonCheckBox->isChecked())
	{
		ui.pythonPathLabel->setVisible(true);
		ui.pythonPathEdit->setVisible(true);
		ui.pythonLocationButton->setVisible(true);
	}
	else
	{
		ui.pythonPathLabel->setVisible(false);
		ui.pythonPathEdit->setVisible(false);
		ui.pythonLocationButton->setVisible(false);
	}
}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------