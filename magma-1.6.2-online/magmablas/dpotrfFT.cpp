#include"FT.h"
#include<iostream>

using namespace std;
//Cholesky Factorization with FT on CPU using ACML
double get(double * matrix, int ld, int n, int i, int j) {
	if (i > ld || j > n)
		cout << "matrix_get_error" << endl;
	return *(matrix + j * ld + i);
}
/**
 * Cholesky factorization with FT on CPU using ACML
 * A: input matrix
 * lda: leading dimension of A
 * n: size of A
 * chksum1: checksum 1
 * inc1: stride between elememts in chksum1
 * chksum2: checksum 2
 * inc2: stride between elememts in chksum2
 */
void dpotrfFT(double * A, int lda, int n, int * info,
				double * chksum, int chksum_ld,
				double * v, int v_ld, 
				bool FT , bool DEBUG) {
	
	double one = 1;
	double zero = 0;
	double negone = -1;
	//do Choleksy factorization
	magma_set_lapack_numthreads(1);
	char uplo = 'L';
	lapackf77_dpotrf(&uplo, &n, A, &n, info);
	if (FT) {

		//recalculate checksum1 and checksum2
		magma_set_lapack_numthreads(8);
		double * chk1 = new double[n];
		double * chk2 = new double[n];
		for (int i = 0; i < n; i++) {
			chk1[i] = 1;
			chk2[i] = i + 1;
		}
		int v1_inc = 1;
		int v2_inc = 1;
		int chk1_inc = 1;
		int chk2_inc = 1;
		char T = 'T';
		char N = 'N';
		char L = 'L';
		int nOfChecksum = 2;
		
		
		blasf77_dtrmv(  &L, &T, &N,
		                &n,
		                A, &lda,
		                chk1, &chk1_inc );
		blasf77_dtrmv(  &L, &T, &N,
						&n,
						A, &lda,
						chk2, &chk2_inc );
				
		//update checksum1 and checksum2
		magma_set_lapack_numthreads(64);
		for (int i = 0; i < n; i++) {
			//chksum1[i] = chksum1[i] / get(A, n, n, i, i);
			*(chksum + i*chksum_ld) = *(chksum + i*chksum_ld) / get(A, n, n, i, i);
			//(n-i-1, negone*chksum1[i], A + i*lda + i+1, 1, chksum1 + i+1, 1 );
			int m = n-i-1;
			double alpha = negone * (*(chksum + i * chksum_ld));
			int incx = 1;
			blasf77_daxpy(&m, &alpha, A + i*lda + i+1, &incx, chksum + (i+1) * chksum_ld, &chksum_ld );
		}
	
		for (int i = 0; i < n; i++) {
			//chksum2[i] = chksum2[i] / get(A, n, n, i, i);
			*(chksum + i*chksum_ld + 1) = *(chksum + i*chksum_ld + 1) / get(A, n, n, i, i);
			//daxpy(n-i-1, negone*chksum2[i], A + i*lda + i+1, 1, chksum2 + i+1, 1 );
			int m = n-i-1;
			double alpha = negone *  (*(chksum + i * chksum_ld + 1));
			int incx = 1;
			blasf77_daxpy(&m, &alpha, A + i * lda + i+1, &incx, chksum + 1 + (i + 1) * chksum_ld, &chksum_ld );
		}
	
		if (DEBUG) {
			cout<<"recalcuated checksum on CPU after factorization:"<<endl;
			printMatrix_host(chk1, 1, 1, n);
			printMatrix_host(chk2, 1, 1, n);
			cout<<"updated checksum on CPU after factorization:"<<endl;
			printMatrix_host(chksum, chksum_ld, 2, n);
		}
		
		ErrorDetectAndCorrectHost(A, lda, n, n, n,
				chksum, chksum_ld,
				chk1, chk1_inc,
				chk2, chk2_inc);
		
	}
}