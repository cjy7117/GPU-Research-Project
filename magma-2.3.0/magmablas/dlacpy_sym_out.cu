/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017

       @author Mark Gates
       @author Azzam Haidar
       @author Ichitaro Yamazaki
       
       @generated from magmablas/zlacpy_sym_out.cu, normal z -> d, Wed Nov 15 00:34:21 2017

*/
#include "magma_internal.h"

#define BLK_X 64
#define BLK_Y 32

/******************************************************************************/
/*
    Divides matrix into ceil( m/BLK_X ) x ceil( n/BLK_Y ) blocks.
    Each block has BLK_X threads.
    Each thread loops across one row, updating BLK_Y entries.

    Code similar to dlaset, dlacpy, dlag2s, clag2z, dgeadd.
*/
static __device__
void dlacpy_sym_out_full_device(
    int m, int n,
    const double *dA, int ldda,
    double       *dB, int lddb )
{
    int ind = blockIdx.x*BLK_X + threadIdx.x;
    int iby = blockIdx.y*BLK_Y;
    /* check if full block-column */
    bool full = (iby + BLK_Y <= n);
    /* do only rows inside matrix */
    if ( ind < m ) {
        dA += ind + iby*ldda;
        dB += ind + iby*lddb;
        if ( full ) {
            // full block-column
            #pragma unroll
            for( int j=0; j < BLK_Y; ++j ) {
                dB[j*lddb] = dA[j*ldda];
            }
        }
        else {
            // partial block-column
            for( int j=0; j < BLK_Y && iby+j < n; ++j ) {
                dB[j*lddb] = dA[j*ldda];
            }
        }
    }
}


/******************************************************************************/
/*
    Similar to dlacpy_full, but updates only the diagonal and below.
    Blocks that are fully above the diagonal exit immediately.

    Code similar to dlaset, dlacpy, zlat2c, clat2z.
*/
static __device__
void dlacpy_sym_out_lower_device(
    int m, int n, magma_int_t *rows, magma_int_t *perm,
    const double *dA, int ldda,
    double       *dB, int lddb )
{
    int ind = blockIdx.x*BLK_X + threadIdx.x; // row
    int iby = blockIdx.y*BLK_Y;               // col

    /* check if full block-column && (below diag) */
    bool full = (iby + BLK_Y <= n);
    for (int jj=0; jj < n; jj++) {
        perm[rows[2*jj+1]] = rows[2*jj+1];
    }
    /* do only rows inside matrix, and blocks not above diag */
    if ( ind < m ) {
        if ( full ) {
            // full block-column, off-diagonal block
            #pragma unroll
            for( int jj=0; jj < BLK_Y; ++jj ) {
                int j = rows[2*(iby+jj)+1];
                if (ind <= j)
                    dB[j + ind*ldda] = MAGMA_D_CONJ( dA[ind + (iby+jj)*lddb] );
                else
                    dB[ind + j*ldda] = dA[ind + (iby+jj)*lddb];
            }
        }
        else {
            // either partial block-column or diagonal block
            for( int jj=0; jj < BLK_Y && iby+jj < n; ++jj ) {
                int j = rows[2*(iby+jj)+1];
                if (ind <= j)
                    dB[j + ind*ldda] = MAGMA_D_CONJ( dA[ind + (iby+jj)*lddb] );
                else
                    dB[ind + j*ldda] = dA[ind + (iby+jj)*lddb];
            }
        }
    }
}


/******************************************************************************/
/*
    Similar to dlacpy_full, but updates only the diagonal and above.
    Blocks that are fully below the diagonal exit immediately.

    Code similar to dlaset, dlacpy, zlat2c, clat2z.
*/
static __device__
void dlacpy_sym_out_upper_device(
    int m, int n,
    const double *dA, int ldda,
    double       *dB, int lddb )
{
    int ind = blockIdx.x*BLK_X + threadIdx.x;
    int iby = blockIdx.y*BLK_Y;
    /* check if full block-column && (above diag) */
    bool full = (iby + BLK_Y <= n && (ind + BLK_X <= iby));
    /* do only rows inside matrix, and blocks not below diag */
    if ( ind < m && ind < iby + BLK_Y ) {
        dA += ind + iby*ldda;
        dB += ind + iby*lddb;
        if ( full ) {
            // full block-column, off-diagonal block
            #pragma unroll
            for( int j=0; j < BLK_Y; ++j ) {
                dB[j*lddb] = dA[j*ldda];
            }
        }
        else {
            // either partial block-column or diagonal block
            for( int j=0; j < BLK_Y && iby+j < n; ++j ) {
                if ( ind <= iby+j ) {
                    dB[j*lddb] = dA[j*ldda];
                }
            }
        }
    }
}


