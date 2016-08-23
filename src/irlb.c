/*
 * irlb: Implicitly restarted Lanczos bidiagonalization partial SVD.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <assert.h>
#include <math.h>

#include <R.h>
#define USE_RINTERNALS
#include <Rinternals.h>
#include <Rdefines.h>

#include "R_ext/BLAS.h"
#include "R_ext/Lapack.h"
#include "R_ext/Rdynload.h"

#include "Matrix.h"
#include "Matrix_stubs.c"

#include "irlb.h"

/* irlb C implementation wrapper
 * X double precision input matrix
 * NU integer number of singular values/vectors to compute must be > 3
 * INIT double precision starting vector length(INIT) must equal ncol(X)
 * WORK integer working subspace dimension must be > NU
 * MAXIT integer maximum number of iterations
 * TOL double tolerance
 * EPS double machine epsilon
 * MULT integer 0 X is a dense matrix (dgemm), 1 sparse (cholmod)
 * RESTART integer 0 no or > 0 indicates restart of dimension n
 * RV, RW, RS optional restart V W and S values of dimension RESTART
 *    (only used when RESTART > 0)
 *
 * Returns a list with 6 elements:
 * 1. vector of estimated singular values
 * 2. matrix of estimated left singular vectors
 * 3. matrix of estimated right singular vectors
 * 4. number of algorithm iterations
 * 5. number of matrix vector products
 * 6. irlb C algorithm return error code (see irlb below)
 */
SEXP
IRLB (SEXP X, SEXP NU, SEXP INIT, SEXP WORK, SEXP MAXIT, SEXP TOL, SEXP EPS,
      SEXP MULT, SEXP RESTART, SEXP RV, SEXP RW, SEXP RS)
{
  SEXP ANS, S, U, V;
  double *V1, *U1, *W, *F, *B, *BU, *BV, *BS, *BW, *res, *T;
  int i, iter, mprod, ret, m, n;

  int mult = INTEGER (MULT)[0];
  void *A;
  switch (mult)
    {
    case 1:
      A = (void *) AS_CHM_SP (X);
      int *dims = INTEGER (GET_SLOT (X, install ("Dim")));
      m = dims[0];
      n = dims[1];
      break;
    default:
      A = (void *) REAL (X);
      m = nrows (X);
      n = ncols (X);
    }
  int nu = INTEGER (NU)[0];
  int work = INTEGER (WORK)[0];
  int maxit = INTEGER (MAXIT)[0];
  double tol = REAL (TOL)[0];
  int lwork = 7 * work * (1 + work);
  int restart = INTEGER (RESTART)[0];
  double eps = REAL (EPS)[0];

  PROTECT (ANS = NEW_LIST (6));
  PROTECT (S = allocVector (REALSXP, nu));
  PROTECT (U = allocVector (REALSXP, m * work));
  PROTECT (V = allocVector (REALSXP, n * work));
  if (restart == 0)
    for (i = 0; i < n; ++i)
      (REAL (V))[i] = (REAL (INIT))[i];

  /* set up intermediate working storage */
  V1 = (double *) R_alloc (n * work, sizeof (double));
  U1 = (double *) R_alloc (m * work, sizeof (double));
  W = (double *) R_alloc (m * work, sizeof (double));
  F = (double *) R_alloc (n, sizeof (double));
  B = (double *) R_alloc (work * work, sizeof (double));
  BU = (double *) R_alloc (work * work, sizeof (double));
  BV = (double *) R_alloc (work * work, sizeof (double));
  BS = (double *) R_alloc (work, sizeof (double));
  BW = (double *) R_alloc (lwork, sizeof (double));
  res = (double *) R_alloc (work, sizeof (double));
  T = (double *) R_alloc (lwork, sizeof (double));
  if (restart > 0)
    {
      memcpy (REAL (V), REAL (RV), n * (restart + 1) * sizeof (double));
      memcpy (W, REAL (RW), m * restart * sizeof (double));
      memset (B, 0, work * work * sizeof (double));
      for (i = 0; i < restart; ++i)
        B[i + work * i] = REAL (RS)[i];
    }

  ret =
    irlb (A, mult, m, n, nu, work, maxit, restart, tol, REAL (S), REAL (U),
          REAL (V), &iter, &mprod, eps, lwork, V1, U1, W, F, B, BU, BV, BS,
          BW, res, T);
  SET_VECTOR_ELT (ANS, 0, S);
  SET_VECTOR_ELT (ANS, 1, U);
  SET_VECTOR_ELT (ANS, 2, V);
  SET_VECTOR_ELT (ANS, 3, ScalarInteger (iter));
  SET_VECTOR_ELT (ANS, 4, ScalarInteger (mprod));
  SET_VECTOR_ELT (ANS, 5, ScalarInteger (ret));
  UNPROTECT (4);
  return ANS;
}

