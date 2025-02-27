/*Blocked Cholesky Factorization with Fault tolerance.
dpotf on CPU and dtrsm on GPU, dgemm on GPU. Compute either upper or lower. Initial data is on GPU, so transfer the data to GPU is not taken care of.
*Jieyang Chen, University of California, Riverside
**/

//Initial Data on GPU
//Hybird GPU (DTRSM & DGEMM)and CPU (DPOTRF) version MAGMA way
//Column Major
//Either upper and lower triangle
//testing function are made to facilitate testing
//CPU and GPU are asynchronized
//CUBLAS are used in DTRSM & DGEMM
//Leading Dimension is used
//Add CUDA Event timing
#include<iostream>
#include<cstdlib>
#include<iomanip>
#include<cmath> 
#include<ctime>
#include"cublas_v2.h"
#include<cuda_runtime.h>
#include<curand.h>
#include"acml.h"
#include"papi.h"
#include"printHelper.h"
#include"matrixGenerator.h"
#include"dpotrfFT.h"
#include"dtrsmFT.h"
#include"dsyrkFT.h"
#include"dgemmFT.h"
#include"checksumGenerator.h"
#include"cuda_profiler_api.h"

#define FMULS_POTRF(__n) ((__n) * (((1. / 6.) * (__n) + 0.5) * (__n) + (1. / 3.)))
#define FADDS_POTRF(__n) ((__n) * (((1. / 6.) * (__n)      ) * (__n) - (1. / 6.)))
#define FLOPS_DPOTRF(__n) (FMULS_POTRF((double)(__n))+FADDS_POTRF((double)(__n)) )

using namespace std;

