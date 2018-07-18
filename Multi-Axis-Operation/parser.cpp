#include "stdafx.h"
#include "parser.h"
#include <iostream>
#include "conversions.h"

#define DELIMITER	":"		// colon
#define SEPARATOR ": \t"	// colon, space, or tab
#define SPACE " \t"			// space or tab
#define COMMA ","			// comma
#define EOL "\r\n"			// end of line

/************************************************************
	This file is designed to support using this app as a 
	slave QProcess to another application. It exposes a 
	set of remote interface commands and queries. This allows
	a master application to use this app for higher-level,
	experiment-specific sequences.

	The Parser::process() method executes in a separate thread
	from the user interface. I/O uses stdin and stdout.

	To enable the parser function, use the command line
	argument "-p" on app launch.

	Please note that the QProcess functionality is not available
	for UWP (Universal Windows) apps as the sandboxing does not 
	allow this type of interprocess communication.
************************************************************/

//---------------------------------------------------------------------------
// Command and query parsing tokens
//---------------------------------------------------------------------------
const char _SYST[] = "SYST";
const char _SYSTEM[] = "SYSTEM";
const char _ERR[] = "ERR";
const char _ERROR[] = "ERROR";
const char _COUN[] = "COUN";
const char _COUNT[] = "COUNT";
const char _CONN[] = "CONN";
const char _CONNECT[] = "CONNECT";
const char _DISC[] = "DISC";
const char _DISCONNECT[] = "DISCONNECT";
const char _CONFIGURE[] = "CONFIGURE";
const char _CONF[] = "CONF";
const char _IDN[] = "*IDN";
const char _CLS[] = "*CLS";
const char _PAUSE[] = "PAUSE";
const char _RAMP[] = "RAMP";
const char _UNITS[] = "UNITS";
const char _FIELD[] = "FIELD";
const char _STATE[] = "STATE";
const char _ZERO[] = "ZERO";
const char _LOAD[] = "LOAD";
const char _SAVE[] = "SAVE";
const char _SET[] = "SET";
const char _SETTINGS[] = "SETTINGS";
const char _ALIGN1[] = "ALIGN1";
const char _ALIGN2[] = "ALIGN2";
const char _CART[] = "CART";
const char _CARTESIAN[] = "CARTESIAN";
const char _TARG[] = "TARG";
const char _TARGET[] = "TARGET";
const char _PLANE[] = "PLANE";
const char _VEC[] = "VEC";
const char _VECTOR[] = "VECTOR";
const char _TAB[] = "TAB";
const char _TABLE[] = "TABLE";
const char _POL[] = "POL";
const char _POLAR[] = "POLAR";
const char _TIME[] = "TIME";
const char _PERS[] = "PERS";
const char _PERSISTENT[] = "PERSISTENT";


//---------------------------------------------------------------------------
// Incoming data type tests and conversions
//---------------------------------------------------------------------------
char *struprt(char *str)
{
	if (str == NULL)
		return NULL;

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
	char *next = str;
	while (*str != '\0')
		*str = toupper((unsigned char)*str);
#else
	str = _strupr(str);	// convert to all uppercase
#endif
	return str;
}

//---------------------------------------------------------------------------
char *trimwhitespace(char *str)
{
	char *end;

	// Trim leading space
	while (isspace((unsigned char)*str)) str++;

	if (*str == 0)  // All spaces?
		return str;

	// Trim trailing space
	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char)*end)) end--;

	// Write new null terminator character
	end[1] = '\0';

	return str;
}

//---------------------------------------------------------------------------
bool isValue(const char* buf)
{
	short digits = 0;   // digits
	short sign = 0;     // optional sign
	short dec = 0;      // optional mantissa decimal
	short exp = 0;      // optional exponent indicator 'E'

	// must check for this first!
	if (buf == NULL)
		return false;

	// skip all leading whitespace
	// no whitespace can occur within rest of string
	// string is null terminated
	while (isspace(*buf))
		buf++;	// next character, NULL not considered whitespace

	// search for mantissa sign and digits
	while (*buf != '\0' && !exp)
	{
		if (*buf == '+' || *buf == '-')
		{
			sign++;

			if (sign > 1)	// more than one sign or it followed a digit/decimal
				return false;

			buf++;			// next character
		}
		else if (isdigit(*buf))
		{
			digits++;
			sign++;			// sign can't follow digit
			buf++;			// next character
		}
		else if (*buf == '.')
		{
			dec++;
			sign++;			// sign can't follow decimal

			if (dec > 1)	// more than one decimal
				return false;

			buf++;			// next character
		}
		else if (*buf == 'E' || *buf == 'e')
		{	// found exponent
			if (!digits)	// need at least one mantissa digit before an exponent
				return false;

			exp++;
			buf++;
		}
		else
		{		
			// unacceptable character or symbol
			return false;
		}
	}	// end while()

	if (!digits)		// must have at least one mantissa digit
		return false;

	// possibly in exponent portion, search for exponent sign and digits
	sign = 0;
	digits = 0;
	while (*buf != '\0' && exp)
	{
		if (*buf == '+' || *buf == '-')
		{
			sign++;
			if (sign > 1)	// more than one sign or it followed a digit/decimal
				return false;
			buf++;			// next character
		}
		else if (isdigit(*buf))
		{
			digits++;
			sign++;			// sign can't follow digit
			buf++;			// next character
		}
		else
		{		
			// unacceptable character or symbol
			return false;
		}
	}	// end while()

	if (exp && !digits)	// if there's an exponent, need at least one digit
		return false;

	return true;	// all tests satisfied, valid number
}


//---------------------------------------------------------------------------
// Class methods
//---------------------------------------------------------------------------
Parser::Parser(QObject *parent)
	: QObject(parent)
{
	stopProcessing = false;
	source = NULL;
}

//---------------------------------------------------------------------------
Parser::~Parser()
{
	stopProcessing = true;
	qDebug("Multi-Axis Operation stdin Parser End");
}