/* irlb: main computation function.
 * returns:
 *  0 on success,
 * -1 on misc error
 * -2 not converged
 * -3 out of memory
 * -4 starting vector near the null space of A
 * -5 other linear dependence error
 *
 * all data must be allocated by caller, required sizes listed below
 */
int
irlb (void *A,                  // Input data matrix
      int mult,                 // 0 -> A is double *, 1 -> A is cholmod
      int m,                    // data matrix number of rows, must be > 3.
      int n,                    // data matrix number of columns, must be > 3.
      int nu,                   // dimension of solution
      int work,                 // working dimension, must be > 3.
      int maxit,                // maximum number of main iterations
      int restart,              // 0->no, n>0 -> restarted algorithm of dimension n
      double tol,               // convergence tolerance
      double *s,                // output singular vectors at least length nu
      double *U,                // output left singular vectors  m x work
      double *V,                // output right singular vectors n x work
      int *ITER,                // ouput number of Lanczos iterations
      int *MPROD,               // output number of matrix vector products
      double eps,               // machine epsilon
      // working intermediate storage, sizes shown
      int lwork, double *V1,    // n x work
      double *U1,               // m x work
      double *W,                // m x work  input when restart > 0
      double *F,                // n
      double *B,                // work x work  input when restart > 0
      double *BU,               // work x work
      double *BV,               // work x work
      double *BS,               // work
      double *BW,               // lwork
      double *res,              // work
      double *T)                // lwork
{
  double d, S, R, alpha, beta, R_F, SS;
  int jj, kk;
  int converged;
  int info, j, k = restart;
  int inc = 1;
  int retval = -3;
  int mprod = 0;
  int iter = 0;
  double Smax = 0;

/* Check for valid input dimensions */
  if (work < 4 || n < 4 || m < 4)
    return -1;

  if (restart == 0)
    memset (B, 0, work * work * sizeof (double));
/* Main iteration */
  while (iter < maxit)
    {
      j = 0;
/*  Normalize starting vector */
      if (iter == 0 && restart == 0)
        {
          d = F77_NAME (dnrm2) (&n, V, &inc);
          if (d < 2 * eps)
            return -1;
          d = 1 / d;
          F77_NAME (dscal) (&n, &d, V, &inc);
        }
      else
        j = k;
/*
 * Compute the Lanczos bidiagonal decomposition:
 * AV  = WB
 * t(A)W = VB + Ft(E)
 * with full reorthogonalization.
 */
      switch (mult)
        {
        case 1:
          dsdmult ('n', m, n, (CHM_SP) A, V + j * n, W + j * m);
          break;
        default:
          alpha = 1;
          beta = 0;
          F77_NAME (dgemv) ("n", &m, &n, &alpha, (double *) A, &m, V + j * n,
                            &inc, &beta, W + j * m, &inc);
        }
      mprod++;

      if (iter > 0)
        {
/* Orthogonalize jth column of W with previous j columns */
          orthog (W, W + j * m, T, m, j, 1);
        }

      S = F77_NAME (dnrm2) (&m, W + j * m, &inc);
      if (S < tol && j == 1)
        return -4;
      if (S < eps)
        return -5;
      SS = 1.0 / S;
      F77_NAME (dscal) (&m, &SS, W + j * m, &inc);

/* The Lanczos process */
      while (j < work)
        {
          switch (mult)
            {
            case 1:
              dsdmult ('t', m, n, (CHM_SP) A, W + j * m, F);
              break;
            default:
              alpha = 1.0;
              beta = 0.0;
              F77_NAME (dgemv) ("t", &m, &n, &alpha, (double *) A, &m,
                                W + j * m, &inc, &beta, F, &inc);
            }
          mprod++;
          SS = -S;
          F77_NAME (daxpy) (&n, &SS, V + j * n, &inc, F, &inc);
          orthog (V, F, T, n, j + 1, 1);
          R_F = F77_NAME (dnrm2) (&n, F, &inc);
          if (j + 1 < work)
            {
              if (R_F < eps)
                return -5;
              R = 1.0 / R_F;
              memmove (V + (j + 1) * n, F, n * sizeof (double));
              F77_NAME (dscal) (&n, &R, V + (j + 1) * n, &inc);
              B[j * work + j] = S;
              B[(j + 1) * work + j] = R_F;
              switch (mult)
                {
                case 1:
                  dsdmult ('n', m, n, (CHM_SP) A, V + (j + 1) * n,
                           W + (j + 1) * m);
                  break;
                default:
                  alpha = 1.0;
                  beta = 0.0;
                  F77_NAME (dgemv) ("n", &m, &n, &alpha, (double *) A, &m,
                                    V + (j + 1) * n, &inc, &beta,
                                    W + (j + 1) * m, &inc);
                }
              mprod++;
/* One step of classical Gram-Schmidt */
              R = -R_F;
              F77_NAME (daxpy) (&m, &R, W + j * m, &inc, W + (j + 1) * m,
                                &inc);
/* full re-orthogonalization of W */
              if (iter > 1)
                orthog (W, W + (j + 1) * m, T, m, j + 1, 1);
              S = F77_NAME (dnrm2) (&m, W + (j + 1) * m, &inc);
              if (S < eps)
                return -5;
              SS = 1.0 / S;
              F77_NAME (dscal) (&m, &SS, W + (j + 1) * m, &inc);
            }
          else
            {
              B[j * work + j] = S;
            }
          j++;
        }

      memmove (BU, B, work * work * sizeof (double));   // Make a working copy of B
      int *BI = (int *) T;
      F77_NAME (dgesdd) ("O", &work, &work, BU, &work, BS, BU, &work, BV,
                         &work, BW, &lwork, BI, &info);
      R = 1.0 / R_F;
      F77_NAME (dscal) (&n, &R, F, &inc);
      for (kk = 0; kk < j; ++kk)
        res[kk] = R_F * BU[kk * work + (j - 1)];

/* Update k to be the number of converged singular values. */
      for (jj = 0; jj < j; ++jj)
        if (BS[jj] > Smax)
          Smax = BS[jj];
      convtests (j, nu, tol, Smax, res, &k, &converged);
      if (converged == 1)
        {
          iter++;
          break;
        }

      alpha = 1;
      beta = 0;
      F77_NAME (dgemm) ("n", "t", &n, &k, &j, &alpha, V, &n, BV, &work, &beta,
                        V1, &n);
      memmove (V, V1, n * k * sizeof (double));
      memmove (V + n * k, F, n * sizeof (double));

      memset (B, 0, work * work * sizeof (double));
      for (jj = 0; jj < k; ++jj)
        {
          B[jj * work + jj] = BS[jj];
          B[k * work + jj] = res[jj];
        }

/*   Update the left approximate singular vectors */
      alpha = 1;
      beta = 0;
      F77_NAME (dgemm) ("n", "n", &m, &k, &j, &alpha, W, &m, BU, &work, &beta,
                        U1, &m);
      memmove (W, U1, m * k * sizeof (double));
      iter++;
    }

/* Results */
  memmove (s, BS, nu * sizeof (double));        /* Singular values */
  alpha = 1;
  beta = 0;
  F77_NAME (dgemm) ("n", "n", &m, &nu, &work, &alpha, W, &m, BU, &work, &beta,
                    U, &m);

  F77_NAME (dgemm) ("n", "t", &n, &nu, &work, &alpha, V, &n, BV, &work, &beta,
                    V1, &n);
  memmove (V, V1, n * nu * sizeof (double));

  *ITER = iter;
  *MPROD = mprod;
  retval = (converged == 1) ? 0 : -2;   // 0 = Success, -2 = not converged.
  return (retval);
}


