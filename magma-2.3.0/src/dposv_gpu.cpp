/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017

       @generated from src/zposv_gpu.cpp, normal z -> d, Wed Nov 15 00:34:18 2017
*/
#include "magma_internal.h"

/***************************************************************************//**
    Purpose
    -------
    DPOSV computes the solution to a real system of linear equations
       A * X = B,
    where A is an N-by-N symmetric positive definite matrix and X and B
    are N-by-NRHS matrices.
    The Cholesky decomposition is used to factor A as
       A = U**H * U,  if UPLO = MagmaUpper, or
       A = L * L**H,  if UPLO = MagmaLower,
    where U is an upper triangular matrix and  L is a lower triangular
    matrix.  The factored form of A is then used to solve the system of
    equations A * X = B.

    Arguments
    ---------
    @param[in]
    uplo    magma_uplo_t
      -     = MagmaUpper:  Upper triangle of A is stored;
      -     = MagmaLower:  Lower triangle of A is stored.

    @param[in]
    n       INTEGER
            The order of the matrix A.  N >= 0.

    @param[in]
    nrhs    INTEGER
            The number of right hand sides, i.e., the number of columns
            of the matrix B.  NRHS >= 0.

    @param[in,out]
    dA      DOUBLE PRECISION array on the GPU, dimension (LDDA,N)
            On entry, the symmetric matrix dA.  If UPLO = MagmaUpper, the leading
            N-by-N upper triangular part of dA contains the upper
            triangular part of the matrix dA, and the strictly lower
            triangular part of dA is not referenced.  If UPLO = MagmaLower, the
            leading N-by-N lower triangular part of dA contains the lower
            triangular part of the matrix dA, and the strictly upper
            triangular part of dA is not referenced.
    \n
            On exit, if INFO = 0, the factor U or L from the Cholesky
            factorization dA = U**H*U or dA = L*L**H.

    @param[in]
    ldda    INTEGER
            The leading dimension of the array A.  LDDA >= max(1,N).

    @param[in,out]
    dB      DOUBLE PRECISION array on the GPU, dimension (LDDB,NRHS)
            On entry, the right hand side matrix B.
            On exit, the solution matrix X.

    @param[in]
    lddb    INTEGER
            The leading dimension of the array B.  LDDB >= max(1,N).

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value

    @ingroup magma_posv
*******************************************************************************/
extern "C" magma_int_t
magma_dposv_gpu(
    magma_uplo_t uplo, magma_int_t n, magma_int_t nrhs,
    magmaDouble_ptr dA, magma_int_t ldda,
    magmaDouble_ptr dB, magma_int_t lddb,
    magma_int_t *info )
{
    *info = 0;
    if ( uplo != MagmaUpper && uplo != MagmaLower )
        *info = -1;
    if ( n < 0 )
        *info = -2;
    if ( nrhs < 0 )
        *info = -3;
    if ( ldda < max(1, n) )
        *info = -5;
    if ( lddb < max(1, n) )
        *info = -7;
    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    /* Quick return if possible */
    if ( (n == 0) || (nrhs == 0) ) {
        return *info;
    }

    magma_dpotrf_gpu( uplo, n, dA, ldda, info );
    if ( *info == 0 ) {
        magma_dpotrs_gpu( uplo, n, nrhs, dA, ldda, dB, lddb, info );
    }

    return *info;
}
