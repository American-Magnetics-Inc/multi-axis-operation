#pragma once

#include <QDialog>
#include "ui_magnetparams.h"

enum Axis
{
	X_AXIS = 0,
	Y_AXIS,
	Z_AXIS
};

enum FieldUnits
{
	ERROR_UNITS = -1,
	KG = 0,
	TESLA
};

enum State
{
	ERROR_STATE = 0,
	RAMPING = 1,
	HOLDING,
	PAUSED,
	MANUAL_UP,
	MANUAL_DOWN,
	ZEROING,
	QUENCH,
	AT_ZERO,
	SWITCH_HEATING,
	SWITCH_COOLING,
	EXTERNAL_RAMPDOWN
};

struct AxesParams
{
	bool activate;
	QString ipAddress;
	double currentLimit;
	double voltageLimit;
	double maxRampRate;
	double coilConst;
	double inductance;
	bool switchInstalled;
	double switchHeaterCurrent;
	int switchCoolingTime;
	int switchHeatingTime;
};

class MagnetParams : public QDialog
{
	Q_OBJECT

public:
	MagnetParams(QWidget *parent = Q_NULLPTR);
	~MagnetParams();
	void setFieldUnits(FieldUnits units);
	void convertFieldValues(FieldUnits newUnits);
	QString getMagnetID(void) { return magnetID; }
	void setMagnetID(QString id) { magnetID = id; }
	double getMagnitudeLimit(void) { return (magnitudeLimit * 1.00001); }	// slight adjustment for round-off error
	void setMagnitudeLimit(double value) { magnitudeLimit = value; }
	bool switchInstalled(void);		// returns if any active axis has a persistent switch installed
	AxesParams *GetXAxisParams(void) { return &x; }
	AxesParams *GetYAxisParams(void) { return &y; }
	AxesParams *GetZAxisParams(void) { return &z; }
	void setReadOnly(void);
	void clearReadOnly(void);
	void syncUI(void);
	void save(void);

public slots:
	void actionActivate(bool checked);
	void switchConfigClicked(bool checked);
	void dialogButtonClicked(QAbstractButton * button);

private:
	Ui::MagnetParams ui;
	AxesParams x, y, z;
	double magnitudeLimit;
	QString magnetID;
	bool readOnly;

	void showError(QString errMsg);
	void xAxisSwitchUiConfig(bool checked, bool updateUI);
	void yAxisSwitchUiConfig(bool checked, bool updateUI);
	void zAxisSwitchUiConfig(bool checked, bool updateUI);
	void restore(void);
};