//---------------------------------------------------------------------------
// --- PROCESS THREAD ---
// Start processing data.
void Parser::process(void)
{
	if (source == NULL)
	{
		qDebug("Multi-Axis Operation parser aborted; no data source specified");
	}
	else
	{
		stopProcessing = false;

		// allocate resources and start parsing
		qDebug("Multi-Axis Operation stdin Parser Start");
		char input[1024];
		char output[1024];

		while (!stopProcessing)
		{
			input[0] = '\0';
			output[0] = '\0';

			std::cin.getline(input, sizeof(input));

			// save original string
			inputStr = QString(input);

			// parse stdin
			parseInput(input, output);
		}
	}

	emit finished();
}

//---------------------------------------------------------------------------
void Parser::addToErrorQueue(SystemError error)
{
	QString errMsg;

	switch (error)
	{
	case ERR_UNRECOGNIZED_COMMAND:
		errMsg = QString::number((int)ERR_UNRECOGNIZED_COMMAND) + ",\"Unrecognized command\"";
		break;

	case ERR_INVALID_ARGUMENT:
		errMsg = QString::number((int)ERR_INVALID_ARGUMENT) + ",\"Invalid argument\"";
		break;

	case ERR_NON_BOOLEAN_ARGUMENT:
		errMsg = QString::number((int)ERR_NON_BOOLEAN_ARGUMENT) + ",\"Non-boolean argument\"";
		break;

	case ERR_MISSING_PARAMETER:
		errMsg = QString::number((int)ERR_MISSING_PARAMETER) + ",\"Missing parameter\"";
		break;

	case ERR_OUT_OF_RANGE:
		errMsg = QString::number((int)ERR_OUT_OF_RANGE) + ",\"Value out of range\"";
		break;

	case ERR_NON_NUMERICAL_ENTRY:
		errMsg = QString::number((int)ERR_NON_NUMERICAL_ENTRY) + ",\"Non-numerical entry\"";
		break;

	case ERR_EXCEEDS_MAGNITUDE_LIMIT:
		errMsg = QString::number((int)ERR_EXCEEDS_MAGNITUDE_LIMIT) + ",\"Magnitude exceeds limit\"";
		break;

	case ERR_NEGATIVE_MAGNITUDE:
		errMsg = QString::number((int)ERR_NEGATIVE_MAGNITUDE) + ",\"Negative magnitude\"";
		break;

	case ERR_INCLINATION_OUT_OF_RANGE:
		errMsg = QString::number((int)ERR_INCLINATION_OUT_OF_RANGE) + ",\"Inclination out of range\"";
		break;

	case ERR_EXCEEDS_X_RANGE:
		errMsg = QString::number((int)ERR_EXCEEDS_X_RANGE) + ",\"Field exceeds x-coil limit\"";
		break;

	case ERR_INACTIVE_X_AXIS:
		errMsg = QString::number((int)ERR_INACTIVE_X_AXIS) + ",\"Field requires x-coil\"";
		break;

	case ERR_EXCEEDS_Y_RANGE:
		errMsg = QString::number((int)ERR_EXCEEDS_Y_RANGE) + ",\"Field exceeds y-coil limit\"";
		break;

	case ERR_INACTIVE_Y_AXIS:
		errMsg = QString::number((int)ERR_INACTIVE_Y_AXIS) + ",\"Field requires y-coil\"";
		break;

	case ERR_EXCEEDS_Z_RANGE:
		errMsg = QString::number((int)ERR_EXCEEDS_Z_RANGE) + ",\"Field exceeds z-coil limit\"";
		break;

	case ERR_INACTIVE_Z_AXIS:
		errMsg = QString::number((int)ERR_INACTIVE_Z_AXIS) + ",\"Field requires z-coil\"";
		break;

	case ERR_UNRECOGNIZED_QUERY:
		errMsg = QString::number((int)ERR_UNRECOGNIZED_QUERY) + ",\"Unrecognized query\"";
		break;

	case ERR_NOT_CONNECTED:
		errMsg = QString::number((int)ERR_NOT_CONNECTED) + ",\"Not connected\"";
		break;

	case ERR_SWITCH_TRANSITION:
		errMsg = QString::number((int)ERR_SWITCH_TRANSITION) + ",\"Switch in transition\"";
		break;

	case ERR_QUENCH_CONDITION:
		errMsg = QString::number((int)ERR_QUENCH_CONDITION) + ",\"Quench condition\"";
		break;

	case ERR_NO_UNITS_CHANGE:
		errMsg = QString::number((int)ERR_NO_UNITS_CHANGE) + ",\"No units change while connected\"";
		break;

	case ERR_NO_PERSISTENCE:
		errMsg = QString::number((int)ERR_NO_PERSISTENCE) + ",\"Cannot enter persistence\"";
		break;

	case ERR_IS_PERSISTENT:
		errMsg = QString::number((int)ERR_IS_PERSISTENT) + ",\"System is persistent\"";
		break;

	case ERR_NO_SWITCH:
		errMsg = QString::number((int)ERR_NO_SWITCH) + ",\"No switch installed\"";
		break;

	case ERR_CANNOT_LOAD:
		errMsg = QString::number((int)ERR_CANNOT_LOAD) + ",\"Cannot LOAD while connected\"";
		break;

	default:
		errMsg = QString::number((int)error) + ",\"Error\"";
		break;
	}

	if (!errMsg.isEmpty())
	{
		errorStack.push(errMsg);
		emit error_msg(errMsg);

		Beep(1000, 600);
	}
}

