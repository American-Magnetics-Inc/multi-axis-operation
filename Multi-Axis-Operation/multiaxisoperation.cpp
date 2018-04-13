#include "stdafx.h"
#include "multiaxisoperation.h"
#include "magnetparams.h"
#include "conversions.h"
#include "aboutdialog.h"

// minimum programmable ramp rate (A/s) for purposes of multi-axis control
const double MIN_RAMP_RATE = 0.001;

// load/save settings file version
const int SAVE_FILE_VERSION = 2;


//---------------------------------------------------------------------------
// Constructor
//---------------------------------------------------------------------------
MultiAxisOperation::MultiAxisOperation(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	// no context menu for toolbar
	ui.mainToolBar->setContextMenuPolicy(Qt::PreventContextMenu);

	// min columns for vector table
	ui.vectorsTableWidget->setMinimumNumCols(5);

	// initialization
	magnetParams = NULL;
	xProcess = NULL;
	yProcess = NULL;
	zProcess = NULL;
	xField = NAN;
	yField = NAN;
	zField = NAN;
	fieldUnits = KG;
	xState = ERROR_STATE;
	yState = ERROR_STATE;
	zState = ERROR_STATE;
	connected = false;
	passCnt = 0;
	simulation = false;
	vectorError = false;
	presentVector = -1;
	lastVector = -1;
	errorStatusIsActive = false;

	// setup status bar
	statusConnectState = new QLabel("", this);
	statusConnectState->setFrameStyle(QFrame::Panel | QFrame::Sunken);
	statusConnectState->setToolTip("Connection status");
	statusConnectState->setFont(QFont("Segoe UI", 9));
	statusMisc = new QLabel("", this);
	statusMisc->setFrameStyle(QFrame::Panel | QFrame::Sunken);
	statusMisc->setFont(QFont("Segoe UI", 9));
	statusState = new QLabel("", this);
	statusState->setFrameStyle(QFrame::Panel | QFrame::Sunken);
	statusState->setFont(QFont("Segoe UI", 9));
	statusState->setToolTip("Operating state");

	statusBar()->addPermanentWidget(statusConnectState, 1);
	statusBar()->addPermanentWidget(statusMisc, 4);
	statusBar()->addPermanentWidget(statusState, 1);
	statusConnectState->setStyleSheet("color: red; font: bold;");
	statusConnectState->setText("DISCONNECTED");

	// restore window position and gui state
	QSettings settings;

	// restore different geometry for different DPI screens
	QString dpiStr = QString::number(QApplication::desktop()->screen()->logicalDpiX());
	restoreGeometry(settings.value("MainWindow/Geometry/" + dpiStr).toByteArray());
	restoreState(settings.value("MainWindow/WindowState/" + dpiStr).toByteArray());
	fieldUnits = (FieldUnits)(settings.value("FieldUnits").toInt());
	convention = (SphericalConvention)(settings.value("SphericalConvention").toInt());
	ui.actionAutosave_Report->setChecked(settings.value("AutosaveReport").toBool());

	// disable vector selection until connect
	ui.manualVectorControlGroupBox->setEnabled(false);
	ui.autostepStartButton->setEnabled(false);

	// create data collection timer
	dataTimer = new QTimer(this);
	dataTimer->setInterval(1000);	// update once per second
	connect(dataTimer, SIGNAL(timeout()), this, SLOT(dataTimerTick()));

	// create switch heating timer
	switchHeatingTimer = new QTimer(this);
	switchHeatingTimer->setInterval(1000);
	connect(switchHeatingTimer, SIGNAL(timeout()), this, SLOT(switchHeatingTimerTick()));

	// create autostep timer
	autostepTimer = new QTimer(this);
	autostepTimer->setInterval(1000);	// update once per second
	connect(autostepTimer, SIGNAL(timeout()), this, SLOT(autostepTimerTick()));

	// create error status timer
	errorStatusTimer = new QTimer(this);
	
	// setup toolbar action groups
	unitsGroup = new QActionGroup(this);
	unitsGroup->addAction(ui.actionKilogauss);
	unitsGroup->addAction(ui.actionTesla);

	sphericalConvention = new QActionGroup(this);
	sphericalConvention->addAction(ui.actionUse_Mathematical_Convention);
	sphericalConvention->addAction(ui.actionUse_ISO_Convention);
	thetaStr = ui.magnetThetaLabel->text();
	phiStr = ui.magnetPhiLabel->text();

	// connect toolbar actions
	connect(ui.actionAbout, SIGNAL(triggered()), this, SLOT(actionAbout()));
	connect(ui.actionView_Help, SIGNAL(triggered()), this, SLOT(actionHelp()));
	connect(ui.actionConnect, SIGNAL(triggered()), this, SLOT(actionConnect()));
	connect(ui.actionLoad_Settings, SIGNAL(triggered()), this, SLOT(actionLoad_Settings()));
	connect(ui.actionSave_Settings, SIGNAL(triggered()), this, SLOT(actionSave_Settings()));
	connect(ui.actionDefine, SIGNAL(triggered()), this, SLOT(actionDefine()));
	connect(ui.actionLoad_Vector_Table, SIGNAL(triggered()), this, SLOT(actionLoad_Vector_Table()));
	connect(ui.actionSave_Vector_Table, SIGNAL(triggered()), this, SLOT(actionSave_Vector_Table()));
	connect(ui.actionExit, SIGNAL(triggered()), this, SLOT(close()));
	connect(ui.actionShow_Cartesian_Coordinates, SIGNAL(triggered()), this, SLOT(actionShow_Cartesian_Coordinates()));
	connect(ui.actionShow_Spherical_Coordinates, SIGNAL(triggered()), this, SLOT(actionShow_Spherical_Coordinates()));
	connect(ui.actionPause, SIGNAL(triggered()), this, SLOT(actionPause()));
	connect(ui.actionRamp, SIGNAL(triggered()), this, SLOT(actionRamp()));
	connect(ui.actionZero, SIGNAL(triggered()), this, SLOT(actionZero()));
	connect(ui.actionKilogauss, SIGNAL(triggered()), this, SLOT(actionChange_Units()));
	connect(ui.actionTesla, SIGNAL(triggered()), this, SLOT(actionChange_Units()));
	connect(ui.actionUse_Mathematical_Convention, SIGNAL(triggered()), this, SLOT(actionChange_Convention()));
	connect(ui.actionUse_ISO_Convention, SIGNAL(triggered()), this, SLOT(actionChange_Convention()));
	connect(ui.action_SphericalHelp, SIGNAL(triggered()), this, SLOT(actionConvention_Help()));
	connect(ui.actionGenerate_Excel_Report, SIGNAL(triggered()), this, SLOT(actionGenerate_Excel_Report()));

	// create magnetParams dialog
	magnetParams = new MagnetParams();

	// set field units to saved preference
	setFieldUnits(fieldUnits, true);

	// set spherical coordinate labeling convention
	setSphericalConvention(convention, true);

	updateWindowTitle();

	// parse command line options
	QCommandLineParser parser;

	// Connect to simulated system (--simulate)
	QCommandLineOption portOption("simulate");
	parser.addOption(portOption);

	// Process the actual command line arguments given by the user
	parser.process(*(QCoreApplication::instance()));

	if (parser.isSet(portOption))
		simulation = true;	// use simulated system
}

