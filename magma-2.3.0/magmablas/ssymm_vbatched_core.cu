/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017

       @generated from magmablas/zhemm_vbatched_core.cu, normal z -> s, Wed Nov 15 00:34:23 2017

       @author Ahmad Abdelfattah
       
*/
#include "magma_internal.h"
#include "batched_kernel_param.h"

#define PRECISION_s
#include "hemm_template_kernel_vbatched.cuh"
/******************************************************************************/
extern "C" void 
magmablas_ssymm_vbatched_core(
        magma_side_t side, magma_uplo_t uplo, 
        magma_int_t *m, magma_int_t *n, 
        float alpha, 
        float **dA_array, magma_int_t *ldda,
        float **dB_array, magma_int_t *lddb, 
        float beta, 
        float **dC_array, magma_int_t *lddc, 
        magma_int_t max_m, magma_int_t max_n, 
        magma_int_t roffA, magma_int_t coffA, magma_int_t roffB, magma_int_t coffB, magma_int_t roffC, magma_int_t coffC, 
        magma_int_t specM, magma_int_t specN, 
        magma_int_t batchCount, magma_queue_t queue )
{        
    if(side == MagmaLeft){
        hemm_template_vbatched<float, SSYMM_BATCHED_LEFT>(
            side, uplo, m, n, 
            dA_array, ldda,
            dB_array, lddb, 
            dC_array, lddc, alpha, beta, 
            max_m, max_n, 
            roffA, coffA, roffB, coffB, roffC, coffC, specM, specN, 
            batchCount, queue);
    }else{
        hemm_template_vbatched<float, SSYMM_BATCHED_RIGHT>(
            side, uplo, m, n, 
            dA_array, ldda,
            dB_array, lddb, 
            dC_array, lddc, alpha, beta, 
            max_m, max_n, 
            roffA, coffA, roffB, coffB, roffC, coffC, specM, specN, 
            batchCount, queue);
    }
}

/******************************************************************************/
