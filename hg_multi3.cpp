
#include <hg_multi.H>
#include <Tracer.H>

#ifdef HG_CONSTANT
#  define CGOPT 2
#else
#  define CGOPT 1
#endif

#ifdef BL_FORT_USE_UNDERSCORE
#  define   FORT_HGRES      hgres_
#  define   FORT_HGRESU     hgresu_
#  define   FORT_HGRLX      hgrlx_
#  define   FORT_HGRLXU     hgrlxu_
#  define   FORT_HGRLXL     hgrlxl_
#  define   FORT_HGRLNF     hgrlnf_
#  define   FORT_HGRLNB     hgrlnb_
#  define   FORT_HGCG       hgcg_
#  define   FORT_HGCG1      hgcg1_
#  define   FORT_HGCG2      hgcg2_
#  define   FORT_HGIP       hgip_
#else
#  define   FORT_HGRES      HGRES
#  define   FORT_HGRESU     HGRESU
#  define   FORT_HGRLX      HGRLX
#  define   FORT_HGRLXU     HGRLXU
#  define   FORT_HGRLXL     HGRLXL
#  define   FORT_HGRLNF     HGRLNF
#  define   FORT_HGRLNB     HGRLNB
#  define   FORT_HGCG       HGCG
#  define   FORT_HGCG1      HGCG1
#  define   FORT_HGCG2      HGCG2
#  define   FORT_HGIP       HGIP
#endif

extern "C" {

#if (BL_SPACEDIM == 1)
  ERROR, not relevant
#else
#  ifdef HG_CONSTANT
  void FORT_HGRES(Real*, intS, Real*, Real*, intS, Real&);
  void FORT_HGRESU(Real*, intS, Real*, Real*, intS, Real&);
#    ifdef HG_CROSS_STENCIL
  void FORT_HGRLXU(Real*, Real*, intS, Real*, intS, Real&);
#    else
  void FORT_HGRLX(Real*, Real*, intS, Real*, intS, Real&);
#    endif
#  else
#    ifdef SIGMA_NODE
  void FORT_HGRES(Real*, intS, Real*, intS, Real*, intS, Real*, intS, intS);
  void FORT_HGRESU(Real*, intS, Real*, Real*, Real*, Real*, intS);
  void FORT_HGRLX(Real*, intS, Real*, intS, Real*, intS, Real*, intS, intS);
  void FORT_HGRLXU(Real*, Real*, Real*, Real*, intS, Real*, intS);
  void FORT_HGRLXL(Real*, intS, Real*, intS, Real*, intS, Real*, intS,
		   intS, intS, const int&);
  void FORT_HGRLNF(Real*, intS, Real*, intS, Real*, intS, Real*, intS,
		   Real*, intS, intS, intS, const int&, const int&);
#    else
  void FORT_HGRES(Real*, intS, Real*, intS, Real*, intS, RealPS, intS, intS,
		  RealRS, const int&, const int&);
  void FORT_HGRLX(Real*, intS, Real*, intS, RealPS, intS, Real*, intS, intS,
		  RealRS, const int&, const int&);
  void FORT_HGRLXL(Real*, intS, Real*, intS, RealPS, intS, Real*, intS,
                   intS, intS, RealRS, const int&, const int&, const int&);
  void FORT_HGRLNF(Real*, intS, Real*, intS, Real*, intS,
		   RealPS, intS, Real*, intS, intS, intS, RealRS,
		   const int&, const int&, const int&, const int&);
#    endif
  void FORT_HGRLNB(Real*, intS, Real*, intS,
		   intS, const int&, const int&);
#  endif

#  if (CGOPT == 1)
  void FORT_HGCG1(Real*, Real*, Real*, Real*, Real*, Real*, Real*,
		  intS, const Real&, Real&);
  void FORT_HGCG2(Real*, Real*, intS, const Real&);
  void FORT_HGIP(Real*, Real*, Real*, intS, Real&);
#  elif (CGOPT == 2)
#    if (BL_SPACEDIM == 2)
  void FORT_HGCG(Real*, Real*, Real*, Real*, Real*, Real*, Real*,
		 const int&, int*, int*,
		 int*, int*, int*, int*, int*, int*, int*,
		 const int&, Real*,
		 int*, int*, int*, int*, int*,
		 const Real&, Real&, Real&, int&, const int&);
#    else
  void FORT_HGCG(Real*, Real*, Real*, Real*, Real*, Real*, Real*,
		 const int&, int*, int*, int*,
		 int*, int*, int*, int*, int*, int*, int*,
		 const int&, Real*,
		 int*, int*, int*, int*, int*, int*, int*, int*,
		 const Real&, Real&, Real&, int&, const int&);
#    endif
#  endif
#endif
}

