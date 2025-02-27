/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017

       @precisions normal z -> s d c
       
       @author Stan Tomov
       @author Mark Gates
*/
#include "magma_internal.h"

/***************************************************************************//**
    Purpose
    -------
    ZGEHRD reduces a COMPLEX_16 general matrix A to upper Hessenberg form H by
    an orthogonal similarity transformation:  Q' * A * Q = H . This version
    stores the triangular matrices used in the factorization so that they can
    be applied directly (i.e., without being recomputed) later. As a result,
    the application of Q is much faster.

    Arguments
    ---------
    @param[in]
    n       INTEGER
            The order of the matrix A.  N >= 0.

    @param[in]
    ilo     INTEGER
    @param[in]
    ihi     INTEGER
            It is assumed that A is already upper triangular in rows
            and columns 1:ILO-1 and IHI+1:N. ILO and IHI are normally
            set by a previous call to ZGEBAL; otherwise they should be
            set to 1 and N respectively. See Further Details.
            1 <= ILO <= IHI <= N, if N > 0; ILO=1 and IHI=0, if N=0.

    @param[in,out]
    A       COMPLEX_16 array, dimension (LDA,N)
            On entry, the N-by-N general matrix to be reduced.
            On exit, the upper triangle and the first subdiagonal of A
            are overwritten with the upper Hessenberg matrix H, and the
            elements below the first subdiagonal, with the array TAU,
            represent the orthogonal matrix Q as a product of elementary
            reflectors. See Further Details.

    @param[in]
    lda     INTEGER
            The leading dimension of the array A.  LDA >= max(1,N).

    @param[out]
    tau     COMPLEX_16 array, dimension (N-1)
            The scalar factors of the elementary reflectors (see Further
            Details). Elements 1:ILO-1 and IHI:N-1 of TAU are set to
            zero.

    @param[out]
    work    (workspace) COMPLEX_16 array, dimension (LWORK)
            On exit, if INFO = 0, WORK[0] returns the optimal LWORK.

    @param[in]
    lwork   INTEGER
            The length of the array WORK.  LWORK >= N*NB,
            where NB is the optimal blocksize.
    \n
            If LWORK = -1, then a workspace query is assumed; the routine
            only calculates the optimal size of the WORK array, returns
            this value as the first entry of the WORK array, and no error
            message related to LWORK is issued by XERBLA.

    @param[out]
    dT      COMPLEX_16 array on the GPU, dimension NB*N,
            where NB is the optimal blocksize. It stores the NB*NB blocks
            of the triangular T matrices used in the reduction.

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value.

    Further Details
    ---------------
    The matrix Q is represented as a product of (ihi-ilo) elementary
    reflectors

       Q = H(ilo) H(ilo+1) . . . H(ihi-1).

    Each H(i) has the form

       H(i) = I - tau * v * v'

    where tau is a complex scalar, and v is a complex vector with
    v(1:i) = 0, v(i+1) = 1 and v(ihi+1:n) = 0; v(i+2:ihi) is stored on
    exit in A(i+2:ihi,i), and tau in TAU(i).

    The contents of A are illustrated by the following example, with
    n = 7, ilo = 2 and ihi = 6:

    @verbatim
    on entry,                        on exit,

    ( a   a   a   a   a   a   a )    (  a   a   h   h   h   h   a )
    (     a   a   a   a   a   a )    (      a   h   h   h   h   a )
    (     a   a   a   a   a   a )    (      h   h   h   h   h   h )
    (     a   a   a   a   a   a )    (      v2  h   h   h   h   h )
    (     a   a   a   a   a   a )    (      v2  v3  h   h   h   h )
    (     a   a   a   a   a   a )    (      v2  v3  v4  h   h   h )
    (                         a )    (                          a )
    @endverbatim

    where a denotes an element of the original matrix A, h denotes a
    modified element of the upper Hessenberg matrix H, and vi denotes an
    element of the vector defining H(i).

    This implementation follows the hybrid algorithm and notations described in

    S. Tomov and J. Dongarra, "Accelerating the reduction to upper Hessenberg
    form through hybrid GPU-based computing," University of Tennessee Computer
    Science Technical Report, UT-CS-09-642 (also LAPACK Working Note 219),
    May 24, 2009.
    
    This version stores the T matrices in dT, for later use in magma_zunghr.

    @ingroup magma_gehrd
*******************************************************************************/
extern "C" magma_int_t
magma_zgehrd(
    magma_int_t n, magma_int_t ilo, magma_int_t ihi,
    magmaDoubleComplex *A, magma_int_t lda,
    magmaDoubleComplex *tau,
    magmaDoubleComplex *work, magma_int_t lwork,
    magmaDoubleComplex_ptr dT,
    magma_int_t *info)
{
    #define  A(i_,j_) ( A + (i_) + (j_)*lda)

    #ifdef HAVE_clBLAS
    #define dA(i_,j_)  dwork, ((i_) + (j_)*ldda + nb*ldda*2)
    #define dT(i_,j_)  dT,    ((i_) + (j_)*nb   + dT_offset)
    #define dV(i_,j_)  dwork, ((i_) + (j_)*ldda + nb*ldda)
    #define dwork(i_)  dwork, ((i_))
    #else
    #define dA(i_,j_) (dA    + (i_) + (j_)*ldda)
    #define dT(i_,j_) (dT    + (i_) + (j_)*nb)
    #define dV(i_,j_) (dV    + (i_) + (j_)*ldda)
    #define dwork(i_) (dwork + (i_))
    #endif

    // Constants
    const magmaDoubleComplex c_one  = MAGMA_Z_ONE;
    const magmaDoubleComplex c_zero = MAGMA_Z_ZERO;

    // Local variables
    magma_int_t nb = magma_get_zgehrd_nb( n );
    magma_int_t ldda = magma_roundup( n, 32 );

    magma_int_t i, nh, iws;
    magma_int_t iinfo;
    magma_int_t lquery;

    *info = 0;
    iws = n*nb;
    work[0] = magma_zmake_lwork( iws );

    lquery = (lwork == -1);
    if (n < 0) {
        *info = -1;
    } else if (ilo < 1 || ilo > max(1,n)) {
        *info = -2;
    } else if (ihi < min(ilo,n) || ihi > n) {
        *info = -3;
    } else if (lda < max(1,n)) {
        *info = -5;
    } else if (lwork < iws && ! lquery) {
        *info = -8;
    }
    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }
    else if (lquery)
        return *info;

    // Adjust from 1-based indexing
    ilo -= 1;
    
    // Quick return if possible
    nh = ihi - ilo;
    if (nh <= 1) {
        work[0] = c_one;
        return *info;
    }

    // Now requires lwork >= iws; else dT won't be computed in unblocked code.
    // If not enough workspace, use unblocked code
    //if ( lwork < iws ) {
    //    nb = 1;
    //}

    if (nb == 1 || nb > nh) {
        // Use unblocked code below
        i = ilo;
    }
    else {
        // Use blocked code
        magma_queue_t queue;
        magma_device_t cdev;
        magma_getdevice( &cdev );
        magma_queue_create( cdev, &queue );
        
        // GPU workspace is:
        //   nb*ldda for dwork for zlahru
        //   nb*ldda for dV
        //   n*ldda  for dA
        magmaDoubleComplex_ptr dwork;
        if (MAGMA_SUCCESS != magma_zmalloc( &dwork, 2*nb*ldda + n*ldda )) {
            *info = MAGMA_ERR_DEVICE_ALLOC;
            return *info;
        }
        magmaDoubleComplex *dV = dwork + nb*ldda;
        magmaDoubleComplex *dA = dwork + nb*ldda*2;
        
        magmaDoubleComplex *T;
        magma_zmalloc_cpu( &T, nb*nb );
        if ( T == NULL ) {
            magma_free( dwork );
            *info = MAGMA_ERR_HOST_ALLOC;
            return *info;
        }
        
        // zero first block of V, which is lower triangular
        magmablas_zlaset( MagmaFull, nb, nb, c_zero, c_zero, dV(0,0), ldda, queue );
        
        // Set elements 0:ILO-1 and IHI-1:N-2 of TAU to zero
        for (i = 0; i < ilo; ++i)
            tau[i] = c_zero;
        
        for (i = max(0,ihi-1); i < n-1; ++i)
            tau[i] = c_zero;
        
        assert( nb % 4 == 0 );
        for (i=0; i < nb*nb; i += 4)
            T[i] = T[i+1] = T[i+2] = T[i+3] = c_zero;
        
        magmablas_zlaset( MagmaFull, nb, n, c_zero, c_zero, dT(0,0), nb, queue );
        
        // Copy the matrix to the GPU
        magma_zsetmatrix( n, n-ilo, A(0,ilo), lda, dA(0,0), ldda, queue );
        
        for (i = ilo; i < ihi-1 - nb; i += nb) {
            // Reduce columns i:i+nb-1 to Hessenberg form, returning the
            // matrices V and T of the block reflector H = I - V*T*V'
            // which performs the reduction, and also the matrix Y = A*V*T
            
            // Get the current panel (no need for the 1st iteration)
            magma_zgetmatrix( ihi-i, nb,
                              dA(i,i-ilo), ldda,
                              A(i,i), lda, queue );
            
            // add 1 to i for 1-based index
            magma_zlahr2( ihi, i+1, nb,
                          dA(0,i-ilo), ldda,
                          dV(0,0),     ldda,
                          A(0,i),      lda,
                          &tau[i], T, nb, work, n, queue );
            
            // Copy T from the CPU to dT on the GPU
            magma_zsetmatrix( nb, nb, T, nb, dT(0,i-ilo), nb, queue );
            
            magma_zlahru( n, ihi, i, nb,
                          A(0,i),      lda,
                          dA(0,i-ilo), ldda, // dA
                          dA(i,i-ilo), ldda, // dY, stored over current panel
                          dV(0,0),     ldda,
                          dT(0,i-ilo), dwork(0), queue );
        }
        
        // Copy remainder to host
        magma_zgetmatrix( n, n-i,
                          dA(0,i-ilo), ldda,
                          A(0,i), lda, queue );
        
        magma_free( dwork );
        magma_free_cpu( T );
        
        magma_queue_destroy( queue );
    }

    // Use unblocked code to reduce the rest of the matrix
    // add 1 to i for 1-based index
    i += 1;
    lapackf77_zgehd2(&n, &i, &ihi, A, &lda, tau, work, &iinfo);
    work[0] = magma_zmake_lwork( iws );

    return *info;
} /* magma_zgehrd */
