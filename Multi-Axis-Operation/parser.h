#ifndef PARSER_H
#define PARSER_H

#include <QObject>
#include <QStack>
#include "multiaxisoperation.h"

//---------------------------------------------------------------------------
// Type declarations
//---------------------------------------------------------------------------

enum SystemError
{
	ERR_NONE = 0,
	ERR_UNRECOGNIZED_COMMAND = -101,
	ERR_INVALID_ARGUMENT = -102,
	ERR_NON_BOOLEAN_ARGUMENT = -103,
	ERR_MISSING_PARAMETER = -104,
	ERR_OUT_OF_RANGE = -105,
	ERR_NON_NUMERICAL_ENTRY = -151,
	ERR_EXCEEDS_MAGNITUDE_LIMIT = -152,
	ERR_NEGATIVE_MAGNITUDE = -153,
	ERR_INCLINATION_OUT_OF_RANGE = -154,
	ERR_EXCEEDS_X_RANGE = -155,
	ERR_INACTIVE_X_AXIS = -156,
	ERR_EXCEEDS_Y_RANGE = -157,
	ERR_INACTIVE_Y_AXIS = -158,
	ERR_EXCEEDS_Z_RANGE = -159,
	ERR_INACTIVE_Z_AXIS = -160,

	ERR_UNRECOGNIZED_QUERY = -201,

	ERR_NOT_CONNECTED = -301,
	ERR_SWITCH_TRANSITION = -302,
	ERR_QUENCH_CONDITION = -303,
	ERR_NO_UNITS_CHANGE = -304,
	ERR_NO_PERSISTENCE = -305,
	ERR_IS_PERSISTENT = -306,
	ERR_NO_SWITCH = -307,
	ERR_CANNOT_LOAD = -308
};



//---------------------------------------------------------------------------
// Parser Class Header (for Python and remote scripting support)
//---------------------------------------------------------------------------
class Parser : public QObject
{
	Q_OBJECT

public:
	Parser(QObject *parent);
	~Parser();
	void setDataSource(MultiAxisOperation *src) { source = src; }
	void stop(void) { stopProcessing = true; }

public slots:
	void process(void);

signals:
	void finished();
	void error_msg(QString err);

	// action signals
	void system_connect(void);
	void system_disconnect(void);
	void ramp(void);
	void pause(void);
	void zero(void);
	void load_settings(FILE *file, bool *success);
	void save_settings(FILE *file, bool *success);
	void set_align1(double mag, double az, double inc);
	void set_align2(double mag, double az, double inc);
	void set_align1_active(void);
	void set_align2_active(void);
	void set_units(int value);
	void set_vector(double mag, double az, double inc, int time);
	void set_vector_cartesian(double x, double y, double z, int time);
	void goto_vector(int tableRow);
	void set_polar(double mag, double angle, int time);
	void goto_polar(int tableRow);
	void set_persistence(bool persistent);
	void exit_app(void);

private:
	// add your variables here
	bool stopProcessing;
	MultiAxisOperation *source;
	QString inputStr;
	QStack<QString> errorStack;

	void addToErrorQueue(SystemError error);
	void parseInput(char *commbuf, char* outputBuffer);
	void parse_query_A(char* word, char* outputBuffer);
	void parse_query_F(char* word, char* outputBuffer);
	void parse_query_T(char* word, char* outputBuffer);
	void parse_configure(char* word, char *outputBuffer);
	void parse_configure_A(char* word, char *outputBuffer);
	void parse_configure_U(char* word, char *outputBuffer);
	void parse_configure_T(char* word, char *outputBuffer);
};

#endif // PARSER_H