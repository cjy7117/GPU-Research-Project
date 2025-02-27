/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017

       @precisions normal z -> c
       @author Stan Tomov
       @author Mark Gates
*/
#include "magma_internal.h"
#include "magma_timer.h"

#define COMPLEX

/*
 * Version1 - LAPACK              (lapack_zgehrd and lapack_zunghr)
 * Version2 - MAGMA without dT    (magma_zgehrd2 and lapack_zunghr)
 * Version3 - MAGMA with dT       (magma_zgehrd  and magma_zunghr)
 */
#define VERSION3

/*
 * TREVC version 1 - LAPACK
 * TREVC version 2 - new blocked LAPACK
 * TREVC version 3 - blocked, single-threaded (MAGMA)
 * TREVC version 4 - blocked, multi-threaded  (MAGMA)
 * TREVC version 5 - blocked, multi-threaded, GPU (MAGMA)
 */
#define TREVC_VERSION 4

/***************************************************************************//**
    Purpose
    -------
    ZGEEV computes for an N-by-N complex nonsymmetric matrix A, the
    eigenvalues and, optionally, the left and/or right eigenvectors.

    The right eigenvector v(j) of A satisfies
        A * v(j) = lambda(j) * v(j)
    where lambda(j) is its eigenvalue.
    The left eigenvector u(j) of A satisfies
        u(j)**H * A = lambda(j) * u(j)**H
    where u(j)**H denotes the conjugate transpose of u(j).

    The computed eigenvectors are normalized to have Euclidean norm
    equal to 1 and largest component real.

    Arguments
    ---------
    @param[in]
    jobvl   magma_vec_t
      -     = MagmaNoVec:        left eigenvectors of A are not computed;
      -     = MagmaVec:          left eigenvectors of are computed.

    @param[in]
    jobvr   magma_vec_t
      -     = MagmaNoVec:        right eigenvectors of A are not computed;
      -     = MagmaVec:          right eigenvectors of A are computed.

    @param[in]
    n       INTEGER
            The order of the matrix A. N >= 0.

    @param[in,out]
    A       COMPLEX_16 array, dimension (LDA,N)
            On entry, the N-by-N matrix A.
            On exit, A has been overwritten.

    @param[in]
    lda     INTEGER
            The leading dimension of the array A.  LDA >= max(1,N).

    @param[out]
    w       COMPLEX_16 array, dimension (N)
            w contains the computed eigenvalues.

    @param[out]
    VL      COMPLEX_16 array, dimension (LDVL,N)
            If JOBVL = MagmaVec, the left eigenvectors u(j) are stored one
            after another in the columns of VL, in the same order
            as their eigenvalues.
            If JOBVL = MagmaNoVec, VL is not referenced.
            u(j) = VL(:,j), the j-th column of VL.

    @param[in]
    ldvl    INTEGER
            The leading dimension of the array VL.  LDVL >= 1; if
            JOBVL = MagmaVec, LDVL >= N.

    @param[out]
    VR      COMPLEX_16 array, dimension (LDVR,N)
            If JOBVR = MagmaVec, the right eigenvectors v(j) are stored one
            after another in the columns of VR, in the same order
            as their eigenvalues.
            If JOBVR = MagmaNoVec, VR is not referenced.
            v(j) = VR(:,j), the j-th column of VR.

    @param[in]
    ldvr    INTEGER
            The leading dimension of the array VR.  LDVR >= 1; if
            JOBVR = MagmaVec, LDVR >= N.

    @param[out]
    work    (workspace) COMPLEX_16 array, dimension (MAX(1,LWORK))
            On exit, if INFO = 0, WORK[0] returns the optimal LWORK.

    @param[in]
    lwork   INTEGER
            The dimension of the array WORK.  LWORK >= (1 +   nb)*N.
            For optimal performance,          LWORK >= (1 + 2*nb)*N.
    \n
            If LWORK = -1, then a workspace query is assumed; the routine
            only calculates the optimal size of the WORK array, returns
            this value as the first entry of the WORK array, and no error
            message related to LWORK is issued by XERBLA.

    @param
    rwork   (workspace) DOUBLE PRECISION array, dimension (2*N)

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value.
      -     > 0:  if INFO = i, the QR algorithm failed to compute all the
                  eigenvalues, and no eigenvectors have been computed;
                  elements and i+1:N of w contain eigenvalues which have
                  converged.

    @ingroup magma_geev
*******************************************************************************/
extern "C" magma_int_t
magma_zgeev(
    magma_vec_t jobvl, magma_vec_t jobvr, magma_int_t n,
    magmaDoubleComplex *A, magma_int_t lda,
    #ifdef COMPLEX
    magmaDoubleComplex *w,
    #else
    double *wr, double *wi,
    #endif
    magmaDoubleComplex *VL, magma_int_t ldvl,
    magmaDoubleComplex *VR, magma_int_t ldvr,
    magmaDoubleComplex *work, magma_int_t lwork,
    #ifdef COMPLEX
    double *rwork,
    #endif
    magma_int_t *info )
{
    #define VL(i,j)  (VL + (i) + (j)*ldvl)
    #define VR(i,j)  (VR + (i) + (j)*ldvr)
    
    const magma_int_t ione  = 1;
    const magma_int_t izero = 0;
    
    double d__1, d__2;
    magmaDoubleComplex tmp;
    double scl;
    double dum[1], eps;
    double anrm, cscale, bignum, smlnum;
    magma_int_t i, k, ilo, ihi;
    magma_int_t ibal, ierr, itau, iwrk, nout, liwrk, nb;
    magma_int_t scalea, minwrk, optwrk, irwork, lquery, wantvl, wantvr, select[1];

    magma_side_t side = MagmaRight;

    magma_timer_t time_total=0, time_gehrd=0, time_unghr=0, time_hseqr=0, time_trevc=0, time_sum=0;
    magma_flops_t flop_total=0, flop_gehrd=0, flop_unghr=0, flop_hseqr=0, flop_trevc=0, flop_sum=0;
    timer_start( time_total );
    flops_start( flop_total );
    
    irwork = 0;
    *info = 0;
    lquery = (lwork == -1);
    wantvl = (jobvl == MagmaVec);
    wantvr = (jobvr == MagmaVec);
    if (! wantvl && jobvl != MagmaNoVec) {
        *info = -1;
    } else if (! wantvr && jobvr != MagmaNoVec) {
        *info = -2;
    } else if (n < 0) {
        *info = -3;
    } else if (lda < max(1,n)) {
        *info = -5;
    } else if ( (ldvl < 1) || (wantvl && (ldvl < n))) {
        *info = -8;
    } else if ( (ldvr < 1) || (wantvr && (ldvr < n))) {
        *info = -10;
    }

    /* Compute workspace */
    nb = magma_get_zgehrd_nb( n );
    if (*info == 0) {
        minwrk = (1 +   nb)*n;
        optwrk = (1 + 2*nb)*n;
        work[0] = magma_zmake_lwork( optwrk );

        if (lwork < minwrk && ! lquery) {
            *info = -12;
        }
    }

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }
    else if (lquery) {
        return *info;
    }

    /* Quick return if possible */
    if (n == 0) {
        return *info;
    }
    
    #if defined(VERSION3)
    magmaDoubleComplex_ptr dT;
    if (MAGMA_SUCCESS != magma_zmalloc( &dT, nb*n )) {
        *info = MAGMA_ERR_DEVICE_ALLOC;
        return *info;
    }
    #endif

    /* Get machine constants */
    eps    = lapackf77_dlamch( "P" );
    smlnum = lapackf77_dlamch( "S" );
    bignum = 1. / smlnum;
    lapackf77_dlabad( &smlnum, &bignum );
    smlnum = magma_dsqrt( smlnum ) / eps;
    bignum = 1. / smlnum;

    /* Scale A if max element outside range [SMLNUM,BIGNUM] */
    anrm = lapackf77_zlange( "M", &n, &n, A, &lda, dum );
    scalea = 0;
    if (anrm > 0. && anrm < smlnum) {
        scalea = 1;
        cscale = smlnum;
    } else if (anrm > bignum) {
        scalea = 1;
        cscale = bignum;
    }
    if (scalea) {
        lapackf77_zlascl( "G", &izero, &izero, &anrm, &cscale, &n, &n, A, &lda, &ierr );
    }

    /* Balance the matrix
     * (CWorkspace: none)
     * (RWorkspace: need N)
     *  - this space is reserved until after gebak */
    ibal = 0;
    lapackf77_zgebal( "B", &n, A, &lda, &ilo, &ihi, &rwork[ibal], &ierr );

    /* Reduce to upper Hessenberg form
     * (CWorkspace: need 2*N, prefer N + N*NB)
     * (RWorkspace: N)
     *  - including N reserved for gebal/gebak, unused by zgehrd */
    itau = 0;
    iwrk = itau + n;
    liwrk = lwork - iwrk;

    timer_start( time_gehrd );
    flops_start( flop_gehrd );
    #if defined(VERSION1)
        // Version 1 - LAPACK
        lapackf77_zgehrd( &n, &ilo, &ihi, A, &lda,
                          &work[itau], &work[iwrk], &liwrk, &ierr );
    #elif defined(VERSION2)
        // Version 2 - LAPACK consistent HRD
        magma_zgehrd2( n, ilo, ihi, A, lda,
                       &work[itau], &work[iwrk], liwrk, &ierr );
    #elif defined(VERSION3)
        // Version 3 - LAPACK consistent MAGMA HRD + T matrices stored,
        magma_zgehrd( n, ilo, ihi, A, lda,
                      &work[itau], &work[iwrk], liwrk, dT, &ierr );
    #endif
    time_sum += timer_stop( time_gehrd );
    flop_sum += flops_stop( flop_gehrd );

    if (wantvl) {
        /* Want left eigenvectors
         * Copy Householder vectors to VL */
        side = MagmaLeft;
        lapackf77_zlacpy( MagmaLowerStr, &n, &n, A, &lda, VL, &ldvl );

        /* Generate unitary matrix in VL
         * (CWorkspace: need 2*N-1, prefer N + (N-1)*NB)
         * (RWorkspace: N)
         *  - including N reserved for gebal/gebak, unused by zunghr */
        timer_start( time_unghr );
        flops_start( flop_unghr );
        #if defined(VERSION1) || defined(VERSION2)
            // Version 1 & 2 - LAPACK
            lapackf77_zunghr( &n, &ilo, &ihi, VL, &ldvl, &work[itau],
                              &work[iwrk], &liwrk, &ierr );
        #elif defined(VERSION3)
            // Version 3 - LAPACK consistent MAGMA HRD + T matrices stored
            magma_zunghr( n, ilo, ihi, VL, ldvl, &work[itau], dT, nb, &ierr );
        #endif
        time_sum += timer_stop( time_unghr );
        flop_sum += flops_stop( flop_unghr );
        
        timer_start( time_hseqr );
        flops_start( flop_hseqr );
        /* Perform QR iteration, accumulating Schur vectors in VL
         * (CWorkspace: need 1, prefer HSWORK (see comments) )
         * (RWorkspace: N)
         *  - including N reserved for gebal/gebak, unused by zhseqr */
        iwrk = itau;
        liwrk = lwork - iwrk;
        lapackf77_zhseqr( "S", "V", &n, &ilo, &ihi, A, &lda, w,
                          VL, &ldvl, &work[iwrk], &liwrk, info );
        time_sum += timer_stop( time_hseqr );
        flop_sum += flops_stop( flop_hseqr );

        if (wantvr) {
            /* Want left and right eigenvectors
             * Copy Schur vectors to VR */
            side = MagmaBothSides;
            lapackf77_zlacpy( "F", &n, &n, VL, &ldvl, VR, &ldvr );
        }
    }
    else if (wantvr) {
        /* Want right eigenvectors
         * Copy Householder vectors to VR */
        side = MagmaRight;
        lapackf77_zlacpy( "L", &n, &n, A, &lda, VR, &ldvr );

        /* Generate unitary matrix in VR
         * (CWorkspace: need 2*N-1, prefer N + (N-1)*NB)
         * (RWorkspace: N)
         *  - including N reserved for gebal/gebak, unused by zunghr */
        timer_start( time_unghr );
        flops_start( flop_unghr );
        #if defined(VERSION1) || defined(VERSION2)
            // Version 1 & 2 - LAPACK
            lapackf77_zunghr( &n, &ilo, &ihi, VR, &ldvr, &work[itau],
                              &work[iwrk], &liwrk, &ierr );
        #elif defined(VERSION3)
            // Version 3 - LAPACK consistent MAGMA HRD + T matrices stored
            magma_zunghr( n, ilo, ihi, VR, ldvr, &work[itau], dT, nb, &ierr );
        #endif
        time_sum += timer_stop( time_unghr );
        flop_sum += flops_stop( flop_unghr );

        /* Perform QR iteration, accumulating Schur vectors in VR
         * (CWorkspace: need 1, prefer HSWORK (see comments) )
         * (RWorkspace: N)
         *  - including N reserved for gebal/gebak, unused by zhseqr */
        timer_start( time_hseqr );
        flops_start( flop_hseqr );
        iwrk = itau;
        liwrk = lwork - iwrk;
        lapackf77_zhseqr( "S", "V", &n, &ilo, &ihi, A, &lda, w,
                          VR, &ldvr, &work[iwrk], &liwrk, info );
        time_sum += timer_stop( time_hseqr );
        flop_sum += flops_stop( flop_hseqr );
    }
    else {
        /* Compute eigenvalues only
         * (CWorkspace: need 1, prefer HSWORK (see comments) )
         * (RWorkspace: N)
         *  - including N reserved for gebal/gebak, unused by zhseqr */
        timer_start( time_hseqr );
        flops_start( flop_hseqr );
        iwrk = itau;
        liwrk = lwork - iwrk;
        lapackf77_zhseqr( "E", "N", &n, &ilo, &ihi, A, &lda, w,
                          VR, &ldvr, &work[iwrk], &liwrk, info );
        time_sum += timer_stop( time_hseqr );
        flop_sum += flops_stop( flop_hseqr );
    }

    /* If INFO > 0 from ZHSEQR, then quit */
    if (*info > 0) {
        goto CLEANUP;
    }

    timer_start( time_trevc );
    flops_start( flop_trevc );
    if (wantvl || wantvr) {
        /* Compute left and/or right eigenvectors
         * (CWorkspace: need 2*N)
         * (RWorkspace: need 2*N)
         *  - including N reserved for gebal/gebak, unused by ztrevc */
        irwork = ibal + n;
        #if TREVC_VERSION == 1
        lapackf77_ztrevc( lapack_side_const(side), "B", select, &n, A, &lda, VL, &ldvl,
                          VR, &ldvr, &n, &nout, &work[iwrk], &rwork[irwork], &ierr );
        #elif TREVC_VERSION == 2
        liwrk = lwork - iwrk;
        lapackf77_ztrevc3( lapack_side_const(side), "B", select, &n, A, &lda, VL, &ldvl,
                           VR, &ldvr, &n, &nout, &work[iwrk], &liwrk, &rwork[irwork], &ierr );
        #elif TREVC_VERSION == 3
        magma_ztrevc3( side, MagmaBacktransVec, select, n, A, lda, VL, ldvl,
                       VR, ldvr, n, &nout, &work[iwrk], liwrk, &rwork[irwork], &ierr );
        #elif TREVC_VERSION == 4
        magma_ztrevc3_mt( side, MagmaBacktransVec, select, n, A, lda, VL, ldvl,
                          VR, ldvr, n, &nout, &work[iwrk], liwrk, &rwork[irwork], &ierr );
        #elif TREVC_VERSION == 5
        magma_ztrevc3_mt_gpu( side, MagmaBacktransVec, select, n, A, lda, VL, ldvl,
                              VR, ldvr, n, &nout, &work[iwrk], liwrk, &rwork[irwork], &ierr );
        #else
        #error Unknown TREVC_VERSION
        #endif
    }
    time_sum += timer_stop( time_trevc );
    flop_sum += flops_stop( flop_trevc );

    if (wantvl) {
        /* Undo balancing of left eigenvectors
         * (CWorkspace: none)
         * (RWorkspace: need N) */
        lapackf77_zgebak( "B", "L", &n, &ilo, &ihi, &rwork[ibal], &n,
                          VL, &ldvl, &ierr );

        /* Normalize left eigenvectors and make largest component real */
        for (i = 0; i < n; ++i) {
            scl = 1. / magma_cblas_dznrm2( n, VL(0,i), 1 );
            blasf77_zdscal( &n, &scl, VL(0,i), &ione );
            for (k = 0; k < n; ++k) {
                /* Computing 2nd power */
                d__1 = MAGMA_Z_REAL( *VL(k,i) );
                d__2 = MAGMA_Z_IMAG( *VL(k,i) );
                rwork[irwork + k] = d__1*d__1 + d__2*d__2;
            }
            k = blasf77_idamax( &n, &rwork[irwork], &ione ) - 1;  // subtract 1; k is 0-based
            tmp = MAGMA_Z_CONJ( *VL(k,i) ) / magma_dsqrt( rwork[irwork + k] );
            blasf77_zscal( &n, &tmp, VL(0,i), &ione );
            *VL(k,i) = MAGMA_Z_MAKE( MAGMA_Z_REAL( *VL(k,i) ), 0 );
        }
    }

    if (wantvr) {
        /* Undo balancing of right eigenvectors
         * (CWorkspace: none)
         * (RWorkspace: need N) */
        lapackf77_zgebak( "B", "R", &n, &ilo, &ihi, &rwork[ibal], &n,
                          VR, &ldvr, &ierr );

        /* Normalize right eigenvectors and make largest component real */
        for (i = 0; i < n; ++i) {
            scl = 1. / magma_cblas_dznrm2( n, VR(0,i), 1 );
            blasf77_zdscal( &n, &scl, VR(0,i), &ione );
            for (k = 0; k < n; ++k) {
                /* Computing 2nd power */
                d__1 = MAGMA_Z_REAL( *VR(k,i) );
                d__2 = MAGMA_Z_IMAG( *VR(k,i) );
                rwork[irwork + k] = d__1*d__1 + d__2*d__2;
            }
            k = blasf77_idamax( &n, &rwork[irwork], &ione ) - 1;  // subtract 1; k is 0-based
            tmp = MAGMA_Z_CONJ( *VR(k,i) ) / magma_dsqrt( rwork[irwork + k] );
            blasf77_zscal( &n, &tmp, VR(0,i), &ione );
            *VR(k,i) = MAGMA_Z_MAKE( MAGMA_Z_REAL( *VR(k,i) ), 0 );
        }
    }

