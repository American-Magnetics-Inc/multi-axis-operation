#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_multiaxisoperation.h"
#include "magnetparams.h"
#include "processmanager.h"

//---------------------------------------------------------------------------
// Type declarations
//---------------------------------------------------------------------------
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

typedef enum
{
	DISCONNECTED = 0,
	SYSTEM_RAMPING,
	SYSTEM_HOLDING,
	SYSTEM_PAUSED,
	SYSTEM_ZEROING,
	SYSTEM_AT_ZERO,
	SYSTEM_QUENCH,
	SYSTEM_HEATING,
	SYSTEM_COOLING
} SystemState;

typedef enum
{
	NO_VECTOR_ERROR = 0,		// no error
	
	NON_NUMERICAL_ENTRY,		// non-numerical parameter
	EXCEEDS_MAGNITUDE_LIMIT,	// vector magnitude exceeds magnet limit
	NEGATIVE_MAGNITUDE,			// magnitude cannot be negative
	INCLINATION_OUT_OF_RANGE,	// inclination angle must be >=0 and <= 180
	EXCEEDS_X_RANGE,			// vector exceeds x-axis current limit
	INACTIVE_X_AXIS,			// vector requires x-axis field component which is inactive
	EXCEEDS_Y_RANGE,			// vector exceeds y-axis current limit
	INACTIVE_Y_AXIS,			// vector requires y-axis field component which is inactive
	EXCEEDS_Z_RANGE,			// vector exceeds z-axis current limit
	INACTIVE_Z_AXIS				// vector requires z-axis field component which is inactive
} VectorError;

typedef enum
{
	NO_SOURCE = 0,
	VECTOR_TABLE,
	ALIGN_TAB,
	POLAR_TABLE
} TargetSource;


//---------------------------------------------------------------------------
// MultiAxisOperation Class Header
//---------------------------------------------------------------------------
class MultiAxisOperation : public QMainWindow
{
	Q_OBJECT

public:
	MultiAxisOperation(QWidget *parent = Q_NULLPTR);
	~MultiAxisOperation();
	void updateWindowTitle();
	void polarToCartesian(double magnitude, double angle, QVector3D *conversion);

	// accessors for parser
	bool isConnected(void) { return connected; }
	bool isPersistent(void) { return ui.actionPersistentMode->isChecked(); }
	bool hasSwitch(void) { return switchInstalled; }
	int get_remaining_time(void) { return remainingTime; }
	int get_state(void) { return (int)systemState; }
	int get_field_units(void) { return (int)fieldUnits; }
	void get_align1(double *mag, double *azimuth, double *inclination);
	void get_align1_cartesian(double *x, double *y, double *z);
	void get_align2(double * mag, double * azimuth, double * inclination);
	void get_align2_cartesian(double *x, double *y, double *z);
	void get_active(double *mag, double *azimuth, double *inclination);
	void get_active_cartesian(double *x, double *y, double *z);
	void get_field(double *mag, double *azimuth, double *inclination);
	void get_field_cartesian(double *x, double *y, double *z);
	void get_plane(double *x, double *y, double *z);
	VectorError check_vector(double x, double y, double z);
	bool vec_table_row_in_range(int tableRow);
	VectorError check_vector_table(int tableRow);
	bool polar_table_row_in_range(int tableRow);
	VectorError check_polar_table(int tableRow);

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
	void actionPersistentMode(void);
	void actionPause(void);
	void actionRamp(void);
	void actionZero(void);
	void actionChange_Units(void);
	void actionChange_Convention(void);
	void actionConvention_Help(void);
	void showErrorString(QString errMsg);
	void parserErrorString(QString errMsg);
	void errorStatusTimeout(void);
	void showUnitsError(QString errMsg);
	void setFieldUnits(FieldUnits units, bool updateMenuState);
	void convertFieldValues(FieldUnits newUnits, bool convertMagnetParams);
	void setSphericalConvention(SphericalConvention selection, bool updateMenuState);

	void actionLoad_Vector_Table(void);
	void setTableHeader(void);
	void actionSave_Vector_Table(void);
	void vectorSelectionChanged(void);
	void vectorTableAddRowAbove(void);
	void initNewRow(int newRow);
	void updatePresentVector(int row, bool removed);
	void vectorTableAddRowBelow(void);
	void vectorTableRemoveRow(void);
	void vectorTableClear(void);
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
	void switchCoolingTimerTick(void);
	VectorError checkNextVector(double x, double y, double z, QString label);
	void sendNextVector(double x, double y, double z);
	void switchControl(bool heatSwitch);

