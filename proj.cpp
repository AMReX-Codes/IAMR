
#ifndef UNICOS
#  define USE_GRAPHICS
#endif

#ifdef USE_GRAPHICS
#  include "amr_graph.H"
#endif

#include "hg_projector.H"

void malloc_info();

#if (BL_SPACEDIM == 2)
/*
Real fct(const IntVect& it, const IntVect& t, int sig, int)
{
  Real x, y;
  real_coords(it, t, sig, x, y);
  //return 1.0 + x;
  return 1.0 + y*y*(1-y)*(1-y);
  //return 1.0 + x*x*(2-x)*(2-x);
}
*/
#endif

void projtest(Array<BoxArray>& m, Array<IntVect>& ratio, Array<Box>& domain);

main(int argc, char **argv)
{
#ifdef USE_GRAPHICS
  gopen(5);
  black();
#endif

  Array<BoxArray> m;
  Array<IntVect> ratio;
  Array<Box> domain;

  fstream grid;
  if (argc > 1)
    grid.open(argv[1], ios::in);
  else {
    cout << "usage:  proj <gridfile>" << endl;
    exit(1);
  }
  amr_multigrid::mesh_read(m, ratio, domain, grid);
  grid.close();

  //amr_multigrid::mesh_write(m, domain, cout);

  //malloc_info();
  projtest(m, ratio, domain);
/*
  malloc_info();
  for (int i = 0; i < 20; i++) {
    projtest(m, ratio, domain);
    malloc_info();
  }
*/
#ifdef USE_GRAPHICS
  gclose();
#endif
}

void init(PArray<MultiFab> u[], PArray<MultiFab>& p, const Array<BoxArray>& m)
{
  int ilev;
#if (BL_SPACEDIM == 2)
  for (ilev = 0; ilev < m.length(); ilev++) {
    u[0][ilev].setVal(0.0);
    u[1][ilev].setVal(0.0);
  }
  //u[0].initialize(fct);
  if (m.length() == 1) {
    for (int igrid = 0; igrid < m[0].length(); igrid++) {
      u[0][0][igrid](m[0][igrid].smallEnd() + Iv(2,2)) = 3.0;
      //u[0][0][igrid](m[0][igrid].smallEnd() + Iv(3,3)) = 1.0;
      //u[1][0][igrid](m[0][igrid].smallEnd() + Iv(3,3)) = 1.0;
      //u[0][0][igrid](m[0][igrid].smallEnd() + Iv(20,2)) = 3.0;
      //u[1][0][igrid](m[0][igrid].smallEnd() + Iv(2,2)) = 3.0;
    }
  }
  else if (m.length() == 2) {
    for (int igrid = 0; igrid < m[1].length(); igrid++) {
      u[0][1][igrid](m[1][igrid].smallEnd() + Iv(2,2)) = 3.0;
    }
    //u[0][1][0](IntVect(20,90)) = 1.0;
    //u[0][1][0](IntVect(50,50)) = 1.0;
    //u[0][1][0](IntVect(22,12)) = 1.0;
    u[0][0][0](IntVect(12,12)) = 3.0;
/*
    if (m[0].domain().length(0) == 32)
      u[0][1][0](IntVect(30,30)) = 1.0;
    else if (m[0].domain().length(0) == 64)
      u[0][1][0](IntVect(60,60)) = 1.0;
    else
      u[0][1][0](IntVect(120,120)) = 1.0;
*/
  }
  else {
    for (ilev = 0; ilev < m.length(); ilev++) {
      for (int igrid = 0; igrid < m[ilev].length(); igrid++) {
	u[0][ilev][igrid].setVal(1.e20);
	u[1][ilev][igrid].setVal(3.e20);
	u[0][ilev][igrid].setVal(0.0, m[ilev][igrid], 0);
	u[1][ilev][igrid].setVal(0.0, m[ilev][igrid], 0);
      }
    }
    u[0][2][0](m[2][0].smallEnd() + Iv(10,10)) = 3.0;
    // for gr2ann
    //u[0][2][0](IntVect(20,20)) = 1.0;
    //u[0][2][0](IntVect(20,20)) = 0.0;
    //u[0][2][2](IntVect(70,80)) = 1.0;
  }
  //u[0][1][0](IntVect(31,31)) = -1.0;
  //u[1][1][0](IntVect(31,30)) = 1.0;
  //u[1][1][0](IntVect(30,31)) = -1.0;
  for (ilev = 0; ilev < p.length(); ilev++)
    p[ilev].setVal(0.0);
  //p.initialize(fct);
#else
  for (ilev = 0; ilev < m.length(); ilev++) {
    u[0][ilev].setVal(0.0);
    u[1][ilev].setVal(0.0);
    u[2][ilev].setVal(0.0);
  }
  if (m.length() == 1) {
    //int ioff = m[0].domain().length(0) / 8;
    int ioff = 2;
    for (int igrid = 0; igrid < m[0].length(); igrid++) {
      // Used for timings3_94:
      //u[0][0][igrid](m[0][igrid].smallEnd() + Iv(2,2,2)) = 3.0;
      u[0][0][igrid](m[0][igrid].smallEnd() + Iv(ioff,ioff,ioff)) = 3.0;
    }
  }
  else if (m.length() == 2) {
    // used for convergence-rate tests:
    //int ioff = m[1].domain().length(0) / 8;
    int ioff = 2;
    for (int igrid = 0; igrid < m[1].length(); igrid++) {
      // Used for timings3_94:
      //u[0][1][igrid](m[1][igrid].smallEnd() + Iv(2,2,2)) = 3.0;
      u[0][1][igrid](m[1][igrid].smallEnd() + Iv(ioff,ioff,ioff)) = 3.0;
    }
    u[0][0][0](IntVect(1,1,1)) = 3.0;
  }
  else if (m.length() == 3) {
    int ioff = 2;
    for (int igrid = 0; igrid < m[2].length(); igrid++) {
      u[0][2][igrid](m[2][igrid].smallEnd() + Iv(ioff,ioff,ioff)) = 3.0;
    }
  }
  for (ilev = 0; ilev < m.length(); ilev++) {
    p[ilev].setVal(0.0);
  }
#endif
}

#if (BL_SPACEDIM == 2)
void hb93_test1(PArray<MultiFab> u[], const Array<BoxArray>& m,
		const Array<Box>& d)
{
  for (int ilev = 0 ; ilev < m.length() ; ilev++) {
    double h = 1.0 / d[ilev].length(0);
    double pi = 3.14159265358979323846;
    for (int igrid = 0; igrid < m[ilev].length() ; igrid++) {
      for (int i = m[ilev][igrid].smallEnd(0);
	   i <= m[ilev][igrid].bigEnd(0); i++) {
	for (int j = m[ilev][igrid].smallEnd(1);
	     j <= m[ilev][igrid].bigEnd(1); j++) {
          double x = (i + 0.5) * h;
          double y = (j + 0.5) * h;
          u[0][ilev][igrid](Iv(i,j)) = -0.5*(1.0-cos(2*pi*x))*sin(2*pi*y);
          u[1][ilev][igrid](Iv(i,j)) =  0.5*(1.0-cos(2*pi*y))*sin(2*pi*x);
          //u[0][ilev][igrid](Iv(i,j)) =  0.2*(x+1)*sin(pi*x)*
	  //  (pi*(y+1)*cos(pi*y)+sin(pi*y));
          //u[1][ilev][igrid](Iv(i,j)) = -0.2*(y+1)*sin(pi*y)*
	  //  (pi*(x+1)*cos(pi*x)+sin(pi*x));
        }
      }
    }
  }
}

void rz_adj(PArray<MultiFab> u[], PArray<MultiFab>& rhs,
	    PArray<MultiFab>& rhoinv, const Array<BoxArray>& m,
	    const Array<Box>& d)
{
  for (int ilev = 0 ; ilev < m.length() ; ilev++) {
    double h = 1.0 / d[ilev].length(0);
    double pi = 3.14159265358979323846;
    for (int igrid = 0; igrid < m[ilev].length() ; igrid++) {
      for (int i = m[ilev][igrid].smallEnd(0);
	   i <= m[ilev][igrid].bigEnd(0); i++) {
	for (int j = m[ilev][igrid].smallEnd(1) - 1;
	     j <= m[ilev][igrid].bigEnd(1); j++) {
          double x = (i + 0.5) * h;
          double y = (j + 0.5) * h;
	  double x0 = i * h, x1 = x0 + h;
          //u[0][ilev][igrid](Iv(i,j)) = 0.0;
          u[1][ilev][igrid](Iv(i,j)) *= x;
          //u[1][ilev][igrid](Iv(i,j)) = x;
	  if (j >= m[ilev][igrid].smallEnd(1))
	    rhoinv[ilev][igrid](Iv(i,j)) *= x;
        }
      }
    }
  }
}
#endif

void projtest(Array<BoxArray>& m, Array<IntVect>& ratio, Array<Box>& domain)
{
  int ilev, i;

  Real h[BL_SPACEDIM];
  for (i = 0; i < BL_SPACEDIM; i++)
    h[i] = 1;

  RegType bc[BL_SPACEDIM][2];

#if (BL_SPACEDIM == 2)
#  ifdef USE_GRAPHICS
  int ncont = 11;
  //int ncont = 101;
#  endif
  //bc[0][0] = inflow;
  //bc[0][1] = outflow;
  bc[0][0] = periodic;
  bc[0][1] = periodic;
  bc[0][0] = refWall;
  bc[0][1] = refWall;
  //bc[1][0] = periodic;
  //bc[1][1] = periodic;
  bc[1][0] = refWall;
  bc[1][1] = refWall;
  //bc[1][0] = inflow;
  //bc[1][1] = outflow;
#else
#  ifdef USE_GRAPHICS
  set_graphics_knobs(30.0, 10.0);
  int ncont = 3;
#  endif
  for (i = 0; i < BL_SPACEDIM; i++) {
    bc[i][0] = refWall;
    bc[i][1] = refWall;
  }
  //bc[0][0] = inflow;
  //bc[0][1] = outflow;
  //bc[0][0] = periodic;
  //bc[0][1] = periodic;
  //bc[1][0] = periodic;
  //bc[1][1] = periodic;
  //bc[2][0] = periodic;
  //bc[2][1] = periodic;
#endif

  PArray<MultiFab> u[BL_SPACEDIM];
  PArray<MultiFab> p, rhoinv, rhs;

  for (i = 0; i < BL_SPACEDIM; i++)
    u[i].resize(m.length());
  p.resize(m.length());
  rhoinv.resize(m.length());
  rhs.resize(m.length());

  for (ilev = 0; ilev < m.length(); ilev++) {
    BoxArray& cmesh = m[ilev];
    BoxArray nmesh = cmesh;
    nmesh.convert(nodevect);
    for (i = 0; i < BL_SPACEDIM; i++)
      u[i].set(ilev, new MultiFab(cmesh, 1, 1));
    rhoinv.set(ilev, new MultiFab(cmesh, 1, 0));
    p.set(ilev, new MultiFab(nmesh, 1, 1));
    rhoinv[ilev].setVal(1.0);
    //rhs.set(ilev, new MultiFab(nmesh, 1, 1));
    rhs.set(ilev, new MultiFab(cmesh, 1, 1));
    rhs[ilev].setVal(0.0);
  }

  init(u, p, m);

#if (BL_SPACEDIM == 2)
  //hb93_test1(u, m);
  //rz_adj(u, rhs, rhoinv, m, domain);
  //rhs[1][0](Iv(16,51)) = 100.0;
  //rhs[1][0](Iv(16,50)) = 100.0;
  //rhs[1][0](Iv(47,20)) = 100.0;
  //rhs[1][0](Iv(30,40)) = -100.0;
  //rhs[0][0](Iv(4,13)) = 100.0;
  //rhs[0][0](Iv(20,90)) = 100.0;
#else
  //rhs[1][0](Iv(16,20,40)) = 100.0;
  //rhs[1][0](Iv(16,20,21)) = 100.0;
#endif

/*
  u[0].assign(0.0);
  u[1].assign(-980.0);

  //Box bb(0,27,63,36);
  Box bb(27,0,36,63);
  //Box bb(26,0,37,63);
  for (ilev = 0; ilev < m.length(); ilev++) {
    for (int igrid = 0; igrid < m[ilev].length(); igrid++) {
      rhoinv[ilev][igrid].assign(100.0, (bb & m[ilev][igrid]));
    }
  }
*/
/*
#if (BL_SPACEDIM == 2)
  //Box bb(6,6,11,11);
  Box bb(0,0,5,5);
  //Box bb(0,0,6,7);
#else
  Box bb(Iv(0,0,0),Iv(8,8,8));
#endif
  for (ilev = 0; ilev < m.length(); ilev++) {
    Box b = refine(bb, m[ilev].sig()/16);
    for (int igrid = 0; igrid < m[ilev].length(); igrid++) {
      rhoinv[ilev][igrid].assign(1000.0, (b & m[ilev][igrid]));
      //rhoinv[ilev][igrid].assign(1.0, (b & m[ilev][igrid]));
    }
  }
*/
/*
  Box bb(0,1,1,4);
  for (ilev = 0; ilev < m.length(); ilev++) {
    Box b = refine(bb, m[ilev].sig()/8);
    for (int igrid = 0; igrid < m[ilev].length(); igrid++) {
      rhoinv[ilev][igrid].assign(1000.0, (b & m[ilev][igrid]));
    }
  }
*/
/* Layer
  //Box bb(0,1,1,4);
  Box bb(0,0,63,4);
  for (ilev = 0; ilev < m.length(); ilev++) {
    Box b = refine(bb, m[ilev].sig()/32);
    for (int igrid = 0; igrid < m[ilev].length(); igrid++) {
      rhoinv[ilev][igrid].assign(0.00001, (b & m[ilev][igrid]));
    }
  }
*/
/* Drop
  Box bbb(16,6,19,9);
  for (ilev = 0; ilev < m.length(); ilev++) {
    Box b = refine(bbb, m[ilev].sig()/32);
    for (int igrid = 0; igrid < m[ilev].length(); igrid++) {
      rhoinv[ilev][igrid].assign(0.00001, (b & m[ilev][igrid]));
    }
  }
*/

#ifdef USE_GRAPHICS
  fit(domain[0]);
  plot(m, ratio);
#  ifndef CONSTANT
  //contour(rhoinv, ratio, ncont);
#  endif
#endif

  int sum = 0;
  cout << "Cells by level: ";
  for (ilev = 0; ilev < m.length(); ilev++) {
    int lsum = 0;
    for (i = 0; i < m[ilev].length(); i++) {
      lsum += m[ilev][i].numPts();
    }
    cout << " " << lsum;
    sum += lsum;
  }
  cout << "\nTotal cells:  " << sum << endl;

  clock_t t0, t1, t2, t3;

#ifdef UNICOS
  //int pcode = 1, nrep = 8;
  int pcode = 1, nrep = 1;
  Real tol = 1.e-6;
  //Real tol = 2.e-10;
#else
  int pcode = 2, nrep = 1;
  Real tol = 1.e-12;
  //int pcode = 1, nrep = 3;
  //Real tol = 1.e-6;
  // for vd tests in May, and most code validation tests:
  //Real tol = 2.e-10;
  //Real tol = 5.e-9;
#endif
  t0 = clock();
  inviscid_fluid_boundary afb(bc);
  i = m.length() - 1;
  holy_grail_amr_projector proj(m, ratio, domain[i], 0, i, i, afb, pcode);
#if (BL_SPACEDIM == 2)
  //proj.SetRZ();
#endif
  //proj.smoother_mode  = 1;
  proj.line_solve_dim = BL_SPACEDIM - 1;

  if (m.length() == 1) {
    t1 = clock();
    proj.project(u, p, null_amr_real, rhoinv, h, tol);
    for (i = 1; i < nrep; i++) {
      init(u, p, m);
      proj.project(u, p, null_amr_real, rhoinv, h, tol);
    }
    t2 = clock();
    cout << "Init time was " << t1 - t0 << endl;
    cout << "Proj time was " << t2 - t1 << endl;
    cout << "Speed was " << double(t2 - t1) / (nrep * sum) << endl;
    cout << setprecision(16);
    cout << "umin = " << u[0][0].min(0)
	 << ", umax = " << u[0][0].max(0) << endl;
    cout << "vmin = " << u[1][0].min(0)
	 << ", vmax = " << u[1][0].max(0) << endl;
#if (BL_SPACEDIM == 3)
    cout << "wmin = " << u[2][0].min(0)
	 << ", wmax = " << u[2][0].max(0) << endl;
#endif
    cout << setprecision(6);
  }
  else if (m.length() == 2) {
    proj.project(u, p, null_amr_real, rhoinv, h, tol, 0, 0);
    proj.project(u, p, null_amr_real, rhoinv, h, tol, 1, 1);
    t1 = clock();
    cout << "First time is " << t1 - t0 << endl;
    for (i = 0; i < p.length(); i++)
      p[i].setVal(0.0);
    t1 = clock();
    //proj.project(u, p, null_amr_real, rhoinv, h, tol, 0, 1);
    proj.manual_project(u, p, rhs, null_amr_real, rhoinv, 1, h, tol, 0, 1);
    for (i = 1; i < nrep; i++) {
      init(u, p, m);
      proj.project(u, p, null_amr_real, rhoinv, h, tol, 0, 1);
    }
    t2 = clock();
    cout << "Second time is " << t2 - t1 << endl;
    cout << "Sync speed was " << double(t2 - t1) / (nrep * sum) << endl;
/*
    for (i = m[1][0].smallEnd(1); i <= m[1][0].bigEnd(1)+1; i++) {
      cout << p[1][0](Iv(0, i)) << endl;
    }
    proj.project(u, p, null_amr_real, rhoinv, h, tol, 1, 1);
    for (i = m[1][0].smallEnd(1); i <= m[1][0].bigEnd(1)+1; i++) {
      cout << p[1][0](Iv(0, i)) << endl;
    }
*/
  }
  else {
    proj.project(u, p, null_amr_real, rhoinv, h, tol, 2, 2);
    t1 = clock();
    cout << "First time is " << t1 - t0 << endl;
    for (i = 0; i < p.length(); i++)
      p[i].setVal(0.0);
    proj.project(u, p, null_amr_real, rhoinv, h, tol, 1, 2);
    t2 = clock();
    cout << "Second time is " << t2 - t1 << endl;
    for (i = 0; i < p.length(); i++)
      p[i].setVal(0.0);
    proj.project(u, p, null_amr_real, rhoinv, h, tol, 0, 2);
    t3 = clock();
    cout << "Third time is " << t3 - t2 << endl;
    cout << "Total time was  " << t3 - t0 << endl;
  }
#ifdef USE_GRAPHICS
  //contour(u[0], ratio, 11);
  //contour(p, ratio, 99);
  for (i = 0; i < BL_SPACEDIM; i++) {
    cout << "Dimension " << i << " norm is " << pmfnorm(u[i]) << endl;
    contour(u[i], ratio, ncont);
    cin.get();
  }
#endif
/*
  if (m.length() < 3) {
    for (i = 0; i < p.length(); i++)
      p[i].setVal(0.0);
    holy_grail_amr_projector proj(m, 0, m.length() - 1, afb, pcode);
    proj.project(u, p, null_amr_real, rhoinv, h, 1.e-14);
    t2 = clock();
    cout << "Second time is " << t2 - t1 << endl;
    cout << "Total time was  " << t2 - t0 << endl;
  }
*/
  for (ilev = 0; ilev < m.length(); ilev++) {
    for (i = 0; i < BL_SPACEDIM; i++)
      delete u[i].remove(ilev);
    delete rhoinv.remove(ilev);
    delete p.remove(ilev);
    delete rhs.remove(ilev);
  }
}
