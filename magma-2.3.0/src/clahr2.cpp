/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017

       @generated from src/zlahr2.cpp, normal z -> c, Wed Nov 15 00:34:20 2017
       @author Stan Tomov
       @author Mark Gates
*/
#include "magma_internal.h"

#define COMPLEX

/***************************************************************************//**
    Purpose
    -------
    CLAHR2 reduces the first NB columns of a complex general n-BY-(n-k+1)
    matrix A so that elements below the k-th subdiagonal are zero. The
    reduction is performed by an orthogonal similarity transformation
    Q' * A * Q. The routine returns the matrices V and T which determine
    Q as a block reflector I - V*T*V', and also the matrix Y = A * V.
    (Note this is different than LAPACK, which computes Y = A * V * T.)

    This is an auxiliary routine called by CGEHRD.

    Arguments
    ---------
    @param[in]
    n       INTEGER
            The order of the matrix A.

    @param[in]
    k       INTEGER
            The offset for the reduction. Elements below the k-th
            subdiagonal in the first NB columns are reduced to zero.
            K < N.

    @param[in]
    nb      INTEGER
            The number of columns to be reduced.

    @param[in,out]
    dA      COMPLEX array on the GPU, dimension (LDDA,N-K+1)
            On entry, the n-by-(n-k+1) general matrix A.
            On exit, the elements in rows K:N of the first NB columns are
            overwritten with the matrix Y.

    @param[in]
    ldda    INTEGER
            The leading dimension of the array dA.  LDDA >= max(1,N).

    @param[out]
    dV      COMPLEX array on the GPU, dimension (LDDV, NB)
            On exit this n-by-nb array contains the Householder vectors of the transformation.

    @param[in]
    lddv    INTEGER
            The leading dimension of the array dV.  LDDV >= max(1,N).

    @param[in,out]
    A       COMPLEX array, dimension (LDA,N-K+1)
            On entry, the n-by-(n-k+1) general matrix A.
            On exit, the elements on and above the k-th subdiagonal in
            the first NB columns are overwritten with the corresponding
            elements of the reduced matrix; the elements below the k-th
            subdiagonal, with the array TAU, represent the matrix Q as a
            product of elementary reflectors. The other columns of A are
            unchanged. See Further Details.

    @param[in]
    lda     INTEGER
            The leading dimension of the array A.  LDA >= max(1,N).

    @param[out]
    tau     COMPLEX array, dimension (NB)
            The scalar factors of the elementary reflectors. See Further
            Details.

    @param[out]
    T       COMPLEX array, dimension (LDT,NB)
            The upper triangular matrix T.

    @param[in]
    ldt     INTEGER
            The leading dimension of the array T.  LDT >= NB.

    @param[out]
    Y       COMPLEX array, dimension (LDY,NB)
            The n-by-nb matrix Y.

    @param[in]
    ldy     INTEGER
            The leading dimension of the array Y. LDY >= N.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    Further Details
    ---------------
    The matrix Q is represented as a product of nb elementary reflectors

       Q = H(1) H(2) . . . H(nb).

    Each H(i) has the form

       H(i) = I - tau * v * v'

    where tau is a complex scalar, and v is a complex vector with
    v(1:i+k-1) = 0, v(i+k) = 1; v(i+k+1:n) is stored on exit in
    A(i+k+1:n,i), and tau in TAU(i).

    The elements of the vectors v together form the (n-k+1)-by-nb matrix
    V which is needed, with T and Y, to apply the transformation to the
    unreduced part of the matrix, using an update of the form:
    A := (I - V*T*V') * (A - Y*T*V').

    The contents of A on exit are illustrated by the following example
    with n = 7, k = 3 and nb = 2:

    @verbatim
       ( a   a   a   a   a )
       ( a   a   a   a   a )
       ( a   a   a   a   a )
       ( h   h   a   a   a )
       ( v1  h   a   a   a )
       ( v1  v2  a   a   a )
       ( v1  v2  a   a   a )
    @endverbatim

    where "a" denotes an element of the original matrix A, h denotes a
    modified element of the upper Hessenberg matrix H, and vi denotes an
    element of the vector defining H(i).

    This implementation follows the hybrid algorithm and notations described in

    S. Tomov and J. Dongarra, "Accelerating the reduction to upper Hessenberg
    form through hybrid GPU-based computing," University of Tennessee Computer
    Science Technical Report, UT-CS-09-642 (also LAPACK Working Note 219),
    May 24, 2009.

    @ingroup magma_lahr2
*******************************************************************************/
extern "C" magma_int_t
magma_clahr2(
    magma_int_t n, magma_int_t k, magma_int_t nb,
    magmaFloatComplex_ptr dA, magma_int_t ldda,
    magmaFloatComplex_ptr dV, magma_int_t lddv,
    magmaFloatComplex *A,     magma_int_t lda,
    magmaFloatComplex *tau,
    magmaFloatComplex *T,     magma_int_t ldt,
    magmaFloatComplex *Y,     magma_int_t ldy,
    magma_queue_t queue )
{
    #define  A(i_,j_) ( A + (i_) + (j_)*lda)
    #define  Y(i_,j_) ( Y + (i_) + (j_)*ldy)
    #define  T(i_,j_) ( T + (i_) + (j_)*ldt)
    #define dA(i_,j_) (dA + (i_) + (j_)*ldda)
    #define dV(i_,j_) (dV + (i_) + (j_)*lddv)
    
    magmaFloatComplex c_zero    = MAGMA_C_ZERO;
    magmaFloatComplex c_one     = MAGMA_C_ONE;
    magmaFloatComplex c_neg_one = MAGMA_C_NEG_ONE;

    magma_int_t ione = 1;
    
    magma_int_t n_k_i_1, n_k;
    magmaFloatComplex scale;

    magma_int_t i;
    magmaFloatComplex ei = MAGMA_C_ZERO;

    magma_int_t info = 0;
    if (n < 0) {
        info = -1;
    } else if (k < 0 || k > n) {
        info = -2;
    } else if (nb < 1 || nb > n) {
        info = -3;
    } else if (ldda < max(1,n)) {
        info = -5;
    } else if (lddv < max(1,n)) {
        info = -7;
    } else if (lda < max(1,n)) {
        info = -9;
    } else if (ldt < max(1,nb)) {
        info = -12;
    } else if (ldy < max(1,n)) {
        info = -13;
    }
    if (info != 0) {
        magma_xerbla( __func__, -(info) );
        return info;
    }

    // adjust from 1-based indexing
    k -= 1;

    if (n <= 1)
        return info;
    
    for (i = 0; i < nb; ++i) {
        n_k_i_1 = n - k - i - 1;
        n_k     = n - k;
        
        if (i > 0) {
            // Update A(k:n-1,i); Update i-th column of A - Y * T * V'
            // This updates one more row than LAPACK does (row k),
            // making the block above the panel an even multiple of nb.
            // Use last column of T as workspace, w.
            // w(0:i-1, nb-1) = VA(k+i, 0:i-1)'
            blasf77_ccopy( &i,
                           A(k+i,0),  &lda,
                           T(0,nb-1), &ione );
            #ifdef COMPLEX
            // If complex, conjugate row of V.
            lapackf77_clacgv(&i, T(0,nb-1), &ione);
            #endif
            
            // w = T(0:i-1, 0:i-1) * w
            blasf77_ctrmv( "Upper", "No trans", "No trans", &i,
                           T(0,0),    &ldt,
                           T(0,nb-1), &ione );
            
            // A(k:n-1, i) -= Y(k:n-1, 0:i-1) * w
            blasf77_cgemv( "No trans", &n_k, &i,
                           &c_neg_one, Y(k,0),    &ldy,
                                       T(0,nb-1), &ione,
                           &c_one,     A(k,i),    &ione );
            
            // Apply I - V * T' * V' to this column (call it b) from the
            // left, using the last column of T as workspace, w.
            //
            // Let  V = ( V1 )   and   b = ( b1 )   (first i-1 rows)
            //          ( V2 )             ( b2 )
            // where V1 is unit lower triangular
            
            // w := b1 = A(k+1:k+i, i)
            blasf77_ccopy( &i,
                           A(k+1,i),  &ione,
                           T(0,nb-1), &ione );
            
            // w := V1' * b1 = VA(k+1:k+i, 0:i-1)' * w
            blasf77_ctrmv( "Lower", "Conj", "Unit", &i,
                           A(k+1,0), &lda,
                           T(0,nb-1), &ione );
            
            // w := w + V2'*b2 = w + VA(k+i+1:n-1, 0:i-1)' * A(k+i+1:n-1, i)
            blasf77_cgemv( "Conj", &n_k_i_1, &i,
                           &c_one, A(k+i+1,0), &lda,
                                   A(k+i+1,i), &ione,
                           &c_one, T(0,nb-1),  &ione );
            
            // w := T'*w = T(0:i-1, 0:i-1)' * w
            blasf77_ctrmv( "Upper", "Conj", "Non-unit", &i,
                           T(0,0), &ldt,
                           T(0,nb-1), &ione );
            
            // b2 := b2 - V2*w = A(k+i+1:n-1, i) - VA(k+i+1:n-1, 0:i-1) * w
            blasf77_cgemv( "No trans", &n_k_i_1, &i,
                           &c_neg_one, A(k+i+1,0), &lda,
                                       T(0,nb-1),  &ione,
                           &c_one,     A(k+i+1,i), &ione );
            
            // w := V1*w = VA(k+1:k+i, 0:i-1) * w
            blasf77_ctrmv( "Lower", "No trans", "Unit", &i,
                           A(k+1,0), &lda,
                           T(0,nb-1), &ione );
            
            // b1 := b1 - w = A(k+1:k+i-1, i) - w
            blasf77_caxpy( &i,
                           &c_neg_one, T(0,nb-1), &ione,
                                       A(k+1,i),  &ione );
            
            // Restore diagonal element, saved below during previous iteration
            *A(k+i,i-1) = ei;
        }
        
        // Generate the elementary reflector H(i) to annihilate A(k+i+1:n-1,i)
        lapackf77_clarfg( &n_k_i_1,
                          A(k+i+1,i),
                          A(k+i+2,i), &ione, &tau[i] );
        // Save diagonal element and set to one, to simplify multiplying by V
        ei = *A(k+i+1,i);
        *A(k+i+1,i) = c_one;

        // dV(i+1:n-k-1, i) = VA(k+i+1:n-1, i)
        magma_csetvector( n_k_i_1,
                          A(k+i+1,i), 1,
                          dV(i+1,i),  1, queue );
        
        // Compute Y(k+1:n,i) = A vi
        // dA(k:n-1, i) = dA(k:n-1, i+1:n-k-1) * dV(i+1:n-k-1, i)
        magma_cgemv( MagmaNoTrans, n_k, n_k_i_1,
                     c_one,  dA(k,i+1), ldda,
                             dV(i+1,i), ione,
                     c_zero, dA(k,i),   ione, queue );
        
        // Compute T(0:i,i) = [ -tau T V' vi ]
        //                    [  tau         ]
        // T(0:i-1, i) = -tau VA(k+i+1:n-1, 0:i-1)' VA(k+i+1:n-1, i)
        scale = MAGMA_C_NEGATE( tau[i]);
        blasf77_cgemv( "Conj", &n_k_i_1, &i,
                       &scale,  A(k+i+1,0), &lda,
                                A(k+i+1,i), &ione,
                       &c_zero, T(0,i),     &ione );
        // T(0:i-1, i) = T(0:i-1, 0:i-1) * T(0:i-1, i)
        blasf77_ctrmv( "Upper", "No trans", "Non-unit", &i,
                       T(0,0), &ldt,
                       T(0,i), &ione );
        *T(i,i) = tau[i];

        // Y(k:n-1, i) = dA(k:n-1, i)
        magma_cgetvector( n-k,
                          dA(k,i), 1,
                          Y(k,i),  1, queue );
    }
    // Restore diagonal element
    *A(k+nb,nb-1) = ei;

    return info;
} /* magma_clahr2 */
