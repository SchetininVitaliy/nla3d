// This file is a part of nla3d project. For information about authors and
// licensing go to project's repository on github:
// https://github.com/dmitryikh/nla3d 

#include "math/EquationSolver.h"
#include "math/Mat.h"
#include "math/SparseMatrix.h"

#ifdef NLA3D_USE_MKL
#include <mkl.h>
#endif //NLA3D_USE_MKL

namespace nla3d {

namespace math {

EquationSolver* defaultEquationSolver = new GaussDenseEquationSolver;

void EquationSolver::setSymmetric (bool symmetric) {
  isSymmetric = symmetric;
}

void EquationSolver::setPositive (bool positive) {
  isPositive = positive;
}


void GaussDenseEquationSolver::solveEquations (math::SparseSymmetricMatrix* matrix, double* rhs, double* unknowns) {
  TIMED_SCOPE(t, "solveEquations");
  numberOfEquations = matrix->getNumberOfRows();
  CHECK(nrhs == 1) << "GaussDenseEquationSolver support only 1 set of rhs values";
  dMat denseMatrix(numberOfEquations, numberOfEquations);
  for (uint16 i = 0; i < numberOfEquations; i++)
    for (uint16 j = 0; j < numberOfEquations; j++)
      //NOTE: SparceMatrix getters works with indexes started from 1
      denseMatrix[i][j] = (*matrix)(i+1, j+1);
  bool res = _solve(unknowns, denseMatrix.ptr(), rhs, numberOfEquations);
  
  CHECK(res == true) << "ERROR during solution";
}


bool GaussDenseEquationSolver::_solve(double* X, double* A,
                               double* B, int n)
{
    // Gaussian elimination, with partial pivoting. It's an error if the
    // matrix is singular, because that means two constraints are
    // equivalent.
    int i, j, ip, jp, imax;
    double max, temp;

    for(i = 0; i < n; i++) {
        // We are trying eliminate the term in column i, for rows i+1 and
        // greater. First, find a pivot (between rows i and N-1).
        max = 0;
        for(ip = i; ip < n; ip++) {
          //if(ffabs(A[ip][i]) > max) {
            if(fabs(A[ip*n+i]) > max) {
                imax = ip;
              //max = ffabs(A[ip][i]);
                max = fabs(A[ip*n+i]);
            }
        }
        // Don't give up on a singular matrix unless it's really bad; the
        // assumption code is responsible for identifying that condition,
        // so we're not responsible for reporting that error.
        if(fabs(max) < 1e-20) return false;

        // Swap row imax with row i
        for(jp = 0; jp < n; jp++) {
          //SWAP(double, A[i][jp], A[imax][jp]);
            double tmp;
            tmp = A[i*n+jp];
            A[i*n+jp] = A[imax*n+jp];
            A[imax*n+jp] = tmp;
        }
      //SWAP(double, B[i], B[imax]);
        double tmp;
        tmp = B[i];
        B[i] = B[imax];
        B[imax] = tmp;

        // For rows i+1 and greater, eliminate the term in column i.
        for(ip = i+1; ip < n; ip++) {
          //temp = A[ip][i]/A[i][i];
            temp = A[ip*n+i]/A[i*n+i];

            for(jp = i; jp < n; jp++) {
              //A[ip][jp] -= temp*(A[i][jp]);
                A[ip*n+jp] -= temp*(A[i*n+jp]);
            }
            B[ip] -= temp*B[i];
        }
    }

    // We've put the matrix in upper triangular form, so at this point we
    // can solve by back-substitution.
    for(i = n - 1; i >= 0; i--) {
      //if(fabs(A[i][i]) < 1e-20) return false;
        if(fabs(A[i*n+i]) < 1e-20) return false;

        temp = B[i];
        for(j = n - 1; j > i; j--) {
          //temp -= X[j]*A[i][j];
            temp -= X[j]*A[i*n+j];
        }
      //X[i] = temp / A[i][i];
        X[i] = temp / A[i*n+i];
    }

    return true;
}


#ifdef NLA3D_USE_MKL
PARDISO_equationSolver::~PARDISO_equationSolver () {
  releasePARDISO();
}

void PARDISO_equationSolver::solveEquations (math::SparseSymmetricMatrix* matrix, double* rhs, double* unknowns) {
  TIMED_SCOPE(t, "solveEquations");
  if (firstRun) {
    initializePARDISO(matrix);
  }
  firstRun = false;

  CHECK(numberOfEquations == matrix->getNumberOfRows());
  
	int phase;

  // initialize error code
	int error = 0; 

	// phase 22 is the numerical factorization
	phase = 22;
  int n = static_cast<int> (numberOfEquations);

	PARDISO(pt, &maxfct, &mnum, &mtype, &phase, &n, matrix->getValuesArray(), 
      (int*) matrix->getIofeirArray(),
      (int*) matrix->getColumnsArray(),
			NULL, &nrhs, iparm, &msglvl, NULL, NULL, &error);
  CHECK(error == 0) << "ERROR during numerical factorization. Error code = " << error;

	//Back substitution and iterative refinement
	phase = 33;
	PARDISO(pt, &maxfct, &mnum, &mtype, &phase,	&n, matrix->getValuesArray(),
      (int*) matrix->getIofeirArray(),
      (int*) matrix->getColumnsArray(),
			NULL, &nrhs, iparm, &msglvl, rhs, unknowns, &error);

	CHECK(error == 0) << "ERROR during solution. Error code = " << error;
}

void PARDISO_equationSolver::initializePARDISO (math::SparseSymmetricMatrix* matrix) {
	for (uint16 i = 0; i < 64; i++) {
    iparm[i]=0;
  }

  for (uint16 i = 0; i < 64; i++) {
    pt[i]=0;
  }

	iparm[0] = 1; //no solver default
	iparm[1] = 2; //fill-in reordering from meris
	iparm[2] = MKL_Get_Max_Threads();
	iparm[3] = 0; //no iterative-direct algorithm
	iparm[4] = 0; //no user fill-in reducing permutation
	iparm[5] = 0; //write solution into x
	iparm[6] = 16; //default logical fortran unit number for output
	iparm[7] = 2; //max numbers of iterative refinement steps
	iparm[9] = 13; //pertrub the pivor elements with 1e-13
	iparm[10] = 1; //use nonsymmetric permutation  and scaling MPS
	iparm[13]=0; //output: number of perturbed pivots
	iparm[17]=-1; //output: number of nonzeros in the factor LU
	iparm[18]=-1; //output: MFLOPS for LU factorization
	iparm[19] = 0; //output: number of CG Iterations

  LOG_IF(!isSymmetric, FATAL) << "For now PARDISO_equationSolver doesn't support non-symmetric matrices";
	if (isPositive) {
    mtype = 2;
    LOG(INFO) << "EquationSolver will use positive symmetric solver";
  } else {
    LOG(INFO) << "EquationSolver will use non-positive symmetric solver";
    mtype = -2;
  }

  numberOfEquations = matrix->getNumberOfRows();
  int n = static_cast<int> (numberOfEquations);

  // initialize error code
	int error = 0; 

  int phase = 11;

  PARDISO(pt, &maxfct, &mnum, &mtype,&phase, &n, matrix->getValuesArray(),
     (int*) matrix->getIofeirArray(), 
     (int*) matrix->getColumnsArray(), 
			NULL, &nrhs, iparm, &msglvl, NULL, NULL, &error);
  CHECK(error == 0) << "ERROR during symbolic factorization. Error code = " << error;
  LOG(INFO) << "Number of nonzeros in factors = " << iparm[17] << ", number of factorization MFLOPS = " << iparm[18];
}

void PARDISO_equationSolver::releasePARDISO () {
  int phase = -1;
  int n = static_cast<int> (numberOfEquations);

  // initialize error code
	int error = 0; 
	//Termination and release of memory
	PARDISO(pt, &maxfct, &mnum, &mtype, &phase,	&n, NULL, NULL, NULL, NULL, &nrhs, iparm, &msglvl, NULL, NULL, &error);
  LOG_IF (error != 0, WARNING) << "ERROR during PARDISO termination. Error code = " << error;
}
#endif //NLA3D_USE_MKL

} //namespace math

} //namespace nla3d
