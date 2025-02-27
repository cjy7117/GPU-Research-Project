/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017

       @generated from magmablas/zlat2c.cu, mixed zc -> ds, Wed Nov 15 00:34:22 2017
       @author Mark Gates
*/
#include "magma_internal.h"

// mixed precision generation has issues with SINGLE PRECISION, so use PRECISION_z
#define PRECISION_d

#define BLK_X 64
#define BLK_Y 32

// TODO get rid of global variable!
static __device__ int flag = 0;


/*
    Divides matrix into ceil( n/BLK_X ) x ceil( n/BLK_Y ) blocks.
    Each block has BLK_X threads.
    Each thread loops across one row, updating BLK_Y entries.
    Updates only the diagonal and below.
    Blocks that are fully above the diagonal exit immediately.
    
    Code similar to dlag2s and zlaset.
*/
__global__
void dlat2s_lower(
    int n,
    const double *A, int lda,
    float *SA,       int ldsa,
    double rmax )
{
    double tmp;
    double neg_rmax = - rmax;
    
    int ind = blockIdx.x*BLK_X + threadIdx.x;
    int iby = blockIdx.y*BLK_Y;
    /* check if full block-column && (below diag) */
    bool full = (iby + BLK_Y <= n && (ind >= iby + BLK_Y));
    /* do only rows inside matrix, and blocks not above diag */
    if ( ind < n && ind + BLK_X > iby ) {
        A  += ind + iby*lda;
        SA += ind + iby*ldsa;
        if ( full ) {
            // full block-column, off-diagonal block
            #pragma unroll
            for( int j=0; j < BLK_Y; ++j ) {
                tmp = A[j*lda];
                if (   (MAGMA_D_REAL(tmp) < neg_rmax) || (MAGMA_D_REAL(tmp) > rmax)
                    #if defined(PRECISION_z) || defined(PRECISION_c)
                    || (MAGMA_D_IMAG(tmp) < neg_rmax) || (MAGMA_D_IMAG(tmp) > rmax)
                    #endif
                    )
                {
                    flag = 1;
                }
                SA[j*ldsa] = MAGMA_S_MAKE( MAGMA_D_REAL(tmp),
                                           MAGMA_D_IMAG(tmp) );
            }
        }
        else {
            // either partial block-column or diagonal block
            for( int j=0; j < BLK_Y && iby+j < n && ind >= iby+j; ++j ) {
                tmp = A[j*lda];
                if (   (MAGMA_D_REAL(tmp) < neg_rmax) || (MAGMA_D_REAL(tmp) > rmax)
                    #if defined(PRECISION_z) || defined(PRECISION_c)
                    || (MAGMA_D_IMAG(tmp) < neg_rmax) || (MAGMA_D_IMAG(tmp) > rmax)
                    #endif
                    )
                {
                    flag = 1;
                }
                SA[j*ldsa] = MAGMA_S_MAKE( MAGMA_D_REAL(tmp),
                                           MAGMA_D_IMAG(tmp) );
            }
        }
    }
}


/*
    Similar to dlat2s_full, but updates only the diagonal and above.
    Blocks that are fully below the diagonal exit immediately.
    
    Code similar to dlag2s and zlaset.
*/
__global__
void dlat2s_upper(
    int n,
    const double *A, int lda,
    float *SA,       int ldsa,
    double rmax )
{
    double tmp;
    double neg_rmax = - rmax;
    
    int ind = blockIdx.x*BLK_X + threadIdx.x;
    int iby = blockIdx.y*BLK_Y;
    /* check if full block-column && (above diag) */
    bool full = (iby + BLK_Y <= n && (ind + BLK_X <= iby));
    /* do only rows inside matrix, and blocks not below diag */
    if ( ind < n && ind < iby + BLK_Y ) {
        A  += ind + iby*lda;
        SA += ind + iby*ldsa;
        if ( full ) {
            // full block-column, off-diagonal block
            #pragma unroll
            for( int j=0; j < BLK_Y; ++j ) {
                tmp = A[j*lda];
                if (   (MAGMA_D_REAL(tmp) < neg_rmax) || (MAGMA_D_REAL(tmp) > rmax)
                    #if defined(PRECISION_z) || defined(PRECISION_c)
                    || (MAGMA_D_IMAG(tmp) < neg_rmax) || (MAGMA_D_IMAG(tmp) > rmax)
                    #endif
                    )
                {
                    flag = 1;
                }
                SA[j*ldsa] = MAGMA_S_MAKE( MAGMA_D_REAL(tmp),
                                           MAGMA_D_IMAG(tmp) );
            }
        }
        else {
            // either partial block-column or diagonal block
            for( int j=0; j < BLK_Y && iby+j < n; ++j ) {
                if ( ind <= iby+j ) {
                    tmp = A[j*lda];
                    if (   (MAGMA_D_REAL(tmp) < neg_rmax) || (MAGMA_D_REAL(tmp) > rmax)
                         #if defined(PRECISION_z) || defined(PRECISION_c)
                         || (MAGMA_D_IMAG(tmp) < neg_rmax) || (MAGMA_D_IMAG(tmp) > rmax)
                         #endif
                        )
                    {
                        flag = 1;
                    }
                    SA[j*ldsa] = MAGMA_S_MAKE( MAGMA_D_REAL(tmp),
                                               MAGMA_D_IMAG(tmp) );
                }
            }
        }
    }
}


