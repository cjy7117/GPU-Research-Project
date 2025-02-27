/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017

       @author Mark Gates
       @generated from control/magma_znan_inf.cpp, normal z -> s, Wed Nov 15 00:34:18 2017

*/
#include <limits>

#include "magma_internal.h"

#define REAL


const float MAGMA_S_NAN
    = MAGMA_S_MAKE( std::numeric_limits<float>::quiet_NaN(),
                    std::numeric_limits<float>::quiet_NaN() );

const float MAGMA_S_INF
    = MAGMA_S_MAKE( std::numeric_limits<float>::infinity(),
                    std::numeric_limits<float>::infinity() );


/***************************************************************************//**
    @param[in] x    Scalar to test.
    @return true if either real(x) or imag(x) is NAN.
    @ingroup magma_nan_inf
*******************************************************************************/
int magma_s_isnan( float x )
{
#ifdef COMPLEX
    return isnan( real( x )) ||
           isnan( imag( x ));
#else
    return isnan( x );
#endif
}


/***************************************************************************//**
    @param[in] x    Scalar to test.
    @return true if either real(x) or imag(x) is INF.
    @ingroup magma_nan_inf
*******************************************************************************/
int magma_s_isinf( float x )
{
#ifdef COMPLEX
    return isinf( real( x )) ||
           isinf( imag( x ));
#else
    return isinf( x );
#endif
}


/***************************************************************************//**
    @param[in] x    Scalar to test.
    @return true if either real(x) or imag(x) is NAN or INF.
    @ingroup magma_nan_inf
*******************************************************************************/
int magma_s_isnan_inf( float x )
{
#ifdef COMPLEX
    return isnan( real( x )) ||
           isnan( imag( x )) ||
           isinf( real( x )) ||
           isinf( imag( x ));
#else
    return isnan( x ) || isinf( x );
#endif
}


/***************************************************************************//**
    Purpose
    -------
    magma_snan_inf checks a matrix that is located on the CPU host
    for NAN (not-a-number) and INF (infinity) values.

    NAN is created by 0/0 and similar.
    INF is created by x/0 and similar, where x != 0.

    Arguments
    ---------
    @param[in]
    uplo    magma_uplo_t
            Specifies what part of the matrix A to check.
      -     = MagmaUpper:  Upper triangular part of A
      -     = MagmaLower:  Lower triangular part of A
      -     = MagmaFull:   All of A

    @param[in]
    m       INTEGER
            The number of rows of the matrix A. m >= 0.

    @param[in]
    n       INTEGER
            The number of columns of the matrix A. n >= 0.

    @param[in]
    A       REAL array, dimension (lda,n), on the CPU host.
            The m-by-n matrix to be printed.

    @param[in]
    lda     INTEGER
            The leading dimension of the array A. lda >= m.

    @param[out]
    cnt_nan INTEGER*
            If non-NULL, on exit contains the number of NAN values in A.

    @param[out]
    cnt_inf INTEGER*
            If non-NULL, on exit contains the number of INF values in A.

    @return
      -     >= 0:  Returns number of NAN + number of INF values.
      -     <  0:  If it returns -i, the i-th argument had an illegal value,
                   or another error occured, such as memory allocation failed.

    @ingroup magma_nan_inf
*******************************************************************************/
extern "C"
magma_int_t magma_snan_inf(
    magma_uplo_t uplo, magma_int_t m, magma_int_t n,
    const float *A, magma_int_t lda,
    magma_int_t *cnt_nan,
    magma_int_t *cnt_inf )
{
    #define A(i_, j_) (A + (i_) + (j_)*lda)
    
    magma_int_t info = 0;
    if (uplo != MagmaLower && uplo != MagmaUpper && uplo != MagmaFull)
        info = -1;
    else if (m < 0)
        info = -2;
    else if (n < 0)
        info = -3;
    else if (lda < m)
        info = -5;
    
    if (info != 0) {
        magma_xerbla( __func__, -(info) );
        return info;
    }
    
    int c_nan = 0;
    int c_inf = 0;
    
    if (uplo == MagmaLower) {
        for (int j = 0; j < n; ++j) {
            for (int i = j; i < m; ++i) {  // i >= j
                if      (magma_s_isnan( *A(i,j) )) { c_nan++; }
                else if (magma_s_isinf( *A(i,j) )) { c_inf++; }
            }
        }
    }
    else if (uplo == MagmaUpper) {
        for (int j = 0; j < n; ++j) {
            for (int i = 0; i < m && i <= j; ++i) {  // i <= j
                if      (magma_s_isnan( *A(i,j) )) { c_nan++; }
                else if (magma_s_isinf( *A(i,j) )) { c_inf++; }
            }
        }
    }
    else if (uplo == MagmaFull) {
        for (int j = 0; j < n; ++j) {
            for (int i = 0; i < m; ++i) {
                if      (magma_s_isnan( *A(i,j) )) { c_nan++; }
                else if (magma_s_isinf( *A(i,j) )) { c_inf++; }
            }
        }
    }
    
    if (cnt_nan != NULL) { *cnt_nan = c_nan; }
    if (cnt_inf != NULL) { *cnt_inf = c_inf; }
    
    return (c_nan + c_inf);
}


/***************************************************************************//**
    Purpose
    -------
    magma_snan_inf checks a matrix that is located on the CPU host
    for NAN (not-a-number) and INF (infinity) values.

    NAN is created by 0/0 and similar.
    INF is created by x/0 and similar, where x != 0.

    Arguments
    ---------
    @param[in]
    uplo    magma_uplo_t
            Specifies what part of the matrix A to check.
      -     = MagmaUpper:  Upper triangular part of A
      -     = MagmaLower:  Lower triangular part of A
      -     = MagmaFull:   All of A

    @param[in]
    m       INTEGER
            The number of rows of the matrix A. m >= 0.

    @param[in]
    n       INTEGER
            The number of columns of the matrix A. n >= 0.

    @param[in]
    dA      REAL array, dimension (ldda,n), on the GPU device.
            The m-by-n matrix to be printed.

    @param[in]
    ldda    INTEGER
            The leading dimension of the array A. ldda >= m.

    @param[out]
    cnt_nan INTEGER*
            If non-NULL, on exit contains the number of NAN values in A.

    @param[out]
    cnt_inf INTEGER*
            If non-NULL, on exit contains the number of INF values in A.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @return
      -     >= 0:  Returns number of NAN + number of INF values.
      -     <  0:  If it returns -i, the i-th argument had an illegal value,
                   or another error occured, such as memory allocation failed.

    @ingroup magma_nan_inf
*******************************************************************************/
extern "C"
magma_int_t magma_snan_inf_gpu(
    magma_uplo_t uplo, magma_int_t m, magma_int_t n,
    magmaFloat_const_ptr dA, magma_int_t ldda,
    magma_int_t *cnt_nan,
    magma_int_t *cnt_inf,
    magma_queue_t queue )
{
    magma_int_t info = 0;
    if (uplo != MagmaLower && uplo != MagmaUpper && uplo != MagmaFull)
        info = -1;
    else if (m < 0)
        info = -2;
    else if (n < 0)
        info = -3;
    else if (ldda < m)
        info = -5;
    
    if (info != 0) {
        magma_xerbla( __func__, -(info) );
        return info;
    }
    
    magma_int_t lda = m;
    float* A;
    magma_smalloc_cpu( &A, lda*n );

    magma_sgetmatrix( m, n, dA, ldda, A, lda, queue );
    
    magma_int_t cnt = magma_snan_inf( uplo, m, n, A, lda, cnt_nan, cnt_inf );
    
    magma_free_cpu( A );
    return cnt;
}