CLEANUP:
    /* Undo scaling if necessary */
    if (scalea) {
        // converged eigenvalues, stored in WR[i+1:n] and WI[i+1:n] for i = INFO
        magma_int_t nval = n - (*info);
        magma_int_t ld   = max( nval, 1 );
        lapackf77_zlascl( "G", &izero, &izero, &cscale, &anrm, &nval, &ione, w + (*info), &ld, &ierr );
        if (*info > 0) {
            // first ilo columns were already upper triangular,
            // so the corresponding eigenvalues are also valid.
            nval = ilo - 1;
            lapackf77_zlascl( "G", &izero, &izero, &cscale, &anrm, &nval, &ione, w, &n, &ierr );
        }
    }

    #if defined(VERSION3)
    magma_free( dT );
    #endif
    
    timer_stop( time_total );
    flops_stop( flop_total );
    timer_printf( "dgeev times n %5lld, gehrd %7.3f, unghr %7.3f, hseqr %7.3f, trevc %7.3f, total %7.3f, sum %7.3f\n",
                  (long long) n, time_gehrd, time_unghr, time_hseqr, time_trevc, time_total, time_sum );
    timer_printf( "dgeev flops n %5lld, gehrd %7lld, unghr %7lld, hseqr %7lld, trevc %7lld, total %7lld, sum %7lld\n",
                  (long long) n, flop_gehrd, flop_unghr, flop_hseqr, flop_trevc, flop_total, flop_sum );

    work[0] = magma_zmake_lwork( optwrk );

    return *info;
} /* magma_zgeev */
