#include "stdafx.h"
#include "processmanager.h"

const int QUERY_TIMEOUT = 3000;	// timeout in msec

//#define LOCAL_DEBUG

//---------------------------------------------------------------------------
// Constructor
//---------------------------------------------------------------------------
ProcessManager::ProcessManager(QObject *parent)
	: QObject(parent)
{
	started = false;
	process = new QProcess(this);
}

//---------------------------------------------------------------------------
// Destructor
//---------------------------------------------------------------------------
ProcessManager::~ProcessManager()
{
	QString cmd = "EXIT\n";
	process->write(cmd.toLocal8Bit());

	process->waitForFinished();
}

//---------------------------------------------------------------------------
void ProcessManager::connectProcess(QString anIPAddress, QString exepath, Axis anAxis, bool simulated, bool minimized)
{
	QStringList args, axisStr, networkStr;

	// ip address to which to connect
	ipAddress = anIPAddress;

	axis = anAxis;
	process->setProgram(exepath);

	if (QFileInfo::exists(exepath))
	{
		if (axis == X_AXIS)
		{
			axisStr << "-x";

			if (simulated)	// for AMI use only
				networkStr << "--port 7183" << "--telnet 7192";
		}
		else if (axis == Y_AXIS)
		{
			axisStr << "-y";

			if (simulated)	// for AMI use only
				networkStr << "--port 7182" << "--telnet 7191";
		}
		else if (axis == Z_AXIS)
		{
			axisStr << "-z";

			if (simulated)	// for AMI use only
				networkStr << "--port 7181" << "--telnet 7190";
		}

		// launch process with axis label with parser function at ipAddress
		if (minimized)
			args << axisStr << "-h" << "-p" << "-a " + ipAddress << networkStr;
		else
			args << axisStr << "-p" << "-a " + ipAddress << networkStr;

#if defined(Q_OS_WIN)
		process->setNativeArguments(args.join(' '));

		qDebug() << process->program() << process->nativeArguments();
#endif

		connect(process, SIGNAL(started()), this, SLOT(processStarted()));
		connect(process, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(processStateChanged(QProcess::ProcessState)));
		connect(process, SIGNAL(readyReadStandardOutput()), this, SLOT(readyReadStandardOutput()));

#if defined(Q_OS_WIN)
		process->start();
#else
        process->setProcessChannelMode(QProcess::SeparateChannels);
        exepath += " " + args.join(' ');
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        process->start(exepath, QIODevice::ReadWrite);
#else
        process->startCommand(exepath, QIODevice::ReadWrite);
#endif
#endif
	}
	else
	{
		qDebug() << "Error:" << exepath << "does not exist: Magnet-DAQ installed?";
	}
}

//---------------------------------------------------------------------------
void ProcessManager::processStateChanged(QProcess::ProcessState newState)
{
	if (newState == QProcess::NotRunning)
		started = false;
	else if (newState == QProcess::Running)
		started = true;
}

//---------------------------------------------------------------------------
void ProcessManager::processStarted(void)
{
	QString cmd("*IDN?\n");

#ifdef LOCAL_DEBUG
	qDebug() << "*IDN?";
#endif
	process->write(cmd.toLocal8Bit());
	process->waitForReadyRead(10000);

	if (reply.isEmpty())
	{
		started = false;
		process->close();
	}

	// parse *IDN? reply
	QStringList strList = reply.split(",");

	if (strList.count() >= 2)
	{
		bool ok;
		double version = strList.at(1).toDouble(&ok);

		if (!ok)
			version = 0.0;

		// should be MagnetDAQ version 1.05 or later
		if (!(strList.at(0) == "MagnetDAQ" && version >= 1.05))
		{
			started = false;
			process->close();

			qDebug() << "Error: Installed Magnet-DAQ version must be 1.05 or later";
			emit magnetDAQError();
		}
	}
	else
	{
		started = false;
		process->close();
	}

	reply.clear();
}

//---------------------------------------------------------------------------
void ProcessManager::readyReadStandardOutput(void)
{
	reply = process->readAllStandardOutput();

	// strip trailing terminators (terminate at first terminator pair)
	int index;

#if defined(Q_OS_LINUX) || defined(Q_OS_MAC)
	index = reply.indexOf("\n");
#else
	index = reply.indexOf("\r\n");
#endif

	if (index >= 0)
		reply.truncate(index);

#ifdef LOCAL_DEBUG
	qDebug() << reply;
#endif
}

