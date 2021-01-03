/*---------------------------------------------------------------------------*\

  FILE........: codec2.h
  AUTHOR......: David Rowe
  DATE CREATED: 21 August 2010

  Codec 2 fully quantised encoder and decoder functions.  If you want use
  Codec 2, these are the functions you need to call.

\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2010 David Rowe

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

#ifndef __CODEC2__
#define  __CODEC2__

#include <complex>

#include "codec2_internal.h"
#include "defines.h"
#include "kiss_fft.h"
#include "nlp.h"
#include "quantise.h"

#define CODEC2_MODE_3200 	0
#define CODEC2_MODE_1600 	2

#ifndef CODEC2_MODE_EN_DEFAULT
#define CODEC2_MODE_EN_DEFAULT 1
#endif

#define CODEC2_RAND_MAX 32767

class CCodec2
{
public:
    CCodec2(bool is_3200);
    ~CCodec2();
    void codec2_encode(unsigned char *bits, const short *speech_in);
    void codec2_decode(short *speech_out, const unsigned char *bits);
    void codec2_set_mode(bool);
    bool codec2_get_mode() {return (c2.mode == 3200); };
    int  codec2_samples_per_frame();
    int  codec2_bits_per_frame();
    void set_decode_gain(double g){ m_decode_gain = g; }

private:
    // merged from other files
    void sample_phase(MODEL *model, std::complex<double> filter_phase[], std::complex<double> A[]);
    void phase_synth_zero_order(int n_samp, MODEL *model, double *ex_phase, std::complex<double> filter_phase[]);
    void postfilter(MODEL *model, double *bg_est);

    C2CONST c2const_create(int Fs, double framelength_ms);

    void make_analysis_window(C2CONST *c2const, FFT_STATE *fft_fwd_cfg, double w[], double W[]);
    void dft_speech(C2CONST *c2const, FFT_STATE &fft_fwd_cfg, std::complex<double> Sw[], double Sn[], double w[]);
    void two_stage_pitch_refinement(C2CONST *c2const, MODEL *model, std::complex<double> Sw[]);
    void estimate_amplitudes(MODEL *model, std::complex<double> Sw[], int est_phase);
    double est_voicing_mbe(C2CONST *c2const, MODEL *model, std::complex<double> Sw[], double W[]);
    void make_synthesis_window(C2CONST *c2const, double Pn[]);
    void synthesise(int n_samp, FFTR_STATE *fftr_inv_cfg, double Sn_[], MODEL *model, double Pn[], int shift);
    int codec2_rand(void);
    void hs_pitch_refinement(MODEL *model, std::complex<double> Sw[], double pmin, double pmax, double pstep);

    void interp_Wo(MODEL *interp, MODEL *prev, MODEL *next, double Wo_min);
    void interp_Wo2(MODEL *interp, MODEL *prev, MODEL *next, double weight, double Wo_min);
    double interp_energy(double prev, double next);
    void interpolate_lsp_ver2(double interp[], double prev[],  double next[], double weight, int order);

    void analyse_one_frame(MODEL *model, const short *speech);
    void synthesise_one_frame(short speech[], MODEL *model, std::complex<double> Aw[], double gain);
    void codec2_encode_3200(unsigned char *bits, const short *speech);
    void codec2_encode_1600(unsigned char *bits, const short *speech);
    void codec2_decode_3200(short *speech, const unsigned char *bits);
    void codec2_decode_1600(short *speech, const unsigned char *bits);
    void ear_protection(double in_out[], int n);
    void lsp_to_lpc(double *freq, double *ak, int lpcrdr);

    void (CCodec2::*encode)(unsigned char *bits, const short *speech);
    void (CCodec2::*decode)(short *speech, const unsigned char *bits);
    Cnlp nlp;
    CQuantize qt;
    CODEC2 c2;
    double m_decode_gain;
};

#endif