void holy_grail_amr_multigrid::level_residual(MultiFab& r,
					      MultiFab& s,
					      MultiFab& d,
					      copy_cache* dbc,
					      int mglev,
					      int iclear)
{
	TRACER("holy_grail_amr_multigrid::level_residual");
  assert(r.boxArray() == s.boxArray());
  assert(r.boxArray() == d.boxArray());

  int igrid;

  {
	TRACER("wrapped call about fill_boundary(...)");
  fill_borders(d, dbc, interface[mglev], mg_boundary);
  }

#ifdef SIGMA_NODE

  for (igrid = 0; igrid < mg_mesh[mglev].length(); igrid++) {
    const Box& rbox = r[igrid].box();
    const Box& freg = interface[mglev].part_fine(igrid);
    FORT_HGRESU(r[igrid].dataPtr(), dimlist(rbox),
                s[igrid].dataPtr(),
                d[igrid].dataPtr(),
                sigma_node[mglev][igrid].dataPtr(),
                mask[mglev][igrid].dataPtr(),
                dimlist(freg));
  }

#else

  Real hx = h[mglev][0];
  Real hy = h[mglev][1];
#  if (BL_SPACEDIM == 3)
  Real hz = h[mglev][2];
#  endif

#  ifdef HG_CONSTANT

  if (!iclear) {
    for (igrid = 0; igrid < mg_mesh[mglev].length(); igrid++) {
      const Box& rbox = r[igrid].box();
      const Box& freg = interface[mglev].part_fine(igrid);
      FORT_HGRESU(r[igrid].dataPtr(), dimlist(rbox),
		  s[igrid].dataPtr(), d[igrid].dataPtr(),
		  dimlist(freg), hx);
    }
  }
  else {
    for (igrid = 0; igrid < mg_mesh[mglev].length(); igrid++) {
      const Box& rbox = r[igrid].box();
      const Box& freg = interface[mglev].part_fine(igrid);
      FORT_HGRES(r[igrid].dataPtr(), dimlist(rbox),
		 s[igrid].dataPtr(), d[igrid].dataPtr(),
		 dimlist(freg), hx);
    }
    clear_part_interface(r, interface[mglev]);
  }

#  else

  for (igrid = 0; igrid < mg_mesh[mglev].length(); igrid++) {
    const Box& rbox = r[igrid].box();
    const Box& sbox = s[igrid].box();
    const Box& dbox = d[igrid].box();
    const Box& freg = interface[mglev].part_fine(igrid);
#    ifndef SIGMA_NODE
    // this branch is the only one that can be reached here
    const Box& sigbox = sigma[mglev][igrid].box();
    FORT_HGRES(r[igrid].dataPtr(), dimlist(rbox),
	       s[igrid].dataPtr(), dimlist(sbox),
	       d[igrid].dataPtr(), dimlist(dbox),
	       sigma_nd[0][mglev][igrid].dataPtr(),
#      if (BL_SPACEDIM == 2)
	       sigma_nd[1][mglev][igrid].dataPtr(), dimlist(sigbox),
	       dimlist(freg), hx, hy,
	       IsRZ(), mg_domain[mglev].bigEnd(0) + 1
#      else
	       sigma_nd[1][mglev][igrid].dataPtr(),
	       sigma_nd[2][mglev][igrid].dataPtr(), dimlist(sigbox),
               dimlist(freg), hx, hy, hz
#      endif
	       );
#    else
    // this branch is unreachable
    const Box& sigbox = sigma_node[mglev][igrid].box();
    FORT_HGRES(r[igrid].dataPtr(), dimlist(rbox),
               s[igrid].dataPtr(), dimlist(sbox),
               d[igrid].dataPtr(), dimlist(dbox),
               sigma_node[mglev][igrid].dataPtr(), dimlist(sigbox),
               dimlist(freg));
#    endif // SIGMA_NODE
  }

  if (iclear) {
    clear_part_interface(r, interface[mglev]);
  }

#  endif // HG_CONSTANT
#endif // SIGMA_NODE
}