//---------------------------------------------------------------------------
void Parser::parseInput(char *commbuf, char* outputBuffer)
{
	char* word;    // string tokens
	char* value;   // start of argument
	char* pos;

	pos = strchr(commbuf, '?');

	// Quench condition is active, refuse all remote scripting
	if (SYSTEM_QUENCH == (SystemState)source->get_state())
	{
		addToErrorQueue(ERR_QUENCH_CONDITION);
		return;
	}

	/************************************************************
	The command is a query.
	************************************************************/
	if (pos != NULL)
	{		// found a ?
			// terminate string at the question mark
		*pos = '\0';

		// no case-sensitive issues with queries, convert all to uppercase
		commbuf = struprt(commbuf);

		// get first token
		word = strtok(commbuf, DELIMITER);

		if (word == NULL)
		{
			addToErrorQueue(ERR_UNRECOGNIZED_QUERY);
			return; // no match
		}

		// switch on first character
		switch (*word)
		{
			// tests: *IDN?,
			case  '*':
				if (!strcmp(word, _IDN))
				{
					QString version(qApp->applicationName() + "," + qApp->applicationVersion() + "\n");
					std::cout.write(version.toLocal8Bit(), version.size());
				}
				else
				{
					addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
				}
				break;

			// A* queries
			case  'A':
				parse_query_A(word, outputBuffer);
				break;

			// F* queries
			case  'F':
				parse_query_F(word, outputBuffer);
				break;

			// P* queries
			case  'P':
				if (!strcmp(word, _PLANE))
				{
					double x, y, z;

					source->get_plane(&x, &y, &z);
					sprintf(outputBuffer, "%0.10g,%0.10g,%0.10g\n", x, y, z);
					std::cout.write(outputBuffer, strlen(outputBuffer));
				}
				else if (strcmp(word, _PERS) == 0 || strcmp(word, _PERSISTENT) == 0)
				{
					sprintf(outputBuffer, "%d\n", source->isPersistent());
					std::cout.write(outputBuffer, strlen(outputBuffer));
				}
				else
				{
					addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
				}
				break;

			// S* queries
			case  'S':
				if (!strcmp(word, _STATE))				
				{
					sprintf(outputBuffer, "%d\n", source->get_state());
					std::cout.write(outputBuffer, strlen(outputBuffer));
				}
				else if (strcmp(word, _SYST) == 0 || strcmp(word, _SYSTEM) == 0)
				{
					// get next token
					word = strtok(NULL, SEPARATOR);

					if (word == NULL)
					{
						addToErrorQueue(ERR_UNRECOGNIZED_QUERY); // no match, error
					}
					else if (strcmp(word, _ERR) == 0 || strcmp(word, _ERROR) == 0)
					{
						// get next token
						word = strtok(NULL, SEPARATOR);

						if (word == NULL)
						{
							if (errorStack.count())
								sprintf(outputBuffer, "%s\n", errorStack.pop().toLocal8Bit().constData());
							else
								sprintf(outputBuffer, "0,\"No error\"\n");

							std::cout.write(outputBuffer, strlen(outputBuffer));
						}
						else if (strcmp(word, _COUN) == 0 || strcmp(word, _COUNT) == 0)
						{
							// get next token
							word = strtok(NULL, SEPARATOR);

							if (word == NULL)
							{
								sprintf(outputBuffer, "%d\n", errorStack.count());
								std::cout.write(outputBuffer, strlen(outputBuffer));
							}
						}
						else
						{
							addToErrorQueue(ERR_UNRECOGNIZED_QUERY); // no match, error
						}
					}
					else
					{
						addToErrorQueue(ERR_UNRECOGNIZED_QUERY); //no match, error
					}
				}
				else
				{
					addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
				}
				break;

			// T* queries
			case 'T':
				parse_query_T(word, outputBuffer);
				break;

			// U* queries
			case 'U':
				if (!strcmp(word, _UNITS))
				{
					sprintf(outputBuffer, "%d\n", source->get_field_units());
					std::cout.write(outputBuffer, strlen(outputBuffer));
				}
				else
				{
					addToErrorQueue(ERR_UNRECOGNIZED_QUERY);	// no match, error
				}
				break;

			// no match
			default:
				addToErrorQueue(ERR_UNRECOGNIZED_QUERY);
				break;
		}	// end switch on first word
	}
			
	/************************************************************
	The command is not a query (no return data).
	************************************************************/
	else
	{
		// get first token
		word = strtok(commbuf, SEPARATOR);
		if (word == NULL)
		{
			return; // no match
		}

		word = struprt(word);	// convert token to uppercase

		// switch on first character
		switch (*word)
		{
			// tests *CLS
			case '*':
				if (!strcmp(word, _CLS))
				{
					errorStack.clear();
				}
				else
				{
					addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
				}
				break;

			// tests all CONFigure commands
			case  'C':
				if (strcmp(word, _CONF) == 0 || strcmp(word, _CONFIGURE) == 0)
				{
					// get next token
					word = strtok(NULL, SEPARATOR);
					word = struprt(word);	// convert token to uppercase

					if (word == NULL)
					{
						addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);
					}
					else
					{
						parse_configure(word, outputBuffer);
					}
				}
				else
				{
					addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
				}
				break;

			// tests LOAD:SETtings
			case 'L':
				if (strcmp(word, _LOAD) == 0)
				{
					word = strtok(NULL, SEPARATOR); // get next token
					word = struprt(word);	// convert token to uppercase

					if (word == NULL)
					{
						// no argument for LOAD, command error
						addToErrorQueue(ERR_MISSING_PARAMETER);
					}

					// LOAD:SETtings
					else if (strcmp(word, _SET) == 0 || strcmp(word, _SETTINGS) == 0)
					{
						word = strtok(NULL, EOL); // get next token
						trimwhitespace(word);

						// case sensitive filenames on Unix systems!
						if (word == NULL)
						{
							// no filename for LOAD:SETtings, command error
							addToErrorQueue(ERR_MISSING_PARAMETER);
						}
						else
						{
							if (source->isConnected())
							{
								addToErrorQueue(ERR_CANNOT_LOAD);
							}
							else
							{
								FILE *file = fopen(word, "r");

								if (file == NULL)
								{
									// error in filename
									addToErrorQueue(ERR_INVALID_ARGUMENT);
								}
								else
								{
									bool success;
									emit load_settings(file, &success);
								}
							}
						}
					}
					else
					{
						addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
					}
				}
				else
				{
					addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
				}
				break;

			// tests RAMP
			case  'R':
				if (strcmp(word, _RAMP) == 0)
				{
					// get next token
					word = strtok(NULL, SEPARATOR);
					word = struprt(word);	// convert token to uppercase

					// Check for the NULL condition here to make sure there are not additional args
					if (word == NULL)
					{
						if (source->isConnected())
						{
							SystemState state = (SystemState)source->get_state();
							if (state == SYSTEM_HEATING || state == SYSTEM_COOLING)
							{
								addToErrorQueue(ERR_SWITCH_TRANSITION);
							}
							else
							{
								if (source->isPersistent())
								{
									addToErrorQueue(ERR_IS_PERSISTENT);
								}
								else
								{
									emit ramp();
								}
							}
						}
						else
						{
							addToErrorQueue(ERR_NOT_CONNECTED); // not connected, error
						}
					}
					else
					{
						addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
					}
				}
				else
				{
					addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
				}
				break;

			// tests PERSistent 0|1,PAUSE
			case  'P':
				if (strcmp(word, _PERS) == 0 || strcmp(word, _PERSISTENT) == 0)
				{
					value = strtok(NULL, SPACE);		// look for value

					if (isValue(value))
					{
						while (isspace(*value))
							value++;
						
						if (*value == '1' || *value == '0')
						{
							if (source->isConnected())
							{
								if (*value == '1')
								{
									SystemState state = (SystemState)source->get_state();
									if (state == SYSTEM_HOLDING || state == SYSTEM_PAUSED)
									{
										if (source->hasSwitch())
										{
											emit set_persistence(true);
										}
										else
										{
											addToErrorQueue(ERR_NO_SWITCH);
										}
									}
									else
									{
										addToErrorQueue(ERR_NO_PERSISTENCE); // cannot enter persistent mode, error
									}
								}
								else if (*value == '0')
								{
									if (source->hasSwitch())
									{
										emit set_persistence(false);
									}
									else
									{
										addToErrorQueue(ERR_NO_SWITCH);
									}
								}
							}
							else
							{
								addToErrorQueue(ERR_NOT_CONNECTED); // not connected, error
							}
						}
						else
						{
							addToErrorQueue(ERR_NON_BOOLEAN_ARGUMENT); // invalid argument, error
						}
					}
					else
					{
						if (value == NULL)
							addToErrorQueue(ERR_MISSING_PARAMETER); // missing argument, error
						else
							addToErrorQueue(ERR_INVALID_ARGUMENT); // invalid argument, error
					}
				}
				else if (strcmp(word, _PAUSE) == 0)
				{
					// get next token
					word = strtok(NULL, SEPARATOR);
					word = struprt(word);	// convert token to uppercase
					
					// Check for the NULL condition here to make sure there are not additional args
					if (word == NULL)
					{
						if (source->isConnected())
						{
							SystemState state = (SystemState)source->get_state();
							if (state == SYSTEM_HEATING || state == SYSTEM_COOLING)
							{
								addToErrorQueue(ERR_SWITCH_TRANSITION);
							}
							else
							{
								if (source->isPersistent())
								{
									addToErrorQueue(ERR_IS_PERSISTENT);
								}
								else
								{
									emit pause();
								}
							}
						}
						else
						{
							addToErrorQueue(ERR_NOT_CONNECTED); // not connected, error
						}
					}
					else
					{
						addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
					}
				}
				else
				{
					addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
				}
				break;

			// tests SYSTem:CONNect,SYSTem:DISConnect,SAVE:SETtings
			case 'S':
				if (strcmp(word, _SYST) == 0 || strcmp(word, _SYSTEM) == 0)
				{
					word = strtok(NULL, SEPARATOR); // get next token
					word = struprt(word);	// convert token to uppercase

					if (word == NULL)
					{
						addToErrorQueue(ERR_UNRECOGNIZED_COMMAND); // no argument for SYSTem, command error
					}

					// SYSTem:CONNect
					else if (strcmp(word, _CONN) == 0 || strcmp(word, _CONNECT) == 0)
					{
						emit system_connect();
					}

					// SYSTem:DISConnect
					else if (strcmp(word, _DISC) == 0 || strcmp(word, _DISCONNECT) == 0)
					{
						emit system_disconnect();
					}
					else
					{
						addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
					}
				}
				else if (strcmp(word, _SAVE))
				{
					word = strtok(NULL, SEPARATOR); // get next token
					word = struprt(word);	// convert token to uppercase

					if (word == NULL)
					{
						// no argument for SAVE, command error
						addToErrorQueue(ERR_MISSING_PARAMETER);
					}

					else if (strcmp(word, _SET) == 0 || strcmp(word, _SETTINGS) == 0)
					{
						word = strtok(NULL, EOL); // get next token
						trimwhitespace(word);

						// case sensitive filenames on Unix systems!
						if (word == NULL)
						{
							// no filename for SAVE:SETtings, command error
							addToErrorQueue(ERR_MISSING_PARAMETER);
						}
						else
						{
							FILE *file = fopen(word, "w");

							if (file == NULL)
							{
								// error in filename
								addToErrorQueue(ERR_INVALID_ARGUMENT);
							}
							else
							{
								bool success;
								emit save_settings(file, &success);
							}
						}
					}
					else
					{
						addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
					}
				}
				else
				{
					addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
				}
				break;

			// tests ZERO
			case  'Z':
				if (strcmp(word, _ZERO) == 0)
				{
					// get next token
					word = strtok(NULL, SEPARATOR);
					word = struprt(word);	// convert token to uppercase

					// Check for the NULL condition here to make sure there are not additional args
					if (word == NULL)
					{
						if (source->isConnected())
						{
							SystemState state = (SystemState)source->get_state();
							if (state == SYSTEM_HEATING || state == SYSTEM_COOLING)
							{
								addToErrorQueue(ERR_SWITCH_TRANSITION);
							}
							else
							{
								if (source->isPersistent())
								{
									addToErrorQueue(ERR_IS_PERSISTENT);
								}
								else
								{
									emit zero();
								}
							}
						}
						else
						{
							addToErrorQueue(ERR_NOT_CONNECTED); // not connected, error
						}
					}
					else
					{
						addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
					}
				}
				else
				{
					addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
				}
				break;

			default:
				addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);
				break;
		}
	}
}