/******************************************************************************/
/*
    kernel wrappers to call the device functions.
*/
__global__
void dlacpy_sym_out_full_kernel(
    int m, int n,
    const double *dA, int ldda,
    double       *dB, int lddb )
{
    dlacpy_sym_out_full_device(m, n, dA, ldda, dB, lddb);
}

__global__
void dlacpy_sym_out_lower_kernel(
    int m, int n, magma_int_t *rows, magma_int_t *perm,
    const double *dA, int ldda,
    double       *dB, int lddb )
{
    dlacpy_sym_out_lower_device(m, n, rows, perm, dA, ldda, dB, lddb);
}

__global__
void dlacpy_sym_out_upper_kernel(
    int m, int n,
    const double *dA, int ldda,
    double       *dB, int lddb )
{
    dlacpy_sym_out_upper_device(m, n, dA, ldda, dB, lddb);
}


/***************************************************************************//**
    Purpose
    -------
    DLACPY_SYM_OUT copies all or part of a two-dimensional matrix dA to another
    matrix dB.
    
    This is the same as DLACPY, but adds queue argument.
    
    Arguments
    ---------
    @param[in]
    uplo    magma_uplo_t
            Specifies the part of the matrix dA to be copied to dB.
      -     = MagmaUpper:      Upper triangular part
      -     = MagmaLower:      Lower triangular part
      -     = MagmaFull:       All of the matrix dA
    
    @param[in]
    m       INTEGER
            The number of rows of the matrix dA.  M >= 0.
    
    @param[in]
    n       INTEGER
            The number of columns of the matrix dA.  N >= 0.

    @param[in]
    rows    INTEGER array, on GPU, dimension (2*n)
            On entry, it stores the new pivots such that rows[i]-th and rows[n+i]-th
            rows are swapped.

    @param[in,out]
    perm    INTEGER array, on GPU, dimension (m)
            On entry, it stores the permutation array such that i-th row will be 
            the original perm[i]-th row after the pivots are applied.
            On exit, it is restored to be identity permutation.

    @param[in,out]
    dA      DOUBLE PRECISION array, dimension (LDDA,N)
            The M-by-N matrix dA.
            If UPLO = MagmaUpper, only the upper triangle or trapezoid is accessed;
            if UPLO = MagmaLower, only the lower triangle or trapezoid is accessed.
            On exit, the matrix after the symmetric pivoting is applied.
    
    @param[in]
    ldda    INTEGER
            The leading dimension of the array dA.  LDDA >= max(1,M).
    
    @param[in]
    dB      DOUBLE PRECISION array, dimension (LDDB,N)
            The M-by-N matrix dB.
            On entry, dB stores the columns after row pivoting is applied.
    
    @param[in]
    lddb    INTEGER
            The leading dimension of the array dB.  LDDB >= max(1,M).
    
    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_lacpy
*******************************************************************************/
extern "C" void
magmablas_dlacpy_sym_out(
    magma_uplo_t uplo, magma_int_t m, magma_int_t n, 
    magma_int_t *rows, magma_int_t *perm,
    magmaDouble_const_ptr dA, magma_int_t ldda,
    magmaDouble_ptr       dB, magma_int_t lddb,
    magma_queue_t queue )
{
    magma_int_t info = 0;
    if ( uplo != MagmaLower && uplo != MagmaUpper && uplo != MagmaFull )
        info = -1;
    else if ( m < 0 )
        info = -2;
    else if ( n < 0 )
        info = -3;
    else if ( ldda < max(1,m))
        info = -5;
    else if ( lddb < max(1,m))
        info = -7;
    
    if ( info != 0 ) {
        magma_xerbla( __func__, -(info) );
        return;  //info;
    }
    
    if ( m == 0 || n == 0 ) {
        return;
    }
    
    dim3 threads( BLK_X, 1 );
    dim3 grid( magma_ceildiv(m, BLK_X), magma_ceildiv(n, BLK_Y) );
    
    if ( uplo == MagmaLower ) {
        dlacpy_sym_out_lower_kernel<<< grid, threads, 0, queue->cuda_stream() >>> ( m, n, rows, perm, dA, ldda, dB, lddb );
    }
    else if ( uplo == MagmaUpper ) {
        dlacpy_sym_out_upper_kernel<<< grid, threads, 0, queue->cuda_stream() >>> ( m, n, dA, ldda, dB, lddb );
    }
    else {
        dlacpy_sym_out_full_kernel <<< grid, threads, 0, queue->cuda_stream() >>> ( m, n, dA, ldda, dB, lddb );
    }
}
