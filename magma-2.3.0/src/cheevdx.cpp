/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017

       @author Stan Tomov
       @author Raffaele Solca
       @author Azzam Haidar

       @generated from src/zheevdx.cpp, normal z -> c, Wed Nov 15 00:34:19 2017

*/
#include "magma_internal.h"
#include "magma_timer.h"

#define COMPLEX

/***************************************************************************//**
    Purpose
    -------
    CHEEVDX computes selected eigenvalues and, optionally, eigenvectors
    of a complex Hermitian matrix A. Eigenvalues and eigenvectors can
    be selected by specifying either a range of values or a range of
    indices for the desired eigenvalues.
    If eigenvectors are desired, it uses a divide and conquer algorithm.

    The divide and conquer algorithm makes very mild assumptions about
    floating point arithmetic. It will work on machines with a guard
    digit in add/subtract, or on those binary machines without guard
    digits which subtract like the Cray X-MP, Cray Y-MP, Cray C-90, or
    Cray-2. It could conceivably fail on hexadecimal or decimal machines
    without guard digits, but we know of none.

    Arguments
    ---------
    @param[in]
    jobz    magma_vec_t
      -     = MagmaNoVec:  Compute eigenvalues only;
      -     = MagmaVec:    Compute eigenvalues and eigenvectors.

    @param[in]
    range   magma_range_t
      -     = MagmaRangeAll: all eigenvalues will be found.
      -     = MagmaRangeV:   all eigenvalues in the half-open interval (VL,VU]
                   will be found.
      -     = MagmaRangeI:   the IL-th through IU-th eigenvalues will be found.

    @param[in]
    uplo    magma_uplo_t
      -     = MagmaUpper:  Upper triangle of A is stored;
      -     = MagmaLower:  Lower triangle of A is stored.

    @param[in]
    n       INTEGER
            The order of the matrix A.  N >= 0.

    @param[in,out]
    A       COMPLEX array, dimension (LDA, N)
            On entry, the Hermitian matrix A.  If UPLO = MagmaUpper, the
            leading N-by-N upper triangular part of A contains the
            upper triangular part of the matrix A.  If UPLO = MagmaLower,
            the leading N-by-N lower triangular part of A contains
            the lower triangular part of the matrix A.
            On exit, if JOBZ = MagmaVec, then if INFO = 0, the first m columns
            of A contains the required
            orthonormal eigenvectors of the matrix A.
            If JOBZ = MagmaNoVec, then on exit the lower triangle (if UPLO=MagmaLower)
            or the upper triangle (if UPLO=MagmaUpper) of A, including the
            diagonal, is destroyed.

    @param[in]
    lda     INTEGER
            The leading dimension of the array A.  LDA >= max(1,N).

    @param[in]
    vl      REAL
    @param[in]
    vu      REAL
            If RANGE=MagmaRangeV, the lower and upper bounds of the interval to
            be searched for eigenvalues. VL < VU.
            Not referenced if RANGE = MagmaRangeAll or MagmaRangeI.

    @param[in]
    il      INTEGER
    @param[in]
    iu      INTEGER
            If RANGE=MagmaRangeI, the indices (in ascending order) of the
            smallest and largest eigenvalues to be returned.
            1 <= IL <= IU <= N, if N > 0; IL = 1 and IU = 0 if N = 0.
            Not referenced if RANGE = MagmaRangeAll or MagmaRangeV.

    @param[out]
    m       INTEGER
            The total number of eigenvalues found.  0 <= M <= N.
            If RANGE = MagmaRangeAll, M = N, and if RANGE = MagmaRangeI, M = IU-IL+1.

    @param[out]
    w       REAL array, dimension (N)
            If INFO = 0, the required m eigenvalues in ascending order.

    @param[out]
    work    (workspace) COMPLEX array, dimension (MAX(1,LWORK))
            On exit, if INFO = 0, WORK[0] returns the optimal LWORK.

    @param[in]
    lwork   INTEGER
            The length of the array WORK.
     -      If N <= 1,                      LWORK >= 1.
     -      If JOBZ = MagmaNoVec and N > 1, LWORK >= N + N*NB.
     -      If JOBZ = MagmaVec   and N > 1, LWORK >= max( N + N*NB, 2*N + N**2 ).
            NB can be obtained through magma_get_chetrd_nb(N).
    \n
            If LWORK = -1, then a workspace query is assumed; the routine
            only calculates the optimal sizes of the WORK, RWORK and
            IWORK arrays, returns these values as the first entries of
            the WORK, RWORK and IWORK arrays, and no error message
            related to LWORK or LRWORK or LIWORK is issued by XERBLA.

    @param[out]
    rwork   (workspace) REAL array,
                                           dimension (LRWORK)
            On exit, if INFO = 0, RWORK[0] returns the optimal LRWORK.

    @param[in]
    lrwork  INTEGER
            The dimension of the array RWORK.
     -      If N <= 1,                      LRWORK >= 1.
     -      If JOBZ = MagmaNoVec and N > 1, LRWORK >= N.
     -      If JOBZ = MagmaVec   and N > 1, LRWORK >= 1 + 5*N + 2*N**2.
    \n
            If LRWORK = -1, then a workspace query is assumed; the
            routine only calculates the optimal sizes of the WORK, RWORK
            and IWORK arrays, returns these values as the first entries
            of the WORK, RWORK and IWORK arrays, and no error message
            related to LWORK or LRWORK or LIWORK is issued by XERBLA.

    @param[out]
    iwork   (workspace) INTEGER array, dimension (MAX(1,LIWORK))
            On exit, if INFO = 0, IWORK[0] returns the optimal LIWORK.

    @param[in]
    liwork  INTEGER
            The dimension of the array IWORK.
     -      If N <= 1,                      LIWORK >= 1.
     -      If JOBZ = MagmaNoVec and N > 1, LIWORK >= 1.
     -      If JOBZ = MagmaVec   and N > 1, LIWORK >= 3 + 5*N.
    \n
            If LIWORK = -1, then a workspace query is assumed; the
            routine only calculates the optimal sizes of the WORK, RWORK
            and IWORK arrays, returns these values as the first entries
            of the WORK, RWORK and IWORK arrays, and no error message
            related to LWORK or LRWORK or LIWORK is issued by XERBLA.

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value
      -     > 0:  if INFO = i and JOBZ = MagmaNoVec, then the algorithm failed
                  to converge; i off-diagonal elements of an intermediate
                  tridiagonal form did not converge to zero;
                  if INFO = i and JOBZ = MagmaVec, then the algorithm failed
                  to compute an eigenvalue while working on the submatrix
                  lying in rows and columns INFO/(N+1) through
                  mod(INFO,N+1).

    Further Details
    ---------------
    Based on contributions by
       Jeff Rutter, Computer Science Division, University of California
       at Berkeley, USA

    Modified description of INFO. Sven, 16 Feb 05.

    @ingroup magma_heevdx
*******************************************************************************/
extern "C" magma_int_t
magma_cheevdx(
    magma_vec_t jobz, magma_range_t range, magma_uplo_t uplo,
    magma_int_t n,
    magmaFloatComplex *A, magma_int_t lda,
    float vl, float vu, magma_int_t il, magma_int_t iu,
    magma_int_t *m, float *w,
    magmaFloatComplex *work, magma_int_t lwork,
    #ifdef COMPLEX
    float *rwork, magma_int_t lrwork,
    #endif
    magma_int_t *iwork, magma_int_t liwork,
    magma_int_t *info)
{
    const char* uplo_  = lapack_uplo_const( uplo  );
    const char* jobz_  = lapack_vec_const( jobz  );
    magma_int_t ione = 1;
    magma_int_t izero = 0;
    float d_one = 1.;

    float d__1;

    float eps;
    magma_int_t inde;
    float anrm;
    magma_int_t imax;
    float rmin, rmax;
    float sigma;
    magma_int_t iinfo, lwmin;
    magma_int_t lower;
    magma_int_t llrwk;
    magma_int_t wantz;
    magma_int_t indwk2, llwrk2;
    magma_int_t iscale;
    float safmin;
    float bignum;
    magma_int_t indtau;
    magma_int_t indrwk, indwrk, liwmin;
    magma_int_t lrwmin, llwork;
    float smlnum;
    magma_int_t lquery;
    magma_int_t alleig, valeig, indeig;

    float* dwork;

    wantz = (jobz == MagmaVec);
    lower = (uplo == MagmaLower);

    alleig = (range == MagmaRangeAll);
    valeig = (range == MagmaRangeV);
    indeig = (range == MagmaRangeI);

    lquery = (lwork == -1 || lrwork == -1 || liwork == -1);

    *info = 0;
    if (! (wantz || (jobz == MagmaNoVec))) {
        *info = -1;
    } else if (! (alleig || valeig || indeig)) {
        *info = -2;
    } else if (! (lower || (uplo == MagmaUpper))) {
        *info = -3;
    } else if (n < 0) {
        *info = -4;
    } else if (lda < max(1,n)) {
        *info = -6;
    } else {
        if (valeig) {
            if (n > 0 && vu <= vl) {
                *info = -8;
            }
        } else if (indeig) {
            if (il < 1 || il > max(1,n)) {
                *info = -9;
            } else if (iu < min(n,il) || iu > n) {
                *info = -10;
            }
        }
    }

    magma_int_t nb = magma_get_chetrd_nb( n );
    if ( n <= 1 ) {
        lwmin  = 1;
        lrwmin = 1;
        liwmin = 1;
    }
    else if ( wantz ) {
        lwmin  = max( n + n*nb, 2*n + n*n );
        lrwmin = 1 + 5*n + 2*n*n;
        liwmin = 3 + 5*n;
    }
    else {
        lwmin  = n + n*nb;
        lrwmin = n;
        liwmin = 1;
    }
    
    work[0]  = magma_cmake_lwork( lwmin );
    rwork[0] = magma_smake_lwork( lrwmin );
    iwork[0] = liwmin;

    if ((lwork < lwmin) && !lquery) {
        *info = -14;
    } else if ((lrwork < lrwmin) && ! lquery) {
        *info = -16;
    } else if ((liwork < liwmin) && ! lquery) {
        *info = -18;
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

    if (n == 1) {
        w[0] = MAGMA_C_REAL(A[0]);
        if (wantz) {
            A[0] = MAGMA_C_ONE;
        }
        return *info;
    }
    /* Check if matrix is very small then just call LAPACK on CPU, no need for GPU */
    if (n <= 128) {
        #ifdef ENABLE_DEBUG
        printf("--------------------------------------------------------------\n");
        printf("  warning matrix too small N=%lld NB=%lld, calling lapack on CPU\n", (long long) n, (long long) nb );
        printf("--------------------------------------------------------------\n");
        #endif
        lapackf77_cheevd(jobz_, uplo_,
                         &n, A, &lda,
                         w, work, &lwork,
                         #ifdef COMPLEX
                         rwork, &lrwork,
                         #endif
                         iwork, &liwork, info);
        return *info;
    }
    /* Get machine constants. */
    safmin = lapackf77_slamch("Safe minimum");
    eps    = lapackf77_slamch("Precision");
    smlnum = safmin / eps;
    bignum = 1. / smlnum;
    rmin = magma_ssqrt(smlnum);
    rmax = magma_ssqrt(bignum);

    /* Scale matrix to allowable range, if necessary. */
    anrm = lapackf77_clanhe("M", uplo_, &n, A, &lda, rwork);
    iscale = 0;
    if (anrm > 0. && anrm < rmin) {
        iscale = 1;
        sigma = rmin / anrm;
    } else if (anrm > rmax) {
        iscale = 1;
        sigma = rmax / anrm;
    }
    if (iscale == 1) {
        lapackf77_clascl(uplo_, &izero, &izero, &d_one, &sigma, &n, &n, A,
                         &lda, info);
    }

    /* Call CHETRD to reduce Hermitian matrix to tridiagonal form. */
    // chetrd rwork: e (n)
    // cstedx rwork: e (n) + llrwk (1 + 4*N + 2*N**2)  ==>  1 + 5n + 2n^2
    inde   = 0;
    indrwk = inde + n;
    llrwk  = lrwork - indrwk;

    // chetrd work: tau (n) + llwork (n*nb)  ==>  n + n*nb
    // cstedx work: tau (n) + z (n^2)
    // cunmtr work: tau (n) + z (n^2) + llwrk2 (n or n*nb)  ==>  2n + n^2, or n + n*nb + n^2
    indtau = 0;
    indwrk = indtau + n;
    indwk2 = indwrk + n*n;
    llwork = lwork - indwrk;
    llwrk2 = lwork - indwk2;

    magma_timer_t time=0;
    timer_start( time );

    magma_chetrd(uplo, n, A, lda, w, &rwork[inde],
                 &work[indtau], &work[indwrk], llwork, &iinfo);

    timer_stop( time );
    timer_printf( "time chetrd = %6.2f\n", time );

    /* For eigenvalues only, call SSTERF.  For eigenvectors, first call
     CSTEDC to generate the eigenvector matrix, WORK(INDWRK), of the
     tridiagonal matrix, then call CUNMTR to multiply it to the Householder
     transformations represented as Householder vectors in A. */
    if (! wantz) {
        lapackf77_ssterf(&n, w, &rwork[inde], info);

        magma_smove_eig(range, n, w, &il, &iu, vl, vu, m);
    }
    else {
        timer_start( time );

        if (MAGMA_SUCCESS != magma_smalloc( &dwork, 3*n*(n/2 + 1) )) {
            *info = MAGMA_ERR_DEVICE_ALLOC;
            return *info;
        }

        magma_cstedx(range, n, vl, vu, il, iu, w, &rwork[inde],
                     &work[indwrk], n, &rwork[indrwk],
                     llrwk, iwork, liwork, dwork, info);

        magma_free( dwork );

        timer_stop( time );
        timer_printf( "time cstedx = %6.2f\n", time );
        timer_start( time );

        magma_smove_eig(range, n, w, &il, &iu, vl, vu, m);

        magma_cunmtr(MagmaLeft, uplo, MagmaNoTrans, n, *m, A, lda, &work[indtau],
                     &work[indwrk + n * (il-1) ], n, &work[indwk2], llwrk2, &iinfo);

        lapackf77_clacpy("A", &n, m, &work[indwrk + n * (il-1)], &n, A, &lda);

        timer_stop( time );
        timer_printf( "time cunmtr + copy = %6.2f\n", time );
    }

    /* If matrix was scaled, then rescale eigenvalues appropriately. */
    if (iscale == 1) {
        if (*info == 0) {
            imax = n;
        } else {
            imax = *info - 1;
        }
        d__1 = 1. / sigma;
        blasf77_sscal(&imax, &d__1, w, &ione);
    }

    work[0]  = magma_cmake_lwork( lwmin );
    rwork[0] = magma_smake_lwork( lrwmin );
    iwork[0] = liwmin;

    return *info;
} /* magma_cheevdx */
