/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017
       
       @author Mark Gates
       @author Azzam Haidar

       See [zcds]gemm_fermi.cu for description of related files.
*/

/******************************************************************************/
extern "C" static __global__
void gemm_kernel_name(precision)(
    int M, int N, int K,
    const FloatingPoint_t* __restrict__ A, int LDA,
    const FloatingPoint_t* __restrict__ B, int LDB,
    FloatingPoint_t*       __restrict__ C, int LDC,
    FloatingPoint_t alpha, FloatingPoint_t beta,
    int offsetA, int offsetB )
{
    devfunc_name(precision)( M, N, K, A, LDA, B, LDB, C, LDC, alpha, beta, offsetA, offsetB );
}
