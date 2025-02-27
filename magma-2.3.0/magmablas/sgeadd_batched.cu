/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017

       @generated from magmablas/zgeadd_batched.cu, normal z -> s, Wed Nov 15 00:34:23 2017
       @author Mark Gates
*/
#include "magma_internal.h"

#define NB 64

/******************************************************************************/
/*
    Batches slacpy of multiple arrays;
    y-dimension of grid is different arrays,
    x-dimension of grid is blocks for each array.
    Matrix is m x n, and is divided into block rows, each NB x n.
    Each CUDA block has NB threads to handle one block row.
    Each thread adds one row, iterating across all columns.
    The bottom block of rows may be partially outside the matrix;
    if so, rows outside the matrix (i >= m) are disabled.
    
    TODO. Block in both directions, for large matrices.
    E.g., each block does 64x64 tile, instead of 64xN tile.
*/
__global__ void
sgeadd_batched_kernel(
    int m, int n,
    float alpha,
    const float * const *dAarray, int ldda,
    float              **dBarray, int lddb )
{
    // dA and dB iterate across row i
    const float *dA = dAarray[ blockIdx.y ];
    float       *dB = dBarray[ blockIdx.y ];
    int i = blockIdx.x*blockDim.x + threadIdx.x;
    if ( i < m ) {
        dA += i;
        dB += i;
        const float *dAend = dA + n*ldda;
        while( dA < dAend ) {
            *dB = alpha*(*dA) + (*dB);
            dA += ldda;
            dB += lddb;
        }
    }
}


/***************************************************************************//**
    Purpose
    -------
    ZGEADD adds two sets of matrices, dAarray[i] = alpha*dAarray[i] + dBarray[i],
    for i = 0, ..., batchCount-1.
    
    Arguments
    ---------
    
    @param[in]
    m       INTEGER
            The number of rows of each matrix dAarray[i].  M >= 0.
    
    @param[in]
    n       INTEGER
            The number of columns of each matrix dAarray[i].  N >= 0.
    
    @param[in]
    alpha   REAL
            The scalar alpha.
            
    @param[in]
    dAarray array on GPU, dimension(batchCount), of pointers to arrays,
            with each array a REAL array, dimension (LDDA,N)
            The m by n matrices dAarray[i].
    
    @param[in]
    ldda    INTEGER
            The leading dimension of each array dAarray[i].  LDDA >= max(1,M).
            
    @param[in,out]
    dBarray array on GPU, dimension(batchCount), of pointers to arrays,
            with each array a REAL array, dimension (LDDB,N)
            The m by n matrices dBarray[i].
    
    @param[in]
    lddb    INTEGER
            The leading dimension of each array dBarray[i].  LDDB >= max(1,M).
    
    @param[in]
    batchCount INTEGER
            The number of matrices to add; length of dAarray and dBarray.
            batchCount >= 0.
    
    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_geadd_batched
*******************************************************************************/
extern "C" void
magmablas_sgeadd_batched(
    magma_int_t m, magma_int_t n,
    float alpha,
    magmaFloat_const_ptr  const dAarray[], magma_int_t ldda,
    magmaFloat_ptr              dBarray[], magma_int_t lddb,
    magma_int_t batchCount,
    magma_queue_t queue )
{
    magma_int_t info = 0;
    if ( m < 0 )
        info = -1;
    else if ( n < 0 )
        info = -2;
    else if ( ldda < max(1,m))
        info = -5;
    else if ( lddb < max(1,m))
        info = -7;
    else if ( batchCount < 0 )
        info = -8;
    
    if ( info != 0 ) {
        magma_xerbla( __func__, -(info) );
        return;
    }
    
    if ( m == 0 || n == 0 || batchCount == 0 )
        return;
    
    dim3 threads( NB );
    dim3 grid( magma_ceildiv( m, NB ), batchCount );
    
    sgeadd_batched_kernel<<< grid, threads, 0, queue->cuda_stream() >>>(
        m, n, alpha, dAarray, ldda, dBarray, lddb );
}
