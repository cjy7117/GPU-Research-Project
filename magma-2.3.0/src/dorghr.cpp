/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017

       @generated from src/zunghr.cpp, normal z -> d, Wed Nov 15 00:34:20 2017

*/
#include "magma_internal.h"

/***************************************************************************//**
    Purpose
    -------
    DORGHR generates a DOUBLE PRECISION orthogonal matrix Q which is defined as the
    product of IHI-ILO elementary reflectors of order N, as returned by
    DGEHRD:

    Q = H(ilo) H(ilo+1) . . . H(ihi-1).

    Arguments
    ---------
    @param[in]
    n       INTEGER
            The order of the matrix Q. N >= 0.

    @param[in]
    ilo     INTEGER
    @param[in]
    ihi     INTEGER
            ILO and IHI must have the same values as in the previous call
            of DGEHRD. Q is equal to the unit matrix except in the
            submatrix Q(ilo+1:ihi,ilo+1:ihi).
            1 <= ILO <= IHI <= N, if N > 0; ILO=1 and IHI=0, if N=0.

    @param[in,out]
    A       DOUBLE PRECISION array, dimension (LDA,N)
            On entry, the vectors which define the elementary reflectors,
            as returned by DGEHRD.
            On exit, the N-by-N orthogonal matrix Q.

    @param[in]
    lda     INTEGER
            The leading dimension of the array A. LDA >= max(1,N).

    @param[in]
    tau     DOUBLE PRECISION array, dimension (N-1)
            TAU(i) must contain the scalar factor of the elementary
            reflector H(i), as returned by DGEHRD.

    @param[in]
    dT      DOUBLE PRECISION array on the GPU device.
            DT contains the T matrices used in blocking the elementary
            reflectors H(i), e.g., this can be the 9th argument of
            magma_dgehrd.

    @param[in]
    nb      INTEGER
            This is the block size used in DGEHRD, and correspondingly
            the size of the T matrices, used in the factorization, and
            stored in DT.

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value

    @ingroup magma_unghr
*******************************************************************************/
extern "C" magma_int_t
magma_dorghr(
    magma_int_t n, magma_int_t ilo, magma_int_t ihi,
    double *A, magma_int_t lda,
    double *tau,
    magmaDouble_ptr dT, magma_int_t nb,
    magma_int_t *info)
{
    #define A(i,j) (A + (j)*lda+ (i))

    magma_int_t i, j, nh, iinfo;

    *info = 0;
    nh = ihi - ilo;
    if (n < 0)
        *info = -1;
    else if (ilo < 1 || ilo > max(1,n))
        *info = -2;
    else if (ihi < min(ilo,n) || ihi > n)
        *info = -3;
    else if (lda < max(1,n))
        *info = -5;

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    /* Quick return if possible */
    if (n == 0)
        return *info;

    /* Shift the vectors which define the elementary reflectors one
       column to the right, and set the first ilo and the last n-ihi
       rows and columns to those of the unit matrix */
    for (j = ihi-1; j >= ilo; --j) {
        for (i = 0; i < j; ++i)
            *A(i, j) = MAGMA_D_ZERO;
        
        for (i = j+1; i < ihi; ++i)
            *A(i, j) = *A(i, j - 1);
        
        for (i = ihi; i < n; ++i)
            *A(i, j) = MAGMA_D_ZERO;
    }
    for (j = 0; j < ilo; ++j) {
        for (i = 0; i < n; ++i)
            *A(i, j) = MAGMA_D_ZERO;
        
        *A(j, j) = MAGMA_D_ONE;
    }
    for (j = ihi; j < n; ++j) {
        for (i = 0; i < n; ++i)
            *A(i, j) = MAGMA_D_ZERO;
        
        *A(j, j) = MAGMA_D_ONE;
    }

    if (nh > 0) {
        /* Generate Q(ilo+1:ihi,ilo+1:ihi) */
        magma_dorgqr(nh, nh, nh,
                     A(ilo, ilo), lda,
                     tau+ilo-1, dT, nb, &iinfo);
    }
    
    return *info;
} /* magma_dorghr */

#undef A