//---------------------------------------------------------------------------
// tests  ALIGN1?, ALIGN1:CARTesian?, ALIGN2?, ALIGN2:CARTesian?
//---------------------------------------------------------------------------
void Parser::parse_query_A(char* word, char* outputBuffer)
{
	if (strcmp(word, _ALIGN1) == 0)
	{
		word = strtok(NULL, DELIMITER);		// get next token

		if (word == NULL)
		{
			 // return spherical coordinates
			double mag, az, inc;

			source->get_align1(&mag, &az, &inc);
			sprintf(outputBuffer, "%0.10g,%0.10g,%0.10g\n", mag, az, inc);
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}

		// ALIGN1:CARTesian?
		else if (strcmp(word, _CART) == 0 || strcmp(word, _CARTESIAN) == 0)
		{
			double x, y, z;

			source->get_align1_cartesian(&x, &y, &z);
			sprintf(outputBuffer, "%0.10g,%0.10g,%0.10g\n", x, y, z);
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
		else
		{
			addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
		}
	}
	else if (strcmp(word, _ALIGN2) == 0)
	{
		word = strtok(NULL, DELIMITER);		// get next token

		if (word == NULL)
		{
			// return spherical coordinates
			double mag, az, inc;

			source->get_align2(&mag, &az, &inc);
			sprintf(outputBuffer, "%0.10g,%0.10g,%0.10g\n", mag, az, inc);
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}

		// ALIGN2:CARTesian?
		else if (strcmp(word, _CART) == 0 || strcmp(word, _CARTESIAN) == 0)
		{
			double x, y, z;

			source->get_align2_cartesian(&x, &y, &z);
			sprintf(outputBuffer, "%0.10g,%0.10g,%0.10g\n", x, y, z);
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
		else
		{
			addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
		}
	}
	else
	{
		addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match
	}
}

//---------------------------------------------------------------------------
// tests FIELD?, FIELD:CARTesian?
//---------------------------------------------------------------------------
void Parser::parse_query_F(char* word, char* outputBuffer)
{
	if (strcmp(word, _FIELD) == 0)
	{
		word = strtok(NULL, DELIMITER);		// get next token

		if (word == NULL)
		{
			if (source->isConnected())
			{
				// return spherical coordinates
				double mag, az, inc;

				source->get_field(&mag, &az, &inc);
				sprintf(outputBuffer, "%0.10g,%0.10g,%0.10g\n", mag, az, inc);
				std::cout.write(outputBuffer, strlen(outputBuffer));
			}
			else
			{
				addToErrorQueue(ERR_NOT_CONNECTED); // not connected, error
			}
		}

		// FIELD:CARTesian?
		else if (strcmp(word, _CART) == 0 || strcmp(word, _CARTESIAN) == 0)
		{
			if (source->isConnected())
			{
				double x, y, z;

				source->get_field_cartesian(&x, &y, &z);
				sprintf(outputBuffer, "%0.10g,%0.10g,%0.10g\n", x, y, z);
				std::cout.write(outputBuffer, strlen(outputBuffer));
			}
			else
			{
				addToErrorQueue(ERR_NOT_CONNECTED); // not connected, error
			}
		}

		else
		{
			addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
		}
	}
	else
	{
		addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
	}
}

//---------------------------------------------------------------------------
// tests TARGet?, TARGet:CARTesian?
//---------------------------------------------------------------------------
void Parser::parse_query_T(char* word, char* outputBuffer)
{
	if (strcmp(word, _TARG) == 0 || strcmp(word, _TARGET) == 0)
	{
		word = strtok(NULL, DELIMITER);		// get next token

		if (word == NULL)
		{
			if (source->isConnected())
			{
				// return spherical coordinates
				double mag, az, inc;

				source->get_active(&mag, &az, &inc);
				sprintf(outputBuffer, "%0.10g,%0.10g,%0.10g\n", mag, az, inc);
				std::cout.write(outputBuffer, strlen(outputBuffer));
			}
			else
			{
				addToErrorQueue(ERR_NOT_CONNECTED);
			}
		}

		// TARGet:CARTesian?
		else if (strcmp(word, _CART) == 0 || strcmp(word, _CARTESIAN) == 0)
		{
			if (source->isConnected())
			{
				double x, y, z;

				source->get_active_cartesian(&x, &y, &z);
				sprintf(outputBuffer, "%0.10g,%0.10g,%0.10g\n", x, y, z);
				std::cout.write(outputBuffer, strlen(outputBuffer));
			}
			else
			{
				addToErrorQueue(ERR_NOT_CONNECTED);
			}
		}
		else if (strcmp(word, _TIME) == 0)
		{
			sprintf(outputBuffer, "%d\n", source->get_remaining_time());
			std::cout.write(outputBuffer, strlen(outputBuffer));
		}
		else
		{
			addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
		}
	}
	else
	{
		addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
	}
}

//---------------------------------------------------------------------------
//	 Parses all CONFigure commands. These are broken out of
//   parseInput() for easier reading.
//---------------------------------------------------------------------------
void Parser::parse_configure(char* word, char *outputBuffer)
{
	if (word == NULL)
	{
		addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);
	}
	else
	{
		switch (*word)
		{
			// CONFigure:A* commands
			case  'A':
				parse_configure_A(word, outputBuffer);
				break;

			// CONFigure:U* commands
			case  'U':
				parse_configure_U(word, outputBuffer);
				break;

			// CONFigure:T* commands
			case  'T':
				parse_configure_T(word, outputBuffer);
				break;

			default:
				addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);	// no match, error
				break;
		}	// end switch(*word) for CONF
	}
}

