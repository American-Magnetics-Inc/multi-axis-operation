#include "stdafx.h"
#include "multiaxisoperation.h"
#include <QtWidgets/QApplication>
#include <QtDebug>
#include <QFile>
#include <QTextStream>

#ifndef DEBUG	// if not debugging, direct output to MultiAxisOperation.log in ~/AppData/Local/Temp for the user

//---------------------------------------------------------------------------
void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
	QString txt;
	QString timestamp = QDateTime::currentDateTime().toString("dd.MMM.yy hh.mm.ss") + ": ";
	QFile outFile(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/MultiAxisOperation.log");
	outFile.open(QIODevice::WriteOnly | QIODevice::Append);
	QTextStream ts(&outFile);

	switch (type)
	{
		case QtDebugMsg:
			txt = QString("Debug: %1 (%2:%3, %4)").arg(msg, QString(context.file), QString::number(context.line), QString(context.function));
			ts << timestamp << txt << endl;
			break;
		case QtWarningMsg:
			txt = QString("Warning: %1 (%2:%3, %4)").arg(msg, QString(context.file), QString::number(context.line), QString(context.function));
			ts << timestamp << txt << endl;
			break;
		case QtCriticalMsg:
			txt = QString("Critical: %1 (%2:%3, %4)").arg(msg, QString(context.file), QString::number(context.line), QString(context.function));
			ts << timestamp << txt << endl;
			break;
		case QtFatalMsg:
			txt = QString("Fatal: %1 (%2:%3, %4)").arg(msg, QString(context.file), QString::number(context.line), QString(context.function));
			ts << timestamp << txt << endl;
			abort();
	}
}

//---------------------------------------------------------------------------
class MyApplication : public QApplication
{
public:
	MyApplication(int & argc, char ** argv) : QApplication(argc, argv) {}
	virtual ~MyApplication() {}

	// reimplemented from QApplication so we can throw exceptions in slots
	virtual bool notify(QObject * receiver, QEvent * event)
	{
		try
		{
			return QApplication::notify(receiver, event);
		}
		catch (std::exception& e)
		{
			qDebug() << "Exception thrown:" << e.what();
		}
		return false;
	}
};

#endif

#if defined(Q_OS_WIN) && defined(DEBUG)

// This code creates a stdin/stdout console in Windows when the app is launched.
// Very useful for debugging the parser interface!
// Be sure to set the entry point in the Linker -> Advanced properties to "MyEntryPoint".

extern "C"
{
	int WinMainCRTStartup(void); // Use the name you obtained in the prior step
	int MyEntryPoint(void);
}

__declspec(noinline) int MyEntryPoint(void)
{
	// __debugbreak(); // Break into the program right at the entry point!
	// Create a new console:
	AllocConsole();
	freopen("CON", "w", stdout);
	freopen("CON", "w", stderr);
	freopen("CON", "r", stdin); // Note: "r", not "w".
	return WinMainCRTStartup();
}

#endif

int main(int argc, char *argv[])
{
	QCoreApplication::setOrganizationName("American Magnetics Inc.");
	QCoreApplication::setApplicationName("MultiAxisOperation");
	QCoreApplication::setApplicationVersion("0.93");
	QCoreApplication::setOrganizationDomain("AmericanMagnetics.com");
	QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	//qputenv("QT_SCALE_FACTOR", "1.0");

#ifndef DEBUG
	MyApplication a(argc, argv);
	qInstallMessageHandler(customMessageHandler);
#else
	QApplication a(argc, argv);
#endif

	MultiAxisOperation w;
	w.show();
	return a.exec();
}
