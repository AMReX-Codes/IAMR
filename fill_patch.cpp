
//
// $Id: fill_patch.cpp,v 1.13 1997-10-03 23:37:35 car Exp $
//

#include <fill_patch.H>
#include <Tracer.H>

#ifdef BL_FORT_USE_UNDERSCORE
#  define FIPRODC   iprodc_
#  define FIPRODN   iprodn_
#  define FFCPYU    fcpyu_
#  define FFCPY2    fcpy2_
#else
#  define FIPRODC   IPRODC
#  define FIPRODN   IPRODN
#  define FFCPYU    FCPYU
#  define FFCPY2    FCPY2
#endif

extern "C" 
{
  void FIPRODC(Real*, intS, Real*, intS, intS, Real*);
  void FIPRODN(Real*, intS, Real*, intS, intS, Real*);
  void FFCPYU(Real*, Real*, intS, const int&);
#if (BL_SPACEDIM == 2)
  void FFCPY2(Real*, intS, Real*, intS, intS, const int&, const int&);
#else
  void FFCPY2(Real*, intS, Real*, intS, intS, const int&, const int&, const int&);
#endif
}

void
fill_borders(MultiFab& r,
#ifdef HG_USE_CACHE
	     const copy_cache* border_cache,
#endif
	     const level_interface& interface,
	     amr_boundary bdy,
	     int w)
{
    TRACER("fill_patch::fill_borders");
#ifdef HG_USE_CACHE
    if (border_cache) 
    {
	// assumes cache built properly---does not check current bdy and w
	border_cache->run();
    }
#else
    if ( 0 );
#endif
    else 
    {
	fill_internal_borders(r, interface, w);
	bdy.fill_borders(r, interface, w);
    }
}

Real 
inner_product(MultiFab& r, MultiFab& s)
{
    assert(r.ok() && s.ok());
    assert(r.nComp() == 1);
    assert(s.nComp() == 1);
    assert(type(r) == type(s));
    
    Real sum = 0.0;
    
    if (type(r) == cellvect)
    {
	// PARALLEL -- REDUCTION
	// for (igrid = 0; igrid < r.length(); igrid++) 
	for ( MultiFabIterator rmfi(r); rmfi.isValid(); ++rmfi)
	{
	    DependentMultiFabIterator smfi(rmfi, s);
	    const Box& rbox = rmfi().box();
	    const Box& sbox = smfi().box();
	    const Box& reg  = rmfi.validbox();
	    FIPRODC(rmfi().dataPtr(), dimlist(rbox),
		smfi().dataPtr(), dimlist(sbox),
		dimlist(reg), &sum);
	}
    }
    else if (type(r) == nodevect) 
    {
	// PARALLEL -- REDUCTION
	// for (igrid = 0; igrid < r.length(); igrid++) 
	for ( MultiFabIterator rmfi(r); rmfi.isValid(); ++rmfi )
	{
	    DependentMultiFabIterator smfi(rmfi, s);
	    const Box& rbox = rmfi().box();
	    const Box& sbox = smfi().box();
	    const Box& reg  = rmfi.validbox();
	    FIPRODN(rmfi().dataPtr(), dimlist(rbox),
		smfi().dataPtr(), dimlist(sbox),
		dimlist(reg), &sum);
	}
    }
    else 
    {
	BoxLib::Error("inner_product---only supported for CELL- or NODE-based data");
    }
    ParallelDescriptor::ReduceRealSum(sum);
    return sum;
}

#if 0
const MultiFab&
initialize(Real (*f)(const Intvect&,const Intvect&,int,int))
{
    // PARALLEL
  for (int i = 0; i < mesh().ngrids(); i++) 
  {
    grid(i).initialize(f, mesh().sig());
  }
  return *this;
}

// Begin fillpatch stuff, still in unfinished state.
// Significant optimizations possible: avoid copying patches
// whenever an existing one will do.
// All Boxes and data objects must have same index type.

// Chops dest according to an intersection with source.  The returned
// box is the half completely outside of source, while dest is modified
// to contain the half that intersects source.  It is assumed that such a
// nontrivial chop is possible, or this routine would not have been called.

static Box 
box_chop(Box& dest, const Box& source)
{
  int i;
  for (i = BL_SPACEDIM - 1; i >= 0; i--) 
  {
    if (dest.type(i) == BOX_CELL) 
    {
      if (dest.smallEnd(i) <= source.bigEnd(i) &&
	  source.bigEnd(i) < dest.bigEnd(i)) 
      {
	return dest.chop(i,source.bigEnd(i)+1);
      }
      if (dest.smallEnd(i) < source.smallEnd(i) &&
	  source.smallEnd(i) <= dest.bigEnd(i)) 
      {
	Box tmp(dest);
	dest = tmp.chop(i,source.smallEnd(i));
	return tmp;
      }
    }
    else 
    {
      if (dest.smallEnd(i) < source.bigEnd(i) &&
	  source.bigEnd(i) < dest.bigEnd(i)) 
      {
	return dest.chop(i,source.bigEnd(i)).growLo(i, -1);
      }
      if (dest.smallEnd(i) < source.smallEnd(i) &&
	  source.smallEnd(i) < dest.bigEnd(i)) 
      {
	Box tmp(dest);
	dest = tmp.chop(i,source.smallEnd(i));
	return tmp.growHi(i, -1);
      }
    }
  }
  // this section only reached for node box outside but touching boundary
  for (i = BL_SPACEDIM - 1; i >= 0; i--) 
  {
    if (dest.bigEnd(i) == source.smallEnd(i) &&
	dest.smallEnd(i) < source.smallEnd(i)) 
    {
      Box tmp(dest);
      dest.setSmall(i, dest.bigEnd(i));
      return tmp.growHi(i, -1);
    }
    if (dest.smallEnd(i) == source.bigEnd(i) &&
	dest.bigEnd(i) > source.bigEnd(i)) 
    {
      Box tmp(dest);
      dest.setBig(i, dest.smallEnd(i));
      return tmp.growLo(i, -1);
    }
  }
  assert(0);
  return Box();
}

static int 
best_match(MultiFab& r, const Box& region, int& igrid, int bord)
{
  int overlap = 0;
  if (bord == r.nGrow()) {
    for (int i = 0; i < r.length(); i++) 
    {
      if (region.intersects(r[i].box()))
      {
        long t_long = (region & r[i].box()).numPts();
        assert(t_long < INT_MAX);
	int overlap1 = int(t_long);
	if (overlap1 > overlap) 
	{
	  igrid = i;
	  overlap = overlap1;
	}
      }
    }
  }
  else 
  {
    for (int i = 0; i < r.length(); i++) 
    {
      Box tb = grow(r[i].box(), bord - r.nGrow());
      if (region.intersects(tb)) 
      {
        long t_long = (region & tb).numPts();
        assert(t_long < INT_MAX);
	int overlap1 = int(t_long);
	if (overlap1 > overlap) 
	{
	  igrid = i;
	  overlap = overlap1;
	}
      }
    }
  }
  return (overlap > 0) ? (overlap == region.numPts() ? 1 : 2) : 0;
}
#endif

#if 0
grid_real get_patch(const Box& region,
		    const level_interface& interface,
		    amr_boundary bdy, int flags)
{
  int igrid;
  if (border() == 0 || (flags & 2)) {
    for (igrid = 0; igrid < mesh().ngrids(); igrid++) {
      if (box(igrid).contains(region)) {
	if (ncomp == fgrid(igrid).nComp())
	  return fgrid(igrid);
	else
	  return grid(igrid);
      }
    }
  }
  else {
    for (igrid = 0; igrid < mesh().ngrids(); igrid++) {
      Box tb = grow(box(igrid), -border());
      if (tb.contains(region)) {
	if (ncomp == fgrid(igrid).nComp())
	  return fgrid(igrid);
	else
	  return grid(igrid);
      }
    }
  }

  grid_real retgr(region, nComp());
  fill_patch(retgr, region, interface, bdy, flags);
  return retgr;
}

int get_patch(Fab& patch, const Box& region,
	      const level_interface& interface,
	      amr_boundary bdy, int flags)
{
  if (flags & 8) {
    for (int iqq = 0; iqq < flags/16; iqq++)
      cout << "  ";
    cout << "Getting " << region << '\n';
    flags += 16;
  }

  int igrid;
  if (border() == 0 || (flags & 2)) {
    for (igrid = 0; igrid < mesh().ngrids(); igrid++) {
      if (box(igrid).contains(region)) {
	if (ncomp == fgrid(igrid).nComp())
	  patch.alias(fgrid(igrid));
	else
	  patch.alias(grid(igrid));
	return 1;
      }
    }
  }
  else {
    for (igrid = 0; igrid < mesh().ngrids(); igrid++) {
      Box tb = grow(box(igrid), -border());
      if (tb.contains(region)) {
	if (ncomp == fgrid(igrid).nComp())
	  patch.alias(fgrid(igrid));
	else
	  patch.alias(grid(igrid));
	return 1;
      }
    }
  }

  patch.alloc(region, nComp());
  return fill_patch(patch, region, interface, bdy, flags);
}
#endif

int 
find_patch(const Box& region, MultiFab& r, int flags)
{
    int igrid;
    if (r.nGrow() == 0 || (flags & 2)) 
    {
	for (igrid = 0; igrid < r.length(); igrid++) 
	{
	    if (r[igrid].box().contains(region))
		return igrid;
	}
    }
    else 
    {
	for (igrid = 0; igrid < r.length(); igrid++) 
	{
	    if (r.box(igrid).contains(region))
		return igrid;
	}
    }
    
    return -1;
}

int 
fill_patch_blindly(Fab& patch,
		   const Box& region,
		   MultiFab& r,
		   int flags)
{
    int igrid;
    if (r.nGrow() == 0 || (flags & 2)) 
    {
	for (igrid = 0; igrid < r.length(); igrid++) 
	{
	    if (r[igrid].box().contains(region)) 
	    {
		patch.copy(r[igrid], region, 0, region, 0, patch.nComp());
		return 1;
	    }
	}
	for (igrid = 0; igrid < r.length(); igrid++) 
	{
	    if (r[igrid].box().intersects(region)) 
	    {
		Box tb = region & r[igrid].box();
		patch.copy(r[igrid], tb, 0, tb, 0, patch.nComp());
	    }
	}
    }
    else
    {
	for (igrid = 0; igrid < r.length(); igrid++) 
	{
	    Box tb = grow(r[igrid].box(), -r.nGrow());
	    if (tb.contains(region)) 
	    {
		patch.copy(r[igrid], region, 0, region, 0, patch.nComp());
		return 1;
	    }
	}
	for (igrid = 0; igrid < r.length(); igrid++) 
	{
	    Box tb = grow(r[igrid].box(), -r.nGrow());
	    if (tb.intersects(region)) 
	    {
		tb &= region;
		patch.copy(r[igrid], tb, 0, tb, 0, patch.nComp());
	    }
	}
    }
    return 0;
}

int 
fill_exterior_patch_blindly(Fab& patch,
			    const Box& region,
			    MultiFab& r,
			    const level_interface& interface,
			    amr_boundary bdy,
			    int flags)
{
    const BoxArray& em = interface.exterior_mesh();
    int igrid;
    for (igrid = 0; igrid < em.length(); igrid++) 
    {
	int jgrid = interface.direct_exterior_ref(igrid);
	if (jgrid >= 0) 
	{
	    Box tb;
	    tb = em[igrid];
	    tb.convert(type(r));
	    if (r.nGrow() > 0 && (flags & 2))
		tb.grow(r.nGrow());
	    if (tb.contains(region)) 
	    {
		bdy.fill(patch, region, r, jgrid, interface.domain());
		return 1;
	    }
	    if (tb.intersects(region)) 
	    {
		tb &= region;
		bdy.fill(patch, tb, r, jgrid, interface.domain());
	    }
	}
    }
    return 0;
}

int 
fill_patch(Fab& patch, const Box& region,
	   MultiFab& r,
	   const level_interface& interface,
	   amr_boundary bdy, int flags,
	   int idim, int index)
{
    if (!region.ok())
	return 1;
    
    if (flags & 4)
	patch.setVal(0.0, region, 0, patch.nComp());
    
    assert(patch.nComp() == r.nComp());
    assert(type(patch) == type(r));
    assert(interface.ok());
    
    Box tdomain = interface.domain();
    tdomain.convert(type(patch));
    Box idomain = grow(tdomain, zerovect - type(r));
    
    if ((flags & 1) == 0) 
    {
	if (idim == -1 || (flags & 2)) 
	{
	    if (idomain.contains(region) || bdy.defined() == 0) 
	    {
		return fill_patch_blindly(patch, region, r, flags);
	    }
	    else if (!tdomain.intersects(region)) 
	    {
		return fill_exterior_patch_blindly(patch, region, r,
		    interface, bdy, flags);
	    }
	    else if (idomain.intersects(region)) 
	    {
		if (fill_patch_blindly(patch, region, r, flags) == 1)
		    return 1;
		else
		    return fill_exterior_patch_blindly(patch, region, r,
		    interface, bdy, flags);
	    }
	    else 
	    {
		if (fill_exterior_patch_blindly(patch, region, r,
		    interface, bdy, flags) == 1)
		    return 1;
		else
		    return fill_patch_blindly(patch, region, r, flags);
	    }
	}
	else if (idim == 0) 
	{
	    int gridnum[level_interface::N_CORNER_GRIDS+1];
	    gridnum[0] = -1;
	    for (int i = 0; i < level_interface::N_CORNER_GRIDS; i++) 
	    {
		int igrid = interface.cgrid(index,i);
		if (igrid != -1) 
		{
		    for (int j = 0; gridnum[j] != igrid; j++) 
		    {
			if (gridnum[j] == -1) 
			{
			    gridnum[j] = igrid;
			    gridnum[j+1] = -1;
			    if (igrid >= 0) 
			    {
				Box tb = r.box(igrid);
				tb &= region;
				const Box& rbox = r[igrid].box();
				FFCPY(patch.dataPtr(), dimlist(patch.box()),
				    dimlist(tb),
				    r[igrid].dataPtr(), dimlist(rbox), patch.nComp());
			    }
			    else 
			    {
				igrid = -2 - igrid;
				Box tb = interface.exterior_mesh()[igrid];
				tb.convert(type(r));
				tb &= region;
				bdy.fill(patch, tb, r,
				    interface.direct_exterior_ref(igrid),
				    interface.domain());
			    }
			    break;
			}
		    }
		}
	    }
	}
#if (BL_SPACEDIM == 3)
	else if (idim == 1) 
	{
	    int gridnum[level_interface::N_EDGE_GRIDS+1];
	    gridnum[0] = -1;
	    for (int i = 0; i < level_interface::N_EDGE_GRIDS; i++) 
	    {
		int igrid = interface.egrid(index,i);
		if (igrid != -1) 
		{
		    for (int j = 0; gridnum[j] != igrid; j++) 
		    {
			if (gridnum[j] == -1) 
			{
			    gridnum[j] = igrid;
			    gridnum[j+1] = -1;
			    if (igrid >= 0) {
				Box tb = r.box(igrid);
				tb &= region;
				const Box& rbox = r[igrid].box();
				FFCPY(patch.dataPtr(), dimlist(patch.box()),
				    dimlist(tb),
				    r[igrid].dataPtr(), dimlist(rbox), patch.nComp());
			    }
			    else 
			    {
				igrid = -2 - igrid;
				Box tb = interface.exterior_mesh()[igrid];
				tb.convert(type(r));
				tb &= region;
				bdy.fill(patch, tb, r,
				    interface.direct_exterior_ref(igrid),
				    interface.domain());
			    }
			    break;
			}
		    }
		}
	    }
	}
#endif
	else if (idim == level_interface::FACEDIM) 
	{
	    int gridnum[level_interface::N_FACE_GRIDS+1];
	    gridnum[0] = -1;
	    for (int i = 0; i < level_interface::N_FACE_GRIDS; i++) 
	    {
		int igrid = interface.fgrid(index,i);
		if (igrid != -1) 
		{
		    for (int j = 0; gridnum[j] != igrid; j++) 
		    {
			if (gridnum[j] == -1) 
			{
			    gridnum[j] = igrid;
			    gridnum[j+1] = -1;
			    if (igrid >= 0) {
				Box tb = r.box(igrid);
				tb &= region;
				const Box& rbox = r[igrid].box();
				FFCPY(patch.dataPtr(), dimlist(patch.box()),
				    dimlist(tb),
				    r[igrid].dataPtr(), dimlist(rbox), patch.nComp());
			    }
			    else 
			    {
				igrid = -2 - igrid;
				Box tb = interface.exterior_mesh()[igrid];
				tb.convert(type(r));
				tb &= region;
				bdy.fill(patch, tb, r,
				    interface.direct_exterior_ref(igrid),
				    interface.domain());
			    }
			    break;
			}
		    }
		}
	    }
	}
  }
  else 
  {
      BoxLib::Error("fill_patch---interface version only defined for blind mode");
  }
  return 1;
}

#if 0
int fill_patch(Fab& patch,
	       const Box& region, const Box& active,
	       amr_boundary bdy, int flags,
	       int igrid)
{
  cout << "Warning:  using obsolete form of fill_patch" << '\n';

  if (!region.ok())
    return 1;

  // This version is particularly inefficient for handling node-based
  // boundary conditions, and sometimes gives incorrect results for
  // node-based periodic boundaries.

  if (flags & 8) {
    for (int iqq = 0; iqq < flags/16; iqq++)
      cout << "  ";
    cout << "Filling " << region << '\n';
    flags += 16;
  }

  if (flags & 4)
    patch.assign(0.0, region);

  chkcomp(patch.nComp());
  if (!region.sameType(patch.box()))
    BoxLib::Error("fill_patch---incompatible patch");

  Box idomain = grow(tdomain(), zerovect - type());

  if (igrid < 0 && (flags & 1) == 0 && idomain.contains(region)) {
    fill_patch_blindly(patch, region, flags);
    return 1;
  }

  if (tdomain().contains(region)) {
    // Interior patch;
    int retval;
    if (igrid >= 0) {
      if (flags & 2)
	retval = obj().gr[igrid].box().contains(region) ? 1 : 2;
      else if (region.cellCentered())
	retval = mesh()[igrid].contains(region) ? 1 : 2;
      else
	retval = mesh().box(igrid, type()).contains(region) ? 1 : 2;
    }
    else {
      retval = best_match(region, igrid, (flags & 2) ? border() : 0);
    }

    if (retval == 0) {
      // no intersections with grids on this level
      if (!idomain.intersects(region)) {
	// last-ditch chance, maybe boundary condition can do something
	cout << "Using fill_patch_special" << '\n';
	return bdy.fill_patch_special(patch, region, *this, flags);
      }
//      else if (!idomain.contains(region)) {
//	Box side1 = region;
//	Box side2 = box_chop(side1, idomain);
//	cout << side1 << ' ' << side2 << '\n';
//	fill_patch(patch, side1, active, bdy, flags);
//	fill_patch(patch, side2, active, bdy, flags);
//	return 0;
//      }
      else {
	return 0;
      }
    }
    else if (retval == 1) {
      patch.fab().copy(fgrid(igrid).fab(), region, comp, region,
		       patch.component(), ncomp);
      return 1;
    }
    else if (retval == 2) {
      Box side1 = region, side2;
      if (flags & 2)
	side2 = obj().gr[igrid].box();
      else
	side2 = mesh().box(igrid, type());
      side2 = box_chop(side1, side2);
      int ret0 = fill_patch(patch, side1, active, bdy, flags, igrid);
      int ret1 = fill_patch(patch, side2, active, bdy, flags);
      return (ret0 && ret1);
    }
  }
  else {
    Box side2 = region;
    int ret0 = 1, ret1 = 1;
    if (idomain.intersects(region)) {
      // patch crosses boundary
      Box side1 = region;
      side2 = box_chop(side1, tdomain());
      ret0 = fill_patch(patch, side1, active, bdy, (flags | 4), igrid);
    }
    // patch outside boundary
    int bdir = bdy.dir(side2, mesh().domain());
    Box bb = bdy.box(side2, mesh().domain(), bdir);
    // tdomain().contains(bb) can be false for certain inflow bc's.
    // If this is the case, bb must be fillable from ghost cells and
    // (flags & 2) must be set or an infinite recursion will occur.
    if (active.contains(bb) && tdomain().contains(bb)) {
      if (!patch.box().contains(active))
	BoxLib::Error("fill_patch---bogus active region");
      bdy.fill(patch, side2, patch, bb, mesh().domain(), bdir);
    }
    else {
      Fab gb;
      ret1 = get_patch(gb, bb, null_level_interface, bdy, (flags | 5));
      bdy.fill(patch, side2, gb, bb, mesh().domain(), bdir);
    }
    if (ret1 == 0 && tdomain().intersects(region)) {
      // bc failed and region just touches boundary---split
      Box side1 = region;
      side2 = box_chop(side1, tdomain());
      ret0 = fill_patch(patch, side1, active, bdy, flags, igrid);
      if (flags & 1) {
	// redo outside to get return value right
	bb = bdy.box(side2, mesh().domain(), bdir);
	Fab gb;
	ret1 = get_patch(gb, bb, null_level_interface, bdy, (flags | 5));
	bdy.fill(patch, side2, gb, bb, mesh().domain(), bdir);
      }
      else {
	ret1 = 1;
      }
    }
    return (ret0 && ret1);
  }
  BoxLib::Error("fill_patch---shouldn't get here");
  return 0;
}
#endif

void
sync_internal_borders(MultiFab& r, const level_interface& interface)
{
    // DECLARE_GEOMETRY_TYPES;
    
    int igrid, jgrid;
    if (type(r) == nodevect) 
    {
	for (int iface = 0; iface < interface.nfaces(); iface++) 
	{
	    igrid = interface.fgrid(iface, 0);
	    jgrid = interface.fgrid(iface, 1);
	    // only do interior faces with fine grid on both sides
	    if (igrid < 0 || jgrid < 0 || interface.fgeo(iface) != level_interface::ALL)
		break;
	    internal_copy(r, jgrid, igrid, interface.node_face(iface));
	}
#if (BL_SPACEDIM == 2)
	for (int icor = 0; icor < interface.ncorners(); icor++) 
	{
	    igrid = interface.cgrid(icor, 0);
	    jgrid = interface.cgrid(icor, 3);
	    // only do interior corners with fine grid on all sides
	    if (igrid < 0 || jgrid < 0 || interface.cgeo(icor) != level_interface::ALL)
		break;
	    if (jgrid == interface.cgrid(icor, 1))
		internal_copy(r, jgrid, igrid, interface.corner(icor));
	}
#else
	for (int iedge = 0; iedge < interface.nedges(); iedge++) 
	{
	    igrid = interface.egrid(iedge, 0);
	    jgrid = interface.egrid(iedge, 3);
	    // only do interior edges with fine grid on all sides
	    if (igrid < 0 || jgrid < 0 || interface.egeo(iedge) != level_interface::ALL)
		break;
	    if (jgrid == interface.egrid(iedge, 1))
		internal_copy(r, jgrid, igrid, interface.node_edge(iedge));
	}
	for (int icor = 0; icor < interface.ncorners(); icor++) 
	{
	    igrid = interface.cgrid(icor, 0);
	    jgrid = interface.cgrid(icor, 7);
	    // only do interior corners with fine grid on all sides
	    if (igrid < 0 || jgrid < 0 || interface.cgeo(icor) != level_interface::ALL)
		break;
	    if (interface.cgrid(icor, 3) == interface.cgrid(icor, 1)) 
	    {
		if (jgrid != interface.cgrid(icor, 3)) 
		{
		    internal_copy(r, jgrid, igrid, interface.corner(icor));
		    jgrid = interface.cgrid(icor, 5);
		    if (jgrid != interface.cgrid(icor, 7))
			internal_copy(r, jgrid, igrid, interface.corner(icor));
		}
	    }
	    else if (interface.cgrid(icor, 5) == interface.cgrid(icor, 1)) 
	    {
		if (jgrid != interface.cgrid(icor, 5)) 
		{
		    internal_copy(r, jgrid, igrid, interface.corner(icor));
		    jgrid = interface.cgrid(icor, 3);
		    if (jgrid != interface.cgrid(icor, 7)) 
		    {
			internal_copy(r, jgrid, igrid, interface.corner(icor));
			if (jgrid == interface.cgrid(icor, 2)) 
			{
			    jgrid = interface.cgrid(icor, 6);
			    if (jgrid != interface.cgrid(icor, 7))
				internal_copy(r, jgrid, igrid, interface.corner(icor));
			}
		    }
		}
	    }
	}
#endif
    }
    else 
    {
	BoxLib::Error("sync_internal_borders---only NODE-based sync defined");
    }
}

// The sequencing used in fill_internal_borders, fcpy2 and set_border_cache
// (narrow x, medium y, wide z) is necessary to avoid overwrite problems
// like those seen in the sync routines.  Boundary copies are all wide
// regardless of direction and come after interior copies---overwrite
// difficulties are avoided since grids can't bridge a boundary.

// Modifications are necessary in 3D to deal with lack of diagonal
// communication across edges at the coarse-fine interface.  These
// modifications take the form of narrowing certain copies to avoid
// overwriting good values with bad ones.

void 
fill_internal_borders(MultiFab& r, const level_interface& interface,
		      int w)
{
    // DECLARE_GEOMETRY_TYPES;
    
    w = (w < 0 || w > r.nGrow()) ? r.nGrow() : w;
    int igrid, jgrid;
    if (type(r) == nodevect) 
    {
	for (int iface = 0; iface < interface.nfaces(); iface++) 
	{
	    igrid = interface.fgrid(iface, 0);
	    jgrid = interface.fgrid(iface, 1);
	    if (igrid < 0 || jgrid < 0 || interface.fgeo(iface) != level_interface::ALL)
		break;
#if 1
	    const Box& b = interface.node_face(iface);
	    Real *const ptra = r[igrid].dataPtr();
	    Real *const ptrb = r[jgrid].dataPtr();
	    const Box& boxa = r[igrid].box();
	    const Box& boxb = r[jgrid].box();
#  if (BL_SPACEDIM == 2)
	    FFCPY2(ptra, dimlist(boxa), ptrb, dimlist(boxb),
		dimlist(b), w, r.nComp());
#  else
	    const int ibord = r.nGrow();
	    FFCPY2(ptra, dimlist(boxa), ptrb, dimlist(boxb),
		dimlist(b), w, ibord, r.nComp());
#  endif
#else
	    const int idim = interface.fdim(iface);
	    Box bj = interface.node_face(iface);
	    Box bi = interface.node_face(iface);
	    for (int i = 0; i < idim; i++) 
	    {
		if (r.box(jgrid).smallEnd(i) == bj.smallEnd(i))
		    bj.growLo(i, w);
		if (r.box(jgrid).bigEnd(i) == bj.bigEnd(i))
		    bj.growHi(i, w);
		if (r.box(igrid).smallEnd(i) == bi.smallEnd(i))
		    bi.growLo(i, w);
		if (r.box(igrid).bigEnd(i) == bi.bigEnd(i))
		    bi.growHi(i, w);
	    }
	    bj.shift(idim, -1).growLo(idim, w-1);
	    bi.shift(idim,  1).growHi(idim, w-1);
	    internal_copy(r, jgrid, igrid, bj);
	    internal_copy(r, igrid, jgrid, bi);
#endif
	}
    }
    else if (type(r) == cellvect) 
    {
	for (int iface = 0; iface < interface.nfaces(); iface++) 
	{
	    igrid = interface.fgrid(iface, 0);
	    jgrid = interface.fgrid(iface, 1);
	    if (igrid < 0 || jgrid < 0 || interface.fgeo(iface) != level_interface::ALL)
		break;
	    const int idim = interface.fdim(iface);
#if (BL_SPACEDIM == 2)
	    Box b = interface.face(iface);
	    if (idim == 1)
		b.grow(0, w);
	    b.growLo(idim, w).convert(cellvect);
	    internal_copy(r, jgrid, igrid, b);
	    internal_copy(r, igrid, jgrid, b.shift(idim, w));
#else
	    Box bj = interface.face(iface);
	    Box bi = interface.face(iface);
	    for (int i = 0; i < idim; i++) 
	    {
		if (r.box(jgrid).smallEnd(i) == bj.smallEnd(i))
		    bj.growLo(i, w);
		if (r.box(jgrid).bigEnd(i) == bj.bigEnd(i))
		    bj.growHi(i, w);
		if (r.box(igrid).smallEnd(i) == bi.smallEnd(i))
		    bi.growLo(i, w);
		if (r.box(igrid).bigEnd(i) == bi.bigEnd(i))
		    bi.growHi(i, w);
	    }
	    bj.growLo(idim, w).convert(cellvect);
	    bi.growHi(idim, w).convert(cellvect);
	    internal_copy(r, jgrid, igrid, bj);
	    internal_copy(r, igrid, jgrid, bi);
#endif
	}
    }
    else
    {
	for (int iface = 0; iface < interface.nfaces(); iface++) 
	{
	    igrid = interface.fgrid(iface, 0);
	    jgrid = interface.fgrid(iface, 1);
	    if (igrid < 0 || jgrid < 0 || interface.fgeo(iface) != level_interface::ALL)
		break;
	    const int idim = interface.fdim(iface);
	    const int a = (type(r, idim) == BOX_NODE);
#if (BL_SPACEDIM == 2)
	    Box b = interface.face(iface);
	    if (idim == 1)
		b.grow(0, w);
	    b.shift(idim, -a).growLo(idim, w-a).convert(type(r));
	    internal_copy(r, jgrid, igrid, b);
	    internal_copy(r, igrid, jgrid, b.shift(idim, w+a));
#else
	    Box bj = interface.face(iface);
	    Box bi = interface.face(iface);
#if 0
	    for (int i = 0; i < idim; i++) 
	    {
		if (mesh().box(jgrid).smallEnd(i) == bj.smallEnd(i))
		    bj.growLo(i, w);
		if (mesh().box(jgrid).bigEnd(i) == bj.bigEnd(i))
		    bj.growHi(i, w);
		if (mesh().box(igrid).smallEnd(i) == bi.smallEnd(i))
		    bi.growLo(i, w);
		if (mesh().box(igrid).bigEnd(i) == bi.bigEnd(i))
		    bi.growHi(i, w);
	    }
#else
	    BoxLib::Error("fill_internal_borders---check index arithmetic for mixed types in 3D");
#endif
	    bj.shift(idim, -a).growLo(idim, w-a).convert(type(r));
	    bi.shift(idim,  a).growHi(idim, w-a).convert(type(r));
	    internal_copy(r, jgrid, igrid, bj);
	    internal_copy(r, igrid, jgrid, bi);
#endif
	}
    }
}

void 
clear_part_interface(MultiFab& r, const level_interface& interface)
{
    if (r.nComp() != 1)
	BoxLib::Error("clear_part_interface---only single components currently supported");
    
    // DECLARE_GEOMETRY_TYPES;
    
    int igrid;
    if (type(r) == nodevect) 
    {
	for (int i = 0; i < BL_SPACEDIM; i++) 
	{
	    for (int ibox = 0; ibox < interface.nboxes(i); ibox++) 
	    {
		// coarse-fine face contained in part_fine grid, or orphan edge/corner
		if ((igrid = interface.aux(i, ibox)) >= 0)
		    r[igrid].setVal(0.0, interface.node_box(i, ibox), 0);
	    }
	}
    }
    else
    {
	BoxLib::Error("clear_part_interface---only NODE-based version defined");
    }
}

void 
interpolate_patch(Fab& patch, const Box& region,
		  MultiFab& r, const IntVect& rat,
		  amr_interpolator interp,
		  const level_interface& interface,
		  amr_boundary bdy)
{
    assert(region.sameType(patch.box()));
    
    Box cb = interp.box(region, rat);
    int igrid = find_patch(cb, r);
    if (igrid == -1) 
    {
	Fab cgr(cb, r.nComp());
	fill_patch(cgr, cb, r, interface, bdy);
	interp.fill(patch, region, cgr, cb, rat);
    }
    else
    {
	interp.fill(patch, region, r[igrid], cb, rat);
    }
}

void
restrict_patch(Fab& patch, const Box& region,
	       MultiFab& r, const IntVect& rat,
#ifdef HG_USE_CACHE
	       const copy_cache* border_cache,
#endif
	       const amr_restrictor_class& restric,
	       const level_interface& interface,
	       amr_boundary bdy)
{
    assert(region.sameType(patch.box()));
    assert(region.type() == type(r));
    
    for (int igrid = 0; igrid < r.length(); igrid++) 
    {
	Box cbox = r.box(igrid);
	cbox = restric.box(cbox, rat);
	if (region.intersects(cbox)) 
	{
	    cbox &= region;
	    restric.fill(patch, cbox, r[igrid], rat);
	}
    }
    
    // Interface restriction is sufficiently rare and specialized that
    // we will let the restrictor handle it---at least for now.
    
    if (!interface.null()) 
    {
	// This assertion difficult in BoxLib since r.mesh() is not cc:
	//assert(r.mesh() == interface.interior_mesh());
	restric.interface(patch, region, r, 
#ifdef HG_USE_CACHE
	    border_cache,
#endif
	    interface, bdy, rat);
    }
}

void
restrict_level(MultiFab& dest, int bflag,
	       MultiFab& r, const IntVect& rat,
#ifdef HG_USE_CACHE
	       const copy_cache* border_cache,
#endif
	       const amr_restrictor_class& restric,
	       const level_interface& interface,
	       amr_boundary bdy)
{
    for (int igrid = 0; igrid < dest.length(); igrid++) 
    {
	if (bflag) 
	{
	    restrict_patch(dest[igrid], r, rat,
#ifdef HG_USE_CACHE
		border_cache,
#endif
		restric, interface, bdy);
	}
	else
	{
	    restrict_patch(dest[igrid], dest.box(igrid), r, rat, 
#ifdef HG_USE_CACHE
		border_cache,
#endif
		restric, interface, bdy);
	}
    }
}