cholmod_common chol_c;
/* Need our own CHOLMOD error handler */
void attribute_hidden
irlba_R_cholmod_error (int status, const char *file, int line,
                       const char *message)
{
  if (status < 0)
    error ("Cholmod error '%s' at file:%s, line %d", message, file, line);
  else
    warning ("Cholmod warning '%s' at file:%s, line %d", message, file, line);
}

#ifdef HAVE_VISIBILITY_ATTRIBUTE
__attribute__ ((visibility ("default")))
#endif
     void R_init_irlba (DllInfo * dll)
{
  M_R_cholmod_start (&chol_c);
  chol_c.final_ll = 1;          /* LL' form of simplicial factorization */

  /* need own error handler, that resets  final_ll (after *_defaults()) : */
  chol_c.error_handler = irlba_R_cholmod_error;
}

void
R_unload_irlba (DllInfo * dll)
{
  M_cholmod_finish (&chol_c);
}


void
dsdmult (char transpose, int m, int n, void *a, double *b, double *c)
{
  DL_FUNC sdmult = R_GetCCallable ("Matrix", "cholmod_sdmult");
  int t = transpose == 't' ? 1 : 0;
  CHM_SP cha = (CHM_SP) a;

  cholmod_dense chb;
  chb.nrow = transpose == 't' ? m : n;
  chb.d = chb.nrow;
  chb.ncol = 1;
  chb.nzmax = chb.nrow;
  chb.xtype = cha->xtype;
  chb.dtype = 0;
  chb.x = (void *) b;
  chb.z = (void *) NULL;

  cholmod_dense chc;
  chc.nrow = transpose == 't' ? n : m;
  chc.d = chc.nrow;
  chc.ncol = 1;
  chc.nzmax = chc.nrow;
  chc.xtype = cha->xtype;
  chc.dtype = 0;
  chc.x = (void *) c;
  chc.z = (void *) NULL;

  double one[] = { 1, 0 }, zero[] =
  {
  0, 0};
  sdmult (cha, t, one, zero, &chb, &chc, &chol_c);
}
