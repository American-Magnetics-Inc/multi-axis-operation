#include "stdafx.h"
#include "multiaxisoperation.h"
#include "magnetparams.h"
#include "conversions.h"
#include "aboutdialog.h"
#include "parser.h"

// minimum programmable ramp rate (A/s) for purposes of multi-axis control
const double MIN_RAMP_RATE = 0.001;

// load/save settings file version
const int SAVE_FILE_VERSION = 4;

// stdin parsing support
static Parser *parser;

// quench logged flag to prevent duplicate log messages
static bool quenchLogged = false;


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

#if defined(Q_OS_LINUX)
    this->setWindowIcon(QIcon(":multiaxis/Resources/app.ico"));
    QGuiApplication::setFont(QFont("Ubuntu", 9));
    this->setFont(QFont("Ubuntu", 9));
    QFont::insertSubstitution("Segoe UI", "Ubuntu");
    ui.vectorsTableWidget->setFont(QFont("Ubuntu", 9));
#elif defined(Q_OS_MACOS)
    // Mac base font scaling is different than Linux and Windows
    // use ~ 96/72 = 1.333 factor for upscaling every font size
    // we must do this everywhere a font size is specified
    QGuiApplication::setFont(QFont(".SF NS Text", 13));
    this->setFont(QFont(".SF NS Text", 13));
    QFont::insertSubstitution("Segoe UI", ".SF NS Text");
    //ui.menuBar->setFont(QFont(".SF NS Text", 14));
    ui.presentFieldFrame->setFont(QFont(".SF NS Text", 14, QFont::Bold));
    /*ui.actionExit->setFont(QFont(".SF NS Text", 13));
    ui.actionTest_Mode->setFont(QFont(".SF NS Text", 13));
    ui.actionDefine->setFont(QFont(".SF NS Text", 13));
    ui.actionRamp->setFont(QFont(".SF NS Text", 13));
    ui.actionPause->setFont(QFont(".SF NS Text", 13));
    ui.mainToolBar->setFont(QFont(".SF NS Text", 13));*/

    this->setStyleSheet("QGroupBox { font-size: 13px; }; QAction { font-size: 13px; }; QToolBar { font-size: 13px; };");
#endif

	// initialization
	loadedCoordinates = SPHERICAL_COORDINATES;
	systemState = DISCONNECTED;
    magnetParams = nullptr;
    xProcess = nullptr;
    yProcess = nullptr;
    zProcess = nullptr;
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
	vectorError = NO_VECTOR_ERROR;
	polarError = NO_VECTOR_ERROR;
	targetSource = NO_SOURCE;
	presentVector = -1;
	lastVector = -1;
	presentPolar = -1;
	lastPolar = -1;
	errorStatusIsActive = false;
	xTarget = 0.0;
	yTarget = 0.0;
	zTarget = 0.0;
	remainingTime = 0;
	autostepRemainingTime = 0;
	polarRemainingTime = 0;

	// setup status bar
	statusConnectState = new QLabel("", this);
	statusConnectState->setFrameStyle(QFrame::Panel | QFrame::Sunken);
	statusConnectState->setToolTip("Connection status");
	statusMisc = new QLabel("", this);
	statusMisc->setFrameStyle(QFrame::Panel | QFrame::Sunken);
	statusState = new QLabel("", this);
	statusState->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    statusState->setToolTip("Operating state");

#if defined(Q_OS_LINUX)
    statusConnectState->setFont(QFont("Ubuntu", 9));
    statusMisc->setFont(QFont("Ubuntu", 9));
    statusState->setFont(QFont("Ubuntu", 9));
#elif defined(Q_OS_MAC)
    statusConnectState->setFont(QFont(".SF NS Text", 12));
    statusMisc->setFont(QFont(".SF NS Text", 12));
    statusState->setFont(QFont(".SF NS Text", 12));
#else
    statusConnectState->setFont(QFont("Segoe UI", 9));
    statusMisc->setFont(QFont("Segoe UI", 9));
	statusState->setFont(QFont("Segoe UI", 9));
