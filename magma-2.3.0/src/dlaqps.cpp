/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017

       @generated from src/zlaqps.cpp, normal z -> d, Wed Nov 15 00:34:19 2017

       @author Stan Tomov
       @author Mark Gates 
       @author Mitch Horton
*/

#include "magma_internal.h"

#define REAL

/***************************************************************************//**
    Purpose
    -------
    DLAQPS computes a step of QR factorization with column pivoting
    of a real M-by-N matrix A by using Blas-3.  It tries to factorize
    NB columns from A starting from the row OFFSET+1, and updates all
    of the matrix with Blas-3 xGEMM.

    In some cases, due to catastrophic cancellations, it cannot
    factorize NB columns.  Hence, the actual number of factorized
    columns is returned in KB.

    Block A(1:OFFSET,1:N) is accordingly pivoted, but not factorized.

    Arguments
    ---------
    @param[in]
    m       INTEGER
            The number of rows of the matrix A. M >= 0.

    @param[in]
    n       INTEGER
            The number of columns of the matrix A. N >= 0

    @param[in]
    offset  INTEGER
            The number of rows of A that have been factorized in
            previous steps.

    @param[in]
    nb      INTEGER
            The number of columns to factorize.

    @param[out]
    kb      INTEGER
            The number of columns actually factorized.

    @param[in,out]
    A       DOUBLE PRECISION array, dimension (LDA,N)
            On entry, the M-by-N matrix A.
            On exit, block A(OFFSET+1:M,1:KB) is the triangular
            factor obtained and block A(1:OFFSET,1:N) has been
            accordingly pivoted, but no factorized.
            The rest of the matrix, block A(OFFSET+1:M,KB+1:N) has
            been updated.

    @param[in]
    lda     INTEGER
            The leading dimension of the array A. LDA >= max(1,M).
    
    @param[in,out]
    dA      DOUBLE PRECISION array, dimension (LDA,N)
            Copy of A on the GPU.
            Portions of  A are updated on the CPU;
            portions of dA are updated on the GPU. See code for details.

    @param[in]
    ldda    INTEGER
            The leading dimension of the array dA. LDDA >= max(1,M).

    @param[in,out]
    jpvt    INTEGER array, dimension (N)
            JPVT(I) = K <==> Column K of the full matrix A has been
            permuted into position I in AP.

    @param[out]
    tau     DOUBLE PRECISION array, dimension (KB)
            The scalar factors of the elementary reflectors.

    @param[in,out]
    vn1     DOUBLE PRECISION array, dimension (N)
            The vector with the partial column norms.

    @param[in,out]
    vn2     DOUBLE PRECISION array, dimension (N)
            The vector with the exact column norms.

    @param[in,out]
    auxv    DOUBLE PRECISION array, dimension (NB)
            Auxiliar vector.

    @param[in,out]
    F       DOUBLE PRECISION array, dimension (LDF,NB)
            Matrix F' = L*Y'*A.

    @param[in]
    ldf     INTEGER
            The leading dimension of the array F. LDF >= max(1,N).

    @param[in,out]
    dF      DOUBLE PRECISION array, dimension (LDDF,NB)
            Copy of F on the GPU. See code for details.

    @param[in]
    lddf    INTEGER
            The leading dimension of the array dF. LDDF >= max(1,N).

    @ingroup magma_laqps
*******************************************************************************/
extern "C" magma_int_t
magma_dlaqps(
    magma_int_t m, magma_int_t n, magma_int_t offset,
    magma_int_t nb, magma_int_t *kb,
    double     *A, magma_int_t lda,
    magmaDouble_ptr dA, magma_int_t ldda,
    magma_int_t *jpvt, double *tau, double *vn1, double *vn2,
    double *auxv,
    double     *F, magma_int_t ldf,
    magmaDouble_ptr dF, magma_int_t lddf)
{
#define  A(i, j) (A  + (i) + (j)*(lda ))
#define dA(i, j) (dA + (i) + (j)*(ldda))
#define  F(i, j) (F  + (i) + (j)*(ldf ))
#define dF(i, j) (dF + (i) + (j)*(lddf))

    double c_zero    = MAGMA_D_MAKE( 0.,0.);
    double c_one     = MAGMA_D_MAKE( 1.,0.);
    double c_neg_one = MAGMA_D_MAKE(-1.,0.);
    magma_int_t ione = 1;
    
    magma_int_t i__1, i__2;
    double d__1;
    double z__1;
    
    magma_int_t j, k, rk;
    double Akk;
    magma_int_t pvt;
    double temp, temp2, tol3z;
    magma_int_t itemp;

    magma_int_t lsticc;
    magma_int_t lastrk;

    lastrk = min( m, n + offset );
    tol3z = magma_dsqrt( lapackf77_dlamch("Epsilon"));

    magma_queue_t queue;
    magma_device_t cdev;
    magma_getdevice( &cdev );
    magma_queue_create( cdev, &queue );

    lsticc = 0;
    k = 0;
    while( k < nb && lsticc == 0 ) {
        rk = offset + k;
        
        /* Determine ith pivot column and swap if necessary */
        // subtract 1 from Fortran idamax; pvt, k are 0-based.
        i__1 = n-k;
        pvt = k + blasf77_idamax( &i__1, &vn1[k], &ione ) - 1;
        
        if (pvt != k) {
            if (pvt >= nb) {
                /* 1. Start copy from GPU                           */
                magma_dgetmatrix_async( m - offset - nb, 1,
                                        dA(offset + nb, pvt), ldda,
                                        A (offset + nb, pvt), lda, queue );
            }

            /* F gets swapped so F must be sent at the end to GPU   */
            i__1 = k;
            blasf77_dswap( &i__1, F(pvt,0), &ldf, F(k,0), &ldf );
            itemp     = jpvt[pvt];
            jpvt[pvt] = jpvt[k];
            jpvt[k]   = itemp;
            vn1[pvt] = vn1[k];
            vn2[pvt] = vn2[k];

            if (pvt < nb) {
                /* no need of transfer if pivot is within the panel */
                blasf77_dswap( &m, A(0, pvt), &ione, A(0, k), &ione );
            }
            else {
                /* 1. Finish copy from GPU                          */
                magma_queue_sync( queue );

                /* 2. Swap as usual on CPU                          */
                blasf77_dswap(&m, A(0, pvt), &ione, A(0, k), &ione);

                /* 3. Restore the GPU                               */
                magma_dsetmatrix_async( m - offset - nb, 1,
                                        A (offset + nb, pvt), lda,
                                        dA(offset + nb, pvt), ldda, queue );
            }
        }

        /* Apply previous Householder reflectors to column K:
           A(RK:M,K) := A(RK:M,K) - A(RK:M,1:K-1)*F(K,1:K-1)'.
           Optimization: multiply with beta=0; wait for vector and subtract */
        if (k > 0) {
            #ifdef COMPLEX
            for (j = 0; j < k; ++j) {
                *F(k,j) = MAGMA_D_CONJ( *F(k,j) );
            }
            #endif

            i__1 = m - rk;
            i__2 = k;
            blasf77_dgemv( MagmaNoTransStr, &i__1, &i__2,
                           &c_neg_one, A(rk, 0), &lda,
                                       F(k,  0), &ldf,
                           &c_one,     A(rk, k), &ione );

            #ifdef COMPLEX
            for (j = 0; j < k; ++j) {
                *F(k,j) = MAGMA_D_CONJ( *F(k,j) );
            }
            #endif
        }
        
        /*  Generate elementary reflector H(k). */
        if (rk < m-1) {
            i__1 = m - rk;
            lapackf77_dlarfg( &i__1, A(rk, k), A(rk + 1, k), &ione, &tau[k] );
        } else {
            lapackf77_dlarfg( &ione, A(rk, k), A(rk, k), &ione, &tau[k] );
        }
        
        Akk = *A(rk, k);
        *A(rk, k) = c_one;

        /* Compute Kth column of F:
           Compute  F(K+1:N,K) := tau(K)*A(RK:M,K+1:N)'*A(RK:M,K) on the GPU */
        if (k < n-1) {
            i__1 = m - rk;
            i__2 = n - k - 1;
        
            /* Send the vector to the GPU */
            magma_dsetmatrix( i__1, 1, A(rk, k), lda, dA(rk,k), ldda, queue );
        
            /* Multiply on GPU */
            // was CALL DGEMV( 'Conjugate transpose', M-RK+1, N-K,
            //                 TAU( K ), A( RK,  K+1 ), LDA,
            //                           A( RK,  K   ), 1,
            //                 CZERO,    F( K+1, K   ), 1 )
            magma_int_t i__3 = nb-k-1;
            magma_int_t i__4 = i__2 - i__3;
            magma_int_t i__5 = nb-k;
            magma_dgemv( MagmaConjTrans, i__1 - i__5, i__2 - i__3,
                         tau[k], dA(rk +i__5, k+1+i__3), ldda,
                                 dA(rk +i__5, k       ), ione,
                         c_zero, dF(k+1+i__3, k       ), ione, queue );
            
            magma_dgetmatrix_async( i__2-i__3, 1,
                                    dF(k + 1 +i__3, k), i__2,
                                    F (k + 1 +i__3, k), i__2, queue );
            
            blasf77_dgemv( MagmaConjTransStr, &i__1, &i__3,
                           &tau[k], A(rk,  k+1), &lda,
                                    A(rk,  k  ), &ione,
                           &c_zero, F(k+1, k  ), &ione );
            
            magma_queue_sync( queue );
            blasf77_dgemv( MagmaConjTransStr, &i__5, &i__4,
                           &tau[k], A(rk, k+1+i__3), &lda,
                                    A(rk, k       ), &ione,
                           &c_one,  F(k+1+i__3, k ), &ione );
        }
        
        /* Padding F(1:K,K) with zeros. */
        for (j = 0; j < k; ++j) {
            *F(j, k) = c_zero;
        }
        
        /* Incremental updating of F:
           F(1:N,K) := F(1:N,K) - tau(K)*F(1:N,1:K-1)*A(RK:M,1:K-1)'*A(RK:M,K). */
        if (k > 0) {
            i__1 = m - rk;
            i__2 = k;
            z__1 = MAGMA_D_NEGATE( tau[k] );
            blasf77_dgemv( MagmaConjTransStr, &i__1, &i__2,
                           &z__1,   A(rk, 0), &lda,
                                    A(rk, k), &ione,
                           &c_zero, auxv, &ione );
            
            i__1 = k;
            blasf77_dgemv( MagmaNoTransStr, &n, &i__1,
                           &c_one, F(0,0), &ldf,
                                   auxv,   &ione,
                           &c_one, F(0,k), &ione );
        }
        
        /* Optimization: On the last iteration start sending F back to the GPU */
        
        /* Update the current row of A:
           A(RK,K+1:N) := A(RK,K+1:N) - A(RK,1:K)*F(K+1:N,1:K)'.               */
        if (k < n-1) {
            i__1 = n - k - 1;
            i__2 = k + 1;
            blasf77_dgemm( MagmaNoTransStr, MagmaConjTransStr, &ione, &i__1, &i__2,
                           &c_neg_one, A(rk, 0  ), &lda,
                                       F(k+1,0  ), &ldf,
                           &c_one,     A(rk, k+1), &lda );
        }
        
        /* Update partial column norms. */
        if (rk < lastrk) {
            for (j = k + 1; j < n; ++j) {
                if (vn1[j] != 0.) {
                    /* NOTE: The following 4 lines follow from the analysis in
                       Lapack Working Note 176. */
                    temp = MAGMA_D_ABS( *A(rk,j) ) / vn1[j];
                    temp = max( 0., ((1. + temp) * (1. - temp)) );
        
                    d__1 = vn1[j] / vn2[j];
                    temp2 = temp * (d__1 * d__1);
        
                    if (temp2 <= tol3z) {
                        vn2[j] = (double) lsticc;
                        lsticc = j;
                    } else {
                        vn1[j] *= magma_dsqrt(temp);
                    }
                }
            }
        }
        
        *A(rk, k) = Akk;
        
        ++k;
    }
    // leave k as the last column done
    --k;
    *kb = k + 1;
    rk = offset + *kb - 1;

    /* Apply the block reflector to the rest of the matrix:
       A(OFFSET+KB+1:M,KB+1:N) := A(OFFSET+KB+1:M,KB+1:N) - A(OFFSET+KB+1:M,1:KB)*F(KB+1:N,1:KB)'  */
    if (*kb < min(n, m - offset)) {
        i__1 = m - rk - 1;
        i__2 = n - *kb;
        
        /* Send F to the GPU */
        magma_dsetmatrix( i__2, *kb,
                          F (*kb, 0), ldf,
                          dF(*kb, 0), i__2, queue );

        magma_dgemm( MagmaNoTrans, MagmaConjTrans, i__1, i__2, *kb,
                     c_neg_one, dA(rk+1, 0  ), ldda,
                                dF(*kb,  0  ), i__2,
                     c_one,     dA(rk+1, *kb), ldda, queue );
    }
    
    /* Recomputation of difficult columns. */
    while( lsticc > 0 ) {
        itemp = (magma_int_t)(vn2[lsticc] >= 0. ? floor(vn2[lsticc] + .5) : -floor(.5 - vn2[lsticc]));
        i__1 = m - rk - 1;
        if (lsticc <= nb) {
            vn1[lsticc] = magma_cblas_dnrm2( i__1, A(rk+1,lsticc), ione );
        }
        else {
            /* Where is the data, CPU or GPU ? */
            double r1, r2;
            
            r1 = magma_cblas_dnrm2( nb-k, A(rk+1,lsticc), ione );
            r2 = magma_dnrm2( m-offset-nb, dA(offset + nb + 1, lsticc), ione, queue );
            
            //vn1[lsticc] = magma_dnrm2( i__1, dA(rk + 1, lsticc), ione, queue );
            vn1[lsticc] = magma_dsqrt(r1*r1 + r2*r2);
        }
        
        /* NOTE: The computation of VN1( LSTICC ) relies on the fact that
           SNRM2 does not fail on vectors with norm below the value of SQRT(DLAMCH('S')) */
        vn2[lsticc] = vn1[lsticc];
        lsticc = itemp;
    }
    
    magma_queue_destroy( queue );

    return MAGMA_SUCCESS;
} /* magma_dlaqps */
