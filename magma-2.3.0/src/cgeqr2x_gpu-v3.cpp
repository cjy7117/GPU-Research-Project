/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017
       
       @author Stan Tomov

       @generated from src/zgeqr2x_gpu-v3.cpp, normal z -> c, Wed Nov 15 00:34:19 2017

*/
#include "magma_internal.h"

/******************************************************************************/
// TODO: how does this differ from larfb_gpu?
extern "C" magma_int_t
magma_clarfb2_gpu(
    magma_int_t m, magma_int_t n, magma_int_t k,
    magmaFloatComplex_const_ptr dV,    magma_int_t lddv,
    magmaFloatComplex_const_ptr dT,    magma_int_t lddt,
    magmaFloatComplex_ptr       dC,    magma_int_t lddc,
    magmaFloatComplex_ptr       dwork, magma_int_t ldwork,
    magma_queue_t queue )
{
    magmaFloatComplex c_zero    = MAGMA_C_ZERO;
    magmaFloatComplex c_one     = MAGMA_C_ONE;
    magmaFloatComplex c_neg_one = MAGMA_C_NEG_ONE;

    if (m <= 0 || n <= 0)
        return MAGMA_SUCCESS;

    // W = C^H V
    magma_cgemm( MagmaConjTrans, MagmaNoTrans,
    //magmablas_cgemm_reduce(
                           n, k, m,
                           c_one,  dC,    lddc,
                                   dV,    lddv,
                           c_zero, dwork, ldwork, queue );

    // W = W T^H = C^H V T^H
    magma_ctrmm( MagmaRight, MagmaUpper, MagmaNoTrans, MagmaNonUnit,
                 n, k,
                 c_one, dT,    lddt,
                        dwork, ldwork, queue );

    // C = C - V W^H = C - V T V^H C = (I - V T V^H) C = H C
    magma_cgemm( MagmaNoTrans, MagmaConjTrans,
                 m, n, k,
                 c_neg_one, dV,    lddv,
                            dwork, ldwork,
                 c_one,     dC,    lddc, queue );
    
    return MAGMA_SUCCESS;
}


/***************************************************************************//**
    Purpose
    -------
    CGEQR2 computes a QR factorization of a complex m by n matrix A:
    A = Q * R.

    This expert routine requires two more arguments than the standard
    cgeqr2, namely, dT and ddA, explained below. The storage for A is
    also not as in the LAPACK's cgeqr2 routine (see below).

    The first is used to output the triangular
    n x n factor T of the block reflector used in the factorization.
    The second holds the diagonal nxn blocks of A, i.e., the diagonal
    submatrices of R. This routine implements the left looking QR.

    This version adds internal blocking.

    Arguments
    ---------
    @param[in]
    m       INTEGER
            The number of rows of the matrix A.  M >= 0.

    @param[in]
    n       INTEGER
            The number of columns of the matrix A.  N >= 0.

    @param[in,out]
    dA      COMPLEX array, dimension (LDDA,N)
            On entry, the m by n matrix A.
            On exit, the unitary matrix Q as a
            product of elementary reflectors (see Further Details).
    \n
            the elements on and above the diagonal of the array
            contain the min(m,n) by n upper trapezoidal matrix R (R is
            upper triangular if m >= n); the elements below the diagonal,
            with the array TAU, represent the unitary matrix Q as a
            product of elementary reflectors (see Further Details).

    @param[in]
    ldda    INTEGER
            The leading dimension of the array A.  LDDA >= max(1,M).

    @param[out]
    dtau    COMPLEX array, dimension (min(M,N))
            The scalar factors of the elementary reflectors (see Further
            Details).

    @param[out]
    dT      COMPLEX array, dimension N x N.
            Stores the triangular N x N factor T of the block reflector
            used in the factorization. The lower triangular part is 0.

    @param[out]
    ddA     COMPLEX array, dimension N x N.
            Stores the elements of the upper N x N diagonal block of A.
            LAPACK stores this array in A. There are 0s below the diagonal.

    @param
    dwork   (workspace) REAL array, dimension (3 N)

    @param[out]
    info    INTEGER
      -     = 0: successful exit
      -     < 0: if INFO = -i, the i-th argument had an illegal value

    Further Details
    ---------------
    The matrix Q is represented as a product of elementary reflectors

       Q = H(1) H(2) . . . H(k), where k = min(m,n).

    Each H(i) has the form

       H(i) = I - tau * v * v'

    where tau is a complex scalar, and v is a complex vector with
    v(1:i-1) = 0 and v(i) = 1; v(i+1:m) is stored on exit in A(i+1:m,i),
    and tau in TAU(i).

    @ingroup magma_geqr2
*******************************************************************************/
extern "C" magma_int_t
magma_cgeqr2x3_gpu(
    magma_int_t m, magma_int_t n,
    magmaFloatComplex_ptr dA, magma_int_t ldda,
    magmaFloatComplex_ptr dtau,
    magmaFloatComplex_ptr dT,
    magmaFloatComplex_ptr ddA,
    magmaFloat_ptr        dwork,
    magma_int_t *info)
{
    #define dA(i_,j_) (dA + (i_) + (j_)*ldda)
    #define BLOCK_SIZE 32

    magma_int_t b, i, min_mn;

    magmaFloat_ptr dnorm = dwork;
    magmaFloatComplex_ptr dwork2 = (magmaFloatComplex_ptr)(dwork + 2*n);

    *info = 0;
    if (m < 0) {
        *info = -1;
    } else if (n < 0) {
        *info = -2;
    } else if (ldda < max(1,m)) {
        *info = -4;
    }
    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    magma_queue_t queue;
    magma_device_t cdev;
    magma_getdevice( &cdev );
    magma_queue_create( cdev, &queue );

    /* Compute the norms of the trailing columns */
    min_mn = min(m,n);
    // magmablas_scnrm2_cols( m, min_mn, dA(0,0), ldda, dnorm, queue );

    for (b=0; b < min_mn; b += BLOCK_SIZE) {
        for (i = b; i < min(min_mn, b+BLOCK_SIZE); ++i) {
            /*   Apply H' to A(:,i) from the left */
            if ( i-b > 0)
                magma_clarfbx_gpu( m-b, i-b, dA(b, b), ldda,
                                  dT+b+b*min_mn, min_mn, dA(b, i), dwork2, queue );

            /*   Adjust the dnorm[i] to hold the norm of A(i:m,i) */
            //if ( i > 0 )
            //    magmablas_scnrm2_adjust( i, dnorm+i, dA(0, i), queue );
            magmablas_scnrm2_cols( m-i, 1, dA(i,i), ldda, dnorm+i, queue );
            
            /*  Generate elementary reflector H(i) to annihilate A(i+1:m,i)
                1. 1 is not yet put on the diagonal of A
                2. Elements above the diagonal are copied in ddA and
                   the ones in A are set to zero
                3. update T */
            magma_clarfgtx_gpu(m-i, dA(i, i), dA(min(i+1,m), i), dtau+i,
                               dnorm+i, ddA + i + i*(n), i,
                               dA(i,0), ldda,  dT, min_mn, dwork2, queue);
        }
        
        /* Apply the transformations to the trailing matrix. */
        //magma_clarfb2_gpu( MagmaLeft, MagmaConjTrans, MagmaForward, MagmaColumnwise,
        magma_clarfb2_gpu(
                           m-b, min_mn-i, BLOCK_SIZE,
                           dA(b, b), ldda, dT+b+b*min_mn, min_mn,
                           dA(b, i), ldda, dwork2, min_mn-i, queue );
    }

    magma_queue_destroy( queue );

    return *info;
} /* magma_cgeqr2 */
