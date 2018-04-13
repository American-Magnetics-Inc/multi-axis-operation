#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_multiaxisoperation.h"
#include "magnetparams.h"
#include "processmanager.h"

typedef enum
{
	SPHERICAL_COORDINATES,
	CARTESIAN_COORDINATES
} CoordinatesSelection;

typedef enum
{
	MATHEMATICAL = 0,
	ISO
} SphericalConvention;


class MultiAxisOperation : public QMainWindow
{
	Q_OBJECT

public:
	MultiAxisOperation(QWidget *parent = Q_NULLPTR);
	~MultiAxisOperation();
	void updateWindowTitle();

private slots:
	void actionConnect(void);
	void actionLoad_Settings(void);
	void loadParams(QTextStream *stream, AxesParams *params);
	void actionSave_Settings(void);
	void saveParams(QTextStream *stream, AxesParams *params);
	void closeConnection(void);
	void actionAbout(void);
	void actionHelp(void);
	void actionDefine(void);
	void actionShow_Cartesian_Coordinates(void);
	void actionShow_Spherical_Coordinates(void);
	void actionPause(void);
	void actionRamp(void);
	void actionZero(void);
	void actionChange_Units(void);
	void actionChange_Convention(void);
	void actionConvention_Help(void);
	void showErrorString(QString errMsg);
	void errorStatusTimeout(void);
	void showUnitsError(QString errMsg);
	void setFieldUnits(FieldUnits units, bool updateMenuState);
	void convertFieldValues(FieldUnits newUnits, bool convertMagnetParams);
	void setSphericalConvention(SphericalConvention selection, bool updateMenuState);
	void actionLoad_Vector_Table(void);
	void setTableHeader(void);
	void actionSave_Vector_Table(void);
	void actionGenerate_Excel_Report(void);
	void saveReport(QString reportFileName);
	void goToSelectedVector(void);
	void goToNextVector(void);
	void goToVector(void);

	void abortAutostep();

	void startAutostep(void);
	void stopAutostep(void);
	void autostepTimerTick(void);
	void doAutosaveReport(void);
	void dataTimerTick(void);
	void switchHeatingTimerTick(void);
	bool sendNextVector(double x, double y, double z);
	void switchControl(bool heatSwitch);

private:
	Ui::MultiAxisOperationClass ui;
	QActionGroup *unitsGroup;
	QActionGroup *sphericalConvention;
	bool connected;	// are we connected?
	bool autosaveReport;
	bool haveAutosavedReport;
	bool simulation;	// use simulated system

	// error handling
	bool vectorError;	// last selected vector had error?
	int lastVector;		// last known good vector
	QString lastStatusString;
	QTimer *errorStatusTimer;
	std::atomic<bool> errorStatusIsActive;

	// present magnet state (all field stored in kG)
	double xField;
	double yField;
	double zField;
	double magnitudeField;

	// persistent switch management
	int longestHeatingTime;
	int elapsedHeatingTicks;
	QTimer *switchHeatingTimer;

	// all angles stored in degrees
	SphericalConvention convention;
	double thetaAngle;
	double phiAngle;
	QString thetaStr;
	QString phiStr;

	// STATE?
	State xState;
	State yState;
	State zState;
	State magnetState;
	int passCnt;

	// magnet parameters dialog
	MagnetParams *magnetParams;
	FieldUnits fieldUnits;
	
	// process managers
	ProcessManager *xProcess;
	ProcessManager *yProcess;
	ProcessManager *zProcess;

	// data collection
	QTimer *dataTimer;

	// status bar items
	QLabel *statusConnectState;
	QLabel *statusState;
	QLabel *statusMisc;

	// save/load all settings
	QString lastSavePath;
	QString saveFileName;

	// vector table
	CoordinatesSelection loadedCoordinates;
	QString vectorsFileName;
	QString lastVectorsLoadPath;
	QString saveVectorsFileName;
	QString lastVectorsSavePath;
	QString lastReportPath;
	QString reportFileName;
	int presentVector;

	// vector table autostepping
	QTimer *autostepTimer;
	int elapsedTimerTicks;
	int autostepStartIndex;
	int autostepEndIndex;
};
