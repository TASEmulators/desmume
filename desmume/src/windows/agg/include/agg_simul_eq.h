//----------------------------------------------------------------------------
// Anti-Grain Geometry (AGG) - Version 2.5
// A high quality rendering engine for C++
// Copyright (C) 2002-2006 Maxim Shemanarev
// Contact: mcseem@antigrain.com
//          mcseemagg@yahoo.com
//          http://antigrain.com
// 
// AGG is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// AGG is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with AGG; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, 
// MA 02110-1301, USA.
//----------------------------------------------------------------------------

#ifndef AGG_SIMUL_EQ_INCLUDED
#define AGG_SIMUL_EQ_INCLUDED

#include <math.h>
#include "agg_basics.h"

namespace agg
{

    //=============================================================swap_arrays
    template<class T> void swap_arrays(T* a1, T* a2, unsigned n)
    {
        unsigned i;
        for(i = 0; i < n; i++)
        {
            T tmp = *a1;
            *a1++ = *a2;
            *a2++ = tmp;
        }
    }


    //============================================================matrix_pivot
    template<unsigned Rows, unsigned Cols>
    struct matrix_pivot
    {
        static int pivot(double m[Rows][Cols], unsigned row)
        {
            int k = int(row);
            double max_val, tmp;

            max_val = -1.0;
            unsigned i;
            for(i = row; i < Rows; i++)
            {
                if((tmp = fabs(m[i][row])) > max_val && tmp != 0.0)
                {
                    max_val = tmp;
                    k = i;
                }
            }

            if(m[k][row] == 0.0)
            {
                return -1;
            }

            if(k != int(row))
            {
                swap_arrays(m[k], m[row], Cols);
                return k;
            }
            return 0;
        }
    };
    


    //===============================================================simul_eq
    template<unsigned Size, unsigned RightCols>
    struct simul_eq
    {
        static bool solve(const double left[Size][Size], 
                          const double right[Size][RightCols],
                          double result[Size][RightCols])
        {
            unsigned i, j, k;
            double a1;

            double tmp[Size][Size + RightCols];

            for(i = 0; i < Size; i++)
            {
                for(j = 0; j < Size; j++)
                {
                    tmp[i][j] = left[i][j];
                } 
                for(j = 0; j < RightCols; j++)
                {
                    tmp[i][Size + j] = right[i][j];
                }
            }

            for(k = 0; k < Size; k++)
            {
                if(matrix_pivot<Size, Size + RightCols>::pivot(tmp, k) < 0)
                {
                    return false; // Singularity....
                }

                a1 = tmp[k][k];

                for(j = k; j < Size + RightCols; j++)
                {
                    tmp[k][j] /= a1;
                }

                for(i = k + 1; i < Size; i++)
                {
                    a1 = tmp[i][k];
                    for (j = k; j < Size + RightCols; j++)
                    {
                        tmp[i][j] -= a1 * tmp[k][j];
                    }
                }
            }


            for(k = 0; k < RightCols; k++)
            {
                int m;
                for(m = int(Size - 1); m >= 0; m--)
                {
                    result[m][k] = tmp[m][Size + k];
                    for(j = m + 1; j < Size; j++)
                    {
                        result[m][k] -= tmp[m][j] * result[j][k];
                    }
                }
            }
            return true;
        }

    };


}

#endif
