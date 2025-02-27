/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017

       @generated from magmablas/zhemm_batched_core.cu, normal z -> d, Wed Nov 15 00:34:23 2017

       @author Ahmad Abdelfattah
       
*/
#include "magma_internal.h"
#include "batched_kernel_param.h"

#define PRECISION_d
#include "hemm_template_kernel_batched.cuh"

/******************************************************************************/
extern "C" void 
magmablas_dsymm_batched_core(
        magma_side_t side, magma_uplo_t uplo, 
        magma_int_t m, magma_int_t n, 
        double alpha, 
        double **dA_array, magma_int_t ldda,
        double **dB_array, magma_int_t lddb, 
        double beta, 
        double **dC_array, magma_int_t lddc, 
        magma_int_t roffA, magma_int_t coffA, magma_int_t roffB, magma_int_t coffB, magma_int_t roffC, magma_int_t coffC, 
        magma_int_t batchCount, magma_queue_t queue )
{
    if(side == MagmaLeft){
        hemm_template_batched<double, DSYMM_BATCHED_LEFT>(
            side, uplo, m, n, 
            dA_array, ldda,
            dB_array, lddb, 
            dC_array, lddc, alpha, beta, 
            roffA, coffA, roffB, coffB, roffC, coffC, batchCount, queue);
    }else{
        hemm_template_batched<double, DSYMM_BATCHED_RIGHT>(
            side, uplo, m, n, 
            dA_array, ldda,
            dB_array, lddb, 
            dC_array, lddc, alpha, beta, 
            roffA, coffA, roffB, coffB, roffC, coffC, batchCount, queue);
    }
}

/***************************************************************************//**
    Purpose
    -------
    DSYMM performs one of the matrix-matrix operations

        C := alpha*A*B + beta*C,
    or
        C := alpha*B*A + beta*C,

    where alpha and beta are scalars, A is a symmetric matrix, and
    B and C are m by n matrices.

    Arguments
    ---------
    @param[in]
    side    magma_side_t
            On entry, side specifies whether each symmetric matrix A
            appears on the left or right in the operation as follows:

            SIDE = MagmaLeft    C := alpha*A*B + beta*C,
            SIDE = MagmaRight   C := alpha*B*A + beta*C.


    @param[in]
    uplo    magma_uplo_t
            On entry, uplo specifies whether the upper or lower
            triangular part of each symmetric matrix A is to be
            referenced as follows:

            uplo = MagmaUpper   Only the upper triangular part of the
                                symmetric matrix is to be referenced.
            uplo = MagmaLower   Only the lower triangular part of the
                                symmetric matrix is to be referenced.

    @param[in]
    m       INTEGER
            On entry, m specifies the number of rows of each matrix C.
            m >= 0.

    @param[in]
    n       INTEGER
            On entry, n specifies the number of columns of each matrix C.
            n >= 0.

    @param[in]
    alpha   DOUBLE PRECISION
            On entry, alpha specifies the scalar alpha.

    @param[in]
    dA_array    Array of pointers, dimension(batchCount).
            Each is a DOUBLE PRECISION array A of DIMENSION ( ldda, ka ), where ka is
            m when side = MagmaLower and is n otherwise.
            Before entry with side = MagmaLeft, the m by m part of
            the array A must contain the symmetric matrix, such that
            when uplo = MagmaUpper, the leading m by m upper triangular
            part of the array A must contain the upper triangular part
            of the symmetric matrix and the strictly lower triangular
            part of A is not referenced, and when uplo = MagmaLower,
            the leading m by m lower triangular part of the array A
            must contain the lower triangular part of the symmetric
            matrix and the strictly upper triangular part of A is not
            referenced.
            Before entry with side = MagmaRight, the n by n part of
            the array A must contain the symmetric matrix, such that
            when uplo = MagmaUpper, the leading n by n upper triangular
            part of the array A must contain the upper triangular part
            of the symmetric matrix and the strictly lower triangular
            part of A is not referenced, and when uplo = MagmaLower,
            the leading n by n lower triangular part of the array A
            must contain the lower triangular part of the symmetric
            matrix and the strictly upper triangular part of A is not
            referenced.
            Note that the imaginary parts of the diagonal elements need
            not be set, they are assumed to be zero.

    @param[in]
    ldda    INTEGER
            On entry, ldda specifies the first dimension of each A as declared
            in the calling (sub) program.
            When side = MagmaLower then ldda >= max( 1, m ),
            otherwise                   ldda >= max( 1, n ).

    @param[in]
    dB_array      Array of pointers, dimension(batchCount). 
            Each is a DOUBLE PRECISION array B of DIMENSION ( lddb, n ).
            Before entry, the leading m by n part of the array B must
            contain the matrix B.

    @param[in]
    lddb    INTEGER
            On entry, lddb specifies the first dimension of B as declared
            in the calling (sub) program. LDDB >= max( 1, m ).

    @param[in]
    beta    DOUBLE PRECISION
            On entry, BETA specifies the scalar beta. When BETA is
            supplied as zero then C need not be set on input.

    @param[in,out]
    dC_array      Array of pointers, dimension(batchCount). 
            Each is a DOUBLE PRECISION array C of DIMENSION ( lddc, n ).
            Before entry, the leading m by n part of the array C must
            contain the matrix C, except when beta is zero, in which
            case C need not be set on entry.
            On exit, the array C is overwritten by the m by n updated
            matrix.

    @param[in]
    lddc    INTEGER
            On entry, lddc specifies the first dimension of C as declared
            in the calling (sub) program. lddc >= max( 1, m ).

    @param[in]
    batchCount  INTEGER
                The number of matrices to operate on.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.
    

    @ingroup magma_hemm_batched
*******************************************************************************/
extern "C" void 
magmablas_dsymm_batched(
        magma_side_t side, magma_uplo_t uplo, 
        magma_int_t m, magma_int_t n, 
        double alpha, 
        double **dA_array, magma_int_t ldda,
        double **dB_array, magma_int_t lddb, 
        double beta, 
        double **dC_array, magma_int_t lddc, 
        magma_int_t batchCount, magma_queue_t queue )
{
    magma_int_t nrowa = (side == MagmaLeft ? m : n);
    magma_int_t info = 0;
    if ( side != MagmaLeft && side != MagmaRight ) {
        info = -1;
    } else if (uplo != MagmaLower && uplo != MagmaUpper ) {
        info = -2;
    } else if ( m < 0 ) {
        info = -3;
    } else if ( n < 0 ) {
        info = -4;
    } else if ( ldda < max(1,nrowa) ) {
        info = -7;
    } else if ( lddb < max(1,m) ) {
        info = -9;
    } else if (lddc < max(1,m)) {
        info = -12;
    }

    if (info != 0) {
        magma_xerbla( __func__, -(info) );
        return;
    }
    
    magmablas_dsymm_batched_core( 
            side, uplo, 
            m, n, 
            alpha, dA_array, ldda, 
                   dB_array, lddb, 
            beta,  dC_array, lddc, 
            0, 0, 0, 0, 0, 0, 
            batchCount, queue );
}
