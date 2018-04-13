#include "stdafx.h"
#include "math.h"

const double RAD_TO_DEG = 180.0 / M_PI;

//---------------------------------------------------------------------------
// Utility functions
//---------------------------------------------------------------------------
// Converts cartesian to spherical coordinates
void cartesianToSpherical(double x, double y, double z, double* magnitude, double* theta, double* phi)
{
	*magnitude = sqrt(x * x + y * y + z * z);
	*theta = atan2l(y, x) * RAD_TO_DEG;
	*phi = atan2l(sqrt(x * x + y * y), z) * RAD_TO_DEG;
}

//---------------------------------------------------------------------------
// Converts spherical to cartesian coordinates
void sphericalToCartesian(double magnitude, double theta, double phi, double* x, double* y, double* z)
{
	*x = magnitude * sin(phi / RAD_TO_DEG) * cos(theta / RAD_TO_DEG);
	*y = magnitude * sin(phi / RAD_TO_DEG) * sin(theta / RAD_TO_DEG);
	*z = magnitude * cos(phi / RAD_TO_DEG);
}

//---------------------------------------------------------------------------
double avoidSignedZeroOutput(double number, int precision)
{
	if ((number < 0.0) && (-log10(fabs(number)) > precision))
	{
		return 0.0;
	}
	else
	{
		return number;
	}
}

//---------------------------------------------------------------------------