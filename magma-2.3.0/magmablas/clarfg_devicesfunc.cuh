/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017

       @author Tingxing Dong

       @generated from magmablas/zlarfg_devicesfunc.cuh, normal z -> c, Wed Nov 15 00:34:29 2017
*/

#include "magma_templates.h"

#ifndef MAGMABLAS_CLARFG_DEVICES_Z_H
#define MAGMABLAS_CLARFG_DEVICES_Z_H

#define COMPLEX

/******************************************************************************/
/*
    lapack clarfg, compute the norm, scale and generate the householder vector   
    assume swork, sscale, scale are already allocated in shared memory
    BLOCK_SIZE is set outside, the size of swork is BLOCK_SIZE
*/
static __device__ void
clarfg_device(
    magma_int_t n,
    magmaFloatComplex* dalpha, magmaFloatComplex* dx, int incx,
    magmaFloatComplex* dtau,  float* swork, float* sscale, magmaFloatComplex* scale)
{
    const int tx = threadIdx.x;

    magmaFloatComplex tmp;
    
    // find max of [dalpha, dx], to use as scaling to avoid unnecesary under- and overflow    

    if ( tx == 0 ) {
        tmp = *dalpha;
        #ifdef COMPLEX
        swork[tx] = max( fabs(real(tmp)), fabs(imag(tmp)) );
        #else
        swork[tx] = fabs(tmp);
        #endif
    }
    else {
        swork[tx] = 0;
    }
    if (tx < BLOCK_SIZE)
    {
        for( int j = tx; j < n-1; j += BLOCK_SIZE ) {
            tmp = dx[j*incx];
            #ifdef COMPLEX
            swork[tx] = max( swork[tx], max( fabs(real(tmp)), fabs(imag(tmp)) ));
            #else
            swork[tx] = max( swork[tx], fabs(tmp) );
            #endif
        }
    }

    magma_max_reduce<BLOCK_SIZE>( tx, swork );

    if ( tx == 0 )
        *sscale = swork[0];
    __syncthreads();
    
    // sum norm^2 of dx/sscale
    // dx has length n-1
    if (tx < BLOCK_SIZE) swork[tx] = 0;
    if ( *sscale > 0 ) {
        if (tx < BLOCK_SIZE)
        {
            for( int j = tx; j < n-1; j += BLOCK_SIZE ) {
                tmp = dx[j*incx] / *sscale;
                swork[tx] += real(tmp)*real(tmp) + imag(tmp)*imag(tmp);
            }
        }
        magma_sum_reduce<BLOCK_SIZE>( tx, swork );
    }
    
    if ( tx == 0 ) {
        magmaFloatComplex alpha = *dalpha;

        if ( swork[0] == 0 && imag(alpha) == 0 ) {
            // H = I
            *dtau = MAGMA_C_ZERO;
        }
        else {
            // beta = norm( [dalpha, dx] )
            float beta;
            tmp  = alpha / *sscale;
            beta = *sscale * sqrt( real(tmp)*real(tmp) + imag(tmp)*imag(tmp) + swork[0] );
            beta = -copysign( beta, real(alpha) );
            // todo: deal with badly scaled vectors (see lapack's larfg)
            *dtau   = MAGMA_C_MAKE( (beta - real(alpha)) / beta, -imag(alpha) / beta );
            *dalpha = MAGMA_C_MAKE( beta, 0 );
            *scale = 1 / (alpha - beta);
        }
    }
    
    // scale x (if norm was not 0)
    __syncthreads();
    if ( swork[0] != 0 ) {
        if (tx < BLOCK_SIZE)
        {
            for( int j = tx; j < n-1; j += BLOCK_SIZE ) {
                dx[j*incx] *= *scale;
            }
        }
    }
}

#endif // MAGMABLAS_CLARFG_DEVICES_Z_H