//---------------------------------------------------------------------------
void ProcessManager::sendParams(AxesParams *params, FieldUnits units, bool testMode, bool useStabilizingResistors, bool disableAutoStabilty)
{
	QString cmd;

	// send field units
	cmd = "CONF:FIELD:UNITS " + QString::number((int)(units)) + "\n";
	process->write(cmd.toLocal8Bit());

	// force ramping timebase to seconds instead of minutes
	cmd = "CONF:RAMP:RATE:UNITS 0\n";
	process->write(cmd.toLocal8Bit());

	// send current limit
	cmd = "CONF:CURR:LIM " + QString::number(params->currentLimit, 'f', 4) + "\n";
	process->write(cmd.toLocal8Bit());

	// send voltage limit
	cmd = "CONF:VOLT:LIM " + QString::number(params->voltageLimit, 'f', 4) + "\n";
	process->write(cmd.toLocal8Bit());

	// set max ramp rate
	cmd = "CONF:RAMP:RATE:CURR 1," + QString::number(params->maxRampRate, 'f', 6) + "," + QString::number(params->currentLimit, 'f', 4) + "\n";
	process->write(cmd.toLocal8Bit());

	// send coil constant
	cmd = "CONF:COIL " + QString::number(params->coilConst, 'f', 6) + "\n";
	process->write(cmd.toLocal8Bit());

	// send inductance
	cmd = "CONF:IND " + QString::number(params->inductance, 'f', 6) + "\n";
	process->write(cmd.toLocal8Bit());

	if (!params->switchInstalled)
	{
		if (testMode)
		{
			// set TEST stability mode
			cmd = "CONF:STAB:MODE 2\n";
			process->write(cmd.toLocal8Bit());

			// since no switch, set Stability Resistor present to allow testing
			// at ramp rates other than the cooled-switch ramp rate
			//cmd = "CONF:STAB:RES 1\n";
			//process->write(cmd.toLocal8Bit());
		}
		else
		{
			// since no switch, set Stability Resistor according to preference
			if (useStabilizingResistors)
				cmd = "CONF:STAB:RES 1\n";
			else
				cmd = "CONF:STAB:RES 0\n";
			process->write(cmd.toLocal8Bit());

			if (disableAutoStabilty)
			{
				// set MANUAL stability mode
				cmd = "CONF:STAB:MODE 1\n";
				process->write(cmd.toLocal8Bit());
			}
			else
			{
				// set AUTO stability mode
				cmd = "CONF:STAB:MODE 0\n";
				process->write(cmd.toLocal8Bit());
			}
		}

		// no switch installed, send last to prevent false quench
		cmd = "CONF:PS 0\n";
		process->write(cmd.toLocal8Bit());

		// PAUSE unit
		process->write("PAUSE\n");
	}
	else
	{
		// switch installed
		cmd = "CONF:PS 1\n";
		process->write(cmd.toLocal8Bit());

		// send switch current
		cmd = "CONF:PS:CURR " + QString::number(params->switchHeaterCurrent, 'f', 1) + "\n";
		process->write(cmd.toLocal8Bit());

		// use timer-based transition
		cmd = "CONF:PS:TRAN 0\n";
		process->write(cmd.toLocal8Bit());

		// send heating time
		cmd = "CONF:PS:HTIME " + QString::number(params->switchHeatingTime) + "\n";
		process->write(cmd.toLocal8Bit());

		// send cooling time
		cmd = "CONF:PS:CTIME " + QString::number(params->switchCoolingTime) + "\n";
		process->write(cmd.toLocal8Bit());

		if (testMode)
		{
			// set TEST stability mode
			cmd = "CONF:STAB:MODE 2\n";
			process->write(cmd.toLocal8Bit());
		}
		else
		{
			if (disableAutoStabilty)
			{
				// set MANUAL stability mode
				cmd = "CONF:STAB:MODE 1\n";
				process->write(cmd.toLocal8Bit());
			}
			else
			{
				// set AUTO stability mode
				cmd = "CONF:STAB:MODE 0\n";
				process->write(cmd.toLocal8Bit());
			}
		}

		// PAUSE unit
		process->write("PAUSE\n");
	}
}

//---------------------------------------------------------------------------
double ProcessManager::getMagnetCurrent(bool *error)
{
	bool ok = false;
	double temp;

	{
		QString cmd("CURR:MAG?\n");
#ifdef LOCAL_DEBUG
		qDebug() << "CURR:MAG?";
#endif
		process->write(cmd.toLocal8Bit());
		process->waitForReadyRead(QUERY_TIMEOUT);

		if (reply.isEmpty())
		{
			*error = ok;
			return NAN;
		}

		// parse reply
		temp = reply.toDouble(&ok);
		*error = ok;
		reply.clear();
	}

	if (ok)
		return temp;
	else
		return NAN;
}

//---------------------------------------------------------------------------
double ProcessManager::getSupplyCurrent(bool *error)
{
	bool ok = false;
	double temp;

	{
		QString cmd("CURR:SUPP?\n");
#ifdef LOCAL_DEBUG
		qDebug() << "CURR:SUPP?";
#endif
		process->write(cmd.toLocal8Bit());
		process->waitForReadyRead(QUERY_TIMEOUT);

		if (reply.isEmpty())
		{
			*error = ok;
			return NAN;
		}

		// parse reply
		temp = reply.toDouble(&ok);
		*error = ok;
		reply.clear();
	}

	if (ok)
		return temp;
	else
		return NAN;
}

