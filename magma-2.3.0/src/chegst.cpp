/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017

       @author Raffaele Solca
       @author Azzam Haidar
       @author Mark Gates

       @generated from src/zhegst.cpp, normal z -> c, Wed Nov 15 00:34:20 2017
*/

#include "magma_internal.h"

/***************************************************************************//**
    Purpose
    -------
    CHEGST reduces a complex Hermitian-definite generalized
    eigenproblem to standard form.
    
    If ITYPE = 1, the problem is A*x = lambda*B*x,
    and A is overwritten by inv(U^H)*A*inv(U) or inv(L)*A*inv(L^H)
    
    If ITYPE = 2 or 3, the problem is A*B*x = lambda*x or
    B*A*x = lambda*x, and A is overwritten by U*A*U^H or L^H*A*L.
    
    B must have been previously factorized as U^H*U or L*L^H by CPOTRF.
    
    Arguments
    ---------
    @param[in]
    itype   INTEGER
            = 1: compute inv(U^H)*A*inv(U) or inv(L)*A*inv(L^H);
            = 2 or 3: compute U*A*U^H or L^H*A*L.
    
    @param[in]
    uplo    magma_uplo_t
      -     = MagmaUpper:  Upper triangle of A is stored and B is factored as U^H*U;
      -     = MagmaLower:  Lower triangle of A is stored and B is factored as L*L^H.
    
    @param[in]
    n       INTEGER
            The order of the matrices A and B.  N >= 0.
    
    @param[in,out]
    A       COMPLEX array, dimension (LDA,N)
            On entry, the Hermitian matrix A.  If UPLO = MagmaUpper, the leading
            N-by-N upper triangular part of A contains the upper
            triangular part of the matrix A, and the strictly lower
            triangular part of A is not referenced.  If UPLO = MagmaLower, the
            leading N-by-N lower triangular part of A contains the lower
            triangular part of the matrix A, and the strictly upper
            triangular part of A is not referenced.
    \n
            On exit, if INFO = 0, the transformed matrix, stored in the
            same format as A.
    
    @param[in]
    lda     INTEGER
            The leading dimension of the array A.  LDA >= max(1,N).
    
    @param[in,out]
    B       COMPLEX array, dimension (LDB,N)
            The triangular factor from the Cholesky factorization of B,
            as returned by CPOTRF.
            
            B is modified by the routine but restored on exit (in lapack chegst/chegs2).
    
    @param[in]
    ldb     INTEGER
            The leading dimension of the array B.  LDB >= max(1,N).
    
    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value

    @ingroup magma_hegst
*******************************************************************************/
extern "C" magma_int_t
magma_chegst(
    magma_int_t itype, magma_uplo_t uplo, magma_int_t n,
    magmaFloatComplex *A, magma_int_t lda,
    magmaFloatComplex *B, magma_int_t ldb,
    magma_int_t *info)
{
    #define A(i_, j_) (A + (i_) + (j_)*lda)
    #define B(i_, j_) (B + (i_) + (j_)*ldb)
    
    #define dA(i_, j_) (dwork + (i_) + (j_)*ldda         )
    #define dB(i_, j_) (dwork + (i_) + (j_)*lddb + n*ldda)

    /* Constants */
    const magmaFloatComplex c_one      = MAGMA_C_ONE;
    const magmaFloatComplex c_neg_one  = MAGMA_C_NEG_ONE;
    const magmaFloatComplex c_half     = MAGMA_C_HALF;
    const magmaFloatComplex c_neg_half = MAGMA_C_NEG_HALF;
    const float             d_one      = 1.0;
    
    /* Local variables */
    const char* uplo_ = lapack_uplo_const( uplo );
    magma_int_t k, kb, kb2, nb;
    magma_int_t ldda = n;
    magma_int_t lddb = n;
    magmaFloatComplex_ptr dwork;
    bool upper = (uplo == MagmaUpper);
    
    /* Test the input parameters. */
    *info = 0;
    if (itype < 1 || itype > 3) {
        *info = -1;
    } else if (! upper && uplo != MagmaLower) {
        *info = -2;
    } else if (n < 0) {
        *info = -3;
    } else if (lda < max(1,n)) {
        *info = -5;
    } else if (ldb < max(1,n)) {
        *info = -7;
    }
    
    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }
    
    /* Quick return */
    if ( n == 0 )
        return *info;
    
    if (MAGMA_SUCCESS != magma_cmalloc( &dwork, 2*n*n )) {
        *info = MAGMA_ERR_DEVICE_ALLOC;
        return *info;
    }
    
    nb = magma_get_chegst_nb( n );
    
    magma_queue_t queues[2];
    magma_device_t cdev;
    magma_getdevice( &cdev );
    magma_queue_create( cdev, &queues[0] );
    magma_queue_create( cdev, &queues[1] );
    
    magma_csetmatrix( n, n, A(0, 0), lda, dA(0, 0), ldda, queues[1] );
    magma_csetmatrix( n, n, B(0, 0), ldb, dB(0, 0), lddb, queues[1] );
    
    /* Use hybrid blocked code */
    if (itype == 1) {
        if (upper) {
            /* Compute inv(U^H)*A*inv(U) */
            for (k = 0; k < n; k += nb) {
                kb  = min( n-k,    nb );
                kb2 = min( n-k-nb, nb );
                
                /* Update the upper triangle of A(k:n,k:n) */
                lapackf77_chegst( &itype, uplo_, &kb, A(k,k), &lda, B(k,k), &ldb, info );
                
                magma_csetmatrix_async( kb, kb,
                                         A(k, k), lda,
                                        dA(k, k), ldda, queues[0] );
                
                if (k+kb < n) {
                    magma_ctrsm( MagmaLeft, MagmaUpper, MagmaConjTrans, MagmaNonUnit,
                                 kb, n-k-kb,
                                 c_one, dB(k,k),    lddb,
                                        dA(k,k+kb), ldda, queues[1] );
                    
                    magma_queue_sync( queues[0] );  // finish set dA(k,k)
                    
                    magma_chemm( MagmaLeft, MagmaUpper,
                                 kb, n-k-kb,
                                 c_neg_half, dA(k,k),    ldda,
                                             dB(k,k+kb), lddb,
                                 c_one,      dA(k,k+kb), ldda, queues[1] );
                    
                    magma_cher2k( MagmaUpper, MagmaConjTrans,
                                  n-k-kb, kb,
                                  c_neg_one, dA(k,k+kb),    ldda,
                                             dB(k,k+kb),    lddb,
                                  d_one,     dA(k+kb,k+kb), ldda, queues[1] );
                    
                    // Start copying next A block
                    magma_queue_sync( queues[1] );
                    magma_cgetmatrix_async( kb2, kb2,
                                            dA(k+kb, k+kb), ldda,
                                             A(k+kb, k+kb),  lda, queues[0] );
                    
                    magma_chemm( MagmaLeft, MagmaUpper,
                                 kb, n-k-kb,
                                 c_neg_half, dA(k,k),    ldda,
                                             dB(k,k+kb), lddb,
                                 c_one,      dA(k,k+kb), ldda, queues[1] );
                    
                    magma_ctrsm( MagmaRight, MagmaUpper, MagmaNoTrans, MagmaNonUnit,
                                 kb, n-k-kb,
                                 c_one, dB(k+kb,k+kb), lddb,
                                        dA(k,k+kb),    ldda, queues[1] );
                    
                    magma_queue_sync( queues[0] );  // finish get A(k+kb,k+kb)
                }
            }
        }
        else {
            /* Compute inv(L)*A*inv(L^H) */
            for (k = 0; k < n; k += nb) {
                kb  = min( n-k,    nb );
                kb2 = min( n-k-nb, nb );
                
                /* Update the lower triangle of A(k:n,k:n) */
                lapackf77_chegst( &itype, uplo_, &kb, A(k,k), &lda, B(k,k), &ldb, info );
                
                magma_csetmatrix_async( kb, kb,
                                         A(k, k), lda,
                                        dA(k, k), ldda, queues[0] );
                
                if (k+kb < n) {
                    magma_ctrsm( MagmaRight, MagmaLower, MagmaConjTrans, MagmaNonUnit,
                                 n-k-kb, kb,
                                 c_one, dB(k,k),    lddb,
                                        dA(k+kb,k), ldda, queues[1] );
                    
                    magma_queue_sync( queues[0] );  // finish set dA(k,k)
                    
                    magma_chemm( MagmaRight, MagmaLower,
                                 n-k-kb, kb,
                                 c_neg_half, dA(k,k),     ldda,
                                             dB(k+kb,k),  lddb,
                                 c_one,      dA(k+kb, k), ldda, queues[1] );
                    
                    magma_cher2k( MagmaLower, MagmaNoTrans,
                                  n-k-kb, kb,
                                  c_neg_one, dA(k+kb,k),    ldda,
                                             dB(k+kb,k),    lddb,
                                  d_one,     dA(k+kb,k+kb), ldda, queues[1] );
                    
                    // Start copying next A block
                    magma_queue_sync( queues[1] );
                    magma_cgetmatrix_async( kb2, kb2,
                                            dA(k+kb, k+kb), ldda,
                                             A(k+kb, k+kb), lda, queues[0] );
                    
                    magma_chemm( MagmaRight, MagmaLower,
                                 n-k-kb, kb,
                                 c_neg_half, dA(k,k),     ldda,
                                             dB(k+kb,k),  lddb,
                                 c_one,      dA(k+kb, k), ldda, queues[1] );
                    
                    magma_ctrsm( MagmaLeft, MagmaLower, MagmaNoTrans, MagmaNonUnit,
                                 n-k-kb, kb,
                                 c_one, dB(k+kb,k+kb), lddb,
                                        dA(k+kb,k),    ldda, queues[1] );
                    
                    magma_queue_sync( queues[0] );  // finish get A(k+kb,k+kb)
                }
            }
        }
    }
    else {  // itype == 2 or 3
        if (upper) {
            /* Compute U*A*U^H */
            for (k = 0; k < n; k += nb) {
                kb = min( n-k, nb );
                
                magma_cgetmatrix_async( kb, kb,
                                        dA(k, k), ldda,
                                         A(k, k),  lda, queues[0] );
                
                /* Update the upper triangle of A(1:k+kb-1,1:k+kb-1) */
                if (k > 0) {
                    magma_ctrmm( MagmaLeft, MagmaUpper, MagmaNoTrans, MagmaNonUnit,
                                 k, kb,
                                 c_one, dB(0,0), lddb,
                                        dA(0,k), ldda, queues[1] );
                    
                    magma_chemm( MagmaRight, MagmaUpper,
                                 k, kb,
                                 c_half, dA(k,k), ldda,
                                         dB(0,k), lddb,
                                 c_one,  dA(0,k), ldda, queues[1] );
                    
                    magma_cher2k( MagmaUpper, MagmaNoTrans,
                                  k, kb,
                                  c_one, dA(0,k), ldda,
                                         dB(0,k), lddb,
                                  d_one, dA(0,0), ldda, queues[1] );
                    
                    magma_chemm( MagmaRight, MagmaUpper,
                                 k, kb,
                                 c_half, dA(k,k), ldda,
                                         dB(0,k), lddb,
                                 c_one,  dA(0,k), ldda, queues[1] );
                    
                    magma_ctrmm( MagmaRight, MagmaUpper, MagmaConjTrans, MagmaNonUnit,
                                 k, kb,
                                 c_one, dB(k,k), lddb,
                                        dA(0,k), ldda, queues[1] );
                }
                
                magma_queue_sync( queues[0] );  // finish get A(k,k)
                
                lapackf77_chegst( &itype, uplo_, &kb, A(k, k), &lda, B(k, k), &ldb, info );

                // this could be done on a 3rd queue
                magma_csetmatrix_async( kb, kb,
                                         A(k, k), lda,
                                        dA(k, k), ldda, queues[1] );
            }
        }
        else {
            /* Compute L^H*A*L */
            for (k = 0; k < n; k += nb) {
                kb = min( n-k, nb );
                
                magma_cgetmatrix_async( kb, kb,
                                        dA(k, k), ldda,
                                         A(k, k),  lda, queues[0] );
                
                /* Update the lower triangle of A(1:k+kb-1,1:k+kb-1) */
                if (k > 0) {
                    magma_ctrmm( MagmaRight, MagmaLower, MagmaNoTrans, MagmaNonUnit,
                                 kb, k,
                                 c_one, dB(0,0), lddb,
                                        dA(k,0), ldda, queues[1] );
                    
                    magma_chemm( MagmaLeft, MagmaLower,
                                 kb, k,
                                 c_half, dA(k,k),  ldda,
                                         dB(k,0),  lddb,
                                 c_one,  dA(k, 0), ldda, queues[1] );
                    
                    magma_cher2k( MagmaLower, MagmaConjTrans,
                                  k, kb,
                                  c_one, dA(k,0), ldda,
                                         dB(k,0), lddb,
                                  d_one, dA(0,0), ldda, queues[1] );
                    
                    magma_chemm( MagmaLeft, MagmaLower,
                                 kb, k,
                                 c_half, dA(k,k),  ldda,
                                         dB(k,0),  lddb,
                                 c_one,  dA(k, 0), ldda, queues[1] );
                    
                    magma_ctrmm( MagmaLeft, MagmaLower, MagmaConjTrans, MagmaNonUnit,
                                 kb, k,
                                 c_one, dB(k,k), lddb,
                                        dA(k,0), ldda, queues[1] );
                }
                
                magma_queue_sync( queues[0] );  // finish get A(k,k)
                
                lapackf77_chegst( &itype, uplo_, &kb, A(k,k), &lda, B(k,k), &ldb, info );

                // this could be done on a 3rd queue
                magma_csetmatrix_async( kb, kb,
                                         A(k, k), lda,
                                        dA(k, k), ldda, queues[1] );
            }
        }
    }
    
    magma_queue_sync( queues[0] );  // finish set dA(k,k) for itype 1
    magma_cgetmatrix( n, n, dA(0, 0), ldda, A(0, 0), lda, queues[1] );
    
    magma_queue_destroy( queues[0] );
    magma_queue_destroy( queues[1] );
    
    magma_free( dwork );
    
    return *info;
} /* magma_chegst_gpu */

#undef A
#undef B
#undef dA
#undef dB
