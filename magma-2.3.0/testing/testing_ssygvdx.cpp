/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017

       @author Raffaele Solca
       @author Azzam Haidar
       @author Mark Gates

       @generated from testing/testing_zhegvdx.cpp, normal z -> s, Wed Nov 15 00:34:24 2017

*/

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// includes, project
#include "magma_v2.h"
#include "magma_lapack.h"
#include "testings.h"

#define REAL

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing ssygvdx
*/
int main( int argc, char** argv)
{
    TESTING_CHECK( magma_init() );
    magma_print_environment();

    /* Constants */
    const float c_zero    = MAGMA_S_ZERO;
    const float c_one     = MAGMA_S_ONE;
    const float c_neg_one = MAGMA_S_NEG_ONE;
    const magma_int_t ione = 1;
    
    /* Local variables */
    real_Double_t   gpu_time;
    float *h_A, *h_R, *h_B, *h_S, *h_Z, *h_work, aux_work[1];
    float *w1, *w2, abstol;
    float result[2] = {0};
    magma_int_t *iwork, *isuppz, *ifail, aux_iwork[1];
    magma_int_t N, Nfound, n2, info, lwork, liwork, lda;
    #ifdef COMPLEX
    float *rwork, aux_rwork[1];
    magma_int_t lrwork;
    #endif
    int status = 0;

    magma_opts opts;
    opts.matrix = "rand_dominant";  // default
    opts.parse_opts( argc, argv );

    float tol    = opts.tolerance * lapackf77_slamch("E");
    float tolulp = opts.tolerance * lapackf77_slamch("P");
    
    #ifdef REAL
    if (opts.version == 2 || opts.version == 3) {
        printf("%% magma_ssygvr and magma_ssygvx are not available for real precisions (single, float).\n");
        return status;
    }
    #endif
    
    // pass ngpu = -1 to test multi-GPU code using 1 gpu
    magma_int_t abs_ngpu = abs( opts.ngpu );
    
    printf("%% itype = %lld, jobz = %s, uplo = %s, ngpu = %lld\n",
           (long long) opts.itype, lapack_vec_const(opts.jobz), lapack_uplo_const(opts.uplo),
           (long long) abs_ngpu);

    if (opts.itype == 1) {
        printf("%%   N     M   GPU Time (sec)   |AZ-BZD|   |D - D_magma|\n");
    }                                                   
    else if (opts.itype == 2) {                      
        printf("%%   N     M   GPU Time (sec)   |ABZ-ZD|   |D - D_magma|\n");
    }                                                   
    else if (opts.itype == 3) {                      
        printf("%%   N     M   GPU Time (sec)   |BAZ-ZD|   |D - D_magma|\n");
    }                                     
        printf("%%======================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[itest];
            lda = N;
            n2  = lda*N;

            magma_range_t range;
            magma_int_t il, iu;
            float vl, vu;
            opts.get_range( N, &range, &vl, &vu, &il, &iu );
            
            abstol = 0;  // auto in ssygvr
            MAGMA_UNUSED( abstol );  // unused in [sd] precisions
            
            // query for workspace sizes
            if ( opts.version == 1 ) {
                magma_ssygvdx( opts.itype, opts.jobz, range, opts.uplo,
                               N, NULL, lda, NULL, lda,    // A, B
                               vl, vu, il, iu, &Nfound, NULL,  // w
                               aux_work,  -1,
                               #ifdef COMPLEX
                               aux_rwork, -1,
                               #endif
                               aux_iwork, -1,
                               &info );
            }
            else if ( opts.version == 2 ) {
                #ifdef COMPLEX
                magma_ssygvr( opts.itype, opts.jobz, range, opts.uplo,
                              N, NULL, lda, NULL, lda,  // A, B
                              vl, vu, il, iu, abstol,
                              &Nfound, NULL,                // w
                              NULL, lda, NULL,          // Z, isuppz
                              aux_work,  -1,
                              #ifdef COMPLEX
                              aux_rwork, -1,
                              #endif
                              aux_iwork, -1,
                              &info );
                #endif
            }
            else if ( opts.version == 3 ) {
                #ifdef COMPLEX
                magma_ssygvx( opts.itype, opts.jobz, range, opts.uplo,
                              N, NULL, lda, NULL, lda,  // A, B
                              vl, vu, il, iu, abstol,
                              &Nfound, NULL,         // w
                              NULL, lda,         // Z
                              aux_work,  -1,
                              #ifdef COMPLEX
                              aux_rwork,
                              #endif
                              aux_iwork,
                              NULL,              // ifail
                              &info );
                // ssyevx doesn't query rwork, iwork; set them for consistency
                aux_rwork[0] = float(7*N);
                aux_iwork[0] = float(5*N);
                #endif
            }
            lwork  = (magma_int_t) MAGMA_S_REAL( aux_work[0] );
            #ifdef COMPLEX
            lrwork = (magma_int_t) aux_rwork[0];
            #endif
            liwork = aux_iwork[0];
            
            TESTING_CHECK( magma_smalloc_cpu( &h_A,    n2     ));
            TESTING_CHECK( magma_smalloc_cpu( &h_B,    n2     ));
            TESTING_CHECK( magma_smalloc_cpu( &w1,     N      ));
            TESTING_CHECK( magma_smalloc_cpu( &w2,     N      ));
            TESTING_CHECK( magma_imalloc_cpu( &iwork,  liwork ));
            
            TESTING_CHECK( magma_smalloc_pinned( &h_R,    n2     ));
            TESTING_CHECK( magma_smalloc_pinned( &h_S,    n2     ));
            TESTING_CHECK( magma_smalloc_pinned( &h_work, max( lwork, N*N ) ));  // check needs N*N
            #ifdef COMPLEX
            TESTING_CHECK( magma_smalloc_pinned( &rwork, lrwork ));
            #endif
            
            if (opts.version == 2) {
                TESTING_CHECK( magma_smalloc_cpu( &h_Z,    N*lda      ));
                TESTING_CHECK( magma_imalloc_cpu( &isuppz, 2*max(1,N) ));
            }
            if (opts.version == 3) {
                TESTING_CHECK( magma_smalloc_cpu( &h_Z,    N*lda      ));
                TESTING_CHECK( magma_imalloc_cpu( &ifail,  N          ));
            }
            
            /* Initialize the matrix */
            magma_generate_matrix( opts, N, N, nullptr, h_A, lda );
            magma_generate_matrix( opts, N, N, nullptr, h_B, lda );
            lapackf77_slacpy( MagmaFullStr, &N, &N, h_A, &lda, h_R, &lda );
            lapackf77_slacpy( MagmaFullStr, &N, &N, h_B, &lda, h_S, &lda );
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */                
            gpu_time = magma_wtime();
            if (opts.version == 1) {
                if (opts.ngpu == 1) {
                    magma_ssygvdx( opts.itype, opts.jobz, range, opts.uplo,
                                   N, h_R, lda, h_S, lda, vl, vu, il, iu, &Nfound, w1,
                                   h_work, lwork,
                                   #ifdef COMPLEX
                                   rwork, lrwork,
                                   #endif
                                   iwork, liwork,
                                   &info );
                }
                else {
                    magma_ssygvdx_m( abs_ngpu, opts.itype, opts.jobz, range, opts.uplo,
                                     N, h_R, lda, h_S, lda, vl, vu, il, iu, &Nfound, w1,
                                     h_work, lwork,
                                     #ifdef COMPLEX
                                     rwork, lrwork,
                                     #endif
                                     iwork, liwork,
                                     &info );
                }
            }
            else if (opts.version == 2) {
                #ifdef COMPLEX
                magma_ssygvr( opts.itype, opts.jobz, range, opts.uplo,
                              N, h_R, lda, h_S, lda, vl, vu, il, iu, abstol, &Nfound, w1,
                              h_Z, lda, isuppz,
                              h_work, lwork,
                              #ifdef COMPLEX
                              rwork, lrwork,
                              #endif
                              iwork, liwork,
                              &info );
                lapackf77_slacpy( "Full", &N, &N, h_Z, &lda, h_R, &lda );
                #endif
            }
            else if (opts.version == 3) {
                #ifdef COMPLEX
                magma_ssygvx( opts.itype, opts.jobz, range, opts.uplo,
                              N, h_R, lda, h_S, lda, vl, vu, il, iu, abstol, &Nfound, w1,
                              h_Z, lda,
                              h_work, lwork,
                              #ifdef COMPLEX
                              rwork, /*lrwork,*/
                              #endif
                              iwork, /*liwork,*/
                              ifail,
                              &info );
                lapackf77_slacpy( "Full", &N, &N, h_Z, &lda, h_R, &lda );
                #endif
            }
            gpu_time = magma_wtime() - gpu_time;
            if (info != 0) {
                printf("magma_ssygvdx returned error %lld: %s.\n",
                       (long long) info, magma_strerror( info ));
            }
            
            if ( opts.check ) {
                /* =====================================================================
                   Check the results following the LAPACK's [zc]hegvdx routine.
                   A x = lambda B x is solved
                   and the following 3 tests computed:
                   (1)    | A Z - B Z D | / ( |A| |Z| N )  (itype = 1)
                          | A B Z - Z D | / ( |A| |Z| N )  (itype = 2)
                          | B A Z - Z D | / ( |A| |Z| N )  (itype = 3)
                   (2)    | D(with V, magma) - D(w/o V, lapack) | / | D |
                   =================================================================== */
                #ifdef REAL
                float *rwork = h_work + N*N;
                #endif
                
                if ( opts.jobz != MagmaNoVec ) {
                    result[0] = 1.;
                    result[0] /= safe_lapackf77_slansy("1", lapack_uplo_const(opts.uplo), &N, h_A, &lda, rwork);
                    result[0] /= lapackf77_slange("1", &N, &Nfound, h_R, &lda, rwork);
                    
                    if (opts.itype == 1) {
                        blasf77_ssymm("L", lapack_uplo_const(opts.uplo), &N, &Nfound, &c_one, h_A, &lda, h_R, &lda, &c_zero, h_work, &N);
                        for (int i=0; i < Nfound; ++i)
                            blasf77_sscal(&N, &w1[i], &h_R[i*N], &ione);
                        blasf77_ssymm("L", lapack_uplo_const(opts.uplo), &N, &Nfound, &c_neg_one, h_B, &lda, h_R, &lda, &c_one, h_work, &N);
                        result[0] *= lapackf77_slange("1", &N, &Nfound, h_work, &N, rwork)/N;
                    }
                    else if (opts.itype == 2) {
                        blasf77_ssymm("L", lapack_uplo_const(opts.uplo), &N, &Nfound, &c_one, h_B, &lda, h_R, &lda, &c_zero, h_work, &N);
                        for (int i=0; i < Nfound; ++i)
                            blasf77_sscal(&N, &w1[i], &h_R[i*N], &ione);
                        blasf77_ssymm("L", lapack_uplo_const(opts.uplo), &N, &Nfound, &c_one, h_A, &lda, h_work, &N, &c_neg_one, h_R, &lda);
                        result[0] *= lapackf77_slange("1", &N, &Nfound, h_R, &lda, rwork)/N;
                    }
                    else if (opts.itype == 3) {
                        blasf77_ssymm("L", lapack_uplo_const(opts.uplo), &N, &Nfound, &c_one, h_A, &lda, h_R, &lda, &c_zero, h_work, &N);
                        for (int i=0; i < Nfound; ++i)
                            blasf77_sscal(&N, &w1[i], &h_R[i*N], &ione);
                        blasf77_ssymm("L", lapack_uplo_const(opts.uplo), &N, &Nfound, &c_one, h_B, &lda, h_work, &N, &c_neg_one, h_R, &lda);
                        result[0] *= lapackf77_slange("1", &N, &Nfound, h_R, &lda, rwork)/N;
                    }
                }
                
                lapackf77_slacpy( MagmaFullStr, &N, &N, h_A, &lda, h_R, &lda );
                lapackf77_slacpy( MagmaFullStr, &N, &N, h_B, &lda, h_S, &lda );
                
                lapackf77_ssygvd( &opts.itype, "N", lapack_uplo_const(opts.uplo), &N,
                                  h_R, &lda, h_S, &lda, w2,
                                  h_work, &lwork,
                                  #ifdef COMPLEX
                                  rwork, &lrwork,
                                  #endif
                                  iwork, &liwork,
                                  &info );
                if (info != 0) {
                    printf("lapackf77_ssygvd returned error %lld: %s.\n",
                           (long long) info, magma_strerror( info ));
                }
                
                float maxw=0, diff=0;
                for (int j=0; j < Nfound; j++) {
                    maxw = max(maxw, fabs(w1[j]));
                    maxw = max(maxw, fabs(w2[j]));
                    diff = max(diff, fabs(w1[j] - w2[j]));
                }
                result[1] = diff / (Nfound*maxw);
            }
            
            /* =====================================================================
               Print execution time
               =================================================================== */
            printf("%5lld %5lld   %9.4f     ",
                   (long long) N, (long long) Nfound, gpu_time);
            if ( opts.check ) {
                bool okay = (result[1] < tolulp);
                if ( opts.jobz != MagmaNoVec ) {
                    okay = okay && (result[0] < tol);
                    printf("   %8.2e", result[0] );
                }
                else {
                    printf("     ---   ");
                }
                printf("        %8.2e  %s\n", result[1], (okay ? "ok" : "failed"));
                status += ! okay;
            }
            else {
                printf("     ---\n");
            }
            
            magma_free_cpu( h_A );
            magma_free_cpu( h_B );
            magma_free_cpu( w1  );
            magma_free_cpu( w2  );
            magma_free_cpu( iwork );
            
            magma_free_pinned( h_R    );
            magma_free_pinned( h_S    );
            magma_free_pinned( h_work );
            #ifdef COMPLEX
            magma_free_pinned( rwork );
            #endif
            
            if ( opts.version == 2 ) {
                magma_free_cpu( h_Z    );
                magma_free_cpu( isuppz );
            }
            if ( opts.version == 3 ) {
                magma_free_cpu( h_Z    );
                magma_free_cpu( ifail  );
            }
            
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
