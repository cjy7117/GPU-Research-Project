/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017

       @author Stan Tomov
       @generated from src/zhetrf_aasen.cpp, normal z -> c, Wed Nov 15 00:34:19 2017
*/
#include "magma_internal.h"
#include "trace.h"

#define COMPLEX


/***************************************************************************//**
    Purpose
    =======
 
    CHETRF_AASEN computes the factorization of a complex Hermitian matrix A
    based on a communication-avoiding variant of the Aasen's algorithm .  
    The form of the factorization is
 
     A = U*D*U**H  or  A = L*D*L**H
 
    where U (or L) is a product of permutation and unit upper (lower)
    triangular matrices, and D is Hermitian and banded matrix of the
    band width equal to the block size.

    Arguments
    ---------
    @param[in]
    uplo    magma_uplo_t
      -     = MagmaUpper:  Upper triangle of A is stored;
      -     = MagmaLower:  Lower triangle of A is stored.
 
    @param[in]
    cpu_panel INTEGER
              If cpu_panel =0, panel factorization is done on GPU.
  
    @param[in]
    n       INTEGER
            The order of the matrix A.  N >= 0.
  
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
            On exit, the banded matrix D and the triangular factor U or L.
 
    @param[in]
    lda     INTEGER
            The leading dimension of the array A.  LDA >= max(1,N).
 
    @param[out]
    ipiv    INTEGER array, dimension (N)
            Details of the interchanges.

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value
      -     > 0:  if INFO = i, D(i,i) is exactly zero.  The factorization
                  has been completed, but the block diagonal matrix D is
                  exactly singular, and division by zero will occur if it
                  is used to solve a system of equations.
    @ingroup magma_hetrf_aasen
*******************************************************************************/
extern "C" magma_int_t
magma_chetrf_aasen(magma_uplo_t uplo, magma_int_t cpu_panel, magma_int_t n,
                   magmaFloatComplex *A, magma_int_t lda, 
                   magma_int_t *ipiv, magma_int_t *info)
{
#define A(i, j)  (A    + (j)*nb*lda  + (i)*nb)
#define dA(i, j) (dwork + (j)*nb*ldda + (i)*nb)
#define dT(i, j) (dwork + (j)*nb*ldda + (i)*nb)
#define dL(i, j) ((i == j) ? (dL + (i)*nb) : (dwork + (j-1)*nb*ldda + (i)*nb))
#define dH(i, j) (dH   + (i)*nb)
#define dW(i)    (dW   + (i)*nb*nb)
#define dX(i)    (dX   + (i)*nb*nb)
#define dY(i)    (dY   + (i)*nb*nb)
//#define dW(i)    (dW   + (i)*nb)

#define da(i, j) (dwork + (j)*ldda + (i))
#define dw(i)    (dW   + (i))
#define dl(i,j)  (dwork + (i) + (j)*ldda)

    /* Constants */
    const float d_one  = 1.0;
    const magmaFloatComplex c_one  = MAGMA_C_ONE;
    const magmaFloatComplex c_zero = MAGMA_C_ZERO;
    const magmaFloatComplex c_neg_one = MAGMA_C_NEG_ONE;
    const magmaFloatComplex c_half = MAGMA_C_MAKE(0.5, 0.0);
    
    /* Local variables */
    magma_int_t        ldda, iinfo;
    magmaFloatComplex_ptr dwork, dH, dW, dX, dY, dL;
    magma_int_t nb = magma_get_chetrf_aasen_nb(n);
    bool upper = (uplo == MagmaUpper);

    *info = 0;
    // if (! upper && uplo != MagmaLower) {
    //     *info = -1;
    if (uplo != MagmaLower) {
        *info = MAGMA_ERR_NOT_IMPLEMENTED;
    } else if (n < 0) {
        *info = -2;
    } else if (lda < max(1,n)) {
        *info = -4;
    }
    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    /* Quick return */
    if ( n == 0 )
        return *info;

    magma_int_t num_queues = 3;
    magma_queue_t queues[5];
    magma_event_t events[5];

    magma_device_t cdev;
    magma_getdevice( &cdev );
    for (magma_int_t i=0; i < num_queues; i++) {
        magma_queue_create( cdev, &queues[i] );
        magma_event_create( &events[i]  );
    }

    /* TODO fix memory leaks, e.g., if last malloc fails */
    magma_int_t lddw = nb*(1+magma_ceildiv(n, nb));
    ldda = magma_roundup(n, 32);
    if (MAGMA_SUCCESS != magma_cmalloc( &dwork, magma_roundup(n, nb) *ldda ) ||
        MAGMA_SUCCESS != magma_cmalloc( &dH,   (2*nb)*ldda ) ||
        MAGMA_SUCCESS != magma_cmalloc( &dL,   nb*ldda ) ||
        MAGMA_SUCCESS != magma_cmalloc( &dX,   nb*lddw ) ||
        MAGMA_SUCCESS != magma_cmalloc( &dY,   nb*lddw ) ||
        MAGMA_SUCCESS != magma_cmalloc( &dW,   nb*lddw )) {
        /* alloc failed for workspace */
        *info = MAGMA_ERR_DEVICE_ALLOC;
        return *info;
    }
    lddw = nb;

    /* permutation info */
    magma_int_t *perm, *rows;
    magma_int_t *drows, *dperm;
    if (MAGMA_SUCCESS != magma_imalloc_cpu(&perm, n) ||
        MAGMA_SUCCESS != magma_imalloc_pinned(&rows, 2*(2*nb)) ||
        MAGMA_SUCCESS != magma_imalloc( &drows, 2*(2*nb) ) ||
        MAGMA_SUCCESS != magma_imalloc( &dperm, n ) ) {
        /* alloc failed for perm info */
        *info = MAGMA_ERR_DEVICE_ALLOC;
        return *info;
    }

    magma_int_t *dinfo_magma, *dipiv_magma;
    magmaFloatComplex **dA_array = NULL;
    magma_int_t **dipiv_array = NULL;
    if (MAGMA_SUCCESS != magma_imalloc( &dipiv_magma, nb) ||
        MAGMA_SUCCESS != magma_imalloc( &dinfo_magma, 1) ||
        MAGMA_SUCCESS != magma_malloc( (void**)&dA_array, sizeof(*dA_array) ) ||
        MAGMA_SUCCESS != magma_malloc( (void**)&dipiv_array, sizeof(*dipiv_array))) {
        /* alloc failed for perm info */
        *info = MAGMA_ERR_DEVICE_ALLOC;
        return *info;
    }

    for (magma_int_t ii=0; ii < n; ii++) {
        perm[ii] = ii;
    }
    magma_isetvector_async(n, perm, 1, dperm, 1, queues[0]);

    /* copy A to GPU */
    magma_csetmatrix_async( n, n, A(0,0), lda, dA(0,0), ldda, queues[0] );
    for (magma_int_t j=0; j < min(n,nb); j++) {
        ipiv[j] = j+1;
    }

    trace_init( 1, 1, num_queues, queues );
    //if (nb <= 1 || nb >= n) {
    //    lapackf77_cpotrf(uplo_, &n, A, &lda, info);
    //} else 
    {
        /* Use hybrid blocked code. */
        if (upper) {
        }
        else {
            //=========================================================
            // Compute the Aasen's factorization P*A*P' = L*T*L'.
            for (magma_int_t j=0; j < magma_ceildiv(n, nb); j ++) {
                magma_int_t jb = min(nb, (n-j*nb));

                // Compute off-diagonal blocks of H(:,j), 
                // i.e., H(i,j) = T(i,i-1)*L(j,i-1)' + T(i,i)*L(j,i)' + T(i,i+1)*L(j,i+1)'
                // H(0,j) and W(0) is not needed since they are multiplied with L(2:N,1)
                // * make sure queues[1] does not start before queues[0] finish everything
                magma_event_record( events[1], queues[0] );
                magma_queue_wait_event( queues[1], events[1] );
                //
                trace_gpu_start( 0, 0, "gemm", "compH" );
                trace_gpu_start( 0, 1, "gemm", "compH" );
                for (magma_int_t i=1; i < j; i++)
                {
                    //printf( " > compute H(%lld,%lld)\n", (long long) i, (long long) j );
                    // > H(i,j) = T(i,i) * L(j,i)', Y
                    magma_cgemm( MagmaNoTrans, MagmaConjTrans,
                                 nb, jb, nb,
                                 c_one,  dT(i,i), ldda,
                                         dL(j,i), ldda,
                                 c_zero, dX(i),   nb,
                                 queues[0] );
                                 //c_zero, dW(i+1), lddw);
                    // > W(i) = T(i,i+1) * L(j,i+1)', Z
                    // W(i) = L(j,i+1)'
                    magma_cgemm( MagmaConjTrans, MagmaConjTrans,
                                 nb, jb, (i < j-1 ? nb : jb),
                                 c_one,  dT(i+1,i), ldda,
                                         dL(j,i+1), ldda,
                                 c_zero, dH(i,j),   ldda,
                                 queues[1] );
                }
                // * insert event to keep track 
                magma_event_record( events[0], queues[0] );
                magma_event_record( events[1], queues[1] );
                // * make sure they are done
                magma_queue_wait_event( queues[0], events[1] );
                magma_queue_wait_event( queues[1], events[0] );
                for (magma_int_t i=1; i < j; i++)
                {
                    // H(i,j) = W(i)+.5*H(i,j)
                    magmablas_cgeadd( nb, jb, 
                                      c_one, dX(i),   nb,
                                             dH(i,j), ldda,
                                      queues[(i-1)%2] );
                    // copy to dY to compute dW
                    magma_ccopymatrix( nb, jb, dH(i,j), ldda, dY(i), nb,
                                       queues[(i-1)%2] );
                }
                // * insert event back to keep track
                magma_event_record( events[0], queues[0] );
                magma_event_record( events[1], queues[1] );
                // * make sure they are done
                magma_queue_wait_event( queues[0], events[1] );
                magma_queue_wait_event( queues[1], events[0] );
                for (magma_int_t i=1; i < j; i++)
                {
                    // W(i) += .5*H(i,j)
                    magmablas_cgeadd(nb, jb, 
                                    -c_half, dX(i), nb,
                                             dY(i), nb,
                                    queues[(i-1)%2]);
                    // transpose W for calling cher2k
                    #ifdef COMPLEX
                    magmablas_ctranspose_conj( nb, jb, dY(i), nb, dW(i), lddw, queues[(i-1)%2] );
                    #else
                    magmablas_ctranspose( nb, jb, dY(i), nb, dW(i), lddw, queues[(i-1)%2] );
                    #endif

                    // > H(i,j) += T(i,i-1) * L(j,i-1)', X
                    if (i > 1) // if i == 1, then L(j,i-1) = 0
                    {
                        // W(i+1) = T(i,i-1)*L(j,i-1)'
                        magma_cgemm( MagmaNoTrans, MagmaConjTrans,
                                     nb, jb, nb,
                                     c_one, dT(i,i-1), ldda,
                                            dL(j,i-1), ldda,
                                     c_one, dH(i,j),   ldda,
                                     queues[(i-1)%2] );
                    }
                }
                trace_gpu_end( 0,0 );
                trace_gpu_end( 0,1 );
                // * insert event back to keep track
                magma_event_record( events[0], queues[0] );
                magma_event_record( events[1], queues[1] );
                // * make sure they are done
                magma_queue_wait_event( queues[0], events[1] );

                // compute T(j, j) = A(j,j) - L(j,1:j)*H(1:j,j) (where T is A in memory)
                trace_gpu_start( 0, 0, "her2k", "compTjj" );
                if (j > 1)
                magma_cher2k( MagmaLower, MagmaNoTrans,
                              jb, (j-1)*nb,
                              c_neg_one, dL(j,1), ldda,
                                         dW(1),   lddw,
                              d_one,     dT(j,j), ldda,
                              queues[0] );
                magmablas_csymmetrize(MagmaLower, jb, dT(j,j), ldda, queues[0]);
                trace_gpu_end( 0,0 );
                // > Compute T(j,j) - L(j,j)^-1 T(j,j) L^(j,j)^-T
                trace_gpu_start( 0, 0, "trsm", "compTjj" );
                if (j > 0) // if j == 1, then L(j,j) = I
                {
                    magma_ctrsm( MagmaLeft, MagmaLower, MagmaNoTrans, MagmaUnit,
                                 jb, jb,
                                 c_one, dL(j,j), ldda,
                                        dT(j,j), ldda,
                                 queues[0] );
                    magma_ctrsm( MagmaRight, MagmaLower, MagmaConjTrans, MagmaUnit,
                                 jb, jb,
                                 c_one, dL(j,j), ldda,
                                        dT(j,j), ldda,
                                 queues[0] );
                }
                trace_gpu_end( 0,0 );
                if (j < magma_ceildiv(n, nb)-1)
                {
                    // ** Panel + Update **
                    magma_int_t ib = n-(j+1)*nb;
                    // compute H(j,j)
                    // > H(j,j) = T(j,j)*L(j,j)'
                    //   H(0,0) is not needed since it is multiplied with L(j+1:n,0)
                    trace_gpu_start( 0, 0, "trmm", "compHjj" );
                    if (j >= 1)
                    {
                        magma_cgemm( MagmaNoTrans, MagmaConjTrans,
                                     jb, jb, nb,
                                     c_one,  dT(j,j), ldda,
                                             dL(j,j), ldda,
                                     c_zero, dH(j,j), ldda,
                                     queues[0] );
                        if (j >= 2)
                        {
                            // > H(j,j) += T(j,j-1)*L(j,j-1)
                            magma_cgemm( MagmaNoTrans, MagmaConjTrans,
                                         jb, jb, nb,
                                         c_one, dT(j,j-1), ldda,
                                                dL(j,j-1), ldda,
                                         c_one, dH(j,j),   ldda,
                                         queues[0] );
                        }
                    }
                    trace_gpu_end( 0,0 );
                    // extract L(:, j+1)
                    trace_gpu_start( 0, 0, "gemm", "compLj" );
                    magma_cgemm( MagmaNoTrans, MagmaNoTrans,
                                 ib, jb, j*nb,
                                 c_neg_one, dL(j+1,1), ldda,
                                            dH(  1,j), ldda,
                                 c_one,     dA(j+1,j), ldda,
                                 queues[0] );
                    trace_gpu_end( 0,0 );

                    // panel factorization
                    if (cpu_panel || j < 2) {
                        // copy panel to CPU
                        magma_cgetmatrix_async( ib, jb,
                                                dA(j+1,j), ldda,
                                                 A(j+1,j), lda, queues[0] );
                        // panel factorization on CPU
                        magma_queue_sync( queues[0] );
                        trace_cpu_start( 0, "getrf", "getrf" );
                        lapackf77_cgetrf( &ib, &jb, A(j+1,j), &lda, &ipiv[(1+j)*nb], &iinfo);
                        if (iinfo != 0) {
                            printf( " cgetrf failed with %lld\n", (long long) iinfo );
                            // TODO handle error
                        }
                        trace_cpu_end( 0 );
                        // copy to GPU (all columns, not just L part)
                        magma_csetmatrix_async( ib, jb, A(j+1,j), lda, dA(j+1,j), ldda,
                                                queues[0]);
                    } else {
                        //#define USE_BATCHED_CGETRF
                        #ifdef USE_BATCHED_CGETRF
                        //dA_array[0] = dA(j+1,j);
                        //dipiv_array[0] = dipiv_magma;
                        magma_cset_pointer( dA_array, dA(j+1,j), ldda, 0, 0, 0, 1 );
                        magma_iset_pointer( dipiv_array, dipiv_magma, 1, 0, 0, min(ib,jb), 1 );
                        iinfo = magma_cgetrf_batched( ib, jb, dA_array, ldda, dipiv_array, dinfo_magma, 1);
                        // copy ipiv to CPU since permu vector is generated on CPU, for now..
                        magma_igetvector_async( min(ib,jb), dipiv_magma, 1, &ipiv[(1+j)*nb], 1, queues[0]);
                        magma_queue_sync( queues[0] );
                        #else
                        magma_cgetf2_gpu( ib, jb, dA(j+1,j), ldda, &ipiv[(1+j)*nb], queues[0], &iinfo );
                        #endif
                    }
                    // save L(j+1,j+1), and make it to unit-lower triangular
                    magma_ccopymatrix( min(ib,jb), min(ib,jb), dA(j+1,j), ldda, dL(j+1,j+1), ldda, queues[0] );
                    magmablas_claset( MagmaUpper, min(ib,jb), min(ib,jb), c_zero, c_one, dL(j+1,j+1), ldda, queues[0]);
                    // extract T(j+1,j)
                    magmablas_claset( MagmaLower, min(ib,jb)-1, jb-1, c_zero, c_zero, dT(j+1,j)+1, ldda, queues[0] );
                    if (j > 0)
                    magma_ctrsm( MagmaRight, MagmaLower, MagmaConjTrans, MagmaUnit,
                                 min(ib,jb), jb,
                                 c_one, dL(j,j), ldda,
                                        dT(j+1,j), ldda,
                                 queues[0] );

                    // apply pivot back
                    trace_gpu_start( 0, 0, "permute", "permute" );
                    magmablas_claswpx( j*nb, dL(j+1, 1), 1, ldda, 
                                       1, min(jb,ib), &ipiv[(j+1)*nb], 1,
                                       queues[0] );
                    // symmetric pivot
                    {
                        for (magma_int_t ii=0; ii < min(jb,ib); ii++) {
                            magma_int_t piv = perm[ipiv[(j+1)*nb+ii]-1];
                            perm[ipiv[(j+1)*nb+ii]-1] = perm[ii];
                            perm[ii] = piv;
                        }
                        magma_int_t count = 0;
                        for (magma_int_t ii=0; ii < n; ii++) {
                            if (perm[ii] != ii) {
                                rows[2*count] = perm[ii];
                                rows[2*count+1] = ii;
                                count ++;
                            }
                        }
                        magma_isetvector_async( 2*count, rows, 1, drows, 1, queues[0]);
                        magmablas_clacpy_sym_in(  MagmaLower, n-(j+1)*nb, count, drows, dperm, dA(j+1,j+1), ldda, dH(0,0),     ldda, queues[0] );
                        magmablas_clacpy_sym_out( MagmaLower, n-(j+1)*nb, count, drows, dperm, dH(0,0),     ldda, dA(j+1,j+1), ldda, queues[0] );

                        // reset perm
                        for (magma_int_t ii=0; ii < count; ii++) {
                            perm[rows[2*ii+1]] = rows[2*ii+1];
                        }
                        //for (magma_int_t k=0; k < n; k++) {
                        //    printf( "%lld ", (long long) perm[k] );
                        //}
                        //printf( "\n" );
                    }
                    for (magma_int_t k=(1+j)*nb; k < (1+j)*nb+min(jb,ib); k++) {
                        ipiv[k] += (j+1)*nb;
                    }
                    trace_gpu_end( 0,0 );
                }
            }
            // copy back to CPU
            for (magma_int_t j=0; j < magma_ceildiv(n, nb); j++)
            {
                magma_int_t jb = min(nb, n-j*nb);
                //#define COPY_BACK_BY_BLOCK_COL
                #if defined(COPY_BACK_BY_BLOCK_COL)
                magma_cgetmatrix_async( n-j*nb, jb, dA(j,j), ldda, A(j,j), lda, queues[0] );
                #else
                // copy T
                magma_cgetmatrix_async( jb, jb, dT(j,j), ldda, A(j,j), lda, queues[0] );
                if (j < magma_ceildiv(n, nb)-1)
                {
                    // copy L
                    magma_int_t jb2 = min(nb, n-(j+1)*nb);
                    magmablas_clacpy( MagmaLower, jb2-1, jb2-1, dL(j+1,j+1)+1, ldda, dA(j+1,j)+1, ldda, queues[0] );
                    magma_cgetmatrix_async( n-j*nb-jb, jb, dA(j+1,j), ldda, A(j+1,j), lda, queues[0] );
                }
                #endif
            }
        }
    }
    
    for (magma_int_t i=0; i < num_queues; i++) {
        magma_queue_sync( queues[i] );
        magma_queue_destroy( queues[i] );
        magma_event_destroy( events[i] );
    }
    trace_finalize( "chetrf.svg", "trace.css" );

    magma_free(dA_array);
    magma_free(dipiv_array);
    magma_free( dipiv_magma);
    magma_free( dinfo_magma );
    magma_free(dperm);
    magma_free(drows);
    magma_free_pinned(rows);
    magma_free_cpu(perm);

    magma_free( dwork );
    magma_free( dH );
    magma_free( dL );
    magma_free( dX );
    magma_free( dY );
    magma_free( dW );
    
    return *info;
} /* magma_chetrf_aasen */