//---------------------------------------------------------------------------
void MultiAxisOperation::updateWindowTitle()
{
	// set window title
	if (magnetParams->getMagnetID().isEmpty())
		this->setWindowTitle("Multi-Axis Operation - v" + QCoreApplication::applicationVersion());
	else
		this->setWindowTitle("Magnet ID " + magnetParams->getMagnetID() + " - Multi-Axis Operation - v" + QCoreApplication::applicationVersion());
}

//---------------------------------------------------------------------------
// Destructor
//---------------------------------------------------------------------------
MultiAxisOperation::~MultiAxisOperation()
{
	closeConnection();

	// delete timers
	delete dataTimer;
	dataTimer = NULL;
	delete switchHeatingTimer;
	switchHeatingTimer = NULL;
	delete autostepTimer;
	autostepTimer = NULL;
	delete errorStatusTimer;
	errorStatusTimer = NULL;

	if (magnetParams)
		delete magnetParams;

	// save window position and state
	QSettings settings;

	// save different geometry for different DPI screens
	QString dpiStr = QString::number(QApplication::desktop()->screen()->logicalDpiX());

	settings.setValue("MainWindow/Geometry/" + dpiStr, saveGeometry());
	settings.setValue("MainWindow/WindowState/" + dpiStr, saveState());
	settings.setValue("FieldUnits", (int)fieldUnits);
	settings.setValue("SphericalConvention", (int)convention);
	settings.setValue("AutosaveReport", ui.actionAutosave_Report->isChecked());
}