void my_dpotrf(char uplo, double * matrix, int ld, int N, int B,
		float * real_time, float * proc_time, long long * flpins,
		float * mflops, bool FT, bool DEBUG) {

	double * temp;
	cudaHostAlloc((void**) &temp, B * B * sizeof(double), cudaHostAllocDefault);

	//intial streams----------------------------
	cudaStream_t stream0;  //for main loop
	cudaStream_t stream1;  //for dgemm part
	cudaStreamCreate(&stream0);
	cudaStreamCreate(&stream1);

	//intial cublas
	cublasStatus_t cublasStatus;
	cublasHandle_t handle1;
	cublasStatus = cublasCreate(&handle1);
	if (cublasStatus != CUBLAS_STATUS_SUCCESS)
		cout << "CUBLAS NOT INITIALIZED(handle1) in my_dpotrf " << endl;
	cublasStatus = cublasSetStream(handle1, stream1);
	if (cublasStatus != CUBLAS_STATUS_SUCCESS)
		cout << "CUBLAS SET STREAM NOT INITIALIZED(handle1) in my_dpotrf"
				<< endl;

	//variables for FT
	double * v;
	int v_ld;
	
	double * vd;
	size_t vd_pitch;
	int vd_ld;
	
	double * chk_update;
	double * chk1_recal;
	double * chk2_recal;
	
	double * chk1d;
	double * chk2d;
	size_t chk1d_pitch;
	size_t chk2d_pitch;
	int chk1d_ld;
	int chk2d_ld;
	

	double * checksum;
	int checksum_ld;
	size_t checksum_pitch;
	

	if (FT) {
		//cout<<"check sum initialization started"<<endl;
		//intialize checksum vector on CPU
		v = new double[B * 2];
		v_ld = B;
		//v2 = new double[B];
		//first vector
		for (int i = 0; i < B; ++i) {
			*(v + i) = 1;
		}
		for (int i = 0; i < B; ++i) {
			*(v + v_ld + i) = i+1;
		}
		
		//printMatrix_host(v, B, 2);
		
		//cout<<"checksum vector on CPU initialized"<<endl;

		//intialize checksum vector on GPU
		cudaMallocPitch((void**) &vd, &vd_pitch, B * sizeof(double), 2);
		vd_ld = vd_pitch / sizeof(double);
		cudaMemcpy2D(vd, vd_pitch, v, B * sizeof(double), B * sizeof(double),
				2, cudaMemcpyHostToDevice);
		//printMatrix_gpu(vd, vd_pitch, B, 2);
		//cout<<"checksum vector on gpu initialized"<<endl;

		
		
		//allocate space for recalculated checksum on CPU
		//chk1_recal = new double[B];
		//chk2_recal = new double[B];
		//chk_update = new double[B * 2];
		
		cudaHostAlloc((void**) &chk1_recal, B * 1 * sizeof(double), cudaHostAllocDefault);
		cudaHostAlloc((void**) &chk2_recal, B * 1 * sizeof(double), cudaHostAllocDefault);
		cudaHostAlloc((void**) &chk_update, B * 2 * sizeof(double), cudaHostAllocDefault);
		//cout<<"allocated space for recalculated checksum on CPU"<<endl;

		//allocate space for reclaculated checksum on CPU
		cudaMallocPitch((void**) &chk1d, &chk1d_pitch, (N / B) * 2 * sizeof(double),B);
		chk1d_ld = chk1d_pitch / sizeof(double);
		
		cudaMallocPitch((void**) &chk2d, &chk2d_pitch, (N / B) * 2 * sizeof(double),B);
		chk2d_ld = chk2d_pitch / sizeof(double);
		//cout<<"allocated space for recalculated checksum on GPU"<<endl;

		//initialize checksums
		checksum = initializeChecksum(handle1, matrix, ld, N, B, vd, vd_ld, checksum_pitch);
		checksum_ld = checksum_pitch / sizeof(double);
		//printMatrix_gpu(checksum, checksum_pitch, (N/B)*2, N);
		//cout<<"checksums initialized"<<endl;

	}
	
	
	if (PAPI_flops(real_time, proc_time, flpins, mflops) < PAPI_OK) {
		cout << "PAPI ERROR" << endl;
		return;
	}
	//start of profiling
	//cudaProfilerStart();
	
	for (int i = 0; i < N; i += B) {

		 //cout<<"i="<<i<<endl;
		 //cout<<"matrix:"<<endl;
		 //printMatrix_gpu(matrix, ld*sizeof(double), N, N);
		 //cout<<"checksum:"<<endl;
		 //printMatrix_gpu(checksum, checksum_pitch, (N/B)*2, N);
		
		if (i > 0) {
			dsyrkFT(handle1, B, i, matrix + i, ld, matrix + i * ld + i, ld,
					checksum + (i / B)*2, checksum_ld,
					checksum + (i / B)*2 + i * checksum_ld, checksum_ld,
					vd, vd_ld, 
					chk1d, chk1d_ld,
					chk2d, chk2d_ld,
					FT, DEBUG);
			
		}
		
		
		
		cudaStreamSynchronize(stream1);
		
		cudaMemcpy2DAsync(temp, B * sizeof(double), matrix + i * ld + i,
				ld * sizeof(double), B * sizeof(double), B,
				cudaMemcpyDeviceToHost, stream0);
		
		
		if (FT) {
			 cudaMemcpy2DAsync(chk_update, 2 * sizeof(double), checksum + (i/B) * 2 + i*checksum_ld,
			 checksum_pitch, 2 * sizeof(double), B,
			 cudaMemcpyDeviceToHost, stream0);
			 //cudaMemcpy2DAsync(chk2, 1 * sizeof(double), checksum2 + (i/B) + i*checksum2_ld,
			 //checksum2_pitch, 1 * sizeof(double), B,
			 //cudaMemcpyDeviceToHost, stream0);
			 
		}
		
		
		
		if (i != 0 && i + B < N) {

			dgemmFT(handle1, N - i - B, B, i, matrix + (i + B), ld, matrix + i,
					ld, matrix + i * ld + (i + B), ld, 
					checksum + ((i + B) / B)*2, checksum_ld,
					checksum + i * checksum_ld + ((i + B) / B)*2, checksum_ld,
					vd, vd_ld, chk1d, chk1d_ld, chk2d, chk2d_ld, FT, DEBUG);
		}
		
		
		
		
		cudaStreamSynchronize(stream0);
		
		
		dpotrfFT(temp, B, B, chk_update, 2, v, v_ld, chk1_recal, chk2_recal, FT, DEBUG);
		
		cudaMemcpy2DAsync(matrix + i * ld + i, ld * sizeof(double), temp,
				B * sizeof(double), B * sizeof(double), B,
				cudaMemcpyHostToDevice, stream0);
		
		
		if (FT) {
			 cudaMemcpy2DAsync(checksum + (i/B) * 2 + i*checksum_ld, checksum_pitch, chk_update, 2 * sizeof(double), 
			 2 * sizeof(double), B,
			 cudaMemcpyHostToDevice, stream0);
			 //cudaMemcpy2DAsync(checksum2 + (i/B) + i*checksum2_ld,checksum2_pitch, chk2, 1 * sizeof(double), 
			 //1 * sizeof(double), B,
			 //cudaMemcpyHostToDevice, stream0);
			 
		}
		
		
		
		//update B    
		
		if (i + B < N) {
		//	cudaStreamSynchronize(stream0);
			dtrsmFT(handle1, N - i - B, B, matrix + i * ld + i, ld,
					matrix + i * ld + i + B, ld,
					checksum + ((i + B) / B )*2 + i * checksum_ld, checksum_ld,
					vd, vd_ld, chk1d, chk1d_ld,chk2d, chk2d_ld, FT, DEBUG);
		}
		
		
	

	}
	
	cudaStreamSynchronize(stream0);
	cudaStreamSynchronize(stream1);
	//end of profiling
	//cudaProfilerStop();

	if (PAPI_flops(real_time, proc_time, flpins, mflops) < PAPI_OK) {
		cout << "PAPI ERROR" << endl;
		return;
	}
	

	cublasDestroy(handle1);
	cudaFreeHost(temp);
	PAPI_shutdown();

}

