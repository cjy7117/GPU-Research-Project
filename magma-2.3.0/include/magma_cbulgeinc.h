/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017

       @generated from include/magma_zbulgeinc.h, normal z -> c, Wed Nov 15 00:34:25 2017
*/

#ifndef MAGMA_CBULGEINC_H
#define MAGMA_CBULGEINC_H

#include "magma_types.h"

#ifdef __cplusplus
extern "C" {
#endif


// =============================================================================
// Configuration

// maximum contexts
#define MAX_THREADS_BLG         256

void findVTpos(
    magma_int_t N, magma_int_t NB, magma_int_t Vblksiz,
    magma_int_t sweep, magma_int_t st,
    magma_int_t *Vpos, magma_int_t *TAUpos, magma_int_t *Tpos,
    magma_int_t *myblkid);

void findVTsiz(
    magma_int_t N, magma_int_t NB, magma_int_t Vblksiz,
    magma_int_t *blkcnt, magma_int_t *LDV);

struct gbstrct_blg {
    magmaFloatComplex *dQ1;
    magmaFloatComplex *dT1;
    magmaFloatComplex *dT2;
    magmaFloatComplex *dV2;
    magmaFloatComplex *dE;
    magmaFloatComplex *T;
    magmaFloatComplex *A;
    magmaFloatComplex *V;
    magmaFloatComplex *TAU;
    magmaFloatComplex *E;
    magmaFloatComplex *E_CPU;
    int cores_num;
    int locores_num;
    int overlapQ1;
    int usemulticpu;
    int NB;
    int NBTILES;
    int N;
    int NE;
    int N_CPU;
    int N_GPU;
    int LDA;
    int LDE;
    int BAND;
    int grsiz;
    int Vblksiz;
    int WANTZ;
    magma_side_t SIDE;
    real_Double_t *timeblg;
    real_Double_t *timeaplQ;
    volatile int *ss_prog;
};

// declare globals here; defined in chetrd_bhe2trc.cpp
extern struct gbstrct_blg core_in_all;


#ifdef __cplusplus
}
#endif

#endif /* MAGMA_CBULGEINC_H */