//---------------------------------------------------------------------------
double ProcessManager::getField(bool *error)
{
	bool ok = false;
	double temp;

	{
		QString cmd("FIELD:MAG?\n");
#ifdef LOCAL_DEBUG
		qDebug() << "FIELD:MAG?";
#endif
		process->write(cmd.toLocal8Bit());
		process->waitForReadyRead(QUERY_TIMEOUT);

		if (reply.isEmpty())
		{
			*error = ok;
			return NAN;
		}

		// parse reply
		temp = reply.toDouble(&ok);
		*error = ok;
		reply.clear();
	}

	if (ok)
		return temp;
	else
		return NAN;
}

//---------------------------------------------------------------------------
double ProcessManager::getQuenchCurrent(bool *error)
{
	bool ok = false;
	double temp;

	{
		QString cmd("QU:CURR?\n");
#ifdef LOCAL_DEBUG
		qDebug() << "QU:CURR?";
#endif
		process->write(cmd.toLocal8Bit());
		process->waitForReadyRead(QUERY_TIMEOUT);

		if (reply.isEmpty())
		{
			*error = ok;
			return NAN;
		}

		// parse reply
		temp = reply.toDouble(&ok);
		*error = ok;
		reply.clear();
	}

	if (ok)
		return temp;
	else
		return NAN;
}

//---------------------------------------------------------------------------
FieldUnits ProcessManager::getUnits(void)
{
	bool ok;
	int temp;

	{
#ifdef LOCAL_DEBUG
		qDebug() << "FIELD:UNITS?";
#endif
		process->write("FIELD:UNITS?\n");
		process->waitForReadyRead(QUERY_TIMEOUT);

		if (reply.isEmpty())
			return ERROR_UNITS;

		// parse reply
		temp = reply.toInt(&ok);
		reply.clear();
	}

	if (ok)
	{
		// units must be 0=KG or 1=TESLA
		if (temp == 0 || temp == 1)
			return (FieldUnits)temp;
		else
			return ERROR_UNITS;
	}
	else
		return ERROR_UNITS;
}

//---------------------------------------------------------------------------
State ProcessManager::getState(void)
{
	bool ok;
	int temp;

	{
#ifdef LOCAL_DEBUG
		qDebug() << "STATE?";
#endif
		process->write("STATE?\n");
		process->waitForReadyRead(QUERY_TIMEOUT);

		if (reply.isEmpty())
			return ERROR_STATE;

		// parse reply
		temp = reply.toInt(&ok);
		reply.clear();
	}

	if (ok)
		return (State)temp;
	else
		return ERROR_STATE;
}

//---------------------------------------------------------------------------
bool ProcessManager::getPSwitchHeaterState(void)
{
	bool ok;
	int temp;

	{
#ifdef LOCAL_DEBUG
		qDebug() << "PS?";
#endif
		process->write("PS?\n");
		process->waitForReadyRead(QUERY_TIMEOUT);

		if (reply.isEmpty())
			return false;

		// parse reply
		temp = reply.toInt(&ok);
		reply.clear();
	}

	if (ok)
		return (bool)temp;
	else
		return false;
}

//---------------------------------------------------------------------------
// Sends RAMP command
void ProcessManager::sendRamp(void)
{
	process->write("RAMP\n");
}

//---------------------------------------------------------------------------
// Sends PAUSE command
void ProcessManager::sendPause(void)
{
	process->write("PAUSE\n");
}

//---------------------------------------------------------------------------
// Sends ZERO command
void ProcessManager::sendRampToZero(void)
{
	process->write("ZERO\n");
}

//---------------------------------------------------------------------------
// Sends target in A given a value in current field units, if isFieldValue is
// true the value is converted by the present coilConst value
void ProcessManager::setTargetCurr(AxesParams *params, double value, bool isFieldValue)
{
	double target = 0;

	if (isFieldValue && (params->coilConst > 0))
	{
		target = value / params->coilConst;
		QString cmd("CONF:CURR:TARG " + QString::number(target, 'f', 10) + "\n");
		process->write(cmd.toLocal8Bit());
	}
	else
	{
		target = value;
		QString cmd("CONF:CURR:TARG " + QString::number(target, 'f', 10) + "\n");
		process->write(cmd.toLocal8Bit());
	}
}

//---------------------------------------------------------------------------
// Sends ramp rate in A/sec
void ProcessManager::setRampRateCurr(AxesParams *params, double rate)
{
	// force ramping timebase to seconds instead of minutes
	QString cmd("CONF:RAMP:RATE:UNITS 0\n");
	process->write(cmd.toLocal8Bit());

	// send down single segment ramp rate
	cmd = "CONF:RAMP:RATE:CURR 1," + QString::number(rate, 'f', 6) + "," + QString::number(params->currentLimit, 'f', 4) + "\n";
	process->write(cmd.toLocal8Bit());
}

//---------------------------------------------------------------------------
void ProcessManager::heatSwitch(void)
{
	// turn ON switch heater
	QString cmd("PS 1\n");
	process->write(cmd.toLocal8Bit());
}

//---------------------------------------------------------------------------
void ProcessManager::coolSwitch(void)
{
	// turn OFF switch heater
	QString cmd("PS 0\n");
	process->write(cmd.toLocal8Bit());
}

//---------------------------------------------------------------------------
