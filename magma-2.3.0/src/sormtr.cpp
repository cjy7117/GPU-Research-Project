/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017

       @author Stan Tomov
       @author Raffaele Solca

       @generated from src/zunmtr.cpp, normal z -> s, Wed Nov 15 00:34:19 2017

*/
#include "magma_internal.h"

/***************************************************************************//**
    Purpose
    -------
    SORMTR overwrites the general real M-by-N matrix C with

                                SIDE = MagmaLeft    SIDE = MagmaRight
    TRANS = MagmaNoTrans:       Q * C               C * Q
    TRANS = MagmaTrans:    Q**H * C            C * Q**H

    where Q is a real orthogonal matrix of order nq,
    with nq = m if SIDE = MagmaLeft
    and  nq = n if SIDE = MagmaRight. Q is defined as the product of
    nq-1 elementary reflectors, as returned by SSYTRD:

    if UPLO = MagmaUpper, Q = H(nq-1) . . . H(2) H(1);

    if UPLO = MagmaLower, Q = H(1) H(2) . . . H(nq-1).

    Arguments
    ---------
    @param[in]
    side    magma_side_t
      -     = MagmaLeft:      apply Q or Q**H from the Left;
      -     = MagmaRight:     apply Q or Q**H from the Right.

    @param[in]
    uplo    magma_uplo_t
      -     = MagmaUpper: Upper triangle of A contains elementary reflectors
                   from SSYTRD;
      -     = MagmaLower: Lower triangle of A contains elementary reflectors
                   from SSYTRD.

    @param[in]
    trans   magma_trans_t
      -     = MagmaNoTrans:    No transpose, apply Q;
      -     = MagmaTrans: Conjugate transpose, apply Q**H.

    @param[in]
    m       INTEGER
            The number of rows of the matrix C. M >= 0.

    @param[in]
    n       INTEGER
            The number of columns of the matrix C. N >= 0.

    @param[in]
    A       REAL array, dimension
                                 (LDA,M) if SIDE = MagmaLeft
                                 (LDA,N) if SIDE = MagmaRight
            The vectors which define the elementary reflectors, as
            returned by SSYTRD.

    @param[in]
    lda     INTEGER
            The leading dimension of the array A.
            LDA >= max(1,M) if SIDE = MagmaLeft;
            LDA >= max(1,N) if SIDE = MagmaRight.

    @param[in]
    tau     REAL array, dimension
                                 (M-1) if SIDE = MagmaLeft
                                 (N-1) if SIDE = MagmaRight
            TAU(i) must contain the scalar factor of the elementary
            reflector H(i), as returned by SSYTRD.

    @param[in,out]
    C       REAL array, dimension (LDC,N)
            On entry, the M-by-N matrix C.
            On exit, C is overwritten by Q*C or Q**H * C or C * Q**H or C*Q.

    @param[in]
    ldc     INTEGER
            The leading dimension of the array C. LDC >= max(1,M).

    @param[out]
    work    (workspace) REAL array, dimension (MAX(1,LWORK))
            On exit, if INFO = 0, WORK[0] returns the optimal LWORK.

    @param[in]
    lwork   INTEGER
            The dimension of the array WORK.
            If SIDE = MagmaLeft,  LWORK >= max(1,N);
            if SIDE = MagmaRight, LWORK >= max(1,M).
            For optimum performance LWORK >= N*NB if SIDE = MagmaLeft, and
            LWORK >= M*NB if SIDE = MagmaRight, where NB is the optimal
            blocksize.
    \n
            If LWORK = -1, then a workspace query is assumed; the routine
            only calculates the optimal size of the WORK array, returns
            this value as the first entry of the WORK array, and no error
            message related to LWORK is issued.

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value

    @ingroup magma_unmtr
*******************************************************************************/
extern "C" magma_int_t
magma_sormtr(
    magma_side_t side, magma_uplo_t uplo, magma_trans_t trans,
    magma_int_t m, magma_int_t n,
    float *A,    magma_int_t lda,
    float *tau,
    float *C,    magma_int_t ldc,
    float *work, magma_int_t lwork,
    magma_int_t *info)
{
    #define A(i_,j_) (A + (i_) + (j_)*lda)
    #define C(i_,j_) (C + (i_) + (j_)*ldc)
    
    float c_one = MAGMA_S_ONE;

    magma_int_t  i__2;
    magma_int_t i1, i2, nb, mi, ni, nq, nw;
    magma_int_t iinfo;
    magma_int_t lwkopt;

    *info = 0;
    bool left   = (side == MagmaLeft);
    bool upper  = (uplo == MagmaUpper);
    bool lquery = (lwork == -1);

    /* NQ is the order of Q and NW is the minimum dimension of WORK */
    if (left) {
        nq = m;
        nw = n;
    } else {
        nq = n;
        nw = m;
    }
    if (! left && side != MagmaRight) {
        *info = -1;
    } else if (! upper && uplo != MagmaLower) {
        *info = -2;
    } else if (trans != MagmaNoTrans &&
               trans != MagmaTrans) {
        *info = -3;
    } else if (m < 0) {
        *info = -4;
    } else if (n < 0) {
        *info = -5;
    } else if (lda < max(1,nq)) {
        *info = -7;
    } else if (ldc < max(1,m)) {
        *info = -10;
    } else if (lwork < max(1,nw) && ! lquery) {
        *info = -12;
    }

    nb = 32;
    lwkopt = max(1,nw) * nb;
    if (*info == 0) {
        work[0] = magma_smake_lwork( lwkopt );
    }

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }
    else if (lquery) {
        return *info;
    }

    /* Quick return if possible */
    if (m == 0 || n == 0 || nq == 1) {
        work[0] = c_one;
        return *info;
    }

    if (left) {
        mi = m - 1;
        ni = n;
    } else {
        mi = m;
        ni = n - 1;
    }

    if (upper) {
        /* Q was determined by a call to SSYTRD with UPLO = MagmaUpper */
        i__2 = nq - 1;
        //lapackf77_sormql(side_, trans_, &mi, &ni, &i__2, A(0,1), &lda,
        //                 tau, C, &ldc, work, &lwork, &iinfo);
        magma_sormql(side, trans, mi, ni, i__2, A(0,1), lda, tau,
                     C, ldc, work, lwork, &iinfo);
    }
    else {
        /* Q was determined by a call to SSYTRD with UPLO = MagmaLower */
        if (left) {
            i1 = 1;
            i2 = 0;
        } else {
            i1 = 0;
            i2 = 1;
        }
        i__2 = nq - 1;
        magma_sormqr(side, trans, mi, ni, i__2, A(1,0), lda, tau,
                     C(i1,i2), ldc, work, lwork, &iinfo);
    }

    work[0] = magma_smake_lwork( lwkopt );

    return *info;
} /* magma_sormtr */
