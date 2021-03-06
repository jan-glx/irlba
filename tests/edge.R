# Tests for a few edge cases
require("irlba")
loc <- ""

test <- function()
{
  on.exit(message("Error occured in: ", loc))
  # Dense matrix
  loc <<- "dense"
  set.seed(1)
  A <- matrix(rnorm(16), 4)
  L <- irlba(A, nu=1, nv=1, tol=1e-9, fastpath=FALSE)
  L1 <- irlba(A, nu=1, nv=1, tol=1e-9, fastpath=TRUE)
  S <- svd(A, nu=1, nv=1)
  if (!isTRUE(all.equal(L$d, S$d[1])))
  {
    stop("Failed tiny reference example ")
  }
  if (!isTRUE(all.equal(L1$d, S$d[1])))
  {
    stop("Failed tiny fastpath example")
  }

  # Tickle misc. checks
  loc <<- "misc"
  set.seed(1)
  A <- matrix(rnorm(100), 10)
  L <- tryCatch(irlba(A, nv=3, tol=1e-9, fastpath=FALSE, work=2, v=rep(0, nrow(A))), error=function(e) "NULLSPACE")
  S <- svd(A)
  L <- irlba(A, nv=3, tol=1e-9, fastpath=FALSE, work=2, v=S$v[, 1])
  A <- S$u %*% diag(c(1, 1, 1, 1, 1, 1, 1, 1, 1, 1e-12)) %*% t(S$v)
  L <- irlba(A, nv=3, tol=1e-9, fastpath=FALSE, work=2, reorth=FALSE)

  # Convergence
  loc <<- "convergence"
  A <- S$u %*% (c(1e-5, rep(1e-9, 9)) * t(S$v))
  for (tol in 10 ^ - (7:11))
  {
    L <- irlba(A, 3, tol=tol, svtol=Inf)
    converged <- svd(A %*% L$v - L$u  %*% diag(L$d))$d[1] < tol * L$d[1]
    stopifnot(converged)
  }

  # Sparse but not dgCMatrix (issue #6)
  loc <<- "misc sparse"
  A <- Matrix(matrix(rnorm(100), 10))
  L <- irlba(A, nv=1)
  S <- svd(A, nu=1, nv=1)
  if (!isTRUE(all.equal(L$d, S$d[1])))
  {
    stop("Failed general sparse matrix example ")
  }

  A <- Matrix(sample(c(FALSE, TRUE), 100, replace=TRUE), 10, 10)
  L <- irlba(A, nv=1)
  S <- svd(A, nu=1, nv=1)
  if (!isTRUE(all.equal(L$d, S$d[1])))
  {
    stop("Failed logical sparse matrix example ")
  }

  # Test for issue #7, a really dumb bug.
  loc <<- "issue 7"
  mx <- matrix(sample(1:10, 10 * 100, replace=TRUE), nrow=10)
  S <- irlba(mx, nv=2, verbose=TRUE, center=colMeans(mx), right_only=TRUE)

  # test for issue #9
  loc <<- "issue 9"
  set.seed(2)
  s1 <- irlba(diag(c(1, 2, 3, 4, 5, 0, 0, 0, 0)), 4, fastpath=FALSE)
  set.seed(2)
  s2 <- irlba(diag(c(1, 2, 3, 4, 5, 0, 0, 0, 0)), 4, fastpath=TRUE)
  stopifnot(all.equal(s1$d, s2$d))
  # Repeat this test with different seed
  set.seed(3)
  s2 <- irlba(diag(c(1, 2, 3, 4, 5, 0, 0, 0, 0)), 4, fastpath=TRUE)
  stopifnot(all.equal(s1$d, s2$d))

  loc <<- "issue 26"
  set.seed(1)
  r <- 10
  n <- 1000
  X1 <- matrix(rnorm(n * r), n)
  X2 <- matrix(rnorm(n * r), n)
  X <- X1 %*% t(X2)
  l <- irlba(X, 20, fastpath=TRUE)$d
  stopifnot(all.equal(tail(l, 10), rep(0, 10)))
  l <- irlba(X, 20, fastpath=FALSE)$d
  stopifnot(all.equal(tail(l, 10), rep(0, 10)))

  on.exit()
}

test()
