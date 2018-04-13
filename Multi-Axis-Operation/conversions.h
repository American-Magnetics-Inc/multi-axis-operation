#pragma once

extern void cartesianToSpherical(double x, double y, double z, double* magnitude, double* theta, double* phi);
extern void sphericalToCartesian(double magnitude, double theta, double phi, double* x, double* y, double* z);
extern double avoidSignedZeroOutput(double number, int precision);