/*---------------------------------------------------------------------------*\

  FILE........: lpc.h
  AUTHOR......: David Rowe
  DATE CREATED: 24/8/09

  Linear Prediction functions written in C.

\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2009-2012 David Rowe

  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2.1, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __LPC__
#define __LPC__

#define LPC_MAX_ORDER 20

class Clpc {
public:
    void autocorrelate(double Sn[], double Rn[], int Nsam, int order);
    void levinson_durbin(double R[],	double lpcs[], int order);
private:
    void pre_emp(double Sn_pre[], double Sn[], double *mem, int Nsam);
    void de_emp(double Sn_se[], double Sn[], double *mem, int Nsam);
    void hanning_window(double Sn[], double Wn[], int Nsam);
    void inverse_filter(double Sn[], double a[], int Nsam, double res[], int order);
    void synthesis_filter(double res[], double a[], int Nsam,	int order, double Sn_[]);
    void find_aks(double Sn[], double a[], int Nsam, int order, double *E);
    void weight(double ak[],	double gamma, int order,	double akw[]);
};

#endif