void holy_grail_amr_multigrid::relax(int mglev, int i1, int is_zero)
{
#if defined(HG_CONSTANT) || !defined(SIGMA_NODE)
  Real hx = h[mglev][0];
  Real hy = h[mglev][1];
#if (BL_SPACEDIM == 3)
  Real hz = h[mglev][2];
#endif
#endif

  DECLARE_GEOMETRY_TYPES;

  Box tdom = mg_domain[mglev];
  tdom.convert(nodevect);

  for (int icount = 0; icount < i1; icount++) {

    if (smoother_mode == 0 || smoother_mode == 1 || line_solve_dim == -1) {

      if (is_zero == 0)
	fill_borders(corr[mglev], corr_bcache[mglev],
		     interface[mglev], mg_boundary);
      else
	is_zero = 0;
      for (int igrid = 0; igrid < mg_mesh[mglev].length(); igrid++) {
	const Box& sbox = resid[mglev][igrid].box();
	const Box& freg = interface[mglev].part_fine(igrid);
	if (line_solve_dim == -1) {
	  // Gauss-Seidel section:
#ifdef HG_CONSTANT
#  ifdef HG_CROSS_STENCIL
	  FORT_HGRLXU(corr[mglev][igrid].dataPtr(),
		      resid[mglev][igrid].dataPtr(), dimlist(sbox),
		      mask[mglev][igrid].dataPtr(),
		      dimlist(freg), hx);
#  else
	  FORT_HGRLX(corr[mglev][igrid].dataPtr(),
		     resid[mglev][igrid].dataPtr(), dimlist(sbox),
		     mask[mglev][igrid].dataPtr(),
		     dimlist(freg), hx);
#  endif
#else
#ifdef SIGMA_NODE
/*
	  const Box& fbox = corr[mglev][igrid].box();
	  const Box& cenbox = cen[mglev][igrid].box();
	  const Box& sigbox = sigma_node[mglev][igrid].box();
	  FORT_HGRLX(corr[mglev][igrid].dataPtr(), dimlist(fbox),
		     resid[mglev][igrid].dataPtr(), dimlist(sbox),
		     sigma_node[mglev][igrid].dataPtr(), dimlist(sigbox),
		     cen[mglev][igrid].dataPtr(), dimlist(cenbox),
		     dimlist(freg));
*/
	  FORT_HGRLXU(corr[mglev][igrid].dataPtr(),
		      resid[mglev][igrid].dataPtr(),
		      sigma_node[mglev][igrid].dataPtr(),
		      cen[mglev][igrid].dataPtr(), dimlist(sbox),
		      mask[mglev][igrid].dataPtr(),
		      dimlist(freg));
#else
	  const Box& fbox = corr[mglev][igrid].box();
	  const Box& cenbox = cen[mglev][igrid].box();
	  const Box& sigbox = sigma[mglev][igrid].box();
	  FORT_HGRLX(corr[mglev][igrid].dataPtr(), dimlist(fbox),
		     resid[mglev][igrid].dataPtr(), dimlist(sbox),
		     sigma_nd[0][mglev][igrid].dataPtr(),
#  if (BL_SPACEDIM == 2)
		     sigma_nd[1][mglev][igrid].dataPtr(), dimlist(sigbox),
		     cen[mglev][igrid].dataPtr(), dimlist(cenbox),
		     dimlist(freg), hx, hy,
		     IsRZ(), mg_domain[mglev].bigEnd(0) + 1
#  else
		     sigma_nd[1][mglev][igrid].dataPtr(),
		     sigma_nd[2][mglev][igrid].dataPtr(), dimlist(sigbox),
		     cen[mglev][igrid].dataPtr(), dimlist(cenbox),
		     dimlist(freg), hx, hy, hz
#  endif
		     );
#endif
#endif
	}
	else {
	  // Grid-by-grid line solve section:
#ifdef HG_CONSTANT
	  BoxLib::Error("Constant-coefficient line solves not implemented");
#else
	  const Box& fbox = corr[mglev][igrid].box();
	  const Box& cenbox = cen[mglev][igrid].box();
#  ifdef SIGMA_NODE
	  const Box& sigbox = sigma_node[mglev][igrid].box();
	  FORT_HGRLXL(corr[mglev][igrid].dataPtr(), dimlist(fbox),
		      resid[mglev][igrid].dataPtr(), dimlist(sbox),
		      sigma_node[mglev][igrid].dataPtr(), dimlist(sigbox),
		      cen[mglev][igrid].dataPtr(), dimlist(cenbox),
		      dimlist(freg), dimlist(tdom), line_solve_dim);
#  else
	  const Box& sigbox = sigma[mglev][igrid].box();
	  FORT_HGRLXL(corr[mglev][igrid].dataPtr(), dimlist(fbox),
		      resid[mglev][igrid].dataPtr(), dimlist(sbox),
		      sigma_nd[0][mglev][igrid].dataPtr(),
#    if (BL_SPACEDIM == 2)
		      sigma_nd[1][mglev][igrid].dataPtr(), dimlist(sigbox),
		      cen[mglev][igrid].dataPtr(), dimlist(cenbox),
		      dimlist(freg), dimlist(tdom), hx, hy,
		      IsRZ(), mg_domain[mglev].bigEnd(0) + 1, line_solve_dim
#    else
		      sigma_nd[1][mglev][igrid].dataPtr(),
		      sigma_nd[2][mglev][igrid].dataPtr(), dimlist(sigbox),
		      cen[mglev][igrid].dataPtr(), dimlist(cenbox),
		      dimlist(freg), dimlist(tdom), hx, hy, hz
#    endif
		      );
#  endif
#endif
	}
      }
      sync_borders(corr[mglev], corr_scache[mglev],
		   interface[mglev], mg_boundary);
    }
    else {
      // Full-level line solve section:
      if (line_order.length() == 0) {
	build_line_order(line_solve_dim);
      }
      int lev = lev_min, i;
      while (ml_index[lev] < mglev)
	lev++;

      for (int ipass = 0; ipass <= 1; ipass++) {
	if (is_zero == 0)
	  fill_borders(corr[mglev], corr_bcache[mglev],
		       interface[mglev], mg_boundary);
	else
	  is_zero = 0;

	// Forward solve:
	for (i = 0; i < mg_mesh[mglev].length(); i++) {

	  // Do grids in order along line_solve_dim:
	  int igrid = line_order[lev][i];
	  const Box& sbox = resid[mglev][igrid].box();
	  const Box& freg = corr[mglev].box(igrid);
#ifdef HG_CONSTANT
	  BoxLib::Error("Constant-coefficient line solves not implemented");
#else
	  const Box& fbox = corr[mglev][igrid].box();
	  const Box& wbox = work[mglev][igrid].box();
	  const Box& cenbox = cen[mglev][igrid].box();
#  ifdef SIGMA_NODE
	  const Box& sigbox = sigma_node[mglev][igrid].box();
	  FORT_HGRLNF(corr[mglev][igrid].dataPtr(), dimlist(fbox),
		      resid[mglev][igrid].dataPtr(), dimlist(sbox),
		      work[mglev][igrid].dataPtr(), dimlist(wbox),
		      sigma_node[mglev][igrid].dataPtr(), dimlist(sigbox),
		      cen[mglev][igrid].dataPtr(), dimlist(cenbox),
		      dimlist(freg), dimlist(tdom), line_solve_dim, ipass);
#  else
	  const Box& sigbox = sigma[mglev][igrid].box();
	  FORT_HGRLNF(corr[mglev][igrid].dataPtr(), dimlist(fbox),
		      resid[mglev][igrid].dataPtr(), dimlist(sbox),
		      work[mglev][igrid].dataPtr(), dimlist(wbox),
		      sigma_nd[0][mglev][igrid].dataPtr(),
#    if (BL_SPACEDIM == 2)
		      sigma_nd[1][mglev][igrid].dataPtr(), dimlist(sigbox),
		      cen[mglev][igrid].dataPtr(), dimlist(cenbox),
		      dimlist(freg), dimlist(tdom), hx, hy,
		      IsRZ(), mg_domain[mglev].bigEnd(0) + 1,
		      line_solve_dim, ipass
#    else
		      sigma_nd[1][mglev][igrid].dataPtr(),
		      sigma_nd[2][mglev][igrid].dataPtr(), dimlist(sigbox),
		      cen[mglev][igrid].dataPtr(), dimlist(cenbox),
		      dimlist(freg), dimlist(tdom), hx, hy, hz
#    endif
		      );
#  endif
#endif

	  // Copy work arrays to following grids:
	  ListIterator<int> j(line_after[lev][igrid]);
	  for ( ; j; j++) {
	    Box b = (freg & corr[mglev].box(j()));
	    internal_copy(corr[mglev], j(), igrid, b);
	    internal_copy(work[mglev], j(), igrid, b);
	  }
	}

	// Back substitution:
	for (i = mg_mesh[mglev].length() - 1; i >= 0; i--) {

	  // Do grids in reverse order along line_solve_dim:
	  int igrid = line_order[lev][i];
	  const Box& freg = corr[mglev].box(igrid);

	  // Copy solution array from following grids:
	  ListIterator<int> j(line_after[lev][igrid]);
	  for ( ; j; j++) {
	    Box b = (freg & corr[mglev].box(j()));
	    internal_copy(corr[mglev], igrid, j(), b);
	  }

	  const Box& fbox = corr[mglev][igrid].box();
	  const Box& wbox = work[mglev][igrid].box();
	  FORT_HGRLNB(corr[mglev][igrid].dataPtr(), dimlist(fbox),
		      work[mglev][igrid].dataPtr(), dimlist(wbox),
		      dimlist(freg), line_solve_dim, ipass);
	}
      }
    }
  }
}