	void alignmentTabInitState(void);
	void alignmentTabSaveState(void);
	void alignmentTabLoadFromStream(QTextStream *stream);
	void alignmentTabSaveToStream(QTextStream *stream);
	void convertAlignmentFieldValues(FieldUnits newUnits);
	void deactivateAlignmentVectors(void);
	void alignmentTabConnect(void);
	void alignmentTabDisconnect(void);
	void recalculateRotationVector(void);
	void alignVector1LockPressed(bool checked);
	void makeAlignVector1Active(bool checked);
	void alignVector1ValueChanged(double value);
	void alignVector1ThetaDialChanged(int value);
	void alignVector1PhiDialChanged(int value);
	bool checkAlignVector1(void);
	void sendAlignVector1(void);
	void alignVector2LockPressed(bool checked);
	void makeAlignVector2Active(bool checked);
	void alignVector2ValueChanged(double value);
	void alignVector2ThetaDialChanged(int value);
	void alignVector2PhiDialChanged(int value);
	bool checkAlignVector2(void);
	void sendAlignVector2(void);

	void setNormalUnitVector(QVector3D *v1, QVector3D *v2);
	void altPolarToCartesian(double magnitude, double angle, QVector3D *conversion);
	void actionLoad_Polar_Table(void);
	void convertPolarFieldValues(FieldUnits newUnits);
	void setPolarTableHeader(void);
	void actionSave_Polar_Table(void);
	void polarSelectionChanged(void);
	void polarTableAddRowAbove(void);
	void polarTableAddRowBelow(void);
	void initNewPolarRow(int newRow);
	void updatePresentPolar(int row, bool removed);
	void polarTableRemoveRow(void);
	void polarTableClear(void);
	void goToSelectedPolarVector(void);
	void goToNextPolarVector(void);
	void goToPolarVector(void);
	void startPolarAutostep(void);
	void abortPolarAutostep(void);
	void stopPolarAutostep(void);
	void autostepPolarTimerTick(void);

	// parser actions
	void system_connect(void);
	void system_disconnect(void);
	void load_settings(FILE *file, bool *success);
	void save_settings(FILE * file, bool * success);
	void set_align1(double mag, double azimuth, double inclination);
	void set_align1_active(void);
	void set_align2(double mag, double azimuth, double inclination);
	void set_align2_active(void);
	void set_units(int value);
	void set_vector(double mag, double az, double inc, int time);
	void set_vector_cartesian(double x, double y, double z, int time);
	void goto_vector(int tableRow);
	void set_polar(double mag, double angle, int time);
	void goto_polar(int tableRow);
	void set_persistence(bool persistent);

private:
	Ui::MultiAxisOperationClass ui;
	QActionGroup *unitsGroup;
	QActionGroup *sphericalConvention;
	bool connected;	// are we connected?
	SystemState systemState;
	bool autosaveReport;
	bool haveAutosavedReport;
	bool simulation;	// use simulated system
	bool useParser;		// if true, enable stdin/stdout parser
	int remainingTime;	// time remaining for arrival at target

	// error handling
	VectorError vectorError;	// last selected vector had error?
	QString lastStatusString;
	QTimer *errorStatusTimer;
	std::atomic<bool> errorStatusIsActive;
	TargetSource targetSource;

	// present magnet state (in current units)
	double xField;
	double yField;
	double zField;
	double magnitudeField;

	// present target vector
	double xTarget;
	double yTarget;
	double zTarget;

	// persistent switch management
	bool switchInstalled; // at least one axis with switch?
	int longestHeatingTime;
	int elapsedHeatingTicks;
	QTimer *switchHeatingTimer;

	int longestCoolingTime;
	int elapsedCoolingTicks;
	QTimer *switchCoolingTimer;

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
	int lastVector;		// last known good vector

	// vector table autostepping
	QTimer *autostepTimer;
	int elapsedTimerTicks;
	int autostepStartIndex;
	int autostepEndIndex;

	// align tab support variables
	// shadow values and previous dial states
	// shadow values are verified in-range and numerical
	double mag1ShadowVal;
	int theta1Dial;
	double theta1ShadowVal;
	int phi1Dial;
	double phi1ShadowVal;
	double mag2ShadowVal;
	int theta2Dial;
	double theta2ShadowVal;
	int phi2Dial;
	double phi2ShadowVal;

	// rotation within alignment axis
	QVector3D crossResult;	// normalized "normal" vector to sample alignment plane
	QQuaternion referenceQuaternion;
	QQuaternion rotationQuaternion;

	// polar table
	QString polarFileName;
	QString lastPolarLoadPath;
	QString savePolarFileName;
	QString lastPolarSavePath;
	int presentPolar;
	int lastPolar;	// last known good polar vector

	// polar table auto-stepping
	QTimer *autostepPolarTimer;
	int elapsedTimerTicksPolar;
	int autostepStartIndexPolar;
	int autostepEndIndexPolar;

	// private methods
	bool loadFromFile(FILE *pFile);	// returns true if success
	bool saveToFile(FILE *pFile);	// returns true if success
	void setStatusMsg(QString msg);
};