//---------------------------------------------------------------------------
// tests CONFigure:UNITS
//---------------------------------------------------------------------------
void Parser::parse_configure_U(char* word, char *outputBuffer)
{
	// CONFigure:UNITS
	if (strcmp(word, _UNITS) == 0)
	{
		if (source->isConnected())
		{
			addToErrorQueue(ERR_NO_UNITS_CHANGE); // cannot change units after connect, error
		}
		else
		{
			char *value = strtok(NULL, SPACE);	// look for value

			if (isValue(value))
			{
				while (isspace(*value))
					value++;

				if (*value == '1')
				{
					emit set_units(1);
				}
				else if (*value == '0')
				{
					emit set_units(0);
				}
				else
				{
					addToErrorQueue(ERR_INVALID_ARGUMENT); // invalid argument, error
				}
			}
			else
			{
				if (value == NULL)
					addToErrorQueue(ERR_MISSING_PARAMETER); // missing argument, error
				else
					addToErrorQueue(ERR_INVALID_ARGUMENT); // invalid argument, error
			}
		}
	}
	else
	{
		addToErrorQueue(ERR_UNRECOGNIZED_COMMAND); // no match, error
	}
}

//---------------------------------------------------------------------------
// tests CONFigure:ALIGN1, CONFigure:ALIGN2
//---------------------------------------------------------------------------
void Parser::parse_configure_A(char* word, char *outputBuffer)
{
	// tests CONFigure:ALIGN1 <>, CONFigure:ALIGN2 <>
	if (strcmp(word, _ALIGN1) == 0 || strcmp(word, _ALIGN2) == 0)
	{
		int alignSelect = strcmp(word, _ALIGN1);

		word = strtok(NULL, COMMA);	// get next token

		if (isValue(word))
		{
			double mag = strtod(word, NULL);

			if (mag >= 0.0)	// magnitude cannot be negative
			{
				word = strtok(NULL, COMMA);	// get next token

				if (isValue(word))
				{
					double azimuth = strtod(word, NULL);

					word = strtok(NULL, COMMA);	// get next token

					if (isValue(word))
					{
						double inclination = strtod(word, NULL);

						if (inclination >= 0.0 && inclination <= 180.0)	// angle from Z-axis must be >= 0 and <= 180 degrees
						{
							// now check vector
							double x, y, z;
							sphericalToCartesian(mag, azimuth, inclination, &x, &y, &z);

							VectorError error = source->check_vector(x, y, z);

							if (error == NO_VECTOR_ERROR)
							{
								if (source->isConnected())
								{
									// good vector!!!
									if (alignSelect)
										emit set_align2(mag, azimuth, inclination);
									else
										emit set_align1(mag, azimuth, inclination);
								}
								else
								{
									addToErrorQueue(ERR_NOT_CONNECTED); // not connected, error
								}
							}
							else
							{
								int errorCode = -150 - (int)error;
								addToErrorQueue((SystemError)errorCode); // report vector error
							}
						}
						else
						{
							addToErrorQueue(ERR_INCLINATION_OUT_OF_RANGE); // inclination out of range, error
						}
					}
					else
					{
						if (word == NULL)
							addToErrorQueue(ERR_MISSING_PARAMETER); // missing argument, error
						else
							addToErrorQueue(ERR_INVALID_ARGUMENT); // invalid argument, error
					}
				}
				else
				{
					if (word == NULL)
						addToErrorQueue(ERR_MISSING_PARAMETER); // missing argument, error
					else
						addToErrorQueue(ERR_INVALID_ARGUMENT); // invalid argument, error
				}
			}
			else
			{
				addToErrorQueue(ERR_NEGATIVE_MAGNITUDE); // negative magnitude, error
			}
		}
		else
		{
			if (word == NULL)
				addToErrorQueue(ERR_MISSING_PARAMETER); // missing argument, error
			else
				addToErrorQueue(ERR_INVALID_ARGUMENT); // invalid argument, error
		}
	}
	else
	{
		addToErrorQueue(ERR_UNRECOGNIZED_COMMAND); // no match, error
	}
}