void holy_grail_amr_multigrid::build_line_order(int lsd)
{
  line_order.resize(lev_max + 1);
  line_after.resize(lev_max + 1);

  for (int lev = lev_min; lev <= lev_max; lev++) {
    int igrid, i, mglev = ml_index[lev], ngrids = mg_mesh[mglev].length();

    line_order[lev].resize(ngrids);
    line_after[lev].resize(ngrids);

    for (igrid = 0; igrid < ngrids; igrid++) {
      line_order[lev].set(igrid, igrid);

      // bubble sort, replace with something faster if necessary:
      for (i = igrid; i > 0; i--) {
	if (ml_mesh[lev][line_order[lev][i]].smallEnd(lsd) <
	    ml_mesh[lev][line_order[lev][i-1]].smallEnd(lsd)) {
	  int tmp              = line_order[lev][i-1];
	  line_order[lev][i-1] = line_order[lev][i];
	  line_order[lev][i]   = tmp;
	}
	else {
	  break;
	}
      }

      for (i = 0; i < ngrids; i++) {
	if (bdryLo(ml_mesh[lev][i], lsd).intersects
	      (bdryHi(ml_mesh[lev][igrid], lsd))) {
	  line_after[lev][igrid].append(i);
	}
      }
    }
/*
    for (igrid = 0; igrid < ngrids; igrid++) {
      cout << line_order[lev][igrid] << "    ";
      ListIterator<int> j(line_after[lev][igrid]);
      for ( ; j; j++) {
	cout << " " << j();
      }
      cout << endl;
    }
*/
  }
}