/***************************************************************************//**
    Purpose
    -------
    DLAT2S converts a double-real matrix, A,
                 to a single-real matrix, SA.
    
    RMAX is the overflow for the single-real arithmetic.
    DLAT2S checks that all the entries of A are between -RMAX and
    RMAX. If not, the conversion is aborted and a flag is raised.
        
    Arguments
    ---------
    @param[in]
    uplo    magma_uplo_t
            Specifies the part of the matrix A to be converted.
      -     = MagmaUpper:      Upper triangular part
      -     = MagmaLower:      Lower triangular part
    
    @param[in]
    n       INTEGER
            The number of columns of the matrix A.  n >= 0.
    
    @param[in]
    A       DOUBLE PRECISION array, dimension (LDA,n)
            On entry, the n-by-n coefficient matrix A.
    
    @param[in]
    lda     INTEGER
            The leading dimension of the array A.  LDA >= max(1,n).
    
    @param[out]
    SA      SINGLE PRECISION array, dimension (LDSA,n)
            On exit, if INFO=0, the n-by-n coefficient matrix SA;
            if INFO > 0, the content of SA is unspecified.
    
    @param[in]
    ldsa    INTEGER
            The leading dimension of the array SA.  LDSA >= max(1,n).
    
    @param[out]
    info    INTEGER
      -     = 0:  successful exit.
      -     < 0:  if INFO = -i, the i-th argument had an illegal value
      -     = 1:  an entry of the matrix A is greater than the SINGLE PRECISION
                  overflow threshold, in this case, the content
                  of SA on exit is unspecified.
    
    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_lat2
*******************************************************************************/
extern "C" void
magmablas_dlat2s(
    magma_uplo_t uplo, magma_int_t n,
    magmaDouble_const_ptr  A, magma_int_t lda,
    magmaFloat_ptr        SA, magma_int_t ldsa,
    magma_queue_t queue,
    magma_int_t *info )
{
    *info = 0;
    if ( uplo != MagmaLower && uplo != MagmaUpper )
        *info = -1;
    else if ( n < 0 )
        *info = -2;
    else if ( lda < max(1,n) )
        *info = -4;
    else if ( ldsa < max(1,n) )
        *info = -6;
    
    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return; //*info;
    }

    /* quick return */
    if ( n == 0 ) {
        return;
    }
    
    double rmax = (double)lapackf77_slamch("O");

    dim3 threads( BLK_X, 1 );
    dim3    grid( magma_ceildiv( n, BLK_X ), magma_ceildiv( n, BLK_Y ) );
    cudaMemcpyToSymbol( flag, info, sizeof(flag) );    // flag = 0
    
    if (uplo == MagmaLower) {
        dlat2s_lower<<< grid, threads, 0, queue->cuda_stream() >>> (n, A, lda, SA, ldsa, rmax);
    }
    else if (uplo == MagmaUpper) {
        dlat2s_upper<<< grid, threads, 0, queue->cuda_stream() >>> (n, A, lda, SA, ldsa, rmax);
    }
    
    cudaMemcpyFromSymbol( info, flag, sizeof(flag) );  // info = flag
}
