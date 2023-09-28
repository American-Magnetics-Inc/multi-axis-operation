#pragma once

#include <QObject>
#include <QProcess>
#include "magnetparams.h"

class ProcessManager : public QObject
{
	Q_OBJECT

public:
	ProcessManager(QObject *parent);
	~ProcessManager();
	bool isActive(void) { return ((process->isOpen() && started) ? true : false); }
	void sendParams(AxesParams *params, FieldUnits units, bool testMode, bool useStabilizingResistors, bool disableAutoStabilty, bool readParams);
	double getMagnetCurrent(bool *error);
	double getSupplyCurrent(bool *error);
	double getField(bool *error);
	double getQuenchCurrent(bool *error);
	double getCurrentLimit(bool* error);
	double getVoltageLimit(bool* error);
	double getCoilConstant(bool* error);
	double getInductance(bool* error);
	bool getSwitchInstallation(bool* error);
	double getSwitchCurrent(bool* error);
	int getSwitchHeatingTime(bool* error);
	int getSwitchCoolingTime(bool* error);
	FieldUnits getUnits(void);
	State getState(void);
	bool getPSwitchHeaterState(void);
	void sendRamp(void);
	void sendPause(void);
	void sendRampToZero(void);
	void setTargetCurr(AxesParams *params, double value, bool isFieldValue);
	void setRampRateCurr(AxesParams *params, double rate);
	void heatSwitch(void);
	void coolSwitch(void);

signals:
	void magnetDAQError(void);

public slots:
	void connectProcess(QString anIPAddress, QString exepath, Axis anAxis, bool simulated, bool minimized);
	void processStarted(void);
	void processStateChanged(QProcess::ProcessState newState);
	void readyReadStandardOutput(void);

private:
	QProcess *process;
	QString ipAddress;
	Axis axis;
	bool started;
	QString reply;
};