#endif

	statusBar()->addPermanentWidget(statusConnectState, 1);
	statusBar()->addPermanentWidget(statusMisc, 4);
	statusBar()->addPermanentWidget(statusState, 1);
	statusConnectState->setStyleSheet("color: red; font: bold;");
	statusConnectState->setText("DISCONNECTED");

	// restore window position and state
	QSettings settings;

	// restore different geometry for different DPI screens
	QString dpiStr = QString::number(QApplication::desktop()->screen()->logicalDpiX());
	restoreGeometry(settings.value("MainWindow/Geometry/" + dpiStr).toByteArray());
	restoreState(settings.value("MainWindow/WindowState/" + dpiStr).toByteArray());

	// restore other interface settings
    fieldUnits = static_cast<FieldUnits>(settings.value("FieldUnits").toInt());
    convention = static_cast<SphericalConvention>(settings.value("SphericalConvention").toInt());
	ui.actionAutosave_Report->setChecked(settings.value("AutosaveReport").toBool());
	ui.actionStabilizingResistors->setChecked(settings.value("StabilizingResistors").toBool());

	// restore tab positions from last exit
	int vectorTabIndex = settings.value("Tabs/VectorTabIndex").toInt();
	int alignTabIndex = settings.value("Tabs/AlignTabIndex").toInt();
	int polarTabIndex = settings.value("Tabs/PolarTabIndex").toInt();

	if (polarTabIndex != ui.mainTabWidget->indexOf(ui.rotationTab))
	{
		ui.mainTabWidget->removeTab(ui.mainTabWidget->indexOf(ui.rotationTab));
		ui.mainTabWidget->insertTab(polarTabIndex, ui.rotationTab, "Rotation in Sample Alignment Tab");
	}

	if (alignTabIndex != ui.mainTabWidget->indexOf(ui.alignmentTab))
	{
		ui.mainTabWidget->removeTab(ui.mainTabWidget->indexOf(ui.alignmentTab));
		ui.mainTabWidget->insertTab(alignTabIndex, ui.alignmentTab, "Sample Alignment");
	}

	ui.mainTabWidget->setCurrentIndex(settings.value("CurrentTab").toInt());

	// restore alignment tab settings
	alignmentTabInitState();

	// disable vector selection until connect
	ui.manualVectorControlGroupBox->setEnabled(false);
	ui.autostepStartButton->setEnabled(false);
	ui.manualPolarControlGroupBox->setEnabled(false);
	ui.autostepStartButtonPolar->setEnabled(false);

	// create data collection timer
	dataTimer = new QTimer(this);
	dataTimer->setInterval(1000);	// update once per second
	connect(dataTimer, SIGNAL(timeout()), this, SLOT(dataTimerTick()));

	// create switch heating timer
	switchHeatingTimer = new QTimer(this);
	switchHeatingTimer->setInterval(1000);
	connect(switchHeatingTimer, SIGNAL(timeout()), this, SLOT(switchHeatingTimerTick()));

	// create switch cooling timer
	switchCoolingTimer = new QTimer(this);
	switchCoolingTimer->setInterval(1000);
	connect(switchCoolingTimer, SIGNAL(timeout()), this, SLOT(switchCoolingTimerTick()));

	// create autostep timer
	autostepTimer = new QTimer(this);
	autostepTimer->setInterval(1000);	// update once per second
	connect(autostepTimer, SIGNAL(timeout()), this, SLOT(autostepTimerTick()));

	// create polar autostep timer
	autostepPolarTimer = new QTimer(this);
	autostepPolarTimer->setInterval(1000);	// update once per second
	connect(autostepPolarTimer, SIGNAL(timeout()), this, SLOT(autostepPolarTimerTick()));

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

	// connect toolbar and menu actions
	connect(ui.actionAbout, SIGNAL(triggered()), this, SLOT(actionAbout()));
	connect(ui.actionAbout_2, SIGNAL(triggered()), this, SLOT(actionAbout()));
	connect(ui.actionView_Help, SIGNAL(triggered()), this, SLOT(actionHelp()));
	connect(ui.actionHelp, SIGNAL(triggered()), this, SLOT(actionHelp()));
	connect(ui.actionConnect, SIGNAL(triggered()), this, SLOT(actionConnect()));
	connect(ui.actionLoad_Settings, SIGNAL(triggered()), this, SLOT(actionLoad_Settings()));
	connect(ui.actionSave_Settings, SIGNAL(triggered()), this, SLOT(actionSave_Settings()));
	connect(ui.actionDefine, SIGNAL(triggered()), this, SLOT(actionDefine()));
	connect(ui.actionLoad_Vector_Table, SIGNAL(triggered()), this, SLOT(actionLoad_Vector_Table()));
	connect(ui.actionSave_Vector_Table, SIGNAL(triggered()), this, SLOT(actionSave_Vector_Table()));
	connect(ui.actionLoad_Polar_Table, SIGNAL(triggered()), this, SLOT(actionLoad_Polar_Table()));
	connect(ui.actionSave_Polar_Table, SIGNAL(triggered()), this, SLOT(actionSave_Polar_Table()));
	connect(ui.actionExit, SIGNAL(triggered()), this, SLOT(close()));
	connect(ui.actionShow_Cartesian_Coordinates, SIGNAL(triggered()), this, SLOT(actionShow_Cartesian_Coordinates()));
	connect(ui.actionShow_Spherical_Coordinates, SIGNAL(triggered()), this, SLOT(actionShow_Spherical_Coordinates()));
	connect(ui.actionPersistentMode, SIGNAL(triggered()), this, SLOT(actionPersistentMode()));
	connect(ui.actionPause, SIGNAL(triggered()), this, SLOT(actionPause()));
	connect(ui.actionRamp, SIGNAL(triggered()), this, SLOT(actionRamp()));
	connect(ui.actionZero, SIGNAL(triggered()), this, SLOT(actionZero()));
	connect(ui.actionKilogauss, SIGNAL(triggered()), this, SLOT(actionChange_Units()));
	connect(ui.actionTesla, SIGNAL(triggered()), this, SLOT(actionChange_Units()));
	connect(ui.actionUse_Mathematical_Convention, SIGNAL(triggered()), this, SLOT(actionChange_Convention()));
	connect(ui.actionUse_ISO_Convention, SIGNAL(triggered()), this, SLOT(actionChange_Convention()));
	connect(ui.action_SphericalHelp, SIGNAL(triggered()), this, SLOT(actionConvention_Help()));
	connect(ui.actionGenerate_Excel_Report, SIGNAL(triggered()), this, SLOT(actionGenerate_Excel_Report()));

	// other actions
	connect(ui.vectorsTableWidget, SIGNAL(itemSelectionChanged()), this, SLOT(vectorSelectionChanged()));
	connect(ui.startIndexEdit, SIGNAL(editingFinished()), this, SLOT(autostepRangeChanged()));
	connect(ui.endIndexEdit, SIGNAL(editingFinished()), this, SLOT(autostepRangeChanged()));
	connect(ui.polarTableWidget, SIGNAL(itemSelectionChanged()), this, SLOT(polarSelectionChanged()));
	connect(ui.startIndexEditPolar, SIGNAL(editingFinished()), this, SLOT(polarRangeChanged()));
	connect(ui.endIndexEditPolar, SIGNAL(editingFinished()), this, SLOT(polarRangeChanged()));

	// create magnetParams dialog
	magnetParams = new MagnetParams();
	setStabilizingResistorAvailability();

	// set field units to saved preference
	setFieldUnits(fieldUnits, true);

	// set spherical coordinate labeling convention
	setSphericalConvention(convention, true);

	updateWindowTitle();

	/************************************************************
	The command line parser allows options:

	-p	Start the stdin/stdout parser function (for QProcess use).
	--simulate	Start in localhost simulation mode (for AMI use only).
	************************************************************/

	// parse command line options
	QCommandLineParser cmdLineParse;

	cmdLineParse.setApplicationDescription("Multi-Axis Operation application for operating AMI MAxes systems");
	cmdLineParse.addHelpOption();
	cmdLineParse.addVersionOption();

	// Connect to simulated system (--simulate)
	QCommandLineOption portOption("simulate",
		QCoreApplication::translate("main", "Query simulated instruments."),
		QCoreApplication::translate("main", "ip-port"));
	cmdLineParse.addOption(portOption);

	// A boolean option with multiple names (-p, --parser)
	QCommandLineOption parsingOption(QStringList() << "p" << "parser",
		QCoreApplication::translate("main", "Enable stdin parsing for interprocess communication."));
	cmdLineParse.addOption(parsingOption);

	// Process the actual command line arguments given by the user
	cmdLineParse.process(*(QCoreApplication::instance()));

	if (cmdLineParse.isSet(portOption))
	{
		simulation = true;	// use simulated system

		addressStr = cmdLineParse.value(portOption);

		// if no port specified, use localhost
		if (addressStr.isEmpty())
		{
			addressStr = "localhost";
		}
	}

	useParser = cmdLineParse.isSet(parsingOption);

	if (useParser)	// start stdin/stdout parser for scripting control
	{
		QThread* parserThread = new QThread;
        parser = new Parser(nullptr);

		parser->setDataSource(this);
		parser->moveToThread(parserThread);
		connect(parser, SIGNAL(error_msg(QString)), this, SLOT(parserErrorString(QString)));
		connect(parserThread, SIGNAL(started()), parser, SLOT(process()));
		connect(parser, SIGNAL(finished()), parserThread, SLOT(quit()));
		connect(parser, SIGNAL(finished()), parser, SLOT(deleteLater()));
		connect(parserThread, SIGNAL(finished()), parserThread, SLOT(deleteLater()));

		// connect cross-thread actions
		connect(parser, SIGNAL(system_connect()), this, SLOT(system_connect()));
		connect(parser, SIGNAL(system_disconnect()), this, SLOT(system_disconnect()));
		connect(parser, SIGNAL(exit_app()), this, SLOT(exit_app()));
		connect(parser, SIGNAL(ramp()), this, SLOT(actionRamp()));
		connect(parser, SIGNAL(pause()), this, SLOT(actionPause()));
		connect(parser, SIGNAL(zero()), this, SLOT(actionZero()));
		connect(parser, SIGNAL(load_settings(FILE *, bool *)), this, SLOT(load_settings(FILE *,bool *)));
		connect(parser, SIGNAL(save_settings(FILE *, bool *)), this, SLOT(save_settings(FILE *, bool *)));
		connect(parser, SIGNAL(set_align1(double, double, double)), this, SLOT(set_align1(double, double, double)));
		connect(parser, SIGNAL(set_align2(double, double, double)), this, SLOT(set_align2(double, double, double)));
		connect(parser, SIGNAL(set_align1_active()), this, SLOT(set_align1_active()));
		connect(parser, SIGNAL(set_align2_active()), this, SLOT(set_align2_active()));
		connect(parser, SIGNAL(set_units(int)), this, SLOT(set_units(int)));
		connect(parser, SIGNAL(set_vector(double, double, double, int)), this, SLOT(set_vector(double, double, double, int)));
		connect(parser, SIGNAL(set_vector_cartesian(double, double, double, int)), this, SLOT(set_vector_cartesian(double, double, double, int)));
		connect(parser, SIGNAL(goto_vector(int)), this, SLOT(goto_vector(int)));
		connect(parser, SIGNAL(set_polar(double, double, int)), this, SLOT(set_polar(double, double, int)));
		connect(parser, SIGNAL(goto_polar(int)), this, SLOT(goto_polar(int)));
		connect(parser, SIGNAL(set_persistence(bool)), this, SLOT(set_persistence(bool)));

		parserThread->start();
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::setStabilizingResistorAvailability(void)
{
	// set stabilizing resistor option availability
	if ((magnetParams->GetXAxisParams()->activate && !magnetParams->GetXAxisParams()->switchInstalled) ||
		(magnetParams->GetYAxisParams()->activate && !magnetParams->GetYAxisParams()->switchInstalled) ||
		(magnetParams->GetZAxisParams()->activate && !magnetParams->GetZAxisParams()->switchInstalled))
	{
		ui.actionStabilizingResistors->setEnabled(true);
	}
	else
	{
		ui.actionStabilizingResistors->setChecked(false);
		ui.actionStabilizingResistors->setEnabled(false);
	}
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
	// stop and destroy parser and associated thread if it exists
	if (parser)
	{
		parser->stop();
        parser = nullptr;
	}

	closeConnection();

	// delete timers
	delete dataTimer;
    dataTimer = nullptr;
	delete switchHeatingTimer;
    switchHeatingTimer = nullptr;
	delete switchCoolingTimer;
    switchCoolingTimer = nullptr;
	delete autostepTimer;
    autostepTimer = nullptr;
	delete errorStatusTimer;
    errorStatusTimer = nullptr;

	if (magnetParams)
		delete magnetParams;

	// save window position and state
	QSettings settings;

	// save different geometry for different DPI screens
	QString dpiStr = QString::number(QApplication::desktop()->screen()->logicalDpiX());

	settings.setValue("MainWindow/Geometry/" + dpiStr, saveGeometry());
	settings.setValue("MainWindow/WindowState/" + dpiStr, saveState());
    settings.setValue("FieldUnits", static_cast<int>(fieldUnits));
    settings.setValue("SphericalConvention", static_cast<int>(convention));
	settings.setValue("AutosaveReport", ui.actionAutosave_Report->isChecked());
	settings.setValue("CurrentTab", ui.mainTabWidget->currentIndex());
	settings.setValue("Tabs/VectorTabIndex", ui.mainTabWidget->indexOf(ui.vectorTableTab));
	settings.setValue("Tabs/AlignTabIndex", ui.mainTabWidget->indexOf(ui.alignmentTab));
	settings.setValue("Tabs/PolarTabIndex", ui.mainTabWidget->indexOf(ui.rotationTab));
	settings.setValue("StabilizingResistors", ui.actionStabilizingResistors->isChecked());

	// save alignment tab state
	alignmentTabSaveState();
}

//---------------------------------------------------------------------------
void MultiAxisOperation::closeConnection()
{
	connected = false;
	systemState = DISCONNECTED;

	// close and delete all connected processes
	if (xProcess)
	{
		xProcess->deleteLater();
        xProcess = nullptr;
	}

	if (yProcess)
	{
		yProcess->deleteLater();
        yProcess = nullptr;
	}

	if (zProcess)
	{
		zProcess->deleteLater();
        zProcess = nullptr;
	}

	statusConnectState->setStyleSheet("color: red; font: bold;");
	statusConnectState->setText("DISCONNECTED");
	ui.actionKilogauss->setEnabled(true);
	ui.actionTesla->setEnabled(true);
	ui.actionTest_Mode->setEnabled(true);
	ui.actionLoad_Settings->setEnabled(true);
	ui.manualVectorControlGroupBox->setEnabled(false);
	ui.autostepStartButton->setEnabled(false);
	ui.manualPolarControlGroupBox->setEnabled(false);
	ui.autostepStartButtonPolar->setEnabled(false);
	setStabilizingResistorAvailability();

	statusState->clear();
	ui.actionConnect->setChecked(false);

	alignmentTabDisconnect();
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
	longestCoolingTime = 0;
	switchInstalled = false;
	processError = false;

	if (ui.actionConnect->isChecked())
	{
		QProgressDialog progressDialog;
		QApplication::setOverrideCursor(Qt::WaitCursor);
		statusConnectState->setStyleSheet("color: green; font: bold;");
		statusConnectState->setText("CONNECTING...");
		lastTargetMsg.clear();
		setStatusMsg("Launching processes, please wait...");

		progressDialog.setFont(QFont("Segoe UI", 9));
		progressDialog.setLabelText(QString("Launching processes, please wait...                   "));
		progressDialog.setWindowTitle("Multi-Axis Connect");
		progressDialog.show();
		progressDialog.setWindowModality(Qt::WindowModal);
		progressDialog.setValue(0);
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		// if X-axis active, create a QProcess controller
		if (magnetParams->GetXAxisParams()->activate)
		{
			if (xProcess == NULL)
			{
				xProcess = new ProcessManager(this);
				connect(xProcess, SIGNAL(magnetDAQError()), this, SLOT(magnetDAQVersionError()));
			}

			if (!xProcess->isActive())
			{
				if (simulation)
					xProcess->connectProcess(addressStr, X_AXIS, simulation);
				else
					xProcess->connectProcess(magnetParams->GetXAxisParams()->ipAddress, X_AXIS, simulation);
			}
		}

		// check for cancellation
		progressDialog.setValue(25);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		if (progressDialog.wasCanceled() || processError)
		{
			closeConnection();
			setStatusMsg("Connect action canceled");
			goto EXIT;
		}

		// if Y-axis active, create a QProcess controller
		if (magnetParams->GetYAxisParams()->activate)
		{
            if (yProcess == nullptr)
			{
				yProcess = new ProcessManager(this);
				connect(yProcess, SIGNAL(magnetDAQError()), this, SLOT(magnetDAQVersionError()));
			}

			if (!yProcess->isActive())
			{
				if (simulation)
					yProcess->connectProcess(addressStr, Y_AXIS, simulation);
				else
					yProcess->connectProcess(magnetParams->GetYAxisParams()->ipAddress, Y_AXIS, simulation);
			}
		}

		// check for cancellation
		progressDialog.setValue(50);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		if (progressDialog.wasCanceled() || processError)
		{
			closeConnection();
			setStatusMsg("Connect action canceled");
			goto EXIT;
		}

		// if Z-axis active, create a QProcess controller
		if (magnetParams->GetZAxisParams()->activate)
		{
            if (zProcess == nullptr)
			{
				zProcess = new ProcessManager(this);
				connect(zProcess, SIGNAL(magnetDAQError()), this, SLOT(magnetDAQVersionError()));
			}

			if (!zProcess->isActive())
			{
				if (simulation)
					zProcess->connectProcess(addressStr, Z_AXIS, simulation);
				else
					zProcess->connectProcess(magnetParams->GetZAxisParams()->ipAddress, Z_AXIS, simulation);
			}
		}

		// check for cancellation
		progressDialog.setValue(75);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		if (progressDialog.wasCanceled() || processError)
		{
			closeConnection();
			setStatusMsg("Connect action canceled");
			goto EXIT;
		}

		// if X-axis active, send setup parameters
		if (magnetParams->GetXAxisParams()->activate)
		{
			if (xProcess->isActive())
			{
				xProcess->sendParams(magnetParams->GetXAxisParams(), fieldUnits, ui.actionTest_Mode->isChecked(), ui.actionStabilizingResistors->isChecked());
				x_activated = true;

				// switch heating/cooling required?
				if (magnetParams->GetXAxisParams()->switchInstalled)
				{
					switchInstalled = true;
					longestHeatingTime = magnetParams->GetXAxisParams()->switchHeatingTime;
					longestCoolingTime = magnetParams->GetXAxisParams()->switchCoolingTime;
				}
			}
		}

		// check for cancellation
		progressDialog.setValue(80);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		if (progressDialog.wasCanceled() || processError)
		{
			closeConnection();
			setStatusMsg("Connect action canceled");
			goto EXIT;
		}

		// if Y-axis active, send setup parameters
		if (magnetParams->GetYAxisParams()->activate)
		{
			if (yProcess->isActive())
			{
				yProcess->sendParams(magnetParams->GetYAxisParams(), fieldUnits, ui.actionTest_Mode->isChecked(), ui.actionStabilizingResistors->isChecked());
				y_activated = true;

				// switch heating/cooling required?
				if (magnetParams->GetYAxisParams()->switchInstalled)
				{
					switchInstalled = true;

					if (magnetParams->GetYAxisParams()->switchHeatingTime > longestHeatingTime)
						longestHeatingTime = magnetParams->GetYAxisParams()->switchHeatingTime;

					if (magnetParams->GetYAxisParams()->switchCoolingTime > longestCoolingTime)
						longestCoolingTime = magnetParams->GetYAxisParams()->switchCoolingTime;
				}
			}
		}

		// check for cancellation
		progressDialog.setValue(90);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		if (progressDialog.wasCanceled() || processError)
		{
			closeConnection();
			setStatusMsg("Connect action canceled");
			goto EXIT;
		}

		// if Z-axis active, send setup parameters
		if (magnetParams->GetZAxisParams()->activate)
		{
			if (zProcess->isActive())
			{
				zProcess->sendParams(magnetParams->GetZAxisParams(), fieldUnits, ui.actionTest_Mode->isChecked(), ui.actionStabilizingResistors->isChecked());
				z_activated = true;

				// switch heating/cooling required?
				if (magnetParams->GetZAxisParams()->switchInstalled)
				{
					switchInstalled = true;

					if (magnetParams->GetZAxisParams()->switchHeatingTime > longestHeatingTime)
						longestHeatingTime = magnetParams->GetZAxisParams()->switchHeatingTime;

					if (magnetParams->GetZAxisParams()->switchCoolingTime > longestCoolingTime)
						longestCoolingTime = magnetParams->GetZAxisParams()->switchCoolingTime;
				}
			}
		}

		progressDialog.setValue(100);
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

		// start data collection and update interface to indicate successfully connected status
		if (x_activated && y_activated && z_activated && !processError)
		{
			connected = true;

			// start data collection timer
			dataTimer->start();

			// if a switch is present, wait the heating time
			if (switchInstalled)
			{
				// switch heating already initiated by ProcessManager::sendParams()
				// start switch heating timer
				systemState = SYSTEM_HEATING;
				setStatusMsg("Heating switches, please wait...");
				elapsedHeatingTicks = 0;
				switchHeatingTimer->start();

				ui.actionPersistentMode->setChecked(false);
				ui.actionPersistentMode->setEnabled(true);
				ui.menuBar->setEnabled(false);
				ui.mainTabWidget->setEnabled(false);
				ui.mainToolBar->setEnabled(false);
			}
			else
			{
				ui.actionPersistentMode->setChecked(false);
				ui.actionPersistentMode->setEnabled(false);
				setStatusMsg("All active axes initialized successfully");
			}

			statusConnectState->setStyleSheet("color: green; font: bold;");
			statusConnectState->setText("CONNECTED");

			statusState->setStyleSheet("color: black; font: bold;");
			statusState->setText("PAUSED");
			systemState = SYSTEM_PAUSED;

			// don't change field units while connected
			ui.actionKilogauss->setEnabled(false);
			ui.actionTesla->setEnabled(false);

			// don't change test mode while connected
			ui.actionTest_Mode->setEnabled(false);

			// can't load settings while connected
			ui.actionLoad_Settings->setEnabled(false);

			// no change in stabilizing resistors while connected
			ui.actionStabilizingResistors->setEnabled(false);

			// allow vector selection
			ui.manualVectorControlGroupBox->setEnabled(true);
			ui.autostepStartButton->setEnabled(true);
			ui.manualPolarControlGroupBox->setEnabled(true);
			ui.autostepStartButtonPolar->setEnabled(true);

			// setup sample alignment interface on connect
			alignmentTabConnect();
		}
		else // something went wrong, indicate an error
		{
			closeConnection();
			setStatusMsg("Failed to communicate with all active magnet axes");
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
		stopPolarAutostep();

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

        if (pFile == nullptr)
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
			// load it!
			loadFromFile(pFile);

			fclose(pFile);

			// sync the UI with the newly loaded values
			magnetParams->syncUI();
			magnetParams->save();

			updateWindowTitle();
			vectorSelectionChanged();
			polarSelectionChanged();
		}
	}
}

//---------------------------------------------------------------------------
bool MultiAxisOperation::loadFromFile(FILE *pFile)
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
        ui.actionShow_Cartesian_Coordinates->setChecked(static_cast<bool>(stream.readLine().toInt()));
		actionShow_Cartesian_Coordinates();
        ui.actionShow_Spherical_Coordinates->setChecked(static_cast<bool>(stream.readLine().toInt()));
		actionShow_Spherical_Coordinates();
        fieldUnits = static_cast<FieldUnits>(stream.readLine().toInt());
		setFieldUnits(fieldUnits, true);
        convention = static_cast<SphericalConvention>(stream.readLine().toInt());
		setSphericalConvention(convention, true);
        ui.actionAutosave_Report->setChecked(static_cast<bool>(stream.readLine().toInt()));
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
				}
			}
			else
				ui.vectorsTableWidget->setColumnCount(columnCount);
		}
		else if (version >= 2)
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

                if (item == nullptr)
					ui.vectorsTableWidget->setHorizontalHeaderItem(i, item = new QTableWidgetItem(tempStr));
				else
					ui.vectorsTableWidget->horizontalHeaderItem(i)->setText(tempStr);
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

                if (item == nullptr)
					ui.vectorsTableWidget->setItem(i, j, item = new QTableWidgetItem(tempStr));
				else
					item->setText(tempStr);

				if (j == 4)
					item->setTextAlignment(Qt::AlignCenter | Qt::AlignVCenter);
				else
					item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
			}
		}

		if (version >= 2)
		{
			// recover coordinate system selection
            loadedCoordinates = static_cast<CoordinatesSelection>(stream.readLine().toInt());

			if (loadedCoordinates == CARTESIAN_COORDINATES)
				setTableHeader();
		}
		else
			loadedCoordinates = SPHERICAL_COORDINATES;	// only type supported < v2

		if (version >= 3)
		{
			// recover alignment tab contents
			alignmentTabLoadFromStream(&stream);

			// reload polar table
			ui.polarTableWidget->setRowCount(stream.readLine().toInt());

			// load columns count
			int columnCount = stream.readLine().toInt();

			// recover horizontal header labels
			ui.polarTableWidget->setColumnCount(columnCount);

			for (int i = 0; i < columnCount; i++)
			{
				// read saved header label
				QString tempStr = stream.readLine();
				QTableWidgetItem *item;

				item = ui.polarTableWidget->horizontalHeaderItem(i);

                if (item == nullptr)
					ui.polarTableWidget->setHorizontalHeaderItem(i, item = new QTableWidgetItem(tempStr));
				else
					ui.polarTableWidget->horizontalHeaderItem(i)->setText(tempStr);
			}

			ui.polarTableWidget->horizontalHeaderItem(1)->setText(thetaStr);

			// recover table text for each item
			for (int i = 0; i < ui.polarTableWidget->rowCount(); i++)
			{
				for (int j = 0; j < columnCount; j++)
				{
					QTableWidgetItem *item;

					item = ui.polarTableWidget->item(i, j);

					// read saved text
					QString tempStr = stream.readLine();

                    if (item == nullptr)
						ui.polarTableWidget->setItem(i, j, item = new QTableWidgetItem(tempStr));
					else
						item->setText(tempStr);

					item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
				}
			}
		}

		if (version >= 4)
		{
			setStabilizingResistorAvailability();
            ui.actionStabilizingResistors->setChecked(static_cast<bool>(stream.readLine().toInt()));
		}
		else
		{
			// value didn't exist in pre-version 4 file saves, so set to unchecked
			setStabilizingResistorAvailability();
			ui.actionStabilizingResistors->setChecked(false);
		}
	}

