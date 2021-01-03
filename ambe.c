/*
 * Copyright (C) 2010 mbelib Author
 * GPG Key ID: 0xEA5EFE2C (9E7A 5527 9CDC EBF7 BF1B  D772 4F98 E863 EA5E FE2C)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include "ambe.h"
//#include "ambe3600x2250_const.h"
//#include "ambe3600x2400_const.h"

extern const double AmbePlusLtable[];
extern const int AmbePlusVuv[16][8];
extern const int AmbePlusLmprbl[57][4];
extern const double AmbePlusDg[64];
extern const double AmbePlusPRBA24[512][3];
extern const double AmbePlusPRBA58[128][4];
extern const double AmbePlusHOCb5[16][4];
extern const double AmbePlusHOCb6[16][4];
extern const double AmbePlusHOCb7[16][4];
extern const double AmbePlusHOCb8[16][4];
extern const double AmbeW0table[120];
extern const double AmbeLtable[120];
extern const int AmbeVuv[32][8];
extern const int AmbeLmprbl[57][4];
extern const double AmbeDg[32];
extern const double AmbePRBA24[512][3];
extern const double AmbePRBA58[128][4];
extern const double AmbeHOCb5[32][4];
extern const double AmbeHOCb6[16][4];
extern const double AmbeHOCb7[16][4];
extern const double AmbeHOCb8[8][4];

static int
mbe_dequantizeAmbeParms (mbe_parms * cur_mp, mbe_parms * prev_mp, const int *b, int dstar)
{

  int ji, i, j, k, l, L, m, am, ak;
  int intkl[57];
  int b0, b1, b2, b3, b4, b5, b6, b7, b8;
  double f0, Cik[5][18], flokl[57], deltal[57];
  double Sum42, Sum43, Tl[57], Gm[9], Ri[9], sum, c1, c2;
  //char tmpstr[13];
  int silence;
  int Ji[5], jl;
  double deltaGamma, BigGamma;
  double unvc, rconst;

  b0 = b[0];
  b1 = b[1];
  b2 = b[2];
  b3 = b[3];
  b4 = b[4];
  b5 = b[5];
  b6 = b[6];
  b7 = b[7];
  b8 = b[8];

  silence = 0;

#ifdef QT_DEBUG
  fprintf (stderr, "\n");
#endif

  // copy repeat from prev_mp
  cur_mp->repeat = prev_mp->repeat;

  if ((b0 >= 120) && (b0 <= 123))
    {
#ifdef QT_DEBUG
      fprintf (stderr, "AMBE Erasure Frame\n");
#endif
      return (2);
    }
  else if ((b0 == 124) || (b0 == 125))
    {
#ifdef QT_DEBUG
      fprintf (stderr, "AMBE Silence Frame\n");
#endif
      silence = 1;
      cur_mp->w0 = (2.0 * M_PI) / 32.0;
      f0 = 1.0 / 32.0;
      L = 14;
      cur_mp->L = 14;
      for (l = 1; l <= L; l++)
        {
          cur_mp->Vl[l] = 0;
        }
    }
  else if ((b0 == 126) || (b0 == 127))
    {
#ifdef QT_DEBUG
      fprintf (stderr, "AMBE Tone Frame\n");
#endif
      return (3);
    }

  if (silence == 0)
    {
      if (dstar)
        f0 = pow(2, (-4.311767578125 - (2.1336e-2 * (b0+0.5))));
      else
      // w0 from specification document
        f0 = AmbeW0table[b0];
      cur_mp->w0 = f0 * 2.0 * M_PI;
      // w0 from patent filings
      //f0 = powf (2, ((double) b0 + (double) 195.626) / -(double) 45.368);
      //cur_mp->w0 = f0 * (double) 2 *M_PI;
    }

  unvc = 0.2046 / sqrt (cur_mp->w0);
  //unvc = (double) 1;
  //unvc = (double) 0.2046 / sqrtf (f0);

  // decode L
  if (silence == 0)
    {
      // L from specification document
      // lookup L in tabl3
      if (dstar)
        L = (int)AmbePlusLtable[b0];
      else
        L = (int)AmbeLtable[b0];
      // L formula form patent filings
      //L=(int)((double)0.4627 / f0);
      cur_mp->L = L;
    }

  // decode V/UV parameters
  for (l = 1; l <= L; l++)
    {
      // jl from specification document
      jl = (int) ( l * 16.0 * f0);
      // jl from patent filings?
      //jl = (int)(((double)l * (double)16.0 * f0) + 0.25);

      if (silence == 0)
        {
          if (dstar)
            cur_mp->Vl[l] = AmbePlusVuv[b1][jl];
          else
            cur_mp->Vl[l] = AmbeVuv[b1][jl];
        }
#ifdef QT_DEBUG
      fprintf (stderr, "jl[%i]:%i Vl[%i]:%i\n", l, jl, l, cur_mp->Vl[l]);
#endif
    }
#ifdef QT_DEBUG
  fprintf (atderr, "\nb0:%i w0:%f L:%i b1:%i\n", b0, cur_mp->w0, L, b1);
#endif
  if (dstar) {
    deltaGamma = AmbePlusDg[b2];
    cur_mp->gamma = deltaGamma + ( 0.5 * prev_mp->gamma);
  } else {
    deltaGamma = AmbeDg[b2];
    cur_mp->gamma = deltaGamma + ( 0.5 * prev_mp->gamma);
  }
#ifdef QT_DEBUG
  fprintf (stderr, "b2: %i, deltaGamma: %f gamma: %f gamma-1: %f\n", b2, deltaGamma, cur_mp->gamma, prev_mp->gamma);
#endif


  // decode PRBA vectors
  Gm[1] = 0;

  if (dstar) {
    Gm[2] = AmbePlusPRBA24[b3][0];
    Gm[3] = AmbePlusPRBA24[b3][1];
    Gm[4] = AmbePlusPRBA24[b3][2];

    Gm[5] = AmbePlusPRBA58[b4][0];
    Gm[6] = AmbePlusPRBA58[b4][1];
    Gm[7] = AmbePlusPRBA58[b4][2];
    Gm[8] = AmbePlusPRBA58[b4][3];
  } else {
    Gm[2] = AmbePRBA24[b3][0];
    Gm[3] = AmbePRBA24[b3][1];
    Gm[4] = AmbePRBA24[b3][2];

    Gm[5] = AmbePRBA58[b4][0];
    Gm[6] = AmbePRBA58[b4][1];
    Gm[7] = AmbePRBA58[b4][2];
    Gm[8] = AmbePRBA58[b4][3];
  }

#ifdef QT_DEBUG
  fprintf (stderr, "b3: %i Gm[2]: %f Gm[3]: %f Gm[4]: %f b4: %i Gm[5]: %f Gm[6]: %f Gm[7]: %f Gm[8]: %f\n", b3, Gm[2], Gm[3], Gm[4], b4, Gm[5], Gm[6], Gm[7], Gm[8]);
#endif

  // compute Ri
  for (i = 1; i <= 8; i++)
    {
      sum = 0;
      for (m = 1; m <= 8; m++)
        {
          if (m == 1)
            {
              am = 1;
            }
          else
            {
              am = 2;
            }
          sum = sum + (am * Gm[m] * cos ((M_PI * (m - 1.0) * ( i - 0.5)) / 8.0));
        }
      Ri[i] = sum;
#ifdef QT_DEBUG
      fprintf (stderr, "R%i: %f ", i, Ri[i]);
#endif
    }
#ifdef QT_DEBUG
  fprintf (stderr, "\n");
#endif

  // generate first to elements of each Ci,k block from PRBA vector
  rconst = ( 1.0 / ( 2.0 * M_SQRT2));
  Cik[1][1] = 0.5 *(Ri[1] + Ri[2]);
  Cik[1][2] = rconst * (Ri[1] - Ri[2]);
  Cik[2][1] = 0.5 *(Ri[3] + Ri[4]);
  Cik[2][2] = rconst * (Ri[3] - Ri[4]);
  Cik[3][1] = 0.5 *(Ri[5] + Ri[6]);
  Cik[3][2] = rconst * (Ri[5] - Ri[6]);
  Cik[4][1] = 0.5 *(Ri[7] + Ri[8]);
  Cik[4][2] = rconst * (Ri[7] - Ri[8]);

  // decode HOC

  // lookup Ji
  if (dstar) {
    Ji[1] = AmbePlusLmprbl[L][0];
    Ji[2] = AmbePlusLmprbl[L][1];
    Ji[3] = AmbePlusLmprbl[L][2];
    Ji[4] = AmbePlusLmprbl[L][3];
  } else {
    Ji[1] = AmbeLmprbl[L][0];
    Ji[2] = AmbeLmprbl[L][1];
    Ji[3] = AmbeLmprbl[L][2];
    Ji[4] = AmbeLmprbl[L][3];
  }
#ifdef QT_DEBUG
  fprintf (stderr, "Ji[1]: %i Ji[2]: %i Ji[3]: %i Ji[4]: %i\n", Ji[1], Ji[2], Ji[3], Ji[4]);
  fprintf (stderr, "b5: %i b6: %i b7: %i b8: %i\n", b5, b6, b7, b8);
#endif

  // Load Ci,k with the values from the HOC tables
  // there appear to be a couple typos in eq. 37 so we will just do what makes sense
  // (3 <= k <= Ji and k<=6)
  for (k = 3; k <= Ji[1]; k++)
    {
      if (k > 6)
        {
          Cik[1][k] = 0;
        }
      else
        {
          if (dstar)
            Cik[1][k] = AmbePlusHOCb5[b5][k - 3];
          else
            Cik[1][k] = AmbeHOCb5[b5][k - 3];
#ifdef QT_DEBUG
          fprintf (stderr, "C1,%i: %f ", k, Cik[1][k]);
#endif
        }
    }
  for (k = 3; k <= Ji[2]; k++)
    {
      if (k > 6)
        {
          Cik[2][k] = 0;
        }
      else
        {
          if (dstar)
            Cik[2][k] = AmbePlusHOCb6[b6][k - 3];
          else
            Cik[2][k] = AmbeHOCb6[b6][k - 3];
#ifdef QT_DEBUG
          fprintf (stderr, "C2,%i: %f ", k, Cik[2][k]);
#endif
        }
    }
  for (k = 3; k <= Ji[3]; k++)
    {
      if (k > 6)
        {
          Cik[3][k] = 0;
        }
      else
        {
          if (dstar)
            Cik[3][k] = AmbePlusHOCb7[b7][k - 3];
          else
            Cik[3][k] = AmbeHOCb7[b7][k - 3];
#ifdef QT_DEBUG
          fprintf (stderr, "C3,%i: %f ", k, Cik[3][k]);
#endif
        }
    }
  for (k = 3; k <= Ji[4]; k++)
    {
      if (k > 6)
        {
          Cik[4][k] = 0;
        }
      else
        {
          if (dstar)
            Cik[4][k] = AmbePlusHOCb8[b8][k - 3];
          else
            Cik[4][k] = AmbeHOCb8[b8][k - 3];
#ifdef QT_DEBUG
          fprintf (stderr, "C4,%i: %f ", k, Cik[4][k]);
#endif
        }
    }
#ifdef QT_DEBUG
  fprintf (stderr, "\n");
#endif

  // inverse DCT each Ci,k to give ci,j (Tl)
  l = 1;
  for (i = 1; i <= 4; i++)
    {
      ji = Ji[i];
      for (j = 1; j <= ji; j++)
        {
          sum = 0;
          for (k = 1; k <= ji; k++)
            {
              if (k == 1)
                {
                  ak = 1;
                }
              else
                {
                  ak = 2;
                }
#ifdef QT_DEBUG
              fprintf (stderr, "j: %i Cik[%i][%i]: %f ", j, i, k, Cik[i][k]);
#endif
              sum = sum + ( ak * Cik[i][k] * cos ((M_PI * (k - 1.0) * (j - 0.5)) / ji));
            }
          Tl[l] = sum;
#ifdef QT_DEBUG
          fprintf (stderr, "Tl[%i]: %f\n", l, Tl[l]);
#endif
          l++;
        }
    }

  // determine log2Ml by applying ci,j to previous log2Ml

  // fix for when L > L(-1)
  if (cur_mp->L > prev_mp->L)
    {
      for (l = (prev_mp->L) + 1; l <= cur_mp->L; l++)
        {
          prev_mp->Ml[l] = prev_mp->Ml[prev_mp->L];
          prev_mp->log2Ml[l] = prev_mp->log2Ml[prev_mp->L];
        }
    }
  prev_mp->log2Ml[0] = prev_mp->log2Ml[1];
  prev_mp->Ml[0] = prev_mp->Ml[1];

  // Part 1
  Sum43 = 0;
  for (l = 1; l <= cur_mp->L; l++)
    {

      // eq. 40
      flokl[l] = (prev_mp->L / cur_mp->L) * l;
      intkl[l] = (int) (flokl[l]);
#ifdef QT_DEBUG
      fprintf (stderr, "flok%i: %f, intk%i: %i ", l, flokl[l], l, intkl[l]);
#endif
      // eq. 41
      deltal[l] = flokl[l] - (double) intkl[l];
#ifdef QT_DEBUG
      fprintf (stderr, "delta%i: %f ", l, deltal[l]);
#endif
      // eq 43
      Sum43 = Sum43 + (((1.0 - deltal[l]) * prev_mp->log2Ml[intkl[l]]) + (deltal[l] * prev_mp->log2Ml[intkl[l] + 1]));
    }
  Sum43 = ((0.65 / cur_mp->L) * Sum43);
#ifdef QT_DEBUG
  fprintf (stderr, "\n");
  fprintf (stderr, "Sum43: %f\n", Sum43);
#endif

  // Part 2
  Sum42 = 0;
  for (l = 1; l <= cur_mp->L; l++)
    {
      Sum42 += Tl[l];
    }
  Sum42 = Sum42 / cur_mp->L;
  BigGamma = cur_mp->gamma - ( 0.5 * (log ((double)cur_mp->L) / log (2.0))) - Sum42;
  //BigGamma=cur_mp->gamma - (0.5 * log((double)cur_mp->L)) - Sum42;

  // Part 3
  for (l = 1; l <= cur_mp->L; l++)
    {
      c1 = ( 0.65 * ( 1.0 - deltal[l]) * prev_mp->log2Ml[intkl[l]]);
      c2 = ( 0.65 * deltal[l] * prev_mp->log2Ml[intkl[l] + 1]);
      cur_mp->log2Ml[l] = Tl[l] + c1 + c2 - Sum43 + BigGamma;
      // inverse log to generate spectral amplitudes
      if (cur_mp->Vl[l] == 1)
        {
          cur_mp->Ml[l] = exp (0.693 * cur_mp->log2Ml[l]);
        }
      else
        {
          cur_mp->Ml[l] = unvc * exp ( 0.693 * cur_mp->log2Ml[l]);
        }
#ifdef QT_DEBUG
      fprintf (stderr, "flokl[%i]: %f, intkl[%i]: %i ", l, flokl[l], l, intkl[l]);
      fprintf (stderr, "deltal[%i]: %f ", l, deltal[l]);
      fprintf (stderr, "prev_mp->log2Ml[%i]: %f\n", l, prev_mp->log2Ml[intkl[l]]);
      fprintf (stderr, "BigGamma: %f c1: %f c2: %f Sum43: %f Tl[%i]: %f log2Ml[%i]: %f Ml[%i]: %f\n", BigGamma, c1, c2, Sum43, l, Tl[l], l, cur_mp->log2Ml[l], l, cur_mp->Ml[l]);
#endif
    }

  return (0);
}

int
mbe_dequantizeAmbeTone(mbe_tone * tone, const int *u)
{
    int bitchk1, bitchk2;
    int AD, ID1, ID2, ID3, ID4;
    bitchk1 = (u[0] >> 6) & 0x3f;
    bitchk2 = (u[3] & 0xf);

    if ((bitchk1 != 63) || (bitchk2 != 0))
        return -1; // Not a valid tone frame

    AD = ((u[0] & 0x3f) << 1) + ((u[3] >> 4) & 0x1);
    ID1 = ((u[1] & 0xfff) >> 4);
    ID2 = ((u[1] & 0xf) << 4) + ((u[2] >> 7) & 0xf);
    ID3 = ((u[2] & 0x7f) << 1) + ((u[3] >> 13) & 0x1);
    ID4 = ((u[3] & 0x1fe0) >> 5);

    if ((ID1 == ID2) && (ID1 == ID3) && (ID1 == ID4) &&
        (((ID1 >= 5) && (ID1 <= 122)) || ((ID1 >= 128) && (ID1 <= 163)) || (ID1 == 255))) {
        if (tone->ID == ID1) {
            tone->AD = AD;
        } else {
            tone->n = 0;
            tone->ID = ID1;
            tone->AD = AD;
        }
        return 0; // valid in-range tone frequency
    }

    return -1;
}

int
mbe_dequantizeAmbe2400Parms (mbe_parms * cur_mp, mbe_parms * prev_mp, const int *b){
    int dstar = 1;
    return (mbe_dequantizeAmbeParms (cur_mp, prev_mp, b, dstar));
}

int
mbe_dequantizeAmbe2250Parms (mbe_parms * cur_mp, mbe_parms * prev_mp, const int *b){
    int dstar = 0;
    return (mbe_dequantizeAmbeParms (cur_mp, prev_mp, b, dstar));
}
