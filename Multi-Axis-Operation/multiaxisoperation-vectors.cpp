#include "stdafx.h"
#include "multiaxisoperation.h"
#include "conversions.h"
#include <QtXlsxWriter/xlsxdocument.h>


//---------------------------------------------------------------------------
// Contains methods related to the Vector Table tab view.
// Broken out from multiaxisoperation.cpp for ease of editing.
//---------------------------------------------------------------------------
void MultiAxisOperation::actionLoad_Vector_Table(void)
{
	QSettings settings;
	int skipCnt = 0;
	lastVectorsLoadPath = settings.value("LastVectorFilePath").toString();
	bool convertFieldUnits = false;

	if (autostepTimer->isActive())
		return;	// no load during autostepping

	vectorsFileName = QFileDialog::getOpenFileName(this, "Choose Vector File", lastVectorsLoadPath, "Vector Definition Files (*.txt *.log *.csv)");

	if (!vectorsFileName.isEmpty())
	{
		QApplication::setOverrideCursor(Qt::WaitCursor);

		lastVectorsLoadPath = vectorsFileName;

		FILE *inFile;
		inFile = fopen(vectorsFileName.toLocal8Bit(), "r");

		if (inFile != NULL)
		{
			QTextStream in(inFile);

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

		// clear status text
		statusMisc->clear();

		// if ramping, set system to PAUSE
		actionPause();

		// now read in data
		ui.vectorsTableWidget->loadFromFile(vectorsFileName, false, skipCnt);

		// save path
		settings.setValue("LastVectorFilePath", lastVectorsLoadPath);

		presentVector = -1;	// no selection
		//ui.vectorsTableWidget->selectRow(presentVector);

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
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::setTableHeader(void)
{
	if (loadedCoordinates == SPHERICAL_COORDINATES)
	{
		if (fieldUnits == KG)
		{
			ui.vectorsTableWidget->horizontalHeaderItem(0)->setText("Mag (kG)");
		}
		else
		{
			ui.vectorsTableWidget->horizontalHeaderItem(0)->setText("Mag (T)");
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
		if (outFile != NULL)
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

			flush(out);
			fclose(outFile);
		}

		// save table contents
		ui.vectorsTableWidget->saveToFile(saveVectorsFileName);

		// save path
		settings.setValue("LastVectorSavePath", lastVectorsSavePath);

		QApplication::restoreOverrideCursor();
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
			xlsx.write(4, i + 1, ui.vectorsTableWidget->horizontalHeaderItem(i)->text(), boldAlignCenterFormat);

		xlsx.write("F4", "X-Axis", boldAlignCenterFormat);
		xlsx.write("G4", "Y-Axis", boldAlignCenterFormat);
		xlsx.write("H4", "Z-Axis", boldAlignCenterFormat);

		// output vector data
		for (int i = 0; i < ui.vectorsTableWidget->rowCount(); i++)
		{
			for (int j = 0; j < ui.vectorsTableWidget->columnCount(); j++)
			{
				QTableWidgetItem *item;

				if (item = ui.vectorsTableWidget->item(i, j))
				{
					if (item->text() == "Pass" || item->text() == "Fail" || item->text().isEmpty())
						xlsx.write(i + 1 + 4, j + 1, ui.vectorsTableWidget->item(i, j)->text(), alignCenterFormat);
					else
						xlsx.write(i + 1 + 4, j + 1, ui.vectorsTableWidget->item(i, j)->text().toDouble(), alignRightFormat);
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
		// find selected vector
		QList<QTableWidgetItem *> list = ui.vectorsTableWidget->selectedItems();

		if (list.count())
		{
			presentVector = list[0]->row();
			magnetState = RAMPING;
			goToVector();
		}
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::goToNextVector(void)
{
	if (connected)
	{
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
				goToVector();
			}
		}
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::goToVector(void)
{
	double coord1, coord2, coord3;
	double x, y, z;
	bool ok, error = false;
	double temp;

	// get vector values and check for numerical conversion
	temp = ui.vectorsTableWidget->item(presentVector, 0)->text().toDouble(&ok);
	if (ok)
		coord1 = temp;
	else
		error = true;	// error

	temp = ui.vectorsTableWidget->item(presentVector, 1)->text().toDouble(&ok);
	if (ok)
		coord2 = temp;
	else
		error = true;	// error

	temp = ui.vectorsTableWidget->item(presentVector, 2)->text().toDouble(&ok);
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
				vectorError = true;
				showErrorString("Vector #" + QString::number(presentVector + 1) + " : Magnitude of vector cannot be a negative value");	// error annunciation
				abortAutostep();
				return;
			}
			if (coord3 < 0.0 || coord3 > 180.0)	// angle from Z-axis must be >= 0 and <= 180 degrees
			{
				vectorError = true;
				showErrorString("Vector #" + QString::number(presentVector + 1) + " : Angle from Z-axis must be from 0 to 180 degrees");	// error annunciation
				abortAutostep();
				return;
			}

			sphericalToCartesian(coord1, coord2, coord3, &x, &y, &z);
		}

		// attempt to go to vector
		if (!(vectorError = !sendNextVector(x, y, z)))
		{
			if (autostepTimer->isActive())
			{
				statusMisc->setText("Auto-Stepping : Vector Table #" + QString::number(presentVector + 1));
			}
			else
			{
				statusMisc->setText("Active Vector : Vector Table #" + QString::number(presentVector + 1));
			}
		}
		else
		{
			abortAutostep();
		}
	}
	else
	{
		vectorError = true;
		showErrorString("Vector #" + QString::number(presentVector + 1) + " has non-numerical entry");	// error annunciation
		abortAutostep();
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::startAutostep(void)
{
	if (!connected)
		return;

	autostepStartIndex = ui.startIndexEdit->text().toInt();
	autostepEndIndex = ui.endIndexEdit->text().toInt();

	if (autostepStartIndex < 1 || autostepStartIndex > ui.vectorsTableWidget->rowCount())
	{
		showErrorString("Starting Index is out of range!");
		return;
	}

	if (autostepEndIndex <= autostepStartIndex || autostepEndIndex > ui.vectorsTableWidget->rowCount())
	{
		showErrorString("Ending Index is out of range!");
		return;
	}

	ui.startIndexEdit->setEnabled(false);
	ui.endIndexEdit->setEnabled(false);

	elapsedTimerTicks = 0;
	autostepTimer->start();

	ui.autostepStartButton->setEnabled(false);
	ui.autostartStopButton->setEnabled(true);
	ui.manualVectorControlGroupBox->setEnabled(false);
	ui.actionLoad_Vector_Table->setEnabled(false);

	// begin with first vector
	presentVector = autostepStartIndex - 1;

	// highlight row in table
	ui.vectorsTableWidget->selectRow(presentVector);
	magnetState = RAMPING;
	haveAutosavedReport = false;

	goToVector();
}

//---------------------------------------------------------------------------
void MultiAxisOperation::abortAutostep()
{
	if (autostepTimer->isActive())	// first checks for active autostep sequence
	{
		autostepTimer->stop();
		while (errorStatusIsActive)	// show any error condition first
		{
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
			Sleep(100);
		}
		statusMisc->setText("Auto-Stepping aborted due to an error with Vector #" + QString::number(presentVector + 1));
		ui.startIndexEdit->setEnabled(true);
		ui.endIndexEdit->setEnabled(true);
		ui.autostepStartButton->setEnabled(true);
		ui.autostartStopButton->setEnabled(false);
		ui.manualVectorControlGroupBox->setEnabled(true);
		ui.actionLoad_Vector_Table->setEnabled(true);
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::stopAutostep(void)
{
	if (autostepTimer->isActive())
	{
		autostepTimer->stop();
		statusMisc->setText("Auto-Stepping aborted via Stop button");
		ui.startIndexEdit->setEnabled(true);
		ui.endIndexEdit->setEnabled(true);
		ui.autostepStartButton->setEnabled(true);
		ui.autostartStopButton->setEnabled(false);
		ui.manualVectorControlGroupBox->setEnabled(true);
		ui.actionLoad_Vector_Table->setEnabled(true);
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::autostepTimerTick(void)
{
	if (magnetState == HOLDING)
	{
		elapsedTimerTicks++;

		bool ok;
		double temp;

		// get time
		temp = ui.vectorsTableWidget->item(presentVector, 3)->text().toDouble(&ok);

		if (ok)
		{
			if (elapsedTimerTicks >= (int)temp)
			{
				elapsedTimerTicks = 0;
				
				if (presentVector + 1 < autostepEndIndex)
				{
					// highlight row in table
					presentVector++;
					ui.vectorsTableWidget->selectRow(presentVector);
					magnetState = RAMPING;

					// next vector!
					goToVector();
				}
				else
				{
					// successfully completed vector table auto-stepping
					statusMisc->setText("Auto-Step Completed @ Vector #" + QString::number(presentVector + 1));
					autostepTimer->stop();
					ui.startIndexEdit->setEnabled(true);
					ui.endIndexEdit->setEnabled(true);
					ui.autostepStartButton->setEnabled(true);
					ui.autostartStopButton->setEnabled(false);
					ui.manualVectorControlGroupBox->setEnabled(true);
					doAutosaveReport();
				}
			}
			else
			{
				// update the HOLDING countdown
				QString tempStr = statusMisc->text();
				int index = tempStr.indexOf('(');
				if (index >= 1)
					tempStr.truncate(index - 1);

				QString timeStr = " (" + QString::number(temp - elapsedTimerTicks) + " sec remaining)";
				statusMisc->setText(tempStr + timeStr);
			}
		}
		else
		{
			autostepTimer->stop();
			statusMisc->setText("Auto-Stepping aborted due to unknown dwell time on line #" + QString::number(presentVector + 1));
			ui.startIndexEdit->setEnabled(true);
			ui.endIndexEdit->setEnabled(true);
			ui.autostepStartButton->setEnabled(true);
			ui.autostartStopButton->setEnabled(false);
			ui.manualVectorControlGroupBox->setEnabled(true);
		}
	}
	else
	{
		elapsedTimerTicks = 0;

		// remove any erroneous countdown text
		QString tempStr = statusMisc->text();
		int index = tempStr.indexOf('(');
		if (index >= 1)
			tempStr.truncate(index - 1);

		statusMisc->setText(tempStr);
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
						statusMisc->setText(statusMisc->text() + " : Saved as " + reportFile.fileName());
						haveAutosavedReport = true;
					}
				}
			}
		}
	}
}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------