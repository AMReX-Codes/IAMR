
#include "amr_multi.H"

int amr_multigrid::c_sys = 0; // default is Cartesian, 1 is RZ

void amr_multigrid::mesh_read(Array<BoxArray>& m, Array<IntVect>& r,
			      Array<Box>& d, istream& is)
{
  Box b;
  int ilev, igrid;
  is >> ilev;
  m.resize(ilev);
  r.resize(ilev-1);
  d.resize(ilev);
  for (ilev = 0; ilev < m.length(); ilev++) {
    is >> b >> igrid;
    d.set(ilev, b);
    if (ilev > 0) {
      r.set(ilev-1, d[ilev].length() / d[ilev-1].length());
    }
    m[ilev].resize(igrid);
    for (igrid = 0; igrid < m[ilev].length(); igrid++) {
      is >> b;
      m[ilev].set(igrid, b);
    }
  }
}

void amr_multigrid::mesh_write(Array<BoxArray>& m,
			       Array<Box>& d, ostream& os)
{
  int ilev, igrid;
  os << m.length() << endl;
  for (ilev = 0; ilev < m.length(); ilev++) {
    os << "    " << d[ilev] << " " << m[ilev].length() << endl;
    for (igrid = 0; igrid < m[ilev].length(); igrid++) {
      os << '\t' << m[ilev][igrid] << endl;;
    }
  }
}

void amr_multigrid::mesh_write(Array<BoxArray>& m, Array<IntVect>& r,
			       Box fd, ostream& os)
{
  int ilev, igrid;
  for (ilev = m.length() - 2; ilev >= 0; ilev--) {
    fd.coarsen(r[ilev]);
  }
  os << m.length() << endl;
  for (ilev = 0; ilev < m.length(); ilev++) {
    os << "    " << fd << " " << m[ilev].length() << endl;
    for (igrid = 0; igrid < m[ilev].length(); igrid++) {
      os << '\t' << m[ilev][igrid] << endl;;
    }
    if (ilev <= m.length() - 2) {
      fd.refine(r[ilev]);
    }
  }
}

amr_multigrid::~amr_multigrid()
{
  for (lev_min = lev_min_min; lev_min <= lev_min_max; lev_min++) {
    delete [] interface_array[lev_min - lev_min_min];
  }
  delete [] interface_array;
}

void amr_multigrid::build_mesh(const Box& fdomain)
{
  mg_domain_array.resize(lev_min_max + 1);
  mg_mesh_array.resize(lev_min_max + 1);
  interface_array = new level_interface*[lev_min_max - lev_min_min + 1];

  if (pcode >= 2) {
    mesh_write(ml_mesh, gen_ratio, fdomain, cout);
  }

  lev_max = lev_max_max;
  for (lev_min = lev_min_min; lev_min <= lev_min_max; lev_min++) {
    // first, build mg_mesh
    int nlev = build_down(ml_mesh[lev_max], fdomain,
			  lev_max, IntVect::TheUnitVector(), 0);
    int i;
#ifndef NDEBUG
    for (i = 0; i < mg_mesh.length(); i++)
      assert(mg_mesh[i].ok());
#endif

    if (pcode >= 2) {
      mesh_write(mg_mesh, mg_domain, cout);
    }

    mg_domain_array.set(lev_min, mg_domain);
    mg_mesh_array.set(lev_min, mg_mesh);

    // initialize interface

    int mglev_common = mg_mesh.length(), ldiff;
    if (lev_min > lev_min_min) {
      ldiff = mg_mesh_array[lev_min - 1].length() - mg_mesh.length();

      // ml_index here is still the index for the previous mg_mesh:
      mglev_common = ml_index[lev_min] - ldiff;
/*
         Before IntVect refinement ratios we used

      mglev_common = ml_index[lev_min - 1] + 1 - ldiff;

         This can no longer be counted on to work since in a (4,1) case
         the intermediate levels no longer match.
*/
      mglev_common = (mglev_common > 0) ? mglev_common : 0;
    }

    // ml_index now becomes the index for the current mg_mesh,
    // will be used below and on the next loop:
    build_index();

    interface = new level_interface[mg_mesh.length()];

    for (int mglev = mg_mesh.length() - 1, lev = lev_max + 1;
	 mglev >= 0; mglev--) {
      if (lev > lev_min && mglev == ml_index[lev - 1])
	lev--;
      if (mglev >= mglev_common) {
	i = mglev + ldiff;
	interface[mglev].copy(interface_array[lev_min - 1 - lev_min_min][i]);
      }
      else {
	if (mglev == ml_index[lev]) {
	  interface[mglev].alloc(mg_mesh[mglev], mg_domain[mglev],
				 mg_boundary);
	}
	else {
	  IntVect rat =
	    mg_domain[mglev+1].length() / mg_domain[mglev].length();
	  interface[mglev].alloc_coarsened(mg_mesh[mglev], mg_boundary,
					   interface[mglev + 1], rat);
	}
      }
    }
    interface_array[lev_min - lev_min_min] = interface;
  }
}

void amr_multigrid::build_index()
{
  ml_index.resize(lev_max + 1);
  for (int i = 0, lev = lev_min; lev <= lev_max; i++) {
    if (mg_mesh[i] == ml_mesh[lev]) {
      ml_index[lev] = i;
      lev++;
    }
  }
}

int amr_multigrid::build_down(const BoxArray& l_mesh, const Box& l_domain,
			      int flev, IntVect rat, int nlev)
{
  if (l_mesh.length() == 0) {
    mg_mesh.resize(nlev);
    mg_domain.resize(nlev);
    nlev = 0;
  }
  else {
    nlev++;
    BoxArray c_mesh = l_mesh;
    Box c_domain = l_domain;
    make_coarser_level(c_mesh, c_domain, flev, rat);
    nlev = build_down(c_mesh, c_domain, flev, rat, nlev);
    mg_mesh.set(nlev, l_mesh);
    mg_domain.set(nlev, l_domain);
    nlev++;
  }
  return nlev;
}

void amr_multigrid::make_coarser_level(BoxArray& mesh, Box& domain,
				       int& flev, IntVect& rat)
{
  if (flev > lev_min) {
    if ((rat * 2) >= gen_ratio[flev-1]) {
      mesh = ml_mesh[flev-1];
      domain.coarsen(gen_ratio[flev-1] / rat);
      flev--;
      rat = IntVect::TheUnitVector();
    }
    else {
      IntVect trat = rat;
      rat *= 2;
      rat.min(gen_ratio[flev-1]);
      trat = (rat / trat);
      mesh.coarsen(trat);
      domain.coarsen(trat);
    }
  }
  else if (can_coarsen(mesh, domain)) {
    rat *= 2;
    mesh.coarsen(2);
    domain.coarsen(2);
  }
  else {
    mesh.clear();
  }
}

void amr_multigrid::alloc(PArray<MultiFab>& Dest, PArray<MultiFab>& Source,
			  PArray<MultiFab>& Coarse_source,
			  int Lev_min, int Lev_max)
{
  lev_min = Lev_min;
  lev_max = Lev_max;

  assert(lev_min >= lev_min_min &&
	 lev_min <= lev_min_max &&
	 lev_max <= lev_max_max);

  int i;
  assert(type(Source[lev_min]) == type(Dest[lev_min]));
  for (i = lev_min; i <= lev_max; i++)
    assert(Source[i].boxArray() == Dest[i].boxArray());

  // old version checked that these matched ml_mesh, but that's
  // harder to do with pure BoxLib.
  //if (source.mesh() != ml_mesh || dest.mesh() != ml_mesh)
  //  error("alloc---meshes no match");

  dest.resize(lev_max + 1);
  source.resize(lev_max + 1);
  coarse_source.resize(lev_max + 1);

  for (i = lev_min; i <= lev_max; i++) {
    dest.set(i, &Dest[i]);
    source.set(i, &Source[i]);
    if (i < Coarse_source.length() && Coarse_source.defined(i))
      coarse_source.set(i, &Coarse_source[i]);
  }

  mg_domain = mg_domain_array[lev_min];
  mg_mesh = mg_mesh_array[lev_min];
  interface = interface_array[lev_min - lev_min_min];

  build_index();

  mglev_max = ml_index[lev_max];

  resid.resize(mglev_max + 1);
  corr.resize(mglev_max + 1);
  work.resize(mglev_max + 1);
  save.resize(lev_max + 1);

  dest_bcache.resize(lev_max + 1, (copy_cache*)0);
  corr_bcache.resize(mglev_max + 1, (copy_cache*)0);
  work_bcache.resize(mglev_max + 1, (copy_cache*)0);

  for (i = 0; i <= mglev_max; i++) {
    BoxArray mesh = mg_mesh[i];
    mesh.convert(type(source[lev_min]));
    resid.set(i, new MultiFab(mesh, 1, source[lev_min].nGrow()));
    corr.set(i, new MultiFab(mesh, 1, dest[lev_min].nGrow()));
    if (type(dest[lev_min]) == cellvect)
      work.set(i, new MultiFab(mesh, 1, 0));
    else
      work.set(i, new MultiFab(mesh, 1, 1));

    resid[i].setVal(0.0);
    // to clear border cells, which will be assigned into corr:
    if (work[i].nGrow() > 0)
      work[i].setVal(0.0);

    // if a cache is desired, it should be created by the derived class:
    corr_bcache.set(i, 0);
    work_bcache.set(i, 0);
  }

  for (i = lev_min + 1; i <= lev_max - 1; i++) {
    save.set(i, new MultiFab(dest[i].boxArray(), 1, 0));
  }

  for (i = lev_min; i <= lev_max; i++) {
    // if a cache is desired, it should be created by the derived class:
    dest_bcache.set(i, 0);
  }
}

void amr_multigrid::clear()
{
  int i;
  for (i = 0; i <= mglev_max; i++) {
    if (resid.defined(i)) delete resid.remove(i);
    if (corr.defined(i))  delete corr.remove(i);
    if (work.defined(i))  delete work.remove(i);
  }
  for (i = lev_min; i <= lev_max; i++) {
    if (save.defined(i))  delete save.remove(i);
    dest.clear(i);
    source.clear(i);
    coarse_source.clear(i);
  }
  mg_mesh.clear();
}

void amr_multigrid::solve(Real reltol, Real abstol, int i1, int i2,
			  int linesolvdim)
{
  assert(linesolvdim == -1); // line solves not supported through this arg

  if (lev_max > lev_min)
    sync_interfaces();

  Real norm = 0.0;
  for (int lev = lev_min; lev <= lev_max; lev++) {
    Real lev_norm = mfnorm(source[lev]);
    norm = (lev_norm > norm) ? lev_norm : norm;
    //if (pcode >= 2)
    //  cout << "Source norm is " << lev_norm << " at level " << lev << endl;
  }
  if (pcode >= 1)
    cout << "Source norm is " << norm << endl;

  Real err = ml_cycle(lev_max, mglev_max, i1, i2, abstol);
  int it = 1;

  norm = (err > norm) ? err : norm;
  Real tol = reltol * norm;
  tol = (tol > abstol) ? tol : abstol;

  while (err > tol) {
    err = ml_cycle(lev_max, mglev_max, i1, i2, tol);
    it++;
    if (it > 100)
      BoxLib::Error("amr_multigrid::solve---multigrid iteration failed");
  }
  if (pcode >= 1)
    cout << it << " cycles required" << endl;

  //This final restriction not needed unless you want coarse and fine
  //potentials to match up for use by the calling program.  Coarse
  //and fine values of dest already match at interfaces, and coarse
  //values under the fine grids have no effect on subsequent calls to
  //this solver.

  //for (lev = lev_max; lev > lev_min; lev--) {
  //  dest[lev].restrict_level(dest[lev-1]);
  //}
}

Real amr_multigrid::ml_cycle(int lev, int mglev, int i1, int i2,
			     Real tol, Real res_norm_fine)
{
  MultiFab& dtmp = dest[lev];
  MultiFab& ctmp = corr[mglev];

  // If level lev+1 exists, resid should be up to date there.

  // includes restriction from next finer level
  Real res_norm = ml_residual(mglev, lev);

  if (pcode >= 2)
    cout << "Residual at level " << lev << " is " << res_norm << endl;

  res_norm = (res_norm_fine > res_norm) ? res_norm_fine : res_norm;

  // resid now correct on this level---relax.
  ctmp.setVal(0.0); // ctmp.setBndry(0.0); // is necessary?
  if (lev > lev_min || res_norm > tol) {
    //mg_cycle(mglev, i1, i2, 1);
    mg_cycle(mglev, (lev == lev_min) ? i1 : 0, i2, 1);
    dtmp.plus(ctmp, 0, 1, 0);
  }

  if (lev > lev_min) {
    MultiFab& stmp = source[lev];
    MultiFab& rtmp = resid[mglev];
    MultiFab& wtmp = work[mglev];
    if (lev < lev_max) {
      save[lev].copy(ctmp);
      level_residual(wtmp, rtmp, ctmp, corr_bcache[mglev], mglev, 0);
      rtmp.copy(wtmp);
    }
    else {
      level_residual(rtmp, stmp, dtmp, dest_bcache[lev], mglev, 0);
    }
    interface_residual(mglev, lev);
    int mgc = ml_index[lev-1];
    res_norm = ml_cycle(lev-1, mgc, i1, i2, tol, res_norm);
    // This assignment is only done to clear the borders of work,
    // so that garbage will not make it into corr and dest.  In
    // some experiments this garbage grew exponentially, creating
    // an overflow danger even though the legitimate parts of the
    // calculation were not affected:
    wtmp.setVal(0.0); // wtmp.setBndry(0.0); // Is necessary?
    mg_interpolate_level(mglev, mgc);
    ctmp.copy(wtmp);
    dtmp.plus(ctmp, 0, 1, 0);
    if (lev < lev_max) {
      save[lev].plus(ctmp, 0, 1, 0);
      level_residual(wtmp, rtmp, ctmp, corr_bcache[mglev], mglev, 0);
      rtmp.copy(wtmp);
    }
    else {
      level_residual(rtmp, stmp, dtmp, dest_bcache[lev], mglev, 0);
    }
    ctmp.setVal(0.0);
    //mg_cycle(mglev, i1, i2, 1);
    mg_cycle(mglev, 0, i2, 1);
    dtmp.plus(ctmp, 0, 1, 0);
    if (lev < lev_max) {
      ctmp.plus(save[lev], 0, 1, 0);
    }
  }

  return res_norm;
}

//#include "amr_graph.H"

Real amr_multigrid::ml_residual(int mglev, int lev)
{
  if (lev > lev_min) {
    // This call is necessary to clear garbage values on outside edges that
    // will not be touched by level_residual, even with the clear flag set.
    // The restriction routine is responsible for only passing correct
    // values down from the fine level.  Garbage values in dead cells are
    // a problem because the norm routine sees them.
    resid[mglev].setVal(0.0);
  }
  // Clear flag set here because we want to compute a norm, and to
  // kill a feedback loop by which garbage in the border of dest
  // could grow exponentially.
  level_residual(resid[mglev], source[lev], dest[lev], dest_bcache[lev],
		 mglev);
  if (lev < lev_max) {
    int mgf = ml_index[lev+1];
    work[mgf].copy(resid[mgf]);
    mg_restrict_level(mglev, mgf);
    if (coarse_source.ready() && coarse_source.defined(lev)) {
      resid[mglev].plus(coarse_source[lev], 0, 1, 0);
    }
  }
  //fit(mg_domain[mglev]);
  //contour(resid[mglev], 3, 1);
  //contour(resid[mglev], unitvect, 11, 1);
  //cout << mglev << " " << mfnorm(resid[mglev]) << endl;
  //cin.get();
  return mfnorm(resid[mglev]);
}

void amr_multigrid::mg_cycle(int mglev, int i1, int i2, int is_zero)
{
  int ltmp;
  if (mglev == 0) {
    cgsolve(mglev);
  }
  else if (get_amr_level(mglev - 1) == -1) {
    MultiFab& ctmp = corr[mglev];
    MultiFab& wtmp = work[mglev];

    relax(mglev, i1, is_zero);

    if (pcode >= 4) {
      wtmp.setVal(0.0);
      level_residual(wtmp, resid[mglev], ctmp, corr_bcache[mglev], mglev, 1);
      cout << "  Residual at multigrid level " << mglev << " is "
        << mfnorm(wtmp) << endl;
    }
    else {
      level_residual(wtmp, resid[mglev], ctmp, corr_bcache[mglev], mglev, 0);
    }

    mg_restrict_level(mglev-1, mglev);
    corr[mglev-1].setVal(0.0);
    mg_cycle(mglev-1, i1, i2, 1);
    //wtmp.assign(0.0);
    mg_interpolate_level(mglev, mglev-1);
    // Pitfall?  If corr and work both have borders, crud there will
    // now be put into corr.  This does not appear to be a problem,
    // but if it is it can be avoided by clearing work before
    // interpolating.
    ctmp.plus(wtmp, 0, 1, 0);
  }
  relax(mglev, i2, 0);
}

// Should include interface points
void amr_multigrid::mg_interpolate_level(int lto, int lfrom)
{
  MultiFab& target = work[lto];
  IntVect rat = mg_domain[lto].length() / mg_domain[lfrom].length();
  if (target.nGrow() == 0) {
    for (int i = 0; i < target.length(); i++) {
      interpolate_patch(target[i], corr[lfrom], rat,
			bilinear_interpolator, interface[lfrom]);
    }
  }
  else {
    for (int i = 0; i < target.length(); i++) {
      interpolate_patch(target[i], target.box(i), corr[lfrom], rat,
			bilinear_interpolator, interface[lfrom]);
    }
  }
}

void amr_multigrid::mg_restrict_level(int lto, int lfrom)
{
  IntVect rat = mg_domain[lfrom].length() / mg_domain[lto].length();
  if (type(resid[lto]) == cellvect) {
    restrict_level(resid[lto], 0, work[lfrom], rat, work_bcache[lfrom],
		   cell_average_restrictor);
  }
  else if (get_amr_level(lto) >= 0) {
    restrict_level(resid[lto], 0, work[lfrom], rat, work_bcache[lfrom],
		   bilinear_restrictor_coarse,
		   interface[lfrom], mg_boundary);
  }
  else {
    restrict_level(resid[lto], 0, work[lfrom], rat, work_bcache[lfrom],
		   bilinear_restrictor,
		   interface[lfrom], mg_boundary);
  }
}
