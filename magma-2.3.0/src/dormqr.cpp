/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017

       @author Stan Tomov
       @author Mark Gates

       @generated from src/zunmqr.cpp, normal z -> d, Wed Nov 15 00:34:19 2017

*/
#include "magma_internal.h"

/***************************************************************************//**
    Purpose
    -------
    DORMQR overwrites the general real M-by-N matrix C with

    @verbatim
                              SIDE = MagmaLeft   SIDE = MagmaRight
    TRANS = MagmaNoTrans:     Q * C              C * Q
    TRANS = MagmaTrans:  Q**H * C           C * Q**H
    @endverbatim

    where Q is a real orthogonal matrix defined as the product of k
    elementary reflectors

        Q = H(1) H(2) . . . H(k)

    as returned by DGEQRF. Q is of order M if SIDE = MagmaLeft and of order N
    if SIDE = MagmaRight.

    Arguments
    ---------
    @param[in]
    side    magma_side_t
      -     = MagmaLeft:      apply Q or Q**H from the Left;
      -     = MagmaRight:     apply Q or Q**H from the Right.

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
    k       INTEGER
            The number of elementary reflectors whose product defines
            the matrix Q.
            If SIDE = MagmaLeft,  M >= K >= 0;
            if SIDE = MagmaRight, N >= K >= 0.

    @param[in]
    A       DOUBLE PRECISION array, dimension (LDA,K)
            The i-th column must contain the vector which defines the
            elementary reflector H(i), for i = 1,2,...,k, as returned by
            DGEQRF in the first k columns of its array argument A.
            A is modified by the routine but restored on exit.

    @param[in]
    lda     INTEGER
            The leading dimension of the array A.
            If SIDE = MagmaLeft,  LDA >= max(1,M);
            if SIDE = MagmaRight, LDA >= max(1,N).

    @param[in]
    tau     DOUBLE PRECISION array, dimension (K)
            TAU(i) must contain the scalar factor of the elementary
            reflector H(i), as returned by DGEQRF.

    @param[in,out]
    C       DOUBLE PRECISION array, dimension (LDC,N)
            On entry, the M-by-N matrix C.
            On exit, C is overwritten by Q*C or Q**H * C or C * Q**H or C*Q.

    @param[in]
    ldc     INTEGER
            The leading dimension of the array C. LDC >= max(1,M).

    @param[out]
    work    (workspace) DOUBLE PRECISION array, dimension (MAX(1,LWORK))
            On exit, if INFO = 0, WORK[0] returns the optimal LWORK.

    @param[in]
    lwork   INTEGER
            The dimension of the array WORK.
            If SIDE = MagmaLeft,  LWORK >= max(1,N);
            if SIDE = MagmaRight, LWORK >= max(1,M).
            For optimum performance
            if SIDE = MagmaLeft,  LWORK >= N*NB;
            if SIDE = MagmaRight, LWORK >= M*NB,
            where NB is the optimal blocksize.
    \n
            If LWORK = -1, then a workspace query is assumed; the routine
            only calculates the optimal size of the WORK array, returns
            this value as the first entry of the WORK array, and no error
            message related to LWORK is issued by XERBLA.

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value

    @ingroup magma_unmqr
*******************************************************************************/
extern "C" magma_int_t
magma_dormqr(
    magma_side_t side, magma_trans_t trans,
    magma_int_t m, magma_int_t n, magma_int_t k,
    double *A,    magma_int_t lda,
    double *tau,
    double *C,    magma_int_t ldc,
    double *work, magma_int_t lwork,
    magma_int_t *info)
{
    #define  A(i_,j_) ( A + (i_) + (j_)*lda)
    #define dC(i_,j_) (dC + (i_) + (j_)*lddc)
    #define dV(i_,j_) (dV + (i_) + (j_)*nq_i)
    #define dT(i_,j_) (dT + (i_) + (j_)*ib)
    #define dwork(i_) (dwork + (i_))
    
    double *T, *T2;
    magma_int_t i, i1, i2, ib, ic, jc, nb, mi, ni, nq, nq_i, nw, step;
    magma_int_t iinfo, ldwork, lwkopt;
    magma_int_t left, notran, lquery;

    *info = 0;
    left   = (side == MagmaLeft);
    notran = (trans == MagmaNoTrans);
    lquery = (lwork == -1);

    /* NQ is the order of Q and NW is the minimum dimension of WORK */
    if (left) {
        nq = m;
        nw = n;
    } else {
        nq = n;
        nw = m;
    }
    
    /* Test the input arguments */
    if (! left && side != MagmaRight) {
        *info = -1;
    } else if (! notran && trans != MagmaTrans) {
        *info = -2;
    } else if (m < 0) {
        *info = -3;
    } else if (n < 0) {
        *info = -4;
    } else if (k < 0 || k > nq) {
        *info = -5;
    } else if (lda < max(1,nq)) {
        *info = -7;
    } else if (ldc < max(1,m)) {
        *info = -10;
    } else if (lwork < max(1,nw) && ! lquery) {
        *info = -12;
    }

    if (*info == 0) {
        nb = magma_get_dgelqf_nb( m, n );
        lwkopt = max(1,nw)*nb;
        work[0] = magma_dmake_lwork( lwkopt );
    }

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }
    else if (lquery) {
        return *info;
    }

    /* Quick return if possible */
    if (m == 0 || n == 0 || k == 0) {
        work[0] = MAGMA_D_ONE;
        return *info;
    }

    ldwork = nw;

    if (nb >= k) {
        /* Use CPU code */
        lapackf77_dormqr( lapack_side_const(side), lapack_trans_const(trans),
            &m, &n, &k, A, &lda, tau, C, &ldc, work, &lwork, &iinfo);
    }
    else {
        /* Use hybrid CPU-GPU code */
        magma_queue_t queue;
        magma_device_t cdev;
        magma_getdevice( &cdev );
        magma_queue_create( cdev, &queue );
        
        /* Allocate work space on the GPU.
         * nw*nb  for dwork (m or n) by nb
         * nq*nb  for dV    (n or m) by nb
         * nb*nb  for dT
         * lddc*n for dC.
         */
        magma_int_t lddc = magma_roundup( m, 32 );
        magmaDouble_ptr dwork, dV, dT, dC;
        magma_dmalloc( &dwork, (nw + nq + nb)*nb + lddc*n );
        if ( dwork == NULL ) {
            *info = MAGMA_ERR_DEVICE_ALLOC;
            return *info;
        }
        dV = dwork + nw*nb;
        dT = dV    + nq*nb;
        dC = dT    + nb*nb;
        
        /* work space on CPU.
         * nb*nb for T
         * nb*nb for T2, used to save and restore diagonal block of panel */
        magma_dmalloc_cpu( &T, 2*nb*nb );
        if ( T == NULL ) {
            magma_free( dwork );
            *info = MAGMA_ERR_HOST_ALLOC;
            return *info;
        }
        T2 = T + nb*nb;
        
        /* Copy matrix C from the CPU to the GPU */
        magma_dsetmatrix( m, n, C, ldc, dC(0,0), lddc, queue );
        
        if ( (left && ! notran) ||  (! left && notran) ) {
            i1 = 0;
            i2 = k;
            step = nb;
        } else {
            i1 = ((k - 1) / nb) * nb;
            i2 = 0;
            step = -nb;
        }

        // silence "uninitialized" warnings
        mi = 0;
        ni = 0;
        
        if (left) {
            ni = n;
            jc = 0;
        } else {
            mi = m;
            ic = 0;
        }
        
        for (i = i1; (step < 0 ? i >= i2 : i < i2); i += step) {
            ib = min(nb, k - i);

            /* Form the triangular factor of the block reflector
               H = H(i) H(i+1) . . . H(i+ib-1) */
            nq_i = nq - i;
            lapackf77_dlarft( "Forward", "Columnwise", &nq_i, &ib,
                              A(i,i), &lda, &tau[i], T, &ib );

            /* 1) set upper triangle of panel in A to identity,
               2) copy the panel from A to the GPU, and
               3) restore A                                      */
            magma_dpanel_to_q( MagmaUpper, ib, A(i,i), lda, T2 );
            magma_dsetmatrix( nq_i,  ib, A(i,i), lda, dV(0,0), nq_i, queue );
            magma_dq_to_panel( MagmaUpper, ib, A(i,i), lda, T2 );

            if (left) {
                /* H or H**H is applied to C(i:m,1:n) */
                mi = m - i;
                ic = i;
            }
            else {
                /* H or H**H is applied to C(1:m,i:n) */
                ni = n - i;
                jc = i;
            }

            /* Apply H or H**H; First copy T to the GPU */
            magma_dsetmatrix( ib, ib, T, ib, dT(0,0), ib, queue );
            magma_dlarfb_gpu( side, trans, MagmaForward, MagmaColumnwise,
                              mi, ni, ib,
                              dV(0,0), nq_i,
                              dT(0,0), ib,
                              dC(ic,jc), lddc,
                              dwork(0), ldwork, queue );
        }
        magma_dgetmatrix( m, n, dC(0,0), lddc, C, ldc, queue );

        magma_queue_destroy( queue );
        
        magma_free( dwork );
        magma_free_cpu( T );
    }
    work[0] = magma_dmake_lwork( lwkopt );

    return *info;
} /* magma_dormqr */