void holy_grail_amr_multigrid::cgsolve(int mglev)
{
  assert(mglev == 0);

  MultiFab& r = cgwork[0];
  MultiFab& p = cgwork[1];
  MultiFab& z = cgwork[2];
  MultiFab& x = cgwork[3];
  MultiFab& w = cgwork[4];
  MultiFab& c = cgwork[5];
  MultiFab& zero_array = cgwork[6];
  MultiFab& ipmask = cgwork[7];


  Real alpha, rho;
  int i = 0, igrid;

  // x (corr[0]) should be all 0.0 at this point
  for (igrid = 0; igrid < mg_mesh[mglev].length(); igrid++) {
    r[igrid].copy(resid[mglev][igrid]);
    r[igrid].negate();
  }
  //r.copy(resid[mglev]);
  //r.negate();

  if (singular) {
    // singular systems are very sensitive to solvability
    w.setVal(1.0);
    alpha = inner_product(r, w) / mg_domain[mglev].volume();
    r.plus(-alpha, 0);
  }

  copy_cache* pbc = cgw1_bcache;
#if (CGOPT == 2)
#error "Unexplored code..."
      if ( cgw1_bcache == 0 )
      {
    BoxLib::Error("cgw1_bcache is zero");
    }
      unroll_cache* ruc = cgw_ucache[0];
      unroll_cache* puc = cgw_ucache[1];
      unroll_cache* zuc = cgw_ucache[2];
      unroll_cache* xuc = cgw_ucache[3];
      unroll_cache* wuc = cgw_ucache[4];
      unroll_cache* cuc = cgw_ucache[5];
      unroll_cache* muc = cgw_ucache[7];

      if ( ruc == 0 || puc == 0 ||
	   zuc == 0 || xuc == ||
	   wuc == 0 || cuc == 0 ||
	   muc == 0 )
	{
	    BoxLib::Error("an unrolled cache is zero");
	}
  FORT_HGCG(ruc->ptr, puc->ptr,
	    zuc->ptr, xuc->ptr,
	    wuc->ptr, cuc->ptr,
	    muc.ptr, mg_mesh[0].length(),
#  if (BL_SPACEDIM == 2)
	    ruc->strid, ruc->nvals,
#  else
	    ruc->strid1, ruc->strid2,
	    ruc->nvals,
#  endif
	    ruc->start, puc->start,
	    zuc->start, xuc->start,
	    wuc->start, cuc->start,
	    muc->start,
	    pbc->nsets, pbc->dptr,
#  if (BL_SPACEDIM == 2)
	    pbc->nvals,
	    pbc->dstart, pbc->sstart,
	    pbc->dstrid, pbc->sstrid,
#  else
	    pbc->nvals1, pbc->nvals2,
	    pbc->dstart, pbc->sstart,
	    pbc->dstrid1, pbc->dstrid2,
	    pbc->sstrid1, pbc->sstrid2,
#  endif
	    h[0][0], alpha, rho, i, pcode);
#elif (CGOPT == 1)
  rho = 0.0;
  for (igrid = 0; igrid < mg_mesh[mglev].length(); igrid++) {
    z[igrid].copy(r[igrid]);
    z[igrid].mult(c[igrid]);
    const Box& reg = p[igrid].box();
    FORT_HGIP(z[igrid].dataPtr(), r[igrid].dataPtr(),
	      ipmask[igrid].dataPtr(),
	      dimlist(reg), rho);
    p[igrid].copy(z[igrid]);
  }
  Real tol = 1.e-3 * rho;

  while (tol > 0.0) {
    i++;
    if (i > 250 && pcode >= 2)
      cout << "Conjugate-gradient iteration failed to converge" << endl;
    Real rho_old = rho;
    // safe to set the clear flag to 0 here---bogus values make it
    // into r but are cleared from z by the mask in c
    level_residual(w, zero_array, p, pbc, 0, 0);
    alpha = 0.0;
    for (igrid = 0; igrid < mg_mesh[mglev].length(); igrid++) {
      const Box& reg = p[igrid].box();
      FORT_HGIP(p[igrid].dataPtr(), w[igrid].dataPtr(),
		ipmask[igrid].dataPtr(),
		dimlist(reg), alpha);
    }
    alpha = rho / alpha;
    rho = 0.0;
    for (igrid = 0; igrid < mg_mesh[mglev].length(); igrid++) {
      const Box& reg = p[igrid].box();
      FORT_HGCG1(r[igrid].dataPtr(), p[igrid].dataPtr(),
		 z[igrid].dataPtr(), x[igrid].dataPtr(),
		 w[igrid].dataPtr(), c[igrid].dataPtr(),
		 ipmask[igrid].dataPtr(), dimlist(reg), alpha, rho);
    }
    if (pcode >= 3)
      cout << i << " " << rho << endl;
    if (rho <= tol || i > 250)
      break;
    alpha = rho / rho_old;
    for (igrid = 0; igrid < mg_mesh[mglev].length(); igrid++) {
      const Box& reg = p[igrid].box();
      FORT_HGCG2(p[igrid].dataPtr(), z[igrid].dataPtr(),
		 dimlist(reg), alpha);
    }
  }
#else
  for (igrid = 0; igrid < mg_mesh[mglev].length(); igrid++) {
    z[igrid].copy(r[igrid]);
    z[igrid].mult(c[igrid]);
  }
  //z.assign(r).mult(c);
  rho = inner_product(z, r);
  Real tol = 1.e-3 * rho;
  //p.assign(0.0);
  for (igrid = 0; igrid < mg_mesh[mglev].length(); igrid++) {
    p[igrid].copy(z[igrid]);
  }

  while (tol > 0.0) {
    i++;
    if (i > 250 && pcode >= 2)
      cout << "Conjugate-gradient iteration failed to converge" << endl;
    Real rho_old = rho;
    // safe to set the clear flag to 0 here---bogus values make it
    // into r but are cleared from z by the mask in c
    level_residual(w, zero_array, p, pbc, 0, 0);
    alpha = rho / inner_product(p, w);
    for (igrid = 0; igrid < mg_mesh[mglev].length(); igrid++) {
      w[igrid].mult(alpha);
      r[igrid].minus(w[igrid]);
      w[igrid].copy(p[igrid]);
      w[igrid].mult(alpha);
      x[igrid].plus(w[igrid]);
      z[igrid].copy(r[igrid]);
      z[igrid].mult(c[igrid]);
    }
    //r.minus(w.mult(alpha));
    //x.plus(w.assign(p).mult(alpha));
    //z.assign(r).mult(c);
    rho = inner_product(z, r);
    if (pcode >= 3)
      cout << i << " " << rho << endl;
    if (rho <= tol || i > 250)
      break;
    for (igrid = 0; igrid < mg_mesh[mglev].length(); igrid++) {
      p[igrid].mult(rho / rho_old);
      p[igrid].plus(z[igrid]);
    }
    //p.mult(rho / rho_old).plus(z);
  }
#endif

  if (pcode >= 2)
    cout << i << " iterations required for conjugate-gradient" << endl;
}
