/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017

       @generated from testing/testing_zhemv.cpp, normal z -> s, Wed Nov 15 00:34:23 2017
       
       @author Mark Gates
*/
// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// includes, project
#include "flops.h"
#include "magma_v2.h"
#include "magma_lapack.h"
#include "magma_operators.h"
#include "testings.h"


/* ////////////////////////////////////////////////////////////////////////////
   -- Testing ssymv
*/
int main(int argc, char **argv)
{
    #ifdef HAVE_clBLAS
    #define dA(i_, j_)  dA, ((i_) + (j_)*ldda)
    #define dX(i_)      dX, ((i_))
    #define dY(i_)      dY, ((i_))
    #define dwork(i_)   dwork, ((i_))
    #else
    #define dA(i_, j_) (dA + (i_) + (j_)*ldda)
    #define dX(i_)     (dX + (i_))
    #define dY(i_)     (dY + (i_))
    #define dwork(i_)  (dwork + (i_))
    #endif
    
    TESTING_CHECK( magma_init() );
    magma_print_environment();

    const float c_neg_one = MAGMA_S_NEG_ONE;
    const magma_int_t        ione      = 1;
    
    real_Double_t   atomics_perf=0, atomics_time=0;
    real_Double_t   gflops, magma_perf=0, magma_time=0, dev_perf, dev_time, cpu_perf, cpu_time;
    float          magma_error=0, atomics_error=0, dev_error, work[1];
    magma_int_t ISEED[4] = {0,0,0,1};
    magma_int_t N, lda, ldda, sizeA, sizeX, sizeY, blocks, ldwork;
    magma_int_t incx = 1;
    magma_int_t incy = 1;
    magma_int_t nb   = 64;
    float alpha = MAGMA_S_MAKE(  1.5, -2.3 );
    float beta  = MAGMA_S_MAKE( -0.6,  0.8 );
    float *A, *X, *Y, *Yatomics, *Ydev, *Ymagma;
    magmaFloat_ptr dA, dX, dY, dwork;
    int status = 0;
    
    magma_opts opts;
    opts.parse_opts( argc, argv );
    
    // See testing_sgemm about tolerance.
    float eps = lapackf77_slamch("E");
    float tol = 3*eps;

    printf("%% uplo = %s\n", lapack_uplo_const(opts.uplo) );
    #ifdef HAVE_CUBLAS
        printf("%%   N   MAGMA Gflop/s (ms)    Atomics Gflop/s      %s Gflop/s       CPU Gflop/s   MAGMA error  %s\n",
                g_platform_str, g_platform_str );
        printf("%%==========================================================================================================\n");
    #else
        printf("%%   N   %s Gflop/s       CPU Gflop/s   MAGMA error  %s\n",
                g_platform_str, g_platform_str );
        printf("%%===============================================================\n");
    #endif
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[itest];
            lda    = N;
            ldda   = magma_roundup( N, opts.align );  // multiple of 32 by default
            sizeA  = N*lda;
            sizeX  = N*incx;
            sizeY  = N*incy;
            gflops = FLOPS_SSYMV( N ) / 1e9;
            
            TESTING_CHECK( magma_smalloc_cpu( &A,        sizeA ));
            TESTING_CHECK( magma_smalloc_cpu( &X,        sizeX ));
            TESTING_CHECK( magma_smalloc_cpu( &Y,        sizeY ));
            TESTING_CHECK( magma_smalloc_cpu( &Yatomics, sizeY ));
            TESTING_CHECK( magma_smalloc_cpu( &Ydev,     sizeY ));
            TESTING_CHECK( magma_smalloc_cpu( &Ymagma,   sizeY ));
            
            TESTING_CHECK( magma_smalloc( &dA, ldda*N ));
            TESTING_CHECK( magma_smalloc( &dX, sizeX ));
            TESTING_CHECK( magma_smalloc( &dY, sizeY ));
            
            blocks = magma_ceildiv( N, nb );
            ldwork = ldda*blocks;
            TESTING_CHECK( magma_smalloc( &dwork, ldwork ));
            
            magmablas_slaset( MagmaFull, ldwork, 1, MAGMA_S_NAN, MAGMA_S_NAN, dwork(0), ldwork, opts.queue );
            magmablas_slaset( MagmaFull, ldda,   N, MAGMA_S_NAN, MAGMA_S_NAN, dA(0,0),  ldda,   opts.queue );
            
            /* Initialize the matrix */
            lapackf77_slarnv( &ione, ISEED, &sizeA, A );
            magma_smake_symmetric( N, A, lda );
            
            // should not use data from the opposite triangle -- fill with NAN to check
            magma_int_t N1 = N-1;
            if ( opts.uplo == MagmaUpper ) {
                lapackf77_slaset( "Lower", &N1, &N1, &MAGMA_S_NAN, &MAGMA_S_NAN, &A[1], &lda );
            }
            else {
                lapackf77_slaset( "Upper", &N1, &N1, &MAGMA_S_NAN, &MAGMA_S_NAN, &A[lda], &lda );
            }
            
            lapackf77_slarnv( &ione, ISEED, &sizeX, X );
            lapackf77_slarnv( &ione, ISEED, &sizeY, Y );
            
            // for error checks
            float Anorm = safe_lapackf77_slansy( "F", lapack_uplo_const(opts.uplo), &N, A, &lda, work );
            float Xnorm = lapackf77_slange( "F", &N, &ione, X, &N, work );
            float Ynorm = lapackf77_slange( "F", &N, &ione, Y, &N, work );
            
            /* =====================================================================
               Performs operation using cuBLAS / clBLAS
               =================================================================== */
            magma_ssetmatrix( N, N, A, lda, dA(0,0), ldda, opts.queue );
            magma_ssetvector( N, X, incx, dX(0), incx, opts.queue );
            magma_ssetvector( N, Y, incy, dY(0), incy, opts.queue );
            
            dev_time = magma_sync_wtime( opts.queue );
            magma_ssymv( opts.uplo, N,
                         alpha, dA(0,0), ldda,
                                dX(0),   incx,
                         beta,  dY(0),   incy, opts.queue );
            dev_time = magma_sync_wtime( opts.queue ) - dev_time;
            dev_perf = gflops / dev_time;
            
            magma_sgetvector( N, dY(0), incy, Ydev, incy, opts.queue );
            
            /* =====================================================================
               Performs operation using cuBLAS - using atomics
               =================================================================== */
            #ifdef HAVE_CUBLAS
                cublasSetAtomicsMode( opts.handle, CUBLAS_ATOMICS_ALLOWED );
                magma_ssetvector( N, Y, incy, dY(0), incy, opts.queue );
                
                // sync on queue doesn't work -- need device sync or use NULL stream -- bug in CUBLAS?
                atomics_time = magma_sync_wtime( NULL /*opts.queue*/ );
                magma_ssymv( opts.uplo,  N,
                             alpha, dA(0,0), ldda,
                                    dX(0),   incx,
                             beta,  dY(0),   incy, opts.queue );
                atomics_time = magma_sync_wtime( NULL /*opts.queue*/ ) - atomics_time;
                atomics_perf = gflops / atomics_time;
                
                magma_sgetvector( N, dY(0), incy, Yatomics, incy, opts.queue );
                cublasSetAtomicsMode( opts.handle, CUBLAS_ATOMICS_NOT_ALLOWED );
            #endif
            
            /* =====================================================================
               Performs operation using MAGMABLAS (only with CUDA)
               =================================================================== */
            #ifdef HAVE_CUBLAS
                magma_ssetvector( N, Y, incy, dY(0), incy, opts.queue );
                
                magma_time = magma_sync_wtime( opts.queue );
                if ( opts.version == 1 ) {
                    magmablas_ssymv_work( opts.uplo, N,
                                          alpha, dA(0,0), ldda,
                                                 dX(0),   incx,
                                          beta,  dY(0),   incy,
                                          dwork(0), ldwork, opts.queue );
                }
                else {
                    // non-work interface (has added overhead)
                    magmablas_ssymv( opts.uplo, N,
                                     alpha, dA(0,0), ldda,
                                            dX(0),   incx,
                                     beta,  dY(0),   incy, opts.queue );
                }
                magma_time = magma_sync_wtime( opts.queue ) - magma_time;
                magma_perf = gflops / magma_time;
                
                magma_sgetvector( N, dY(0), incy, Ymagma, incy, opts.queue );
            #endif
            
            /* =====================================================================
               Performs operation using CPU BLAS
               =================================================================== */
            cpu_time = magma_wtime();
            blasf77_ssymv( lapack_uplo_const(opts.uplo), &N, &alpha, A, &lda, X, &incx, &beta, Y, &incy );
            cpu_time = magma_wtime() - cpu_time;
            cpu_perf = gflops / cpu_time;
            
            /* =====================================================================
               Check the result
               =================================================================== */
            // See testing_sgemm for formula. Here K = N.
            blasf77_saxpy( &N, &c_neg_one, Y, &incy, Ydev, &incy );
            dev_error = lapackf77_slange( "M", &N, &ione, Ydev, &N, work )
                            / (sqrt(float(N+2))*fabs(alpha)*Anorm*Xnorm + 2*fabs(beta)*Ynorm);
            
            #ifdef HAVE_CUBLAS
                blasf77_saxpy( &N, &c_neg_one, Y, &incy, Yatomics, &incy );
                atomics_error = lapackf77_slange( "M", &N, &ione, Yatomics, &N, work )
                            / (sqrt(float(N+2))*fabs(alpha)*Anorm*Xnorm + 2*fabs(beta)*Ynorm);
                
                blasf77_saxpy( &N, &c_neg_one, Y, &incy, Ymagma, &incy );
                magma_error = lapackf77_slange( "M", &N, &ione, Ymagma, &N, work )
                            / (sqrt(float(N+2))*fabs(alpha)*Anorm*Xnorm + 2*fabs(beta)*Ynorm);
            #endif
            
            bool okay = (magma_error < tol && dev_error < tol && atomics_error < tol);
            status += ! okay;
            printf("%5lld   %7.2f (%7.2f)   %7.2f (%7.2f)   %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e   %8.2e   %8.2e   %s\n",
                   (long long) N,
                   magma_perf,   1000.*magma_time,
                   atomics_perf, 1000.*atomics_time,
                   dev_perf,     1000.*dev_time,
                   cpu_perf,     1000.*cpu_time,
                   magma_error, dev_error, atomics_error,
                   (okay ? "ok" : "failed"));
            
            magma_free_cpu( A );
            magma_free_cpu( X );
            magma_free_cpu( Y );
            magma_free_cpu( Ydev     );
            magma_free_cpu( Yatomics );
            magma_free_cpu( Ymagma   );
            
            magma_free( dA );
            magma_free( dX );
            magma_free( dY );
            magma_free( dwork );
            fflush( stdout );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    opts.cleanup();
    TESTING_CHECK( magma_finalize() );
    return status;
}
