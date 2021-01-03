/*---------------------------------------------------------------------------*\

  FILE........: quantise.h
  AUTHOR......: David Rowe
  DATE CREATED: 31/5/92

  Quantisation functions for the sinusoidal coder.

\*---------------------------------------------------------------------------*/

/*
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

#ifndef __QUANTISE__
#define __QUANTISE__

#include <complex>

#include "qbase.h"

class CQuantize : public CQbase {
public:
    void aks_to_M2(FFTR_STATE *fftr_fwd_cfg, double ak[], int order, MODEL *model, double E, double *snr, int sim_pf, int pf, int bass_boost, double beta, double gamma, std::complex<double> Aw[]);

    int   encode_Wo(C2CONST *c2const, double Wo, int bits);
    double decode_Wo(C2CONST *c2const, int index, int bits);
    void  encode_lsps_scalar(int indexes[], double lsp[], int order);
    void  decode_lsps_scalar(double lsp[], int indexes[], int order);
    void  encode_lspds_scalar(int indexes[], double lsp[], int order);
    void  decode_lspds_scalar(double lsp[], int indexes[], int order);

    int encode_energy(double e, int bits);
    double decode_energy(int index, int bits);

    void pack(unsigned char * bits, unsigned int *nbit, int index, unsigned int index_bits);
    void pack_natural_or_gray(unsigned char * bits, unsigned int *nbit, int index, unsigned int index_bits, unsigned int gray);
    int  unpack(const unsigned char * bits, unsigned int *nbit, unsigned int index_bits);
    int  unpack_natural_or_gray(const unsigned char * bits, unsigned int *nbit, unsigned int index_bits, unsigned int gray);

    int lsp_bits(int i);
    int lspd_bits(int i);

    void apply_lpc_correction(MODEL *model);
    double speech_to_uq_lsps(double lsp[], double ak[], double Sn[], double w[], int m_pitch, int order);
    int check_lsp_order(double lsp[], int lpc_order);
    void bw_expand_lsps(double lsp[], int order, double min_sep_low, double min_sep_high);

private:
    void compute_weights(const double *x, double *w, int ndim);
    int find_nearest(const double *codebook, int nb_entries, double *x, int ndim);
    void lpc_post_filter(FFTR_STATE *fftr_fwd_cfg, double Pw[], double ak[], int order, double beta, double gamma, int bass_boost, double E);
    int lpc_to_lsp (double *a, int lpcrdr, double *freq, int nb, double delta);
    double cheb_poly_eva(double *coef,double x,int order);
};

#endif
