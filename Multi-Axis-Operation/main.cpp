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
	QFile outFile(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/MultiAxisOperation.log");
	outFile.open(QIODevice::WriteOnly | QIODevice::Append);
	QTextStream ts(&outFile);

	switch (type)
	{
		case QtDebugMsg:
			txt = QString("Debug: %1 (%2:%3, %4)").arg(msg, QString(context.file), QString::number(context.line), QString(context.function));
			ts << txt << endl;
			break;
		case QtWarningMsg:
			txt = QString("Warning: %1 (%2:%3, %4)").arg(msg, QString(context.file), QString::number(context.line), QString(context.function));
			ts << txt << endl;
			break;
		case QtCriticalMsg:
			txt = QString("Critical: %1 (%2:%3, %4)").arg(msg, QString(context.file), QString::number(context.line), QString(context.function));
			ts << txt << endl;
			break;
		case QtFatalMsg:
			txt = QString("Fatal: %1 (%2:%3, %4)").arg(msg, QString(context.file), QString::number(context.line), QString(context.function));
			ts << txt << endl;
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

int main(int argc, char *argv[])
{
	QCoreApplication::setOrganizationName("American Magnetics Inc.");
	QCoreApplication::setApplicationName("MultiAxisOperation");
	QCoreApplication::setApplicationVersion("0.60");
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
