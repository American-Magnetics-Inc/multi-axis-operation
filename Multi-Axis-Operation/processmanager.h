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
	void sendParams(AxesParams *params, FieldUnits units, bool testMode, bool useStabilizingResistors);
	double getMagnetCurrent(bool *error);
	double getField(bool *error);
	double getQuenchCurrent(bool *error);
	FieldUnits getUnits(void);
	State getState(void);
	void sendRamp(void);
	void sendPause(void);
	void setTargetCurr(AxesParams *params, double value);
	void setRampRateCurr(AxesParams *params, double rate);
	void heatSwitch(void);
	void coolSwitch(void);

public slots:
	void connectProcess(QString anIPAddress, Axis anAxis, bool simulated);
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
