/*
    Copyright (C) 2019-2021 Doug McLain

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef INCLUDED_AMBE_H
#define INCLUDED_AMBE_H
#ifdef __cplusplus

extern "C" {
#endif
#include "mbelib.h"
struct mbe_tones
{
  int ID;
  int AD;
  int n;
};
typedef struct mbe_tones mbe_tone;

int mbe_dequantizeAmbe2250Parms (mbe_parms * cur_mp, mbe_parms * prev_mp, const int *b);
int mbe_dequantizeAmbe2400Parms (mbe_parms * cur_mp, mbe_parms * prev_mp, const int *b);
int mbe_dequantizeAmbeTone(mbe_tone * tone, const int *u);
#ifdef __cplusplus
}
#endif
#endif /* INCLUDED_AMBE_H */