#if defined(Q_OS_MACOS)
        ui.vectorsTableWidget->repaint();
        ui.polarTableWidget->repaint();
#endif
	return true;
}

//---------------------------------------------------------------------------
void MultiAxisOperation::loadParams(QTextStream *stream, AxesParams *params)
{
    params->activate = (static_cast<bool>(stream->readLine().toInt()));
	params->ipAddress = stream->readLine();
	params->currentLimit = stream->readLine().toDouble();
	params->voltageLimit = stream->readLine().toDouble();
	params->maxRampRate = stream->readLine().toDouble();
	params->coilConst = stream->readLine().toDouble();
	params->inductance = stream->readLine().toDouble();
    params->switchInstalled = (static_cast<bool>(stream->readLine().toInt()));
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

        if (pFile == nullptr)
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
			// save it!
			saveToFile(pFile);

			fclose(pFile);
		}
	}
}

//---------------------------------------------------------------------------
bool MultiAxisOperation::saveToFile(FILE *pFile)
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

        if (item == nullptr)
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

            if (item == nullptr)
				stream << "\n";
			else
				stream << item->text() << "\n";
		}
	}

	// save coordinate selection
	stream << loadedCoordinates << "\n";

	// save alignment tab contents
	alignmentTabSaveToStream(&stream);

	// save polar table
	stream << ui.polarTableWidget->rowCount() << "\n";
	stream << ui.polarTableWidget->columnCount() << "\n";

	// save horizontal header labels
	for (int i = 0; i < ui.polarTableWidget->columnCount(); i++)
	{
		QTableWidgetItem *item;

		item = ui.polarTableWidget->horizontalHeaderItem(i);

        if (item == nullptr)
			stream << "\n";
		else
			stream << item->text() << "\n";
	}

	// save polar table contents
	for (int i = 0; i < ui.polarTableWidget->rowCount(); i++)
	{
		for (int j = 0; j < ui.polarTableWidget->columnCount(); j++)
		{
			QTableWidgetItem *item;

			item = ui.polarTableWidget->item(i, j);

            if (item == nullptr)
				stream << "\n";
			else
				stream << item->text() << "\n";
		}
	}

	// save stabilizing resistors selection
	stream << ui.actionStabilizingResistors->isChecked() << "\n";

	stream.flush();

	return true;
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
    if (magnetParams == nullptr)
		magnetParams = new MagnetParams();

	if (ui.actionConnect->isChecked())
		magnetParams->setReadOnly();
	else
		magnetParams->clearReadOnly();

	magnetParams->exec();

	updateWindowTitle();
	setStabilizingResistorAvailability();
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
		ui.magnetFieldLabel->setVisible(true);
	}
	else
	{
		ui.magnetXLabel->setVisible(false);
		ui.magnetXValue->setVisible(false);
		ui.magnetYLabel->setVisible(false);
		ui.magnetYValue->setVisible(false);
		ui.magnetZLabel->setVisible(false);
		ui.magnetZValue->setVisible(false);

		if (!ui.actionShow_Spherical_Coordinates->isChecked())
			ui.magnetFieldLabel->setVisible(false);
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
		ui.magnetFieldLabel->setVisible(true);
	}
	else
	{
		ui.magnetMagnitudeLabel->setVisible(false);
		ui.magnetMagnitudeValue->setVisible(false);
		ui.magnetThetaLabel->setVisible(false);
		ui.magnetThetaValue->setVisible(false);
		ui.magnetPhiLabel->setVisible(false);
		ui.magnetPhiValue->setVisible(false);

		if (!ui.actionShow_Cartesian_Coordinates->isChecked())
			ui.magnetFieldLabel->setVisible(false);
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

	if (autostepTimer->isActive())	// first checks for active autostep sequence
	{
		calculateAutostepRemainingTime(presentVector + 1, autostepEndIndex);
	}

	if (autostepPolarTimer->isActive())	// first checks for active polar autostep sequence
	{
		calculatePolarRemainingTime(presentPolar + 1, autostepEndIndexPolar);
	}

	if (!lastTargetMsg.isEmpty())
		setStatusMsg(lastTargetMsg);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionZero(void)
{
	sendNextVector(0, 0, 0);
	lastTargetMsg = "Target Vector : Zero Field Vector";
	setStatusMsg(lastTargetMsg);
	targetSource = NO_SOURCE;
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionChange_Units(void)
{
    FieldUnits newUnits = static_cast<FieldUnits>(!ui.actionKilogauss->isChecked());
	setFieldUnits(newUnits, false);
	convertFieldValues(newUnits, true);
	convertAlignmentFieldValues(newUnits);
	convertPolarFieldValues(newUnits);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionChange_Convention(void)
{
    SphericalConvention newSetting = static_cast<SphericalConvention>(!ui.actionUse_Mathematical_Convention->isChecked());
	setSphericalConvention(newSetting, false);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionConvention_Help(void)
{
	QString link = "https://en.wikipedia.org/wiki/Spherical_coordinate_system";
	QDesktopServices::openUrl(QUrl(link));
}

//---------------------------------------------------------------------------
void MultiAxisOperation::setStatusMsg(QString msg)
{
	// always save the msg
	lastStatusString = msg;

	if (!errorStatusIsActive)	// show it now!
		statusMisc->setText(msg);
}

//---------------------------------------------------------------------------
void MultiAxisOperation::showErrorString(QString errMsg)
{
	// reselect last good vector row if applicable
	if (targetSource == VECTOR_TABLE)
	{
		if (lastVector >= 0 && lastVector < ui.vectorsTableWidget->rowCount())
		{
			ui.vectorsTableWidget->selectRow(lastVector);
			presentVector = lastVector;
		}
	}
	else if (targetSource == POLAR_TABLE)
	{
		if (lastPolar >= 0 && lastPolar < ui.polarTableWidget->rowCount())
		{
			ui.polarTableWidget->selectRow(lastPolar);
			presentPolar = lastPolar;
		}
	}

	errorStatusIsActive = true;
	statusMisc->setStyleSheet("color: red; font: bold");
	statusMisc->setText("ERROR: " + errMsg);
	qDebug() << statusMisc->text();		// send to log
	QTimer::singleShot(5000, this, SLOT(errorStatusTimeout()));
}

//---------------------------------------------------------------------------
void MultiAxisOperation::parserErrorString(QString errMsg)
{
	errorStatusIsActive = true;
	statusMisc->setStyleSheet("color: red; font: bold");
	statusMisc->setText("Remote Error: " + errMsg);
	QTimer::singleShot(5000, this, SLOT(errorStatusTimeout()));
}

//---------------------------------------------------------------------------
void MultiAxisOperation::errorStatusTimeout(void)
{
	statusMisc->setStyleSheet("");
	statusMisc->setText(lastStatusString);	// restore normal messages
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

		tempStr = ui.alignMagLabel1->text().replace("(T)", "(kG)");
		ui.alignMagLabel1->setText(tempStr);

		tempStr = ui.alignMagLabel2->text().replace("(T)", "(kG)");
		ui.alignMagLabel2->setText(tempStr);

		tempStr = ui.polarTableWidget->horizontalHeaderItem(0)->text().replace("(T)", "(kG)");
		ui.polarTableWidget->horizontalHeaderItem(0)->setText(tempStr);
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

		tempStr = ui.alignMagLabel1->text().replace("(kG)", "(T)");
		ui.alignMagLabel1->setText(tempStr);

		tempStr = ui.alignMagLabel2->text().replace("(kG)", "(T)");
		ui.alignMagLabel2->setText(tempStr);

		tempStr = ui.polarTableWidget->horizontalHeaderItem(0)->text().replace("(kG)", "(T)");
		ui.polarTableWidget->horizontalHeaderItem(0)->setText(tempStr);
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
		ui.magnetThetaLabel->setText("Azimuth, " + thetaStr);
		ui.magnetPhiLabel->setText("Inclination, " + phiStr);
		ui.alignThetaLabel1->setText("Azimuth, " + thetaStr);
		ui.alignThetaLabel2->setText("Azimuth, " + thetaStr);
		ui.alignPhiLabel1->setText("Inclination, " + phiStr);
		ui.alignPhiLabel2->setText("Inclination, " + phiStr);
		if (loadedCoordinates == SPHERICAL_COORDINATES) // don't relabel tables in Cartesian coordinates
		{
			ui.vectorsTableWidget->horizontalHeaderItem(1)->setText(thetaStr);
			ui.vectorsTableWidget->horizontalHeaderItem(2)->setText(phiStr);
		}
	}
	else if (convention == ISO)
	{
		ui.magnetThetaLabel->setText("Azimuth, " + phiStr);
		ui.magnetPhiLabel->setText("Inclination, " + thetaStr);
		ui.alignThetaLabel1->setText("Azimuth, " + phiStr);
		ui.alignThetaLabel2->setText("Azimuth, " + phiStr);
		ui.alignPhiLabel1->setText("Inclination, " + thetaStr);
		ui.alignPhiLabel2->setText("Inclination, " + thetaStr);
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
					showUnitsError("Remote X-axis field units have been changed. Please verify remote units selection and reconnect.");
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
					showUnitsError("Remote Y-axis field units have been changed. Please verify remote units selection and reconnect.");
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
					showUnitsError("Remote Z-axis field units have been changed. Please verify remote units selection and reconnect.");
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

	//---------------------------------------------------------------------------
	// update state
	//---------------------------------------------------------------------------
	if ((x_activated && xState == QUENCH) ||
		(y_activated && yState == QUENCH) ||
		(z_activated && zState == QUENCH))
	{
		bool autoSave = false;
		magnetState = QUENCH;
		systemState = SYSTEM_QUENCH;
		statusState->setStyleSheet("color: red; font: bold;");
		statusState->setText("QUENCH!");

		// stop any autostep cycle
		if (autostepTimer->isActive())
		{
			autoSave = true;	// do the autosave on quench condition
			stopAutostep();
			lastTargetMsg.clear();
			setStatusMsg("Auto-Stepping aborted due to quench detection");
		}

		if (autostepPolarTimer->isActive())
		{
			stopPolarAutostep();
			lastTargetMsg.clear();
			setStatusMsg("Polar Auto-Stepping aborted due to quench detection");
		}

		// mark vector as fail only in Vector Table
		if (targetSource == VECTOR_TABLE)
		{
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
					QString cellStr = QString::number(xProcess->getQuenchCurrent(&ok), 'g', 3);

					if (ok)
					{
						QTableWidgetItem *cell = ui.vectorsTableWidget->item(presentVector, 5);

                        if (cell == nullptr)
							ui.vectorsTableWidget->setItem(presentVector, 5, cell = new QTableWidgetItem(cellStr));
						else
							cell->setText(cellStr);

						cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
					}
				}

				if (y_activated && yState == QUENCH)
				{
					bool ok;
					QString cellStr = QString::number(yProcess->getQuenchCurrent(&ok), 'g', 3);

					if (ok)
					{
						QTableWidgetItem *cell = ui.vectorsTableWidget->item(presentVector, 6);

                        if (cell == nullptr)
							ui.vectorsTableWidget->setItem(presentVector, 6, cell = new QTableWidgetItem(cellStr));
						else
							cell->setText(cellStr);

						cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
					}
				}

				if (z_activated && zState == QUENCH)
				{
					bool ok;
					QString cellStr = QString::number(zProcess->getQuenchCurrent(&ok), 'g', 3);

					if (ok)
					{
						QTableWidgetItem *cell = ui.vectorsTableWidget->item(presentVector, 7);

                        if (cell == nullptr)
							ui.vectorsTableWidget->setItem(presentVector, 7, cell = new QTableWidgetItem(cellStr));
						else
							cell->setText(cellStr);

						cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
					}
				}
			}

			doAutosaveReport();
		}
		else if (!quenchLogged) // log only once
		{
			// Save quench data in log
			bool ok;
			QString msg = "Quench Detect!! ";

			if (x_activated && xState == QUENCH)
				msg += ": X=" + QString::number(xProcess->getQuenchCurrent(&ok), 'g', 3) + " A";

			if (y_activated && yState == QUENCH)
				msg += ": Y=" + QString::number(yProcess->getQuenchCurrent(&ok), 'g', 3) + " A";

			if (z_activated && zState == QUENCH)
				msg += ": Z=" + QString::number(zProcess->getQuenchCurrent(&ok), 'g', 3) + " A";

			qDebug() << msg;
			quenchLogged = true;
		}
	}
	else if (switchHeatingTimer->isActive())
	{
		magnetState = SWITCH_HEATING;
		systemState = SYSTEM_HEATING;
		statusState->setStyleSheet("color: black; font: bold;");
		statusState->setText("HEATING SWITCH");
		if (switchInstalled)
			ui.actionPersistentMode->setEnabled(false);
	}
	else if (switchCoolingTimer->isActive())
	{
		magnetState = SWITCH_COOLING;
		systemState = SYSTEM_COOLING;
		statusState->setStyleSheet("color: black; font: bold;");
		statusState->setText("COOLING SWITCH");
		if (switchInstalled)
			ui.actionPersistentMode->setEnabled(true);
	}
	else if ((x_activated && xState == RAMPING) ||
			 (y_activated && yState == RAMPING) ||
			 (z_activated && zState == RAMPING))
	{
		if (remainingTime)
			remainingTime--;	// decrement by one second

		if (autostepTimer->isActive())	// first checks for active autostep sequence
		{
			if (autostepRemainingTime)
			{
				autostepRemainingTime--;	// decrement by one second
				displayAutostepRemainingTime();
			}
		}

		if (autostepPolarTimer->isActive())	// first checks for active polar autostep sequence
		{
			if (polarRemainingTime)
			{
				polarRemainingTime--;	// decrement by one second
				displayPolarRemainingTime();
			}
		}

		magnetState = RAMPING;
		systemState = SYSTEM_RAMPING;
		statusState->setStyleSheet("color: black; font: bold;");
		if (switchInstalled)
			ui.actionPersistentMode->setEnabled(false);

		if (remainingTime)	// display formatted remaining ramping time hh:mm:ss
		{
			int hours, minutes, seconds, remainder;

			hours = remainingTime / 3600;
			remainder = remainingTime % 3600;
			minutes = remainder / 60;
			seconds = remainder % 60;

			QString timeStr;

			if (hours > 0)
				timeStr = QString("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
			else if (minutes > 0)
				timeStr = QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
			else
				timeStr = QString("%1").arg(seconds);

			statusState->setText("RAMPING (" + timeStr + ")");
		}
		else
			statusState->setText("RAMPING");

		quenchLogged = false;
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
			// update total remaining time on first transition to HOLDING
			if (magnetState != HOLDING)
			{
				if (autostepTimer->isActive())	// first checks for active autostep sequence
				{
					calculateAutostepRemainingTime(presentVector + 1, autostepEndIndex);
				}

				if (autostepPolarTimer->isActive())	// first checks for active autostep sequence
				{
					calculatePolarRemainingTime(presentPolar + 1, autostepEndIndexPolar);
				}
			}

			magnetState = HOLDING;
			systemState = SYSTEM_HOLDING;
			statusState->setStyleSheet("color: green; font: bold;");
			statusState->setText("HOLDING");
			quenchLogged = false;
			if (switchInstalled)
				ui.actionPersistentMode->setEnabled(true);

			// mark vector as pass only in Vector Table
			if (targetSource == VECTOR_TABLE)
			{
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
			}

			passCnt = 0;	// reset
		}

		if (autostepTimer->isActive())	// first checks for active autostep sequence
		{
			if (autostepRemainingTime)
			{
				autostepRemainingTime--;	// decrement by one second
				displayAutostepRemainingTime();
			}
		}

		if (autostepPolarTimer->isActive())	// first checks for active polar autostep sequence
		{
			if (polarRemainingTime)
			{
				polarRemainingTime--;	// decrement by one second
				displayPolarRemainingTime();
			}
		}
	}
	else if ((x_activated && xState == PAUSED) ||
			 (y_activated && yState == PAUSED) ||
			 (z_activated && zState == PAUSED))
	{
		magnetState = PAUSED;
		systemState = SYSTEM_PAUSED;
		statusState->setStyleSheet("color: black; font: bold;");
		statusState->setText("PAUSED");
		quenchLogged = false;
		if (switchInstalled)
			ui.actionPersistentMode->setEnabled(true);
	}
	else if ((x_activated && xState == ZEROING) ||
			 (y_activated && yState == ZEROING) ||
			 (z_activated && zState == ZEROING))
	{
		magnetState = ZEROING;
		systemState = SYSTEM_ZEROING;
		statusState->setStyleSheet("color: black; font: bold;");
		statusState->setText("ZEROING");
		quenchLogged = false;
		if (switchInstalled)
			ui.actionPersistentMode->setEnabled(false);
	}
	else if ((!x_activated || (x_activated && xState == AT_ZERO)) &&
			 (!y_activated || (y_activated && yState == AT_ZERO)) &&
			 (!z_activated || (z_activated && zState == AT_ZERO)))
	{
		magnetState = AT_ZERO;
		systemState = SYSTEM_AT_ZERO;
		statusState->setStyleSheet("color: black; font: bold;");
		statusState->setText("AT ZERO");
		quenchLogged = false;
		if (switchInstalled)
			ui.actionPersistentMode->setEnabled(false);
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

		// allow target changes
		ui.makeAlignActiveButton1->setEnabled(true);
		ui.makeAlignActiveButton2->setEnabled(true);
		ui.manualVectorControlGroupBox->setEnabled(true);
		ui.autoStepGroupBox->setEnabled(true);
		ui.manualPolarControlGroupBox->setEnabled(true);
		ui.autoStepGroupBoxPolar->setEnabled(true);
		ui.actionRamp->setEnabled(true);
		ui.actionPause->setEnabled(true);
		ui.actionZero->setEnabled(true);

		setStatusMsg("All active axes initialized successfully; switches heated");
	}
	else
	{
		if (!errorStatusIsActive)
		{
			// update the heating countdown
			QString tempStr = statusMisc->text();
			int index = tempStr.indexOf('(');
			if (index >= 1)
				tempStr.truncate(index - 1);

			QString timeStr = " (" + QString::number(longestHeatingTime - elapsedHeatingTicks) + " sec remaining)";
			setStatusMsg(tempStr + timeStr);
		}
	}
}

//---------------------------------------------------------------------------
void MultiAxisOperation::switchCoolingTimerTick(void)
{
	elapsedCoolingTicks++;

	if (elapsedCoolingTicks >= longestCoolingTime)
	{
		// release interface to continue
		switchCoolingTimer->stop();
		ui.menuBar->setEnabled(true);
		ui.mainTabWidget->setEnabled(true);
		ui.mainToolBar->setEnabled(true);

		// disallow target changes
		ui.makeAlignActiveButton1->setEnabled(false);
		ui.makeAlignActiveButton2->setEnabled(false);
		ui.manualVectorControlGroupBox->setEnabled(false);
		ui.autoStepGroupBox->setEnabled(false);
		ui.manualPolarControlGroupBox->setEnabled(false);
		ui.autoStepGroupBoxPolar->setEnabled(false);
		ui.actionRamp->setEnabled(false);
		ui.actionPause->setEnabled(false);
		ui.actionZero->setEnabled(false);

		setStatusMsg("All installed switches cooled, magnet in persistent mode");
	}
	else
	{
		if (!errorStatusIsActive)
		{
			// update the cooling countdown
			QString tempStr = statusMisc->text();
			int index = tempStr.indexOf('(');
			if (index >= 1)
				tempStr.truncate(index - 1);

			QString timeStr = " (" + QString::number(longestCoolingTime - elapsedCoolingTicks) + " sec remaining)";
			setStatusMsg(tempStr + timeStr);
		}
	}
}

//---------------------------------------------------------------------------
VectorError MultiAxisOperation::checkNextVector(double x, double y, double z, QString label)
{
	// check limits first; if an axis is not activated yet has a non-zero vector value, throw an error
	if (magnetParams->GetXAxisParams()->activate)
	{
		double checkValue = fabs(x / magnetParams->GetXAxisParams()->coilConst);

		if (checkValue > magnetParams->GetXAxisParams()->currentLimit)
		{
			showErrorString(label + " exceeds X-axis Current Limit!");
			QApplication::beep();
			return EXCEEDS_X_RANGE;
		}
	}
	else
	{
		if (fabs(x) > 1e-12)
		{
			// can't achieve the vector as there is no X-axis activated
			showErrorString(label + " requires an active X-axis field component!");
			QApplication::beep();
			return INACTIVE_X_AXIS;
		}
	}

	if (magnetParams->GetYAxisParams()->activate)
	{
		double checkValue = fabs(y / magnetParams->GetYAxisParams()->coilConst);

		if (checkValue > magnetParams->GetYAxisParams()->currentLimit)
		{
			showErrorString(label + " exceeds Y-axis Current Limit!");
			QApplication::beep();
			return EXCEEDS_Y_RANGE;
		}
	}
	else
	{
		if (fabs(y) > 1e-12)
		{
			// can't achieve the vector as there is no Y-axis activated
			showErrorString(label + " requires an active Y-axis field component!");
			QApplication::beep();
			return INACTIVE_Y_AXIS;
		}
	}

	if (magnetParams->GetZAxisParams()->activate)
	{
		double checkValue = fabs(z / magnetParams->GetZAxisParams()->coilConst);

		if (checkValue > magnetParams->GetZAxisParams()->currentLimit)
		{
			showErrorString(label + " exceeds Z-axis Current Limit!");
			QApplication::beep();
			return EXCEEDS_Z_RANGE;
		}
	}
	else
	{
		if (fabs(z) > 1e-12)
		{
			// can't achieve the vector as there is no Z-axis activated
			showErrorString(label + " requires an active Z-axis field component!");
			QApplication::beep();
			return INACTIVE_Z_AXIS;
		}
	}

	{
		double temp = sqrt(x * x + y * y + z * z);

		if (temp > magnetParams->getMagnitudeLimit())
		{
			showErrorString(label + " exceeds Magnitude Limit of Magnet!");
			QApplication::beep();
			return EXCEEDS_MAGNITUDE_LIMIT;
		}
	}

	return NO_VECTOR_ERROR;
}

//---------------------------------------------------------------------------
// Sends next vector down to axes
//---------------------------------------------------------------------------
void MultiAxisOperation::sendNextVector(double x, double y, double z)
{
	double xRampRate, yRampRate, zRampRate;	// A/sec

	// calculate ramping rates and time
	remainingTime = calculateRampingTime(x, y, z, xField, yField, zField, xRampRate, yRampRate, zRampRate);

	// target field is good, save active target values
	xTarget = x;
	yTarget = y;
	zTarget = z;

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
}

//---------------------------------------------------------------------------
// Uses the maxRampRate for each axis to determine the ramp rate required
// for each axes to arrive at the next vector simultaneously. The vector
// (x, y, z) must be specified in Cartesian coordinates.
//---------------------------------------------------------------------------
int MultiAxisOperation::calculateRampingTime(double x, double y, double z, double _xField, double _yField, double _zField, double &xRampRate, double &yRampRate, double &zRampRate)
{
	double deltaX, deltaY, deltaZ;	// kG
	double xTime, yTime, zTime;		// sec
	int rampTime = 0;

	// find delta field for each axis
	deltaX = fabsl(_xField - x);
	deltaY = fabsl(_yField - y);
	deltaZ = fabsl(_zField - z);

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
	if ((xTime >= yTime && xTime >= zTime) && xTime != 0.0)
	{
		rampTime = static_cast<int>(round(xTime));

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
	else if ((yTime >= xTime && yTime >= zTime) && yTime != 0.0)
	{
		rampTime = static_cast<int>(round(yTime));

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
	else if ((zTime >= xTime && zTime >= yTime) && zTime != 0.0)
	{
		rampTime = static_cast<int>(round(zTime));

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
	else
	{
		if (magnetParams->GetXAxisParams()->activate)
			xRampRate = MIN_RAMP_RATE;
		else
			xRampRate = 0;

		if (magnetParams->GetYAxisParams()->activate)
			yRampRate = MIN_RAMP_RATE;
		else
			yRampRate = 0;

		if (magnetParams->GetZAxisParams()->activate)
			zRampRate = MIN_RAMP_RATE;
		else
			zRampRate = 0;
	}

	return rampTime;
}

//---------------------------------------------------------------------------
void MultiAxisOperation::actionPersistentMode(void)
{
	if (ui.actionPersistentMode->isChecked())
	{
		// first, check for HOLDING or PAUSED state
		if (systemState == SYSTEM_HOLDING || systemState == SYSTEM_PAUSED)
		{
			// enter persistent mode, cool switch(es)
			switchControl(false);	// turn off heater
			systemState = SYSTEM_COOLING;
			setStatusMsg("Cooling switches, please wait...");
			elapsedCoolingTicks = 0;
			switchCoolingTimer->start();

			ui.menuBar->setEnabled(false);
			ui.mainTabWidget->setEnabled(false);
			ui.mainToolBar->setEnabled(false);
		}
		else
		{
			// error message, cannot enter persistent mode while not HOLDING or PAUSED
			ui.actionPersistentMode->setChecked(false);
			errorStatusIsActive = true;
			statusMisc->setStyleSheet("color: red; font: bold");
			statusMisc->setText("ERROR: Cannot enter persistent mode unless in HOLDING or PAUSED state");
			QTimer::singleShot(5000, this, SLOT(errorStatusTimeout()));
		}
	}
	else
	{
		// exit persistent mode, heat switch(es)
		switchControl(true);	// turn on heater
		systemState = SYSTEM_HEATING;
		setStatusMsg("Heating switches, please wait...");
		elapsedHeatingTicks = 0;
		switchHeatingTimer->start();

		ui.menuBar->setEnabled(false);
		ui.mainTabWidget->setEnabled(false);
		ui.mainToolBar->setEnabled(false);
	}
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
void MultiAxisOperation::magnetDAQVersionError(void)
{
    if (!processError)
    {
        processError = true;

        QMessageBox msgBox;

        msgBox.setWindowTitle("Magnet-DAQ Version Error");
        msgBox.setText("Magnet-DAQ installed version must be 1.05 or later!\n\nPlease download and install version 1.05 or later.");
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.setIcon(QMessageBox::Critical);
        int ret = msgBox.exec();
    }
}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