void test_mydpotrf(int N, int B, float * real_time, float * proc_time,
		long long * flpins, float * mflops, bool FT, bool DEBUG) {

	char uplo = 'l';
	double * matrix;
	//double * result;
	size_t matrix_pitch;
	//size_t result_pitch;
	//Memory allocation on RAM and DRAM
	cudaMallocPitch((void**) &matrix, &matrix_pitch, N * sizeof(double), N);
	//cudaMallocPitch((void**) &result, &result_pitch, N * sizeof(double), N);

	int matrix_ld = matrix_pitch / sizeof(double);
	//int result_ld = result_pitch / sizeof(double);

	//matrixGenerator_gpu2(uplo, matrix, matrix_ld, result, result_ld, N, 2);
	matrixGenerator_gpu(uplo, matrix, matrix_ld, N, 2);

	my_dpotrf(uplo, matrix, matrix_ld, N, B, real_time, proc_time, flpins, \
			mflops, FT, DEBUG);

	//Verify result
	/*if(resultVerify_gpu(result,result_ld,matrix,matrix_ld,N,2)){
	 cout<<"Result passed!"<<endl;
	 }else{
	 cout<<"Result failed!"<<endl;
	 }
	 */
	cudaFree(matrix);
	//cudaFree(result);

}

int main(int argc, char**argv) {
	int N = atoi(argv[1]);
	int B = atoi(argv[2]);
	bool FT = false;
	if (argv[3][0] == '1')
		FT = true;
	bool DEBUG = false;
	if (argv[4][0] == '1')
		DEBUG = true;
	int TEST_NUM = 1;
	cout << "Input config:N=" << N << ", B=" << B << ", FT=" << FT << endl;

	//int n[10] = { 1024, 2048, 3072, 4096, 5120, 6144, 7168, 8192, 9216, 10240 };
	//int b = 256; 
	//for (int k = 0; k < 1; k++) {
	float total_real_time = 0.0;
	float total_proc_time = 0.0;
	long long total_flpins = 0.0;
	float total_mflops = 0.0;
	float real_time = 0.0;
	float proc_time = 0.0;
	long long flpins = 0.0;
	float mflops = 0.0;
	double flops = FLOPS_DPOTRF(N) / 1e9;
	//cout<<"flops:"<<flops<<"  ";

	for (int i = 0; i < TEST_NUM; i++) {
		test_mydpotrf(N, B, &real_time, &proc_time, &flpins, &mflops, FT, DEBUG);
		total_real_time += real_time;
		total_proc_time += proc_time;
		total_flpins += flpins;
		total_mflops += mflops;
	}
	if (FT)
		cout << "FT enabled" << endl;
	cout << "Size:" << N << "(" << B << ")---Real_time:"
			<< total_real_time / (double) TEST_NUM << "---" << "Proc_time:"
			<< total_proc_time / (double) TEST_NUM << "---" << "Total GFlops:"
			<< flops / (total_proc_time / (double) TEST_NUM) << endl;
	cudaDeviceReset();
}