//---------------------------------------------------------------------------
// tests CONFigure:TARGet:ALIGN1
//		 CONFigure:TARGet:ALIGN2
//		 CONFigure:TARGet:VECtor
//		 CONFigure:TARGet:VECtor:CARTesian
//		 CONFigure:TARGet:VECtor:TABle
//		 CONFigure:TARGet:POLar
//		 CONFigure:TARGet:POLar:TABle
//---------------------------------------------------------------------------
void Parser::parse_configure_T(char* word, char *outputBuffer)
{
	SystemState state = (SystemState)source->get_state();

	if (strcmp(word, _TARG) == 0 || strcmp(word, _TARGET) == 0)
	{
		word = strtok(NULL, SEPARATOR);	// get next token
		word = struprt(word);	// convert to uppercase

		if (word == NULL)
		{
			addToErrorQueue(ERR_UNRECOGNIZED_COMMAND); // no match, error
		}
		
		else if (state == SYSTEM_HEATING || state == SYSTEM_COOLING)
		{
			addToErrorQueue(ERR_SWITCH_TRANSITION);
		}

		else if (source->isPersistent())
		{
			addToErrorQueue(ERR_IS_PERSISTENT);
		}

		// tests CONFigure:TARGet:ALIGN1, CONFigure:TARGet:ALIGN2
		else if (strcmp(word, _ALIGN1) == 0 || strcmp(word, _ALIGN2) == 0)
		{
			int alignSelect = strcmp(word, _ALIGN1);
			word = strtok(NULL, SEPARATOR);	// get next token

			if (word == NULL)
			{
				if (source->isConnected())
				{
					if (alignSelect)
						emit set_align2_active();
					else
						emit set_align1_active();
				}
				else
				{
					addToErrorQueue(ERR_NOT_CONNECTED); // not connected, error
				}
			}
			else
			{
				addToErrorQueue(ERR_UNRECOGNIZED_COMMAND); // no match, extraneous args, error
			}
		}

		// tests CONFigure:TARGet:VECtor, CONFigure:TARGet:VECtor:CARTesian
		//		 CONFigure:TARGet:VECtor:TABle
		else if (strcmp(word, _VEC) == 0 || strcmp(word, _VECTOR) == 0)
		{
			char args[256];
			word = strtok(NULL, DELIMITER);	// get next token
			strncpy(args, word, 255);		// make a copy

			word = strtok(word, SEPARATOR);	// get next command token
			word = struprt(word);	// convert to uppercase

			if (word == NULL)
			{
				addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);
			}

			// tests CONFigure:TARGet:VECtor:CARTesian
			else if (strcmp(word, _CART) == 0 || strcmp(word, _CARTESIAN) == 0)
			{
				// check for value
				word = strtok(NULL, COMMA);	// get next token

				// check values for CONFigure:TARGet:VECtor:CARTesian
				if (isValue(word))
				{
					double x = strtod(word, NULL);

					word = strtok(NULL, COMMA);	// get next token

					if (isValue(word))
					{
						double y = strtod(word, NULL);

						word = strtok(NULL, COMMA);	// get next token

						if (isValue(word))
						{
							double z = strtod(word, NULL);

							// now check vector
							VectorError error = source->check_vector(x, y, z);

							if (error == NO_VECTOR_ERROR)
							{
								if (source->isConnected())
								{
									word = strtok(NULL, COMMA);	// get next token

									// check time arg
									if (isValue(word))
									{
										double time = strtod(word, NULL);

										if (time >= 0)
										{
											// good vector!!!
											emit set_vector_cartesian(x, y, z, (int)time);
										}
										else
										{
											addToErrorQueue(ERR_INVALID_ARGUMENT); // time can't be negative, error
										}
									}
									else
									{
										if (word == NULL)
										{
											// good vector!!! assume zero time with no 4th arg
											emit set_vector_cartesian(x, y, z, 0);
										}
										else
											addToErrorQueue(ERR_INVALID_ARGUMENT); // invalid argument, error
									}
								}
								else
								{
									addToErrorQueue(ERR_NOT_CONNECTED); // not connected, error
								}
							}
							else
							{
								int errorCode = -150 - (int)error;
								addToErrorQueue((SystemError)errorCode); // report vector error
							}
						}
						else
						{
							if (word == NULL)
								addToErrorQueue(ERR_MISSING_PARAMETER); // missing argument, error
							else
								addToErrorQueue(ERR_INVALID_ARGUMENT); // invalid argument, error
						}
					}
					else
					{
						if (word == NULL)
							addToErrorQueue(ERR_MISSING_PARAMETER); // missing argument, error
						else
							addToErrorQueue(ERR_INVALID_ARGUMENT); // invalid argument, error
					}
				}
				else
				{
					if (word == NULL)
						addToErrorQueue(ERR_MISSING_PARAMETER); // missing argument, error
					else
						addToErrorQueue(ERR_INVALID_ARGUMENT); // invalid argument, error
				}
			}

			// tests CONFigure:TARGet:VECtor:TABle
			else if (strcmp(word, _TAB) == 0 || strcmp(word, _TABLE) == 0)
			{
				// check for value
				word = strtok(NULL, SEPARATOR);	// get next token

				if (isValue(word))
				{
					if (source->isConnected())
					{
						int tableRow = (int)strtod(word, NULL) - 1;	// table has programmatic index 0, while on-screen starts at 1

						// check for valid table index
						bool indexInRange = source->vec_table_row_in_range(tableRow);

						if (indexInRange)
						{
							// check range and vector value
							VectorError error = source->check_vector_table(tableRow);

							if (error == NO_VECTOR_ERROR)
							{
								emit goto_vector(tableRow);
							}
							else
							{
								int errorCode = -150 - (int)error;
								addToErrorQueue((SystemError)errorCode); // report vector error
							}
						}
						else
						{
							// table index out of range
							addToErrorQueue(ERR_OUT_OF_RANGE);
						}
					}
					else
					{
						addToErrorQueue(ERR_NOT_CONNECTED); // not connected, error
					}
				}
				else
				{
					if (word == NULL)
						addToErrorQueue(ERR_MISSING_PARAMETER); // missing argument, error
					else
						addToErrorQueue(ERR_INVALID_ARGUMENT); // invalid argument, error
				}
			}
			
			else // check string for values for CONFigure:TARGet:VECtor
			{
				// check for value
				word = args;
				word = strtok(word, COMMA);	// get next token
					
				if (isValue(word))
				{
					double mag = strtod(word, NULL);

					if (mag >= 0.0)	// magnitude cannot be negative
					{
						word = strtok(NULL, COMMA);	// get next token

						if (isValue(word))
						{
							double azimuth = strtod(word, NULL);

							word = strtok(NULL, COMMA);	// get next token

							if (isValue(word))
							{
								double inclination = strtod(word, NULL);

								if (inclination >= 0.0 && inclination <= 180.0)	// angle from Z-axis must be >= 0 and <= 180 degrees
								{
									// now check vector
									double x, y, z;
									sphericalToCartesian(mag, azimuth, inclination, &x, &y, &z);

									VectorError error = source->check_vector(x, y, z);

									if (error == NO_VECTOR_ERROR)
									{
										if (source->isConnected())
										{
											word = strtok(NULL, COMMA);	// get next token

											// check time arg
											if (isValue(word))
											{
												double time = strtod(word, NULL);

												if (time >= 0)
												{
													// good vector!!!
													emit set_vector(mag, azimuth, inclination, (int)time);
												}
												else
												{
													addToErrorQueue(ERR_INVALID_ARGUMENT); // time can't be negative, error
												}
											}
											else
											{
												if (word == NULL)
												{
													// good vector!!! zero time with no arg
													emit set_vector(mag, azimuth, inclination, 0);
												}
												else
													addToErrorQueue(ERR_INVALID_ARGUMENT); // invalid argument, error
											}
										}
										else
										{
											addToErrorQueue(ERR_NOT_CONNECTED); // not connected, error
										}
									}
									else
									{
										int errorCode = -150 - (int)error;
										addToErrorQueue((SystemError)errorCode); // report vector error
									}
								}
								else
								{
									addToErrorQueue(ERR_INCLINATION_OUT_OF_RANGE); // inclination out of range, error
								}
							}
							else
							{
								if (word == NULL)
									addToErrorQueue(ERR_MISSING_PARAMETER); // missing argument, error
								else
									addToErrorQueue(ERR_INVALID_ARGUMENT); // invalid argument, error
							}
						}
						else
						{
							if (word == NULL)
								addToErrorQueue(ERR_MISSING_PARAMETER); // missing argument, error
							else
								addToErrorQueue(ERR_INVALID_ARGUMENT); // invalid argument, error
						}
					}
					else
					{
						addToErrorQueue(ERR_NEGATIVE_MAGNITUDE); // negative magnitude, error
					}
				}
				else
				{
					if (word == NULL)
						addToErrorQueue(ERR_MISSING_PARAMETER); // missing argument, error
					else
						addToErrorQueue(ERR_INVALID_ARGUMENT); // invalid argument, error
				}
			}
		}

		// tests CONFigure:TARGet:POLar, CONFigure:TARGet:POLar:TABle
		else if (strcmp(word, _POL) == 0 || strcmp(word, _POLAR) == 0)
		{
			char args[256];
			word = strtok(NULL, DELIMITER);	// get next token
			strncpy(args, word, 255);		// make a copy

			word = strtok(word, SEPARATOR);	// get next token
			word = struprt(word);	// convert to uppercase

			if (word == NULL)
			{
				addToErrorQueue(ERR_UNRECOGNIZED_COMMAND);
			}
			else if (strcmp(word, _TAB) == 0 || strcmp(word, _TABLE) == 0)
			{
				// check for value
				word = strtok(NULL, SEPARATOR);	// get next token

				if (isValue(word))
				{
					if (source->isConnected())
					{
						int tableRow = (int)strtod(word, NULL) - 1;	// table has programmatic index 0, while on-screen starts at 1
						
						// check for valid table index
						bool indexInRange = source->polar_table_row_in_range(tableRow);

						if (indexInRange)
						{
							// check range and vector value
							VectorError error = source->check_polar_table(tableRow);

							if (error == NO_VECTOR_ERROR)
							{
								emit goto_polar(tableRow);
							}
							else
							{
								int errorCode = -150 - (int)error;
								addToErrorQueue((SystemError)errorCode); // report vector error
							}
						}
						else
						{
							// table index out of range
							addToErrorQueue(ERR_OUT_OF_RANGE);
						}
					}
					else
					{
						addToErrorQueue(ERR_NOT_CONNECTED); // not connected, error
					}
				}
				else
				{
					if (word == NULL)
						addToErrorQueue(ERR_MISSING_PARAMETER); // missing argument, error
					else
						addToErrorQueue(ERR_INVALID_ARGUMENT); // invalid argument, error
				}
			}

			else // check string for values for CONFigure:TARGet:POLar
			{
				// check for value
				word = args;
				word = strtok(word, COMMA);	// get next token

				if (isValue(word))
				{
					double mag = strtod(word, NULL);

					if (mag >= 0.0)	// magnitude cannot be negative
					{
						word = strtok(NULL, COMMA);	// get next token

						if (isValue(word))
						{
							double angle = strtod(word, NULL);

							word = strtok(NULL, COMMA);	// get next token

							// now check vector
							QVector3D vector;

							source->polarToCartesian(mag, angle, &vector);
							vector *= mag;	// multiply by magnitude

							VectorError error = source->check_vector(vector.x(), vector.y(), vector.z());

							if (error == NO_VECTOR_ERROR)
							{
								if (source->isConnected())
								{
									word = strtok(NULL, COMMA);	// get next token

									// check time arg
									if (isValue(word))
									{
										double time = strtod(word, NULL);

										if (time >= 0)
										{
											// good vector!!!
											emit set_polar(mag, angle, (int)time);
										}
										else
										{
											addToErrorQueue(ERR_INVALID_ARGUMENT); // time can't be negative, error
										}
									}
									else
									{
										if (word == NULL)
										{
											// good vector!!! zero time with no arg
											emit set_polar(mag, angle, 0);
										}
										else
											addToErrorQueue(ERR_INVALID_ARGUMENT); // invalid argument, error
									}
								}
								else
								{
									addToErrorQueue(ERR_NOT_CONNECTED); // not connected, error
								}
							}
							else
							{
								int errorCode = -150 - (int)error;
								addToErrorQueue((SystemError)errorCode); // report vector error
							}
						}
						else
						{
							if (word == NULL)
								addToErrorQueue(ERR_MISSING_PARAMETER); // missing argument, error
							else
								addToErrorQueue(ERR_INVALID_ARGUMENT); // invalid argument, error
						}
					}
					else
					{
						addToErrorQueue(ERR_NEGATIVE_MAGNITUDE); // negative magnitude, error
					}
				}
				else
				{
					if (word == NULL)
						addToErrorQueue(ERR_MISSING_PARAMETER); // missing argument, error
					else
						addToErrorQueue(ERR_INVALID_ARGUMENT); // invalid argument, error
				}
			}
		}

		else
		{
			addToErrorQueue(ERR_UNRECOGNIZED_COMMAND); // no match, error
		}
	}
	else
	{
		addToErrorQueue(ERR_UNRECOGNIZED_COMMAND); // no match, error
	}
}


//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