//---------------------------------------------------------------------------
void MultiAxisOperation::closeConnection()
{
	connected = false;

	// close and delete all connected processes
	if (xProcess)
	{
		xProcess->deleteLater();
		xProcess = NULL;
	}

	if (yProcess)
	{
		yProcess->deleteLater();
		yProcess = NULL;
	}

	if (zProcess)
	{
		zProcess->deleteLater();
		zProcess = NULL;
	}

	statusConnectState->setStyleSheet("color: red; font: bold;");
	statusConnectState->setText("DISCONNECTED");
	ui.actionKilogauss->setEnabled(true);
	ui.actionTesla->setEnabled(true);
	ui.actionTest_Mode->setEnabled(true);
	ui.actionLoad_Settings->setEnabled(true);
	ui.manualVectorControlGroupBox->setEnabled(false);
	ui.autostepStartButton->setEnabled(false);

	statusState->clear();
	ui.actionConnect->setChecked(false);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionAbout(void)
{
	aboutdialog about;
	about.exec();
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionHelp(void)
{
#if defined(Q_OS_MACOS)
	QString link = "file:///" + QCoreApplication::applicationDirPath() + "//..//Resources//Help//index.html";
#else
	QString link = "file:///" + QCoreApplication::applicationDirPath() + "//Help//index.html";
#endif

	QDesktopServices::openUrl(QUrl(link));
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionConnect(void)
{
	bool x_activated = !magnetParams->GetXAxisParams()->activate;
	bool y_activated = !magnetParams->GetYAxisParams()->activate;
	bool z_activated = !magnetParams->GetZAxisParams()->activate;
	longestHeatingTime = 0;

	if (ui.actionConnect->isChecked())
	{
		QProgressDialog progressDialog;
		QApplication::setOverrideCursor(Qt::WaitCursor);
		statusConnectState->setStyleSheet("color: green; font: bold;");
		statusConnectState->setText("CONNECTING...");
		statusMisc->setText("Launching processes, please wait...");
		
		progressDialog.setFont(QFont("Segoe UI", 9));
		progressDialog.setLabelText(QString("Launching processes, please wait...                   "));
		progressDialog.setWindowTitle("Multi-Axis Operation Connect...");
		progressDialog.show();
		progressDialog.setWindowModality(Qt::WindowModal);
		progressDialog.setValue(0);
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		// if X-axis active, create a QProcess controller
		if (magnetParams->GetXAxisParams()->activate)
		{
			if (xProcess == NULL)
				xProcess = new ProcessManager(this);

			if (!xProcess->isActive())
				xProcess->connectProcess(magnetParams->GetXAxisParams()->ipAddress, X_AXIS, simulation);
		}

		// check for cancellation
		progressDialog.setValue(25);
		if (progressDialog.wasCanceled())
		{
			closeConnection();
			statusMisc->setText("Connect action canceled");
			goto EXIT;
		}
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		// if Y-axis active, create a QProcess controller
		if (magnetParams->GetYAxisParams()->activate)
		{
			if (yProcess == NULL)
				yProcess = new ProcessManager(this);

			if (!yProcess->isActive())
				yProcess->connectProcess(magnetParams->GetYAxisParams()->ipAddress, Y_AXIS, simulation);
		}

		// check for cancellation
		progressDialog.setValue(50);
		if (progressDialog.wasCanceled())
		{
			closeConnection();
			statusMisc->setText("Connect action canceled");
			goto EXIT;
		}
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		// if Z-axis active, create a QProcess controller
		if (magnetParams->GetZAxisParams()->activate)
		{
			if (zProcess == NULL)
				zProcess = new ProcessManager(this);

			if (!zProcess->isActive())
				zProcess->connectProcess(magnetParams->GetZAxisParams()->ipAddress, Z_AXIS, simulation);
		}

		// check for cancellation
		progressDialog.setValue(75);
		if (progressDialog.wasCanceled())
		{
			closeConnection();
			statusMisc->setText("Connect action canceled");
			goto EXIT;
		}
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		// if X-axis active, send setup parameters
		if (magnetParams->GetXAxisParams()->activate)
		{
			if (xProcess->isActive())
			{
				xProcess->sendParams(magnetParams->GetXAxisParams(), fieldUnits, ui.actionTest_Mode->isChecked());
				x_activated = true;

				// switch heating required?
				if (magnetParams->GetXAxisParams()->switchInstalled)
				{
					longestHeatingTime = magnetParams->GetXAxisParams()->switchHeatingTime;
				}
			}
		}

		// check for cancellation
		progressDialog.setValue(80);
		if (progressDialog.wasCanceled())
		{
			closeConnection();
			statusMisc->setText("Connect action canceled");
			goto EXIT;
		}
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		// if Y-axis active, send setup parameters
		if (magnetParams->GetYAxisParams()->activate)
		{
			if (yProcess->isActive())
			{
				yProcess->sendParams(magnetParams->GetYAxisParams(), fieldUnits, ui.actionTest_Mode->isChecked());
				y_activated = true;

				// switch heating required?
				if (magnetParams->GetYAxisParams()->switchInstalled && (magnetParams->GetYAxisParams()->switchHeatingTime > longestHeatingTime))
				{
					longestHeatingTime = magnetParams->GetYAxisParams()->switchHeatingTime;
				}
			}
		}

		// check for cancellation
		progressDialog.setValue(90);
		if (progressDialog.wasCanceled())
		{
			closeConnection();
			statusMisc->setText("Connect action canceled");
			goto EXIT;
		}
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		// if Z-axis active, send setup parameters
		if (magnetParams->GetZAxisParams()->activate)
		{
			if (zProcess->isActive())
			{
				zProcess->sendParams(magnetParams->GetZAxisParams(), fieldUnits, ui.actionTest_Mode->isChecked());
				z_activated = true;

				// switch heating required?
				if (magnetParams->GetZAxisParams()->switchInstalled && (magnetParams->GetZAxisParams()->switchHeatingTime > longestHeatingTime))
				{
					longestHeatingTime = magnetParams->GetZAxisParams()->switchHeatingTime;
				}
			}
		}

		progressDialog.setValue(100);
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		// start data collection and update interface to indicate successfully connected status
		if (x_activated && y_activated && z_activated)
		{
			connected = true;

			// start data collection timer
			dataTimer->start();

			// if a switch is present, wait the heating time
			if (longestHeatingTime > 0)
			{
				// switch heating already initiated by ProcessManager::sendParams()
				// start switch heating timer
				statusMisc->setText("Heating switches, please wait...");
				elapsedHeatingTicks = 0;
				switchHeatingTimer->start();
				ui.menuBar->setEnabled(false);
				ui.mainTabWidget->setEnabled(false);
				ui.mainToolBar->setEnabled(false);
			}
			else
			{
				statusMisc->setText("All active axes initialized successfully");
			}

			statusConnectState->setStyleSheet("color: green; font: bold;");
			statusConnectState->setText("CONNECTED");
			
			statusState->setStyleSheet("color: black; font: bold;");
			statusState->setText("PAUSED");

			// don't change field units while connected
			ui.actionKilogauss->setEnabled(false);
			ui.actionTesla->setEnabled(false);

			// don't change test mode while connected
			ui.actionTest_Mode->setEnabled(false);

			// can't load settings while connected
			ui.actionLoad_Settings->setEnabled(false);

			// allow vector selection
			ui.manualVectorControlGroupBox->setEnabled(true);
			ui.autostepStartButton->setEnabled(true);
		}
		else // something went wrong, indicate an error
		{
			closeConnection();
			statusMisc->setText("Failed to communicate with all active magnet axes");
		}

EXIT:
		progressDialog.close();
		QApplication::restoreOverrideCursor();
	}
	else // operator initiated disconnection
	{
		// stop data collection timer
		dataTimer->stop();

		// stop any active auto-stepping
		stopAutostep();

		closeConnection();
		statusMisc->clear();
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionLoad_Settings(void)
{
	QSettings settings;
	lastSavePath = settings.value("LastSavePath").toString();

	saveFileName = QFileDialog::getOpenFileName(this, "Load All Settings", lastSavePath, "Saved Settings (*.sav)");

	if (!saveFileName.isEmpty())
	{
		// save load path
		lastSavePath = saveFileName;
		settings.setValue("LastSavePath", lastSavePath);

		FILE *pFile = fopen(saveFileName.toLocal8Bit().constData(), "r");

		if (pFile == NULL)
		{
			// load error
			QMessageBox msgBox;

			msgBox.setWindowTitle("Load Settings Error!");
			msgBox.setText(saveFileName + " could not be opened.");
			msgBox.setStandardButtons(QMessageBox::Ok);
			msgBox.setDefaultButton(QMessageBox::Ok);
			msgBox.setIcon(QMessageBox::Critical);
			int ret = msgBox.exec();
		}
		else
		{
			AxesParams *params;
			QTextStream stream(pFile, QIODevice::ReadOnly);
			int version;

			// read file version
			version = stream.readLine().toInt();

			if (version >= 1)
			{
				// paths
				lastSavePath = stream.readLine();
				lastVectorsLoadPath = stream.readLine();
				lastVectorsSavePath = stream.readLine();
				lastReportPath = stream.readLine();

				// preferences and ui settings
				ui.actionShow_Cartesian_Coordinates->setChecked((bool)(stream.readLine().toInt()));
				actionShow_Cartesian_Coordinates();
				ui.actionShow_Spherical_Coordinates->setChecked((bool)(stream.readLine().toInt()));
				actionShow_Spherical_Coordinates();
				fieldUnits = (FieldUnits)(stream.readLine().toInt());
				setFieldUnits(fieldUnits, true);
				convention = (SphericalConvention)(stream.readLine().toInt());
				setSphericalConvention(convention, true);
				ui.actionAutosave_Report->setChecked((bool)(stream.readLine().toInt()));
				ui.startIndexEdit->setText(stream.readLine());
				ui.endIndexEdit->setText(stream.readLine());

				// magnet params
				magnetParams->setMagnetID(stream.readLine());
				magnetParams->setMagnitudeLimit(stream.readLine().toDouble());

				// x-axis params
				params = magnetParams->GetXAxisParams();
				loadParams(&stream, params);

				// y-axis params
				params = magnetParams->GetYAxisParams();
				loadParams(&stream, params);

				// z-axis params
				params = magnetParams->GetZAxisParams();
				loadParams(&stream, params);

				// reload vector table
				ui.vectorsTableWidget->setRowCount(stream.readLine().toInt());

				// load columns count
				int columnCount = stream.readLine().toInt();

				if (version == 1)
				{
					if (columnCount > ui.vectorsTableWidget->columnCount())
					{
						ui.vectorsTableWidget->setColumnCount(columnCount);

						if (columnCount >= 8)
						{
							ui.vectorsTableWidget->setHorizontalHeaderItem(5, new QTableWidgetItem("X Quench (A)"));
							ui.vectorsTableWidget->horizontalHeaderItem(5)->font().setBold(true);
							ui.vectorsTableWidget->setHorizontalHeaderItem(6, new QTableWidgetItem("Y Quench (A)"));
							ui.vectorsTableWidget->horizontalHeaderItem(6)->font().setBold(true);
							ui.vectorsTableWidget->setHorizontalHeaderItem(7, new QTableWidgetItem("Z Quench (A)"));
							ui.vectorsTableWidget->horizontalHeaderItem(7)->font().setBold(true);
						}
					}
					else
						ui.vectorsTableWidget->setColumnCount(columnCount);
				}
				else if (version == 2)
				{
					// recover horizontal header labels (v2)
					if (columnCount > ui.vectorsTableWidget->columnCount())
						ui.vectorsTableWidget->setColumnCount(columnCount);
					else
						ui.vectorsTableWidget->setColumnCount(columnCount);

					for (int i = 0; i < columnCount; i++)
					{
						// read saved header label
						QString tempStr = stream.readLine();
						QTableWidgetItem *item;

						item = ui.vectorsTableWidget->horizontalHeaderItem(i);

						if (item == NULL)
							ui.vectorsTableWidget->setHorizontalHeaderItem(i, item = new QTableWidgetItem(tempStr));
						else
							ui.vectorsTableWidget->horizontalHeaderItem(i)->setText(tempStr);

						ui.vectorsTableWidget->horizontalHeaderItem(i)->font().setBold(true);
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

				// recover table text for each item
				for (int i = 0; i < ui.vectorsTableWidget->rowCount(); i++)
				{
					for (int j = 0; j < columnCount; j++)
					{
						QTableWidgetItem *item;

						item = ui.vectorsTableWidget->item(i, j);

						// read saved text
						QString tempStr = stream.readLine();

						if (item == NULL)
							ui.vectorsTableWidget->setItem(i, j, item = new QTableWidgetItem(tempStr));
						else
							item->setText(tempStr);

						if (j == 4)
							item->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
						else
							item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
					}
				}

				if (version == 2)
				{
					// recover coordinate system selection
					loadedCoordinates = (CoordinatesSelection)(stream.readLine().toInt());

					if (loadedCoordinates == CARTESIAN_COORDINATES)
						setTableHeader();
				}
				else
					loadedCoordinates = SPHERICAL_COORDINATES;	// only type supported < v2
			}

			fclose(pFile);

			// sync the UI with the newly loaded values
			magnetParams->syncUI();
			
			updateWindowTitle();
		}
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::loadParams(QTextStream *stream, AxesParams *params)
{
	params->activate = ((bool)(stream->readLine().toInt()));
	params->ipAddress = stream->readLine();
	params->currentLimit = stream->readLine().toDouble();
	params->voltageLimit = stream->readLine().toDouble();
	params->maxRampRate = stream->readLine().toDouble();
	params->coilConst = stream->readLine().toDouble();
	params->inductance = stream->readLine().toDouble();
	params->switchInstalled = ((bool)(stream->readLine().toInt()));
	params->switchHeaterCurrent = stream->readLine().toDouble();
	params->switchCoolingTime = stream->readLine().toInt();
	params->switchHeatingTime = stream->readLine().toInt();
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionSave_Settings(void)
{
	QSettings settings;
	lastSavePath = settings.value("LastSavePath").toString();

	saveFileName = QFileDialog::getSaveFileName(this, "Save All Settings", lastSavePath, "Saved Settings (*.sav)");

	if (!saveFileName.isEmpty())
	{
		// save path
		lastSavePath = saveFileName;
		settings.setValue("LastSavePath", lastSavePath);

		FILE *pFile = fopen(saveFileName.toLocal8Bit().constData(), "w");
		
		if (pFile == NULL)
		{
			// save error
			QMessageBox msgBox;

			msgBox.setWindowTitle("Save Settings Error!");
			msgBox.setText(saveFileName + " could not be opened for writing.");
			msgBox.setStandardButtons(QMessageBox::Ok);
			msgBox.setDefaultButton(QMessageBox::Ok);
			msgBox.setIcon(QMessageBox::Critical);
			int ret = msgBox.exec();
		}
		else
		{
			AxesParams *params;
			QTextStream stream(pFile, QIODevice::ReadWrite);

			QSettings settings;
			lastSavePath = settings.value("LastSavePath").toString();
			lastVectorsLoadPath = settings.value("LastVectorFilePath").toString();
			lastVectorsSavePath = settings.value("LastVectorSavePath").toString();
			lastReportPath = settings.value("LastReportPath").toString();

			// save file version
			stream << SAVE_FILE_VERSION << "\n";

			// paths
			stream << lastSavePath << "\n";
			stream << lastVectorsLoadPath << "\n";
			stream << lastVectorsSavePath << "\n";
			stream << lastReportPath << "\n";

			// preferences and ui settings
			stream << ui.actionShow_Cartesian_Coordinates->isChecked() << "\n";
			stream << ui.actionShow_Spherical_Coordinates->isChecked() << "\n";
			stream << fieldUnits << "\n";
			stream << convention << "\n";
			stream << ui.actionAutosave_Report->isChecked() << "\n";
			stream << ui.startIndexEdit->text() << "\n";
			stream << ui.endIndexEdit->text() << "\n";

			// magnet params
			stream << magnetParams->getMagnetID() << "\n";
			stream << magnetParams->getMagnitudeLimit() << "\n";

			// x-axis params
			params = magnetParams->GetXAxisParams();
			saveParams(&stream, params);

			// y-axis params
			params = magnetParams->GetYAxisParams();
			saveParams(&stream, params);

			// z-axis params
			params = magnetParams->GetZAxisParams();
			saveParams(&stream, params);

			// save vector table
			stream << ui.vectorsTableWidget->rowCount() << "\n";
			stream << ui.vectorsTableWidget->columnCount() << "\n";

			// save horizontal header labels
			for (int i = 0; i < ui.vectorsTableWidget->columnCount(); i++)
			{
				QTableWidgetItem *item;

				item = ui.vectorsTableWidget->horizontalHeaderItem(i);

				if (item == NULL)
					stream << "\n";
				else
					stream << item->text() << "\n";
			}

			// save vector table contents
			for (int i = 0; i < ui.vectorsTableWidget->rowCount(); i++)
			{
				for (int j = 0; j < ui.vectorsTableWidget->columnCount(); j++)
				{
					QTableWidgetItem *item;

					item = ui.vectorsTableWidget->item(i, j);

					if (item == NULL)
						stream << "\n";
					else
						stream << item->text() << "\n";
				}
			}

			// save coordinate selection
			stream << loadedCoordinates << "\n";

			stream.flush();
			fclose(pFile);
		}
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::saveParams(QTextStream *stream, AxesParams *params)
{
	*stream << params->activate << "\n";
	*stream << params->ipAddress << "\n";
	*stream << params->currentLimit << "\n";
	*stream << params->voltageLimit << "\n";
	*stream << params->maxRampRate << "\n";
	*stream << params->coilConst << "\n";
	*stream << params->inductance << "\n";
	*stream << params->switchInstalled << "\n";
	*stream << params->switchHeaterCurrent << "\n";
	*stream << params->switchCoolingTime << "\n";
	*stream << params->switchHeatingTime << "\n";
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionDefine(void)
{
	if (magnetParams == NULL)
		magnetParams = new MagnetParams();

	if (ui.actionConnect->isChecked())
		magnetParams->setReadOnly();
	else
		magnetParams->clearReadOnly();

	magnetParams->exec();

	updateWindowTitle();
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionShow_Cartesian_Coordinates(void)
{
	if (ui.actionShow_Cartesian_Coordinates->isChecked())
	{
		ui.magnetXLabel->setVisible(true);
		ui.magnetXValue->setVisible(true);
		ui.magnetYLabel->setVisible(true);
		ui.magnetYValue->setVisible(true);
		ui.magnetZLabel->setVisible(true);
		ui.magnetZValue->setVisible(true);
	}
	else
	{
		ui.magnetXLabel->setVisible(false);
		ui.magnetXValue->setVisible(false);
		ui.magnetYLabel->setVisible(false);
		ui.magnetYValue->setVisible(false);
		ui.magnetZLabel->setVisible(false);
		ui.magnetZValue->setVisible(false);
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionShow_Spherical_Coordinates(void)
{
	if (ui.actionShow_Spherical_Coordinates->isChecked())
	{
		ui.magnetMagnitudeLabel->setVisible(true);
		ui.magnetMagnitudeValue->setVisible(true);
		ui.magnetThetaLabel->setVisible(true);
		ui.magnetThetaValue->setVisible(true);
		ui.magnetPhiLabel->setVisible(true);
		ui.magnetPhiValue->setVisible(true);
	}
	else
	{
		ui.magnetMagnitudeLabel->setVisible(false);
		ui.magnetMagnitudeValue->setVisible(false);
		ui.magnetThetaLabel->setVisible(false);
		ui.magnetThetaValue->setVisible(false);
		ui.magnetPhiLabel->setVisible(false);
		ui.magnetPhiValue->setVisible(false);
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionPause(void)
{
	if (magnetParams->GetXAxisParams()->activate)
	{
		if (xProcess)
		{
			if (xProcess->isActive())
			{
				xProcess->sendPause();
			}
		}
	}

	if (magnetParams->GetYAxisParams()->activate)
	{
		if (yProcess)
		{
			if (yProcess->isActive())
			{
				yProcess->sendPause();
			}
		}
	}

	if (magnetParams->GetZAxisParams()->activate)
	{
		if (zProcess)
		{
			if (zProcess->isActive())
			{
				zProcess->sendPause();
			}
		}
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionRamp(void)
{
	if (magnetParams->GetXAxisParams()->activate)
	{
		if (xProcess)
		{
			if (xProcess->isActive())
			{
				xProcess->sendRamp();
			}
		}
	}

	if (magnetParams->GetYAxisParams()->activate)
	{
		if (yProcess)
		{
			if (yProcess->isActive())
			{
				yProcess->sendRamp();
			}
		}
	}

	if (magnetParams->GetZAxisParams()->activate)
	{
		if (zProcess)
		{
			if (zProcess->isActive())
			{
				zProcess->sendRamp();
			}
		}
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionZero(void)
{
	sendNextVector(0, 0, 0);
	statusMisc->setText("Active Vector : Zero Field Vector");
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionChange_Units(void)
{
	FieldUnits newUnits = (FieldUnits)!ui.actionKilogauss->isChecked();
	setFieldUnits(newUnits, false);
	convertFieldValues(newUnits, true);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionChange_Convention(void)
{
	SphericalConvention newSetting = (SphericalConvention)!ui.actionUse_Mathematical_Convention->isChecked();
	setSphericalConvention(newSetting, false);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionConvention_Help(void)
{
	QString link = "https://en.wikipedia.org/wiki/Spherical_coordinate_system";
	QDesktopServices::openUrl(QUrl(link));
}

//---------------------------------------------------------------------------
void MultiAxisOperation::showErrorString(QString errMsg)
{
	errorStatusIsActive = true;
	if (lastVector >= 0 && lastVector < ui.vectorsTableWidget->rowCount())
		ui.vectorsTableWidget->selectRow(lastVector);

	lastStatusString = statusMisc->text();
	statusMisc->setStyleSheet("color: red; font: bold");
	statusMisc->setText("ERROR: " + errMsg);
	qDebug() << statusMisc->text();		// send to log
	QTimer::singleShot(5000, this, SLOT(errorStatusTimeout()));
}

//---------------------------------------------------------------------------
void MultiAxisOperation::errorStatusTimeout(void)
{
	statusMisc->setStyleSheet("");
	statusMisc->setText(lastStatusString);
	errorStatusIsActive = false;
}

//---------------------------------------------------------------------------
void MultiAxisOperation::showUnitsError(QString errMsg)
{
	// stop data collection timer
	dataTimer->stop();

	// stop any active auto-stepping
	stopAutostep();

	closeConnection();
	statusMisc->clear();
	ui.actionConnect->setChecked(false);

	// units changed
	QMessageBox msgBox;

	msgBox.setWindowTitle("Remote Units Changed!");
	msgBox.setText(errMsg);
	msgBox.setStandardButtons(QMessageBox::Ok);
	msgBox.setDefaultButton(QMessageBox::Ok);
	msgBox.setIcon(QMessageBox::Critical);
	int ret = msgBox.exec();
}

//---------------------------------------------------------------------------
void MultiAxisOperation::setFieldUnits(FieldUnits newUnits, bool updateMenuState)
{
	QString tempStr;

	if (fieldUnits == newUnits && !updateMenuState)
		return;	// no change

	if (newUnits == KG)
	{
		ui.magnetXLabel->setText("X (kG)");
		ui.magnetYLabel->setText("Y (kG)");
		ui.magnetZLabel->setText("Z (kG)");

		tempStr = ui.magnetMagnitudeLabel->text().replace("(T)", "(kG)");
		ui.magnetMagnitudeLabel->setText(tempStr);

		tempStr = ui.vectorsTableWidget->horizontalHeaderItem(0)->text().replace("(T)", "(kG)");
		ui.vectorsTableWidget->horizontalHeaderItem(0)->setText(tempStr);
	}
	else if (newUnits == TESLA)
	{
		ui.magnetXLabel->setText("X (T)");
		ui.magnetYLabel->setText("Y (T)");
		ui.magnetZLabel->setText("Z (T)");

		tempStr = ui.magnetMagnitudeLabel->text().replace("(kG)", "(T)");
		ui.magnetMagnitudeLabel->setText(tempStr);

		tempStr = ui.vectorsTableWidget->horizontalHeaderItem(0)->text().replace("(kG)", "(T)");
		ui.vectorsTableWidget->horizontalHeaderItem(0)->setText(tempStr);
	}

	fieldUnits = newUnits;

	if (magnetParams)
		magnetParams->setFieldUnits(fieldUnits);

	if (updateMenuState)
	{
		ui.actionKilogauss->setChecked(!fieldUnits);
		ui.actionTesla->setChecked(fieldUnits);
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::convertFieldValues(FieldUnits newUnits, bool convertMagnetParams)
{
	if (loadedCoordinates == CARTESIAN_COORDINATES)
	{
		// loop through vector table and convert X,Y,Z values
		int numRows = ui.vectorsTableWidget->rowCount();

		for (int i = 0; i < numRows; i++)
		{
			for (int j = 0; j < 3; j++)
			{
				QTableWidgetItem *cell = ui.vectorsTableWidget->item(i, j);
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

	}
	else if (loadedCoordinates == SPHERICAL_COORDINATES)
	{
		// loop through vector table and convert magnitude only
		int numRows = ui.vectorsTableWidget->rowCount();

		for (int i = 0; i < numRows; i++)
		{
			QTableWidgetItem *cell = ui.vectorsTableWidget->item(i, 0);
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

	// now convert the magnet parameters
	if (convertMagnetParams)
		magnetParams->convertFieldValues(newUnits);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::setSphericalConvention(SphericalConvention selection, bool updateMenuState)
{
	convention = selection;

	if (convention == MATHEMATICAL)
	{
		ui.magnetThetaLabel->setText(thetaStr);
		ui.magnetPhiLabel->setText(phiStr);
		if (loadedCoordinates == SPHERICAL_COORDINATES) // don't relabel tables in Cartesian coordinates
		{
			ui.vectorsTableWidget->horizontalHeaderItem(1)->setText(thetaStr);
			ui.vectorsTableWidget->horizontalHeaderItem(2)->setText(phiStr);
		}
	}
	else if (convention == ISO)
	{
		ui.magnetThetaLabel->setText(phiStr);
		ui.magnetPhiLabel->setText(thetaStr);
		if (loadedCoordinates == SPHERICAL_COORDINATES) // don't relabel tables in Cartesian coordinates
		{
			ui.vectorsTableWidget->horizontalHeaderItem(1)->setText(phiStr);
			ui.vectorsTableWidget->horizontalHeaderItem(2)->setText(thetaStr);
		}
	}

	if (updateMenuState)
	{
		ui.actionUse_Mathematical_Convention->setChecked(!convention);
		ui.actionUse_ISO_Convention->setChecked(convention);
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::dataTimerTick(void)
{
	bool x_activated = magnetParams->GetXAxisParams()->activate;
	bool y_activated = magnetParams->GetYAxisParams()->activate;
	bool z_activated = magnetParams->GetZAxisParams()->activate;
	QString xFieldStr, yFieldStr, zFieldStr;
	int precision = 3;

	if (fieldUnits == TESLA)
		precision = 4;

	if (xProcess)
	{
		if (xProcess->isActive() && x_activated)
		{
			FieldUnits units = xProcess->getUnits();

			if (units != ERROR_UNITS)
			{
				if (units == fieldUnits)
				{
					bool ok;
					double temp = xProcess->getField(&ok);

					if (ok)
					{
						xField = temp;
						xFieldStr = "<font color=white>" + (QString::number(avoidSignedZeroOutput(xField, precision), 'f', precision)) + "</font>";
					}

					xState = xProcess->getState();
				}
				else
				{
					// wrong units
					showUnitsError("Remote X-axis units have been changed. Please verify remote units selection and reconnect.");
					return;
				}
			}
			else
			{
				// no or invalid reply
				xFieldStr = "<font color=red>" + (QString::number(avoidSignedZeroOutput(xField, precision), 'f', precision)) + "</font>";
			}
		}
		else
		{
			xFieldStr = "<font color=red>" + (QString::number(avoidSignedZeroOutput(xField, precision), 'f', precision)) + "</font>";
		}
	}
	else
		xField = 0;

	if (yProcess)
	{
		if (yProcess->isActive() && y_activated)
		{
			FieldUnits units = yProcess->getUnits();

			if (units != ERROR_UNITS)
			{
				if (units == fieldUnits)
				{
					bool ok;
					double temp = yProcess->getField(&ok);

					if (ok)
					{
						yField = temp;
						yFieldStr = "<font color=white>" + (QString::number(avoidSignedZeroOutput(yField, precision), 'f', precision)) + "</font>";
					}

					yState = yProcess->getState();
				}
				else
				{
					// wrong units
					showUnitsError("Remote Y-axis units have been changed. Please verify remote units selection and reconnect.");
					return;
				}
			}
			else
			{
				// no or invalid reply
				yFieldStr = "<font color=red>" + (QString::number(avoidSignedZeroOutput(yField, precision), 'f', precision)) + "</font>";
			}
		}
		else
		{
			yFieldStr = "<font color=red>" + (QString::number(avoidSignedZeroOutput(yField, precision), 'f', precision)) + "</font>";
		}
	}
	else
		yField = 0;

	if (zProcess)
	{
		if (zProcess->isActive() && z_activated)
		{
			FieldUnits units = zProcess->getUnits();

			if (units != ERROR_UNITS)
			{
				if (units == fieldUnits)
				{
					bool ok;
					double temp = zProcess->getField(&ok);

					if (ok)
					{
						zField = temp;
						zFieldStr = "<font color=white>" + (QString::number(avoidSignedZeroOutput(zField, precision), 'f', precision)) + "</font>";
					}

					zState = zProcess->getState();
				}
				else
				{
					// wrong units
					showUnitsError("Remote Z-axis units have been changed. Please verify remote units selection and reconnect.");
					return;
				}
			}
			else
			{
				// no or invalid reply
				zFieldStr = "<font color=red>" + (QString::number(avoidSignedZeroOutput(zField, precision), 'f', precision)) + "</font>";
			}
		}
		else
		{
			zFieldStr = "<font color=red>" + (QString::number(avoidSignedZeroOutput(zField, precision), 'f', precision)) + "</font>";
		}
	}
	else
		zField = 0;

	// update displayed field
	ui.magnetXValue->setText(xFieldStr);
	ui.magnetYValue->setText(yFieldStr);
	ui.magnetZValue->setText(zFieldStr);

	cartesianToSpherical(xField, yField, zField, &magnitudeField, &thetaAngle, &phiAngle);
	ui.magnetMagnitudeValue->setText(QString::number(avoidSignedZeroOutput(magnitudeField, precision), 'f', precision));
	ui.magnetThetaValue->setText(QString::number(avoidSignedZeroOutput(thetaAngle, 2), 'f', 2));
	ui.magnetPhiValue->setText(QString::number(avoidSignedZeroOutput(phiAngle, 2), 'f', 2));

	// update state
	if ((x_activated && xState == QUENCH) ||
		(y_activated && yState == QUENCH) ||
		(z_activated && zState == QUENCH))
	{
		bool autoSave = false;
		magnetState = QUENCH;
		statusState->setStyleSheet("color: red; font: bold;");
		statusState->setText("QUENCH!");
		
		// stop any autostep cycle
		if (autostepTimer->isActive())
		{
			autoSave = true;	// do the autosave on quench condition
			stopAutostep();
			statusMisc->setText("Auto-Stepping aborted due to quench detection");
		}

		// mark vector as fail
		if (presentVector >= 0)
		{
			QTableWidgetItem *cell = ui.vectorsTableWidget->item(presentVector, 4);

			if (cell)
				cell->setText("Fail");

			// if needed, add columns for X/Y/Z quench currents
			if (ui.vectorsTableWidget->columnCount() < 6)
			{
				ui.vectorsTableWidget->setColumnCount(8);
				ui.vectorsTableWidget->setHorizontalHeaderItem(5, new QTableWidgetItem("X Quench (A)"));
				ui.vectorsTableWidget->horizontalHeaderItem(5)->font().setBold(true);
				ui.vectorsTableWidget->setHorizontalHeaderItem(6, new QTableWidgetItem("Y Quench (A)"));
				ui.vectorsTableWidget->horizontalHeaderItem(6)->font().setBold(true);
				ui.vectorsTableWidget->setHorizontalHeaderItem(7, new QTableWidgetItem("Z Quench (A)"));
				ui.vectorsTableWidget->horizontalHeaderItem(7)->font().setBold(true);
				ui.vectorsTableWidget->repaint();
			}

			// add quench data
			if (x_activated && xState == QUENCH)
			{
				bool ok;
				QString cellStr = QString::number(xProcess->getQuenchCurrent(&ok), 'f', 3);

				if (ok)
				{
					QTableWidgetItem *cell = ui.vectorsTableWidget->item(presentVector, 5);

					if (cell == NULL)
						ui.vectorsTableWidget->setItem(presentVector, 5, cell = new QTableWidgetItem(cellStr));
					else
						cell->setText(cellStr);

					cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
				}
			}

			if (y_activated && yState == QUENCH)
			{
				bool ok;
				QString cellStr = QString::number(yProcess->getQuenchCurrent(&ok), 'f', 3);

				if (ok)
				{
					QTableWidgetItem *cell = ui.vectorsTableWidget->item(presentVector, 6);

					if (cell == NULL)
						ui.vectorsTableWidget->setItem(presentVector, 6, cell = new QTableWidgetItem(cellStr));
					else
						cell->setText(cellStr);

					cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
				}
			}

			if (z_activated && zState == QUENCH)
			{
				bool ok;
				QString cellStr = QString::number(zProcess->getQuenchCurrent(&ok), 'f', 3);

				if (ok)
				{
					QTableWidgetItem *cell = ui.vectorsTableWidget->item(presentVector, 7);

					if (cell == NULL)
						ui.vectorsTableWidget->setItem(presentVector, 7, cell = new QTableWidgetItem(cellStr));
					else
						cell->setText(cellStr);

					cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
				}
			}
		}

		doAutosaveReport();
	}
	else if ((x_activated && xState == RAMPING) ||
			 (y_activated && yState == RAMPING) ||
			 (z_activated && zState == RAMPING))
	{
		magnetState = RAMPING;
		statusState->setStyleSheet("color: black; font: bold;");
		statusState->setText("RAMPING");
	}
	else if ((!x_activated || (x_activated && xState == HOLDING)) &&
			 (!y_activated || (y_activated && yState == HOLDING)) &&
			 (!z_activated || (z_activated && zState == HOLDING)))
	{
		if (passCnt < 2 && magnetState != HOLDING)
		{
			// this is a hack to prevent a leftover HOLDING status
			// from carrying over to next target vector
			passCnt++;	// increment pass count @ HOLDING
		}
		else if (passCnt >= 2 || magnetState == HOLDING)
		{
			magnetState = HOLDING;
			statusState->setStyleSheet("color: green; font: bold;");
			statusState->setText("HOLDING");

			// mark vector as pass
			if (presentVector >= 0 && !vectorError)
			{
				QTableWidgetItem *cell = ui.vectorsTableWidget->item(presentVector, 4);

				if (cell)
					cell->setText("Pass");

				// clear any quench data
				cell = ui.vectorsTableWidget->item(presentVector, 5);

				if (cell)
					cell->setText("");

				cell = ui.vectorsTableWidget->item(presentVector, 6);

				if (cell)
					cell->setText("");

				cell = ui.vectorsTableWidget->item(presentVector, 7);

				if (cell)
					cell->setText("");
			}

			passCnt = 0;	// reset
		}
	}
	else if ((x_activated && xState == PAUSED) ||
			 (y_activated && yState == PAUSED) ||
			 (z_activated && zState == PAUSED))
	{
		magnetState = PAUSED;
		statusState->setStyleSheet("color: black; font: bold;");
		statusState->setText("PAUSED");
	}
	else if ((x_activated && xState == ZEROING) ||
			 (y_activated && yState == ZEROING) ||
			 (z_activated && zState == ZEROING))
	{
		magnetState = ZEROING;
		statusState->setStyleSheet("color: black; font: bold;");
		statusState->setText("ZEROING");
	}
	else if ((!x_activated || (x_activated && xState == AT_ZERO)) &&
			 (!y_activated || (y_activated && yState == AT_ZERO)) &&
			 (!z_activated || (z_activated && zState == AT_ZERO)))
	{
		magnetState = AT_ZERO;
		statusState->setStyleSheet("color: black; font: bold;");
		statusState->setText("AT ZERO");
	} 
}

//---------------------------------------------------------------------------
void MultiAxisOperation::switchHeatingTimerTick(void)
{
	elapsedHeatingTicks++;

	if (elapsedHeatingTicks >= longestHeatingTime)
	{
		// release interface to continue
		switchHeatingTimer->stop();
		ui.menuBar->setEnabled(true);
		ui.mainTabWidget->setEnabled(true);
		ui.mainToolBar->setEnabled(true);

		statusMisc->setText("All active axes initialized successfully; switches heated");
	}
	else
	{
		// update the heating countdown
		QString tempStr = statusMisc->text();
		int index = tempStr.indexOf('(');
		if (index >= 1)
			tempStr.truncate(index - 1);

		QString timeStr = " (" + QString::number(longestHeatingTime - elapsedHeatingTicks) + " sec remaining)";
		statusMisc->setText(tempStr + timeStr);
	}
}

//---------------------------------------------------------------------------
bool MultiAxisOperation::sendNextVector(double x, double y, double z)
{
	// Sends next vector down to axes, using the maxRampRate for each axis
	// to determine the ramp rate required for each axes to arrive at the
	// next vector simultaneously. The vector must be specified in cartesian
	// coordinates.
	double deltaX, deltaY, deltaZ;	// kG
	double xTime, yTime, zTime;		// sec
	double xRampRate, yRampRate, zRampRate;	// A/sec

	// check limits first; if an axis is not activated yet has a non-zero vector value, throw an error
	if (magnetParams->GetXAxisParams()->activate)
	{
		double checkValue = fabs(x / magnetParams->GetXAxisParams()->coilConst);

		if (checkValue > magnetParams->GetXAxisParams()->currentLimit)
		{
			showErrorString("Vector #" + QString::number(presentVector + 1) + " exceeds X-axis Current Limit!");
			QApplication::beep();
			return false;
		}
	}
	else
	{
		if (fabs(x) > 0.0)
		{
			// can't achieve the vector as there is no X-axis activated
			showErrorString("Vector #" + QString::number(presentVector + 1) + " requires an active X-axis field component!");
			QApplication::beep();
			return false;
		}
	}

	if (magnetParams->GetYAxisParams()->activate)
	{
		double checkValue = fabs(y / magnetParams->GetYAxisParams()->coilConst);

		if (checkValue > magnetParams->GetYAxisParams()->currentLimit)
		{
			showErrorString("Vector #" + QString::number(presentVector + 1) + " exceeds Y-axis Current Limit!");
			QApplication::beep();
			return false;
		}
	}
	else
	{
		if (fabs(y) > 0.0)
		{
			// can't achieve the vector as there is no Y-axis activated
			showErrorString("Vector #" + QString::number(presentVector + 1) + " requires an active Y-axis field component!");
			QApplication::beep();
			return false;
		}
	}

	if (magnetParams->GetZAxisParams()->activate)
	{
		double checkValue = fabs(z / magnetParams->GetZAxisParams()->coilConst);

		if (checkValue > magnetParams->GetZAxisParams()->currentLimit)
		{
			showErrorString("Vector #" + QString::number(presentVector + 1) + " exceeds Z-axis Current Limit!");
			QApplication::beep();
			return false;
		}
	}
	else
	{
		if (fabs(z) > 0.0)
		{
			// can't achieve the vector as there is no Z-axis activated
			showErrorString("Vector #" + QString::number(presentVector + 1) + " requires an active Z-axis field component!");
			QApplication::beep();
			return false;
		}
	}
	
	{
		double temp = sqrt(x * x + y * y + z * z);

		if (temp > magnetParams->getMagnitudeLimit())
		{
			showErrorString("Vector #" + QString::number(presentVector + 1) + " exceeds Magnitude Limit of Magnet!");
			QApplication::beep();
			return false;
		}
	}

	// next, find delta field for each axis
	deltaX = fabsl(xField - x);
	deltaY = fabsl(yField - y);
	deltaZ = fabsl(zField - z);

	// find time required for each delta given maxRampRate for each axis
	if (magnetParams->GetXAxisParams()->activate)
		xTime = deltaX / (magnetParams->GetXAxisParams()->maxRampRate * magnetParams->GetXAxisParams()->coilConst);
	else
		xTime = 0;

	if (magnetParams->GetYAxisParams()->activate)
		yTime = deltaY / (magnetParams->GetYAxisParams()->maxRampRate * magnetParams->GetYAxisParams()->coilConst);
	else
		yTime = 0;

	if (magnetParams->GetZAxisParams()->activate)
		zTime = deltaZ / (magnetParams->GetZAxisParams()->maxRampRate * magnetParams->GetZAxisParams()->coilConst);
	else
		zTime = 0;

	// which is the longest time? it is the limiter
	if (xTime >= yTime && xTime >= zTime)
	{
		xRampRate = magnetParams->GetXAxisParams()->maxRampRate;

		if (magnetParams->GetYAxisParams()->activate)
		{
			yRampRate = deltaY / magnetParams->GetYAxisParams()->coilConst / xTime;
			if (yRampRate < MIN_RAMP_RATE)
				yRampRate = MIN_RAMP_RATE;
		}
		else
			yRampRate = 0;

		if (magnetParams->GetZAxisParams()->activate)
		{
			zRampRate = deltaZ / magnetParams->GetZAxisParams()->coilConst / xTime;
			if (zRampRate < MIN_RAMP_RATE)
				zRampRate = MIN_RAMP_RATE;
		}
		else
			zRampRate = 0;
	}
	else if (yTime >= xTime && yTime >= zTime)
	{
		yRampRate = magnetParams->GetYAxisParams()->maxRampRate;

		if (magnetParams->GetXAxisParams()->activate)
		{
			xRampRate = deltaX / magnetParams->GetXAxisParams()->coilConst / yTime;
			if (xRampRate < MIN_RAMP_RATE)
				xRampRate = MIN_RAMP_RATE;
		}
		else
			xRampRate = 0;

		if (magnetParams->GetZAxisParams()->activate)
		{
			zRampRate = deltaZ / magnetParams->GetZAxisParams()->coilConst / yTime;
			if (zRampRate < MIN_RAMP_RATE)
				zRampRate = MIN_RAMP_RATE;
		}
		else
			zRampRate = 0;
	}
	else if (zTime >= xTime && zTime >= yTime)
	{
		zRampRate = magnetParams->GetZAxisParams()->maxRampRate;

		if (magnetParams->GetXAxisParams()->activate)
		{
			xRampRate = deltaX / magnetParams->GetXAxisParams()->coilConst / zTime;
			if (xRampRate < MIN_RAMP_RATE)
				xRampRate = MIN_RAMP_RATE;
		}
		else
			xRampRate = 0;

		if (magnetParams->GetYAxisParams()->activate)
		{
			yRampRate = deltaY / magnetParams->GetYAxisParams()->coilConst / zTime;
			if (yRampRate < MIN_RAMP_RATE)
				yRampRate = MIN_RAMP_RATE;
		}
		else
			yRampRate = 0;
	}

	// send down new ramp rates, new targets, and ramp
	if (magnetParams->GetXAxisParams()->activate)
	{
		if (xProcess)
		{
			if (xProcess->isActive())
			{
				xProcess->setRampRateCurr(magnetParams->GetXAxisParams(), xRampRate);
				xProcess->setTargetCurr(magnetParams->GetXAxisParams(), x);
				xProcess->sendRamp();
			}
		}
	}

	if (magnetParams->GetYAxisParams()->activate)
	{
		if (yProcess)
		{
			if (yProcess->isActive())
			{
				yProcess->setRampRateCurr(magnetParams->GetYAxisParams(), yRampRate);
				yProcess->setTargetCurr(magnetParams->GetYAxisParams(), y);
				yProcess->sendRamp();
			}
		}
	}

	if (magnetParams->GetZAxisParams()->activate)
	{
		if (zProcess)
		{
			if (zProcess->isActive())
			{
				zProcess->setRampRateCurr(magnetParams->GetZAxisParams(), zRampRate);
				zProcess->setTargetCurr(magnetParams->GetZAxisParams(), z);
				zProcess->sendRamp();
			}
		}
	}

	lastVector = presentVector;
	return true;
}

//---------------------------------------------------------------------------
void MultiAxisOperation::switchControl(bool heatSwitch)
{
	// initiate switch heating
	if (magnetParams->GetXAxisParams()->activate && magnetParams->GetXAxisParams()->switchInstalled)
	{
		if (xProcess)
		{
			if (xProcess->isActive())
			{
				if (heatSwitch)
					xProcess->heatSwitch();
				else
					xProcess->coolSwitch();
			}
		}
	}

	if (magnetParams->GetYAxisParams()->activate && magnetParams->GetYAxisParams()->switchInstalled)
	{
		if (yProcess)
		{
			if (yProcess->isActive())
			{
				if (heatSwitch)
					yProcess->heatSwitch();
				else
					yProcess->coolSwitch();
			}
		}
	}

	if (magnetParams->GetZAxisParams()->activate && magnetParams->GetZAxisParams()->switchInstalled)
	{
		if (zProcess)
		{
			if (zProcess->isActive())
			{
				if (heatSwitch)
					zProcess->heatSwitch();
				else
					zProcess->coolSwitch();
			}
		}
	}
}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------