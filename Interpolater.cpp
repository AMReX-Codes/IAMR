
//
// $Id: Interpolater.cpp,v 1.7 1997-10-08 20:15:33 car Exp $
//

#ifdef BL_USE_NEW_HFILES
#include <cmath>
#else
#include <math.h>
#endif

#include <Interpolater.H>
#include <INTERP_F.H>

// ------------------------------------------------------------
// ------  CONSTRUCT A GLOBAL OBJECT OF EACH VERSION  ---------
// ------------------------------------------------------------
NodeBilinear     node_bilinear_interp;
CellBilinear     cell_bilinear_interp;
CellConservative cell_cons_interp;
CellQuadratic    quadratic_interp;
CellConservative unlimited_cc_interp(0);
PCInterp         pc_interp;
CellConservativeLinear lincc_interp;
CellConservativeLinear nonlincc_interp(0);

// ------------------------------------------------------------
// --------  Virtual base class Interpolater  -----------------
// ------------------------------------------------------------
Interpolater::~Interpolater() {}

// ------------------------------------------------------------
// --------  Bilinear Node Centered Interpolation  ------------
// ------------------------------------------------------------

NodeBilinear::NodeBilinear()
{
    strip = slope = 0;
    strip_len = slope_len = 0;
}

NodeBilinear::~NodeBilinear()
{
    delete strip;
    delete slope;
}

// ------------------------------------------------------------
void
NodeBilinear::interp(FArrayBox& crse, int crse_comp,
		     FArrayBox& fine, int fine_comp,
		     int ncomp,
		     const Box& fine_region, const IntVect & ratio,
                     const Geometry& /* crse_geom */,
                     const Geometry& /* fine_geom */,
		     Array<BCRec>& /*bcr*/)
{
      // set up to call FORTRAN
    Box cbox(crse.box());
    const int* clo = cbox.loVect();
    const int* chi = cbox.hiVect();
    const int* flo = fine.loVect();
    const int* fhi = fine.hiVect();
    const int* lo = fine_region.loVect();
    const int* hi = fine_region.hiVect();
    int num_slope = (int) pow(2.0,BL_SPACEDIM)-1;
    int len0 = cbox.length()[0];
    int slp_len = num_slope*len0;
    if (slope_len < slp_len) {
	delete slope;
	slope_len = slp_len;
	slope = new Real[slope_len];
    }
    int strp_len = len0*ratio[0];
    for( int n=1; n<BL_SPACEDIM; n++ ) {
      strp_len *= ratio[n]+1;
    }
    if (strip_len < strp_len) {
	delete strip;
	strip_len = strp_len;
	strip = new Real[strip_len];
    }
    int strip_lo = ratio[0] * clo[0];
    int strip_hi = ratio[0] * chi[0];

    const Real* cdat = crse.dataPtr(crse_comp);
    Real*       fdat = fine.dataPtr(fine_comp);
    FORT_NBINTERP (cdat,ARLIM(clo),ARLIM(chi),ARLIM(clo),ARLIM(chi),
                   fdat,ARLIM(flo),ARLIM(fhi),ARLIM(lo),ARLIM(hi),
		   ratio.getVect(),&ncomp,
		   slope,&num_slope,strip,&strip_lo,&strip_hi);
}

// ------------------------------------------------------------
// --------  Bilinear Cell Centered Interpolation  ------------
// ------------------------------------------------------------

CellBilinear::CellBilinear()
{
    strip = slope = 0;
    strip_len = slope_len = 0;
}

CellBilinear::~CellBilinear()
{
    delete strip;
    delete slope;
}

// ------------------------------------------------------------
Box
CellBilinear::CoarseBox(const Box& fine, int ratio)
{
    return CoarseBox(fine, ratio*IntVect::TheUnitVector());
}

// ------------------------------------------------------------
Box
CellBilinear::CoarseBox(const Box& fine, const IntVect & ratio)
{
    Box crse(::coarsen(fine,ratio));
    const int* lo = fine.loVect();
    const int* hi = fine.hiVect();
    int i;
    for (i = 0; i < BL_SPACEDIM; i++) {
      int iratio = ratio[i];
      int hrat = iratio/2;
      if (lo[i]%iratio < hrat) crse.growLo(i,1);
      if (hi[i]%iratio >= hrat) crse.growHi(i,1);
    }
    return crse;
}

// ------------------------------------------------------------
void
CellBilinear::interp(FArrayBox& crse, int crse_comp,
		     FArrayBox& fine, int fine_comp,
		     int ncomp,
		     const Box& fine_region, const IntVect & ratio,
                     const Geometry& /* crse_geom */,
                     const Geometry& /* fine_geom */,
		     Array<BCRec>& /*bcr*/)
{
    BoxLib::Error("interp: not implemented");

      // set up to call FORTRAN
    Box cbox(crse.box());
    const int* clo = cbox.loVect();
    const int* chi = cbox.hiVect();
    const int* flo = fine.loVect();
    const int* fhi = fine.hiVect();
    const int* lo = fine_region.loVect();
    const int* hi = fine_region.hiVect();
    int num_slope = (int) pow(2.0,BL_SPACEDIM)-1;
    int len0 = cbox.length()[0];
    int slp_len = num_slope*len0;
    if (slope_len < slp_len) {
	delete slope;
	slope_len = slp_len;
	slope = new Real[slope_len];
    }
    int strp_len = len0*ratio[0];
    if (strip_len < strp_len) {
	delete strip;
	strip_len = strp_len;
	strip = new Real[strip_len];
    }
    int strip_lo = ratio[0] * clo[0];
    int strip_hi = ratio[0] * chi[0];

    const Real* cdat = crse.dataPtr(crse_comp);
    Real*       fdat = fine.dataPtr(fine_comp);
    FORT_CBINTERP (cdat,ARLIM(clo),ARLIM(chi),ARLIM(clo),ARLIM(chi),
		   fdat,ARLIM(flo),ARLIM(fhi),ARLIM(lo),ARLIM(hi),
		   ratio.getVect(),&ncomp,
		   slope,&num_slope,strip,&strip_lo,&strip_hi);
}

// ------------------------------------------------------------
// --------  Conservative Cell Centered Interpolation  --------
// ------------------------------------------------------------

CellConservative::CellConservative(int limit)
{
    do_limited_slope = limit;
    strip = cslope = 0;
    strip_len = slope_len = 0;
}

CellConservative::~CellConservative()
{
    delete strip;
    delete cslope;
}

// ------------------------------------------------------------
Box
CellConservative::CoarseBox(const Box& fine, const IntVect& ratio)
{
    Box crse(::coarsen(fine,ratio));
    crse.grow(1);
    return crse;
}

// ------------------------------------------------------------
Box
CellConservative::CoarseBox(const Box& fine, int ratio)
{
    Box crse(::coarsen(fine,ratio));
    crse.grow(1);
    return crse;
}

// ------------------------------------------------------------
void
CellConservative::interp(FArrayBox& crse, int crse_comp,
			 FArrayBox& fine, int fine_comp,
			 int ncomp,
			 const Box& fine_region, const IntVect & ratio,
			 const Geometry& crse_geom,
			 const Geometry& fine_geom,
			 Array<BCRec>& bcr)
{

    assert( bcr.length() >= ncomp );
    const Box& domain = fine_geom.Domain();
    if (!domain.contains(fine_region)) {
	BoxLib::Error("interp:fine_region not in domain");
    }

    // make box which is intersection of fine_region and domain of fine
    Box target_fine_region = fine_region & fine.box();

    Box crse_bx(::coarsen(target_fine_region,ratio));
    Box fslope_bx(::refine(crse_bx,ratio));
    Box cslope_bx(crse_bx);
    cslope_bx.grow(1);
    if (! crse.box().contains(cslope_bx) ) {
	BoxLib::Error("CellConservative: crse databox size mismatch");
    }

      // alloc temp space for coarse grid slopes
    long t_long = cslope_bx.numPts();
    assert(t_long < INT_MAX);
    int c_len = int(t_long);
    if (slope_len < BL_SPACEDIM*c_len) {
	slope_len = BL_SPACEDIM*c_len;
	delete cslope;
	cslope = new Real[slope_len];
    }
    int loslp    = cslope_bx.index(crse_bx.smallEnd());
    int hislp    = cslope_bx.index(crse_bx.bigEnd());

    t_long = cslope_bx.numPts();
    assert(t_long < INT_MAX);
    int cslope_vol = int(t_long);
    int clo = 1 - loslp;
    int chi = clo + cslope_vol - 1;
    c_len = hislp - loslp + 1;

      // alloc temp space for one strip of fine grid slopes
    int dir;
    int f_len = fslope_bx.longside(dir);
    if (strip_len < (BL_SPACEDIM+2)*f_len) {
	strip_len = (BL_SPACEDIM+2)*f_len;
	delete strip;
	strip = new Real[strip_len];
    }
    Real *fstrip = strip;
    Real *foff = fstrip + f_len;
    Real *fslope = foff + f_len;

      // get coarse in fine edge centered volume coordinates
    Array<Real> fvc[BL_SPACEDIM];
    Array<Real> cvc[BL_SPACEDIM];
    for (dir = 0; dir < BL_SPACEDIM; dir++) {
        fine_geom.GetEdgeVolCoord(fvc[dir],target_fine_region,dir);
        crse_geom.GetEdgeVolCoord(cvc[dir],crse_bx,dir);
    }

      // alloc tmp space for slope calc and to allow for vectorization
    Real* fdat      = fine.dataPtr(fine_comp);
    const Real* cdat= crse.dataPtr(crse_comp);
    const int* flo  = fine.loVect();
    const int* fhi  = fine.hiVect();
    const int* fblo = target_fine_region.loVect();
    const int* fbhi = target_fine_region.hiVect();
    const int* cblo = crse_bx.loVect();
    const int* cbhi = crse_bx.hiVect();
    const int* cflo = crse.loVect();
    const int* cfhi = crse.hiVect();
    const int* fslo = fslope_bx.loVect();
    const int* fshi = fslope_bx.hiVect();
    int slope_flag = (do_limited_slope ? 1 : 0);

    FORT_CCINTERP (fdat,ARLIM(flo),ARLIM(fhi),
                   fblo[0], fblo[1],
#if (BL_SPACEDIM == 3)
                   fblo[2],
#endif 
                   fbhi[0], fbhi[1],
#if (BL_SPACEDIM == 3)
                   fbhi[2],
#endif 
                   &ncomp,ratio.getVect(),
		   cdat,&clo,&chi,
                   cblo[0], cblo[1],
#if (BL_SPACEDIM == 3)
                   cblo[2],
#endif 
                   cbhi[0], cbhi[1],
#if (BL_SPACEDIM == 3)
                   cbhi[2],
#endif 
                   fslo,fshi,
		   cslope,&c_len,fslope,fstrip,&f_len,foff,
		   (int*)bcr.dataPtr(), &slope_flag,
                   D_DECL(fvc[0].dataPtr(),fvc[1].dataPtr(),fvc[2].dataPtr()),
                   D_DECL(cvc[0].dataPtr(),cvc[1].dataPtr(),cvc[2].dataPtr())
                   );
}

// -------------------------------------------------------------------
// --------  Linear Conservative Cell Centered Interpolation  --------
// -------------------------------------------------------------------
//  The interpolation is linear in that it uses a limiting scheme that 
//  preserves the value of any linear combination of the
//  coarse grid data components--e.g., if 
//  sum_ivar a(ic,jc,ivar)*fab(ic,jc,ivar) = 0, then
//  sum_ivar a(ic,jc,ivar)*fab(if,jf,ivar) = 0 is satisfied
//  in all fine cells if,jf covering coarse cell ic,jc.
//
//  If do_linear_limiting = 0, the interpolation scheme is identical to
//  the used in ConservativeLinear for do_limited_slope=1; 
//  the results should be exactly the same -- difference = hard 0.
// -------------------------------------------------------------------

CellConservativeLinear::CellConservativeLinear(int do_linear_limiting_)
{
    do_linear_limiting = do_linear_limiting_;
}

CellConservativeLinear::~CellConservativeLinear()
{
}

// ------------------------------------------------------------
Box
CellConservativeLinear::CoarseBox(const Box& fine, const IntVect& ratio)
{
    Box crse(::coarsen(fine,ratio));
    crse.grow(1);
    return crse;
}

// ------------------------------------------------------------
Box
CellConservativeLinear::CoarseBox(const Box& fine, int ratio)
{
    Box crse(::coarsen(fine,ratio));
    crse.grow(1);
    return crse;
}

// ------------------------------------------------------------
void
CellConservativeLinear::interp(FArrayBox& crse, int crse_comp,
			 FArrayBox& fine, int fine_comp,
			 int ncomp,
			 const Box& fine_region, const IntVect & ratio,
			 const Geometry& crse_geom,
			 const Geometry& fine_geom,
			 Array<BCRec>& bcr)
{
    assert( bcr.length() >= ncomp );
    const Box& domain = fine_geom.Domain();
    if (!domain.contains(fine_region)) {
	BoxLib::Error("interp:fine_region not in domain");
    }

    // make box which is intersection of fine_region and domain of fine
    Box target_fine_region = fine_region & fine.box();

    // crse_bx is coarsening of target_fine_region, grown by 1
    Box crse_bx = CoarseBox(target_fine_region,ratio);

    // slopes are needed only on coarsening of target_fine_region
    Box cslope_bx(crse_bx);
    cslope_bx.grow(-1);

      // get coarse in fine edge centered volume coordinates
    Array<Real> fvc[BL_SPACEDIM];
    Array<Real> cvc[BL_SPACEDIM];
    int dir;
    for (dir = 0; dir < BL_SPACEDIM; dir++) {
        fine_geom.GetEdgeVolCoord(fvc[dir],target_fine_region,dir);
        crse_geom.GetEdgeVolCoord(cvc[dir],crse_bx,dir);
    }

    // alloc tmp space for slope calc 

    // In ucc_slopes and lcc_slopes , there is a slight abuse of 
    // the number of compenents argument
    // --> there is a slope for each component in each coordinate 
    //     direction
    FArrayBox ucc_slopes(cslope_bx,ncomp*BL_SPACEDIM);
    FArrayBox lcc_slopes(cslope_bx,ncomp*BL_SPACEDIM);
    FArrayBox slope_factors(cslope_bx,BL_SPACEDIM);

    Real* fdat       = fine.dataPtr(fine_comp);
    const Real* cdat = crse.dataPtr(crse_comp);
    Real* ucc_xsldat = ucc_slopes.dataPtr(0);
    Real* lcc_xsldat = lcc_slopes.dataPtr(0);
    Real* xslfac_dat = slope_factors.dataPtr(0);
    Real* ucc_ysldat = ucc_slopes.dataPtr(ncomp);
    Real* lcc_ysldat = lcc_slopes.dataPtr(ncomp);
    Real* yslfac_dat = slope_factors.dataPtr(1);
#if (BL_SPACEDIM==3)
    Real* ucc_zsldat = ucc_slopes.dataPtr(2*ncomp);
    Real* lcc_zsldat = lcc_slopes.dataPtr(2*ncomp);
    Real* zslfac_dat = slope_factors.dataPtr(2);
#endif
    
    const int* flo  = fine.loVect();
    const int* fhi  = fine.hiVect();
    const int* clo  = crse.loVect();
    const int* chi  = crse.hiVect();
    const int* fblo = target_fine_region.loVect();
    const int* fbhi = target_fine_region.hiVect();
    const int* csbhi = cslope_bx.hiVect();
    const int* csblo = cslope_bx.loVect();

    int lin_limit = (do_linear_limiting ? 1 : 0);

    const int* cvcblo = crse_bx.loVect();
    const int* fvcblo = target_fine_region.loVect();
    int cvcbhi[BL_SPACEDIM];
    int fvcbhi[BL_SPACEDIM];
    for (dir=0; dir<BL_SPACEDIM; dir++) {
      cvcbhi[dir] = cvcblo[dir] + cvc[dir].length() - 1;
      fvcbhi[dir] = fvcblo[dir] + fvc[dir].length() - 1;
    }

    Real *voffx = new Real[fvc[0].length()];
    Real *voffy = new Real[fvc[1].length()];
#if (BL_SPACEDIM==3)
    Real *voffz = new Real[fvc[2].length()];
#endif

    FORT_LINCCINTERP (fdat,ARLIM(flo),ARLIM(fhi),
                      fblo, fbhi,
                      ARLIM(fvcblo), ARLIM(fvcbhi),
		      cdat,ARLIM(clo),ARLIM(chi),
                      ARLIM(cvcblo), ARLIM(cvcbhi),
                      ucc_xsldat, lcc_xsldat, xslfac_dat,
                      ucc_ysldat, lcc_ysldat, yslfac_dat,
#if (BL_SPACEDIM==3)
                      ucc_zsldat, lcc_zsldat, zslfac_dat,
#endif
                      ARLIM(csblo), ARLIM(csbhi),
                      csblo, csbhi,
                      &ncomp,ratio.getVect(),
		      (int*)bcr.dataPtr(), &lin_limit,
                      D_DECL(fvc[0].dataPtr(),fvc[1].dataPtr(),fvc[2].dataPtr()),
                      D_DECL(cvc[0].dataPtr(),cvc[1].dataPtr(),cvc[2].dataPtr()),
                      D_DECL(voffx,voffy,voffz)
                      );
    delete voffx;
    delete voffy;
#if (BL_SPACEDIM==3)
    delete voffz;
#endif

}

// ------------------------------------------------------------
// --------  Quadratic Cell-Centered Interpolation  --------
// ------------------------------------------------------------

CellQuadratic::CellQuadratic()
{
    strip = cslope = 0;
    strip_len = slope_len = 0;
}

CellQuadratic::~CellQuadratic()
{
    delete strip;
    delete cslope;
}

// ------------------------------------------------------------
Box
CellQuadratic::CoarseBox(const Box& fine, const IntVect& ratio)
{
    Box crse(::coarsen(fine,ratio));
    crse.grow(1);
    return crse;
}

// ------------------------------------------------------------
Box
CellQuadratic::CoarseBox(const Box& fine, int ratio)
{
    Box crse(::coarsen(fine,ratio));
    crse.grow(1);
    return crse;
}

// ------------------------------------------------------------
void
CellQuadratic::interp(FArrayBox& crse, int crse_comp,
		      FArrayBox& fine, int fine_comp,
		      int ncomp,
		      const Box& fine_region, const IntVect & ratio,
		      const Geometry& crse_geom,
		      const Geometry& fine_geom,
		      Array<BCRec>& bcr)
{

    assert( bcr.length() >= ncomp );
    const Box& domain = fine_geom.Domain();
    if (!domain.contains(fine_region)) {
	BoxLib::Error("interp:fine_region not in domain");
    }

    // make box which is intersection of fine_region and domain of fine
    Box target_fine_region = fine_region & fine.box();

    Box crse_bx(::coarsen(target_fine_region,ratio));
    Box fslope_bx(::refine(crse_bx,ratio));
    Box cslope_bx(crse_bx);
    cslope_bx.grow(1);
    if (! crse.box().contains(cslope_bx) ) {
	BoxLib::Error("CellQuadratic: crse databox size mismatch");
    }

      // alloc temp space for coarse grid slopes: here we use 5 
      //  instead of BL_SPACEDIM because of the x^2, y^2 and xy terms
    long t_long = cslope_bx.numPts();
    assert(t_long < INT_MAX);
    int c_len = int(t_long);
    if (slope_len < 5*c_len) {
	slope_len = 5*c_len;
	delete cslope;
	cslope = new Real[slope_len];
    }
    int loslp    = cslope_bx.index(crse_bx.smallEnd());
    int hislp    = cslope_bx.index(crse_bx.bigEnd());

    t_long = cslope_bx.numPts();
    assert(t_long < INT_MAX);
    int cslope_vol = int(t_long);
    int clo = 1 - loslp;
    int chi = clo + cslope_vol - 1;
    c_len = hislp - loslp + 1;

      // alloc temp space for one strip of fine grid slopes: here we use 5 
      //  instead of BL_SPACEDIM because of the x^2, y^2 and xy terms
    int dir;
    int f_len = fslope_bx.longside(dir);
    if (strip_len < (5+2)*f_len) {
	strip_len = (5+2)*f_len;
	delete strip;
	strip = new Real[strip_len];
    }
    Real *fstrip = strip;
    Real *foff = fstrip + f_len;
    Real *fslope = foff + f_len;

      // get coarse in fine edge centered volume coordinates
    Array<Real> fvc[BL_SPACEDIM];
    Array<Real> cvc[BL_SPACEDIM];
    for (dir = 0; dir < BL_SPACEDIM; dir++) {
        fine_geom.GetEdgeVolCoord(fvc[dir],target_fine_region,dir);
        crse_geom.GetEdgeVolCoord(cvc[dir],crse_bx,dir);
    }

      // alloc tmp space for slope calc and to allow for vectorization
    Real* fdat      = fine.dataPtr(fine_comp);
    const Real* cdat= crse.dataPtr(crse_comp);
    const int* flo  = fine.loVect();
    const int* fhi  = fine.hiVect();
    const int* fblo = target_fine_region.loVect();
    const int* fbhi = target_fine_region.hiVect();
    const int* cblo = crse_bx.loVect();
    const int* cbhi = crse_bx.hiVect();
    const int* cflo = crse.loVect();
    const int* cfhi = crse.hiVect();
    const int* fslo = fslope_bx.loVect();
    const int* fshi = fslope_bx.hiVect();
    int slope_flag = (do_limited_slope ? 1 : 0);

    FORT_CQINTERP (fdat,ARLIM(flo),ARLIM(fhi),
                   fblo[0], fblo[1],
#if (BL_SPACEDIM == 3)
                   fblo[2],
#endif 
                   fbhi[0], fbhi[1],
#if (BL_SPACEDIM == 3)
                   fbhi[2],
#endif 
                   &ncomp,ratio.getVect(),
		   cdat,&clo,&chi,
                   cblo[0], cblo[1],
#if (BL_SPACEDIM == 3)
                   cblo[2],
#endif 
                   cbhi[0], cbhi[1],
#if (BL_SPACEDIM == 3)
                   cbhi[2],
#endif 
                   fslo,fshi,
		   cslope,&c_len,fslope,fstrip,&f_len,foff,
		   (int*)bcr.dataPtr(), &slope_flag,
                   D_DECL(fvc[0].dataPtr(),fvc[1].dataPtr(),fvc[2].dataPtr()),
                   D_DECL(cvc[0].dataPtr(),cvc[1].dataPtr(),cvc[2].dataPtr())
                   );
}

// ------------------------------------------------------------
// --------  Bilinear Node Centered Interpolation  ------------
// ------------------------------------------------------------

PCInterp::PCInterp()
{
    strip = 0;
    strip_len = 0;
}

PCInterp::~PCInterp()
{
    delete strip;
}

// ------------------------------------------------------------
void
PCInterp::interp(FArrayBox& crse, int crse_comp,
		 FArrayBox& fine, int fine_comp,
		 int ncomp,
		 const Box& fine_region, const IntVect & ratio,
                 const Geometry& /* crse_geom */,
                 const Geometry& /* fine_geom */,
		 Array<BCRec>& /* bcr */)
{
      // set up to call FORTRAN
    Box cbox(crse.box());
    const int* clo = cbox.loVect();
    const int* chi = cbox.hiVect();
    const int* flo = fine.loVect();
    const int* fhi = fine.hiVect();
    const int* fblo = fine_region.loVect();
    const int* fbhi = fine_region.hiVect();
    Box cregion(::coarsen(fine_region,ratio));
    const int* cblo = cregion.loVect();
    const int* cbhi = cregion.hiVect();
    int long_dir;
    int long_len = cregion.longside(long_dir);
    int s_len = long_len*ratio[long_dir];
    if (strip_len < s_len) {
	delete strip;
	strip_len = s_len;
	strip = new Real[strip_len];
    }
    int strip_lo = ratio[long_dir] * cblo[long_dir];
    int strip_hi = ratio[long_dir] * (cbhi[long_dir]+1) - 1;

      // convert long_dir to FORTRAN (1 based) index
    long_dir++;
    const Real* cdat = crse.dataPtr(crse_comp);
    Real*       fdat = fine.dataPtr(fine_comp);
    FORT_PCINTERP (cdat,ARLIM(clo),ARLIM(chi),cblo,cbhi,
		   fdat,ARLIM(flo),ARLIM(fhi),fblo,fbhi,
		   &long_dir,ratio.getVect(),&ncomp,strip,&strip_lo,&strip_hi);
}

