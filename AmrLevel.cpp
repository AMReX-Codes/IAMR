
//
// $Id: AmrLevel.cpp,v 1.18 1997-11-18 18:31:28 car Exp $
//

// #define ADVANCE_DEBUG 1

#ifdef	_MSC_VER
#ifdef BL_USE_NEW_HFILES
#include <sstream>
#else
#include <strstrea.h>
#endif
#else
#include <strstream.h>
#endif

#ifdef BL_USE_NEW_HFILES
#include <cstdio>
#include <cstring>
#else
#include <stdio.h>
#include <string.h>
#endif

#include <AmrLevel.H>
#include <Derive.H>
#include <BoxDomain.H>
#include <ParallelDescriptor.H>

const char NL = '\n';

// -------------------------------------------------------------
// static data initialization
DescriptorList  AmrLevel::desc_lst;
DeriveList      AmrLevel::derive_lst;
// -------------------------------------------------------------

// -------------------------------------------------------------
AmrLevel::AmrLevel() 
{
   parent = 0;
   level = -1;
}

// -------------------------------------------------------------
AmrLevel::AmrLevel(Amr &papa, int lev, const Geometry &level_geom,
                   const BoxArray& ba, Real time)
    : geom(level_geom),grids(ba)
{
    level = lev;
    parent = &papa;

    fine_ratio = IntVect::TheUnitVector(); fine_ratio.scale(-1);
    crse_ratio = IntVect::TheUnitVector(); crse_ratio.scale(-1);

    if (level > 0) crse_ratio = parent->refRatio(level-1);
    if (level < parent->maxLevel()) fine_ratio = parent->refRatio(level);

    Real dt = parent->dtLevel(lev);
    int ndesc = desc_lst.length();
    state.resize(ndesc);
    const Box& domain = geom.Domain();
    int i;
    for (i = 0; i < ndesc; i++) {
	state[i].define(domain,grids,desc_lst[i],time, dt);
    }

    finishConstructor();
}

// -------------------------------------------------------------
void
AmrLevel::restart(Amr &papa, istream &is)
{
    parent = &papa;

    is >> level;
    is >> geom;

    fine_ratio = IntVect::TheUnitVector(); fine_ratio.scale(-1);
    crse_ratio = IntVect::TheUnitVector(); crse_ratio.scale(-1);

    if (level > 0) crse_ratio = parent->refRatio(level-1);
    if (level < parent->maxLevel()) fine_ratio = parent->refRatio(level);

      // read BoxArray
    grids.define(is);

    int nstate;
    is >> nstate;
    int ndesc = desc_lst.length();
    assert (nstate == ndesc);

    state.resize(ndesc);
    int i;
    for (i = 0; i < ndesc; i++) state[i].restart(is,desc_lst[i]);

    finishConstructor();

}

// -------------------------------------------------------------
void
AmrLevel::finishConstructor()
{
   // set physical locations of grids
    int num_grids = grids.length();
    grid_loc.resize(num_grids);
    const Real* prob_lo = geom.ProbLo();
    const Real* dx = geom.CellSize();
    int i;
    for (i = 0; i < num_grids; i++) {
	grid_loc[i] = RealBox(grids[i],dx,prob_lo);
    }
}

// -------------------------------------------------------------
void
AmrLevel::setTimeLevel(Real time, Real dt_old, Real dt_new)
{
    int ndesc = desc_lst.length();
    int k;
    for (k = 0; k < ndesc; k++)
	state[k].setTimeLevel(time,dt_old,dt_new);
}


// -------------------------------------------------------------
int
AmrLevel::isStateVariable(const aString &name, int& typ, int& n)
{
    int ndesc = desc_lst.length();
    for (typ = 0; typ < ndesc; typ++) {
	const StateDescriptor &desc = desc_lst[typ];
	for (n = 0; n < desc.nComp(); n++) {
	    if (desc.name(n) == name)
		return true;
	}
    }
    return false;
}

// -------------------------------------------------------------
long
AmrLevel::countCells()
{
    long cnt = 0;
    int i;
    for (i = 0; i < grids.length(); i++) {
	cnt += grids[i].numPts();
    }
    return cnt;
}

// -------------------------------------------------------------
void
AmrLevel::checkPoint(ostream& os)
{
  if(ParallelDescriptor::IOProcessor()) 
  {
    os << level << '\n';
    os << geom << '\n';

      // output BoxArray
    grids.writeOn(os);
  }

      // output state data
    int ndesc = desc_lst.length();
  if(ParallelDescriptor::IOProcessor()) {
    os << ndesc << '\n';
  }
    int i;
    for (i = 0; i < ndesc; i++)
	state[i].checkPoint(os);
}


// -------------------------------------------------------------
AmrLevel::~AmrLevel()
{
    parent = 0;
}

// -------------------------------------------------------------
void
AmrLevel::allocOldData()
{
    int ndesc = desc_lst.length();
    int i;
    for (i = 0; i < ndesc; i++)
	state[i].allocOldData();
}

// -------------------------------------------------------------
void
AmrLevel::removeOldData()
{
    int ndesc = desc_lst.length();
    int i;
    for (i = 0; i < ndesc; i++)
	state[i].removeOldData();
}

// -------------------------------------------------------------
void
AmrLevel::reset()
{
    int ndesc = desc_lst.length();
    int i;
    for (i = 0; i < ndesc; i++)
	state[i].reset();
}

// -------------------------------------------------------------
MultiFab&
AmrLevel::get_data(int state_indx, Real time)
{
    Real old_time = state[state_indx].prevTime();
    Real new_time = state[state_indx].curTime();
    Real eps = 0.001*(new_time - old_time);
    if (time > old_time-eps && time < old_time+eps) {
	return get_old_data(state_indx);
    } else if (time > new_time-eps && time < new_time+eps) {
	return get_new_data(state_indx);
    } else {
	BoxLib::Error("get_data: invalid time");
	static MultiFab bogus;
	return bogus;
    }
}

// -------------------------------------------------------------
void
AmrLevel::setPhysBoundaryValues(int state_indx, int comp, int ncomp,
			     Real time)
{
    Real old_time = state[state_indx].prevTime();
    Real new_time = state[state_indx].curTime();
    Real eps = 0.001*(new_time - old_time);
    int do_new;
    if (time > old_time-eps && time < old_time+eps) {
	do_new = 0;
    } else if (time > new_time-eps && time < new_time+eps) {
	do_new = 1;
    } else {
	BoxLib::Error("setPhysBoundaryValues: invalid time");
    }
    state[state_indx].FillBoundary(geom.CellSize(),geom.ProbDomain(),
			       comp,ncomp,do_new);
}



// -------------------------------------------------------------
// -------------------------------------------------------------
// -------------------------------------------------------------
FillPatchIterator::FillPatchIterator(AmrLevel &amrlevel,
			    MultiFab &leveldata)
                  : amrLevel(amrlevel),
                    MultiFabIterator(leveldata),
                    levelData(leveldata),
                    multiFabCopyDesc(true),
		    bIsInitialized(false)
{
}


#if (NEWFPMINBOX == 0)
// -------------------------------------------------------------
FillPatchIterator::FillPatchIterator(AmrLevel &amrlevel,
                            MultiFab &leveldata,
                            const int boxGrow,
                            int dest_comp, Real time,
                            int state_index, int src_comp, int ncomp,
                            Interpolater *mapper)
                       : amrLevel(amrlevel),
                         MultiFabIterator(leveldata),
                         levelData(leveldata),
                         multiFabCopyDesc(true),
		         bIsInitialized(false)
{
  Initialize(boxGrow, dest_comp, time,
             state_index, src_comp, ncomp, mapper);
}
// -------------------------------------------------------------
void FillPatchIterator::Initialize(const int boxGrow,
                            int dest_comp, Real time,
                            int state_index, int src_comp, int ncomp,
                            Interpolater *mapper)
{
// this function sets up and performs the communication pattern for
// filling fabs of size levelData[i].box().grow(boxGrow) from amrlevel's
// state data, possibly interpolating between the new and old data
// the fill is done from this level and, if necessary, coarser levels

  growSize   = boxGrow;
  stateIndex = state_index;
  srcComp    = src_comp;
  destComp   = dest_comp;
  nComp      = ncomp;
  interpTime = time;

  int myproc = ParallelDescriptor::MyProc();
  int currentLevel;
  PArray<AmrLevel> &amrLevels = amrLevel.parent->getAmrLevels();
  cumulativeRefRatios.resize(amrLevel.level + 1);
  map.resize(amrLevel.level + 1);

  cumulativeRefRatios[amrLevel.level] = IntVect::TheUnitVector();
  for(currentLevel = amrLevel.level - 1; currentLevel >= 0; --currentLevel) {
    cumulativeRefRatios[currentLevel] = cumulativeRefRatios[currentLevel + 1] *
                                        amrLevels[currentLevel + 1].crse_ratio;
  }

  stateDataMFId.resize(amrLevel.level + 1);
  for(currentLevel = 0; currentLevel <= amrLevel.level; ++currentLevel) {
    StateData &currentState = amrLevels[currentLevel].state[stateIndex];
    const StateDescriptor &currentDesc =
                                amrLevels[currentLevel].desc_lst[stateIndex];

    currentState.RegisterData(multiFabCopyDesc, stateDataMFId[currentLevel]);
    map[currentLevel] = mapper;
    if(map[currentLevel] == NULL) { map[currentLevel] = currentDesc.interp(); }
  }

  localMFBoxes.resize(levelData.boxArray().length());
  fillBoxId.resize(levelData.boxArray().length());
  savedFineBox.resize(levelData.boxArray().length());
  for(int iLocal = 0; iLocal < localMFBoxes.length(); ++iLocal) {
    if(levelData.DistributionMap().ProcessorMap()[iLocal] == myproc) {  // local
      localMFBoxes.set(iLocal, levelData.boxArray()[iLocal]);
      fillBoxId[iLocal].resize(amrLevel.level + 1);
      savedFineBox[iLocal].resize(amrLevel.level + 1);
    }
  }
  localMFBoxes.grow(growSize);  // these are the ones we want to fillpatch

  IndexType boxType(levelData.boxArray()[0].ixType());
  BoxList unfilledBoxesOnThisLevel(boxType);
  BoxList unfillableBoxesOnThisLevel(boxType);

  // do this for all local (grown) fab boxes
  for(int ibox = 0; ibox < localMFBoxes.length(); ++ibox) {
    if(levelData.DistributionMap().ProcessorMap()[ibox] != myproc) {  // not local
      continue;
    }

    unfilledBoxesOnThisLevel.clear();
    assert(unfilledBoxesOnThisLevel.ixType() == boxType);
    assert(unfilledBoxesOnThisLevel.ixType() == localMFBoxes[ibox].ixType());
    unfilledBoxesOnThisLevel.add(localMFBoxes[ibox]);

    // find the boxes that can be filled on each level--these are all
    // defined at their level of refinement
    bool needsFilling = true;
    for(currentLevel = amrLevel.level; currentLevel >= 0 && needsFilling;
        --currentLevel)
    {
      unfillableBoxesOnThisLevel.clear();

      StateData &currentState   = amrLevels[currentLevel].state[stateIndex];
      const Box &currentPDomain = currentState.getDomain();

// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv periodic
      int is_periodic = amrLevels[currentLevel].geom.isAnyPeriodic();

      if(is_periodic) {
        BoxList tempUnfilledBoxes(boxType);
        for(BoxListIterator pbli(unfilledBoxesOnThisLevel); pbli; ++pbli) {
          assert(pbli().ok());
          Box dbox(pbli());
          int inside = currentPDomain.contains(dbox);
          if( ! inside) {
            Array<IntVect> pshifts(27);
            amrLevels[currentLevel].geom.periodicShift(currentPDomain,dbox,pshifts);
            for(int iiv = 0; iiv < pshifts.length(); iiv++ ) {
              IntVect iv = pshifts[iiv];
              Box shbox(dbox);
              D_TERM( shbox.shift(0,iv[0]);,
                      shbox.shift(1,iv[1]);,
                      shbox.shift(2,iv[2]); )
              shbox &= currentPDomain;
              if(shbox.ok()) {
                tempUnfilledBoxes.add(shbox);
              }
            }
          }
        }  // end (bli()...)
        unfilledBoxesOnThisLevel.join(tempUnfilledBoxes);
      }  // end if(is_periodic)
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ end periodic

      fillBoxId[ibox][currentLevel].resize(unfilledBoxesOnThisLevel.length());
      savedFineBox[ibox][currentLevel].resize(unfilledBoxesOnThisLevel.length());

      int currentBLI = 0;
      for(BoxListIterator bli(unfilledBoxesOnThisLevel); bli; ++bli) {
        assert(bli().ok());
        Box coarseDestBox(bli());
        Box fineTruncDestBox(coarseDestBox & currentPDomain);
        if(fineTruncDestBox.ok()) {
          fineTruncDestBox.refine(cumulativeRefRatios[currentLevel]);

          Box tempCoarseBox;
          if(currentLevel == amrLevel.level) {
            tempCoarseBox = fineTruncDestBox;
          } else {
            tempCoarseBox = map[currentLevel]->CoarseBox(fineTruncDestBox,
                                      cumulativeRefRatios[currentLevel]);
          }

          savedFineBox[ibox][currentLevel][currentBLI] = fineTruncDestBox;
          if( ! is_periodic) {
            assert(localMFBoxes[ibox].intersects(fineTruncDestBox));
          }

          BoxList tempUnfillableBoxes(boxType);
          currentState.linInterpAddBox(multiFabCopyDesc,
                               stateDataMFId[currentLevel],
                               tempUnfillableBoxes,
                               fillBoxId[ibox][currentLevel][currentBLI],
                               tempCoarseBox,
                               interpTime, srcComp, destComp, nComp);

          unfillableBoxesOnThisLevel.join(tempUnfillableBoxes);
          ++currentBLI;
        }
      }  // end for(bli...)

      unfilledBoxesOnThisLevel.clear();
      unfilledBoxesOnThisLevel =
                        unfillableBoxesOnThisLevel.intersect(currentPDomain);

      if(unfilledBoxesOnThisLevel.isEmpty()) {
        needsFilling = false;
      } else {
        Box coarseLocalMFBox(localMFBoxes[ibox]);
        coarseLocalMFBox.coarsen(cumulativeRefRatios[currentLevel]);
        if( ! is_periodic) {
          unfilledBoxesOnThisLevel.intersect(coarseLocalMFBox);
        }
        unfilledBoxesOnThisLevel.coarsen(amrLevels[currentLevel].crse_ratio);

        if(currentLevel == 0) {
          BoxList unfilledInside =
                          unfilledBoxesOnThisLevel.intersect(currentPDomain);
          if( ! unfilledInside.isEmpty()) {
            unfilledInside.intersect(coarseLocalMFBox);
            assert(unfilledInside.isEmpty());
          }
        }
      }
    }  // end for(currentLevel...)
  }  // end for(ibox...)

  multiFabCopyDesc.CollectData();

  bIsInitialized = true;

}  // end FillPatchIterator(...)



// -------------------------------------------------------------
bool FillPatchIterator::isValid() 
{

// if the currentIndex is valid,
// this function will fill the currentFillPatchedFab from state
// so it is ready to be used if requested by operator()
// the fill is done from this level and, if necessary, coarser levels
// with values from the FillPatchIterator constructor

  assert(bIsInitialized);

  if( ! MultiFabIterator::isValid()) {  // this does a sync if not valid
    return false;
  }

  int myproc = ParallelDescriptor::MyProc();
  PArray<AmrLevel> &amrLevels = amrLevel.parent->getAmrLevels();

  Box destBox(validbox());
  destBox.grow(growSize);

  currentFillPatchedFab.resize(destBox, nComp);
// should be able to delete this setVal vvvvvvvvvvvvv
  currentFillPatchedFab.setVal(1.e30);

  int currentLevel;
  for(currentLevel = 0; currentLevel <= amrLevel.level; ++currentLevel) {
    int is_periodic = amrLevels[currentLevel].geom.isAnyPeriodic();
    StateData &currentState = amrLevels[currentLevel].state[stateIndex];
    const Box &currentPDomain = currentState.getDomain();

    for(int currentBox = 0;
        currentBox < fillBoxId[currentIndex][currentLevel].length();
        ++currentBox)
    {
      Box tempCoarseBox(fillBoxId[currentIndex][currentLevel][currentBox][0].box());

      FArrayBox tempCoarseDestFab(tempCoarseBox, nComp);
// should be able to delete this setVal vvvvvvvvvvvvv
      tempCoarseDestFab.setVal(1.e30);
      // get the state data

      currentState.linInterpFillFab(multiFabCopyDesc,
                            stateDataMFId[currentLevel],
                            fillBoxId[currentIndex][currentLevel][currentBox],
                            tempCoarseDestFab,
                            //interpTime, srcComp, destComp, nComp);
                            interpTime, 0, destComp, nComp);


      Box intersectDestBox(savedFineBox[currentIndex][currentLevel][currentBox]);
      if( ! is_periodic) {
        intersectDestBox &= currentFillPatchedFab.box();
      }

      const BoxArray &filledBoxes =
              fillBoxId[currentIndex][currentLevel][currentBox][0].FilledBoxes();
      BoxArray fboxes(filledBoxes);
      FArrayBox *copyFromThisFab;
      const BoxArray  *copyFromTheseBoxes;
      FArrayBox tempCurrentFillPatchedFab;

      if(intersectDestBox.ok()) {
       if(currentLevel != amrLevel.level) {
        // get boundary conditions for this patch
        Array<BCRec> bcCoarse(nComp);
        const StateDescriptor &desc = amrLevels[currentLevel].desc_lst[stateIndex];
        setBC(intersectDestBox, currentPDomain, srcComp, 0, nComp,
              desc.getBCs(), bcCoarse);

        fboxes.refine(cumulativeRefRatios[currentLevel]);

      // interpolate up to fine patch
        tempCurrentFillPatchedFab.resize(intersectDestBox, nComp);
        map[currentLevel]->interp(tempCoarseDestFab, 0, tempCurrentFillPatchedFab,
                                  destComp, nComp, intersectDestBox,
                                  cumulativeRefRatios[currentLevel],
                                  amrLevels[currentLevel].geom,
                                  amrLevels[amrLevel.level].geom,
                                  bcCoarse);
        copyFromThisFab = &tempCurrentFillPatchedFab;
        copyFromTheseBoxes = &fboxes;

      } else {
        copyFromThisFab = &tempCoarseDestFab;
        copyFromTheseBoxes = &filledBoxes;
      }

      for(int iFillBox = 0; iFillBox < copyFromTheseBoxes->length(); ++iFillBox) {
        Box srcdestBox((*copyFromTheseBoxes)[iFillBox]);
        srcdestBox &= currentFillPatchedFab.box();
        srcdestBox &= intersectDestBox;
        if(srcdestBox.ok()) {
          currentFillPatchedFab.copy(*copyFromThisFab, srcdestBox, 0,
                                     srcdestBox, destComp, nComp);
        }
      }

// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv periodic
      if(is_periodic) {
        //StateData &currentState = amrLevels[currentLevel].state[stateIndex];
        StateData &fineState    = amrLevels[amrLevel.level].state[stateIndex];
        //const Box &currentPDomain = currentState.getDomain();
        const Box &finePDomain    = fineState.getDomain();
        int inside = finePDomain.contains(currentFillPatchedFab.box());

        if( ! inside) {
          Array<IntVect> pshifts(27);
          Box dbox(currentFillPatchedFab.box());
          amrLevels[amrLevel.level].geom.periodicShift(finePDomain, dbox, pshifts);
          for(int iiv = 0; iiv < pshifts.length(); iiv++) {
            IntVect iv = pshifts[iiv];
            currentFillPatchedFab.shift(iv);
            for(int iFillBox = 0; iFillBox < copyFromTheseBoxes->length();
                ++iFillBox)
            {
              Box srcdestBox((*copyFromTheseBoxes)[iFillBox]);
              srcdestBox &= currentFillPatchedFab.box();
              srcdestBox &= copyFromThisFab->box();
              if(srcdestBox.ok()) {
                currentFillPatchedFab.copy(*copyFromThisFab, srcdestBox, 0,
                                           srcdestBox, destComp, nComp);
              }
            }
            currentFillPatchedFab.shift(-iv);
          }
        }  // end if( ! inside)
      }  // end if(is_periodic)
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ end periodic

     }  // end if(intersectDestBox.ok())

    }  // end for(currentBox...)
  }  // end for(currentLevel...)

  // do non-periodic BCs on the finest level
  StateData &currentState = amrLevel.state[stateIndex];
  const Box &p_domain = amrLevel.state[stateIndex].getDomain();
  if( ! (p_domain.contains(destBox))) {
    const Real *dx = amrLevel.geom.CellSize();
    const RealBox &realProbDomain = amrLevel.geom.ProbDomain();

    currentState.FillBoundary(currentFillPatchedFab, interpTime, dx,
                              realProbDomain,
                              destComp, srcComp, nComp);
  }

  return true;

}  // end FillPatchIterator::isValid()

#endif

#if (NEWFPMINBOX == 1)
// -------------------------------------------------------------
FillPatchIterator::FillPatchIterator(AmrLevel &amrlevel,
                            MultiFab &levelData,
                            const int boxGrow,
                            int dest_comp, Real time,
                            int state_index, int src_comp, int ncomp,
                            Interpolater *mapper)
                       : MultiFabIterator(levelData),
                         amrLevel(amrlevel),
                         growSize(boxGrow),
                         stateIndex(state_index),
                         srcComp(src_comp),
                         destComp(dest_comp),
                         nComp(ncomp),
                         interpTime(time),
                         multiFabCopyDesc(true)
{
// this function sets up and performs the communication pattern for
// filling fabs of size levelData[i].box().grow(boxGrow) from amrlevel's
// state data, possibly interpolating between the new and old data
// the fill is done from this level and, if necessary, coarser levels

  int myproc = ParallelDescriptor::MyProc();
  int currentLevel;
  PArray<AmrLevel> &amrLevels = amrLevel.parent->getAmrLevels();
  map.resize(amrLevel.level + 1);

  stateDataMFId.resize(amrLevel.level + 1);
  for(currentLevel = 0; currentLevel <= amrLevel.level; ++currentLevel) {
    StateData &currentState = amrLevels[currentLevel].state[stateIndex];
    const StateDescriptor &currentDesc =
                                amrLevels[currentLevel].desc_lst[stateIndex];

    currentState.RegisterData(multiFabCopyDesc, stateDataMFId[currentLevel]);
    map[currentLevel] = mapper;
    if(map[currentLevel] == NULL) { map[currentLevel] = currentDesc.interp(); }
  }

  localMFBoxes.resize(levelData.boxArray().length());
  fillBoxId.resize(levelData.boxArray().length());
  savedFineBox.resize(levelData.boxArray().length());
  for(int iLocal = 0; iLocal < localMFBoxes.length(); ++iLocal) {
    if(levelData.DistributionMap().ProcessorMap()[iLocal] == myproc) {  // local
      localMFBoxes.set(iLocal, levelData.boxArray()[iLocal]);
      fillBoxId[iLocal].resize(amrLevel.level + 1);
      savedFineBox[iLocal].resize(amrLevel.level + 1);
    }
  }
  localMFBoxes.grow(growSize);  // these are the ones we want to fillpatch

  IndexType boxType(levelData.boxArray()[0].ixType());
  Box unfilledBoxOnThisLevel(boxType);
  BoxList unfillableBoxesOnThisLevel(boxType);

  // do this for all local (grown) fab boxes
  for(int ibox = 0; ibox < localMFBoxes.length(); ++ibox) {
    if(levelData.DistributionMap().ProcessorMap()[ibox] != myproc) {  // not local
      continue;
    }
cout << "dest.box() = " << localMFBoxes[ibox] << NL;
    unfilledBoxOnThisLevel = localMFBoxes[ibox] &
                            amrLevels[amrLevel.level].state[stateIndex].getDomain();
    assert(unfilledBoxOnThisLevel.ok());
    bool needsFilling = true;

    // ________________________________________ level loop
    for(currentLevel = amrLevel.level; currentLevel >= 0 && needsFilling;
        --currentLevel)
    {
cout << NL << "currentLevel = " << currentLevel << NL;
int refRatio = amrLevels[amrLevel.level].crse_ratio;
refRatio = 2;

      StateData &currentState   = amrLevels[currentLevel].state[stateIndex];
      const Box &currentPDomain = currentState.getDomain();
      unfillableBoxesOnThisLevel.clear();

      fillBoxId[ibox][currentLevel].resize(1);
      savedFineBox[ibox][currentLevel].resize(1);

      int currentBLI = 0;
      savedFineBox[ibox][currentLevel][currentBLI] = unfilledBoxOnThisLevel;
      //if(currentLevel != amrLevel.level) {
        //unfilledBoxOnThisLevel.coarsen(refRatio);
      //}

          Box tempCoarseBox;
          if(currentLevel == amrLevel.level) {
            tempCoarseBox = unfilledBoxOnThisLevel;
          } else {
            tempCoarseBox = map[currentLevel]->CoarseBox(unfilledBoxOnThisLevel,
                                                         refRatio);
cout << "currentLevel = " << currentLevel << NL;
cout << "mapped->CoarseBox = " << tempCoarseBox << "  from box " << unfilledBoxOnThi
sLevel << NL;
          }
cout << "about to linInterp with tempCoarseBox = " << tempCoarseBox << NL;


          currentState.linInterpAddBox(multiFabCopyDesc,
                               stateDataMFId[currentLevel],
                               unfillableBoxesOnThisLevel,
                               fillBoxId[ibox][currentLevel][currentBLI],
                               tempCoarseBox,
                               interpTime, srcComp, destComp, nComp);

cout << "      fillBoxId.box[]  = "
     << fillBoxId[ibox][currentLevel][currentBLI][0].box()  << NL;

      unfillableBoxesOnThisLevel.intersect(currentPDomain);
      unfilledBoxOnThisLevel = unfillableBoxesOnThisLevel.minimalBox();
      unfilledBoxOnThisLevel &= currentPDomain;

      if(unfilledBoxOnThisLevel.ok()) {
cout << "unfilled box on level " << currentLevel
     << " = " << unfilledBoxOnThisLevel << NL;
      } else {
        needsFilling = false;
      }
    }  // end for(currentLevel...)
  }  // end for(ibox...)

  multiFabCopyDesc.CollectData();

}  // end FillPatchIterator(...)


// -------------------------------------------------------------
bool FillPatchIterator::isValid() 
{

// if the currentIndex is valid,
// this function will fill the currentFillPatchedFab from state
// so it is ready to be used if requested by operator()
// the fill is done from this level and, if necessary, coarser levels
// with values from the FillPatchIterator constructor

  assert(bIsInitialized);

  if( ! MultiFabIterator::isValid()) {  // this does a sync if not valid
    return false;
  }

//static int fabcount = 0;
//cout << NL;
//cout << "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]] _in isValid()" << NL;

  int myproc = ParallelDescriptor::MyProc();
  PArray<AmrLevel> &amrLevels = amrLevel.parent->getAmrLevels();

  Box destBox(box());
  destBox.grow(growSize);

  currentFillPatchedFab.resize(destBox, nComp);
  currentFillPatchedFab.setVal(1.e30);

  FArrayBox tempCoarseDestFab, tempFineDestFab;
  FArrayBox *coarseDestFabPtr = NULL, *fineDestFabPtr = NULL;
  int currentLevel;
  int coarsestFillLevel = amrLevel.level;
  for(currentLevel = 0; currentLevel < amrLevel.level; ++currentLevel) {
    if(fillBoxId[currentIndex][currentLevel].length() > 0) {
      coarsestFillLevel = currentLevel;
      break;  // exit for loop
    }
  }
  cout << "coarsestFillLevel = " << coarsestFillLevel << NL;
  assert(coarsestFillLevel >= 0 && coarsestFillLevel <= amrLevel.level);

  for(currentLevel = coarsestFillLevel; currentLevel < amrLevel.level;
      ++currentLevel)
  {
    if(fillBoxId[currentIndex][currentLevel].length() == 0) {
      continue;
    }
    assert(fillBoxId[currentIndex][currentLevel].length() == 1);
cout << "currentLevel     = " << currentLevel     << NL;

int ivRefRatio = 2;
    //int fineLevel = currentLevel + 1;

    int currentBox = 0;
      StateData &currentState = amrLevels[currentLevel].state[stateIndex];
      Box tempCoarseBox(fillBoxId[currentIndex][currentLevel][currentBox][0].box());

    if(currentLevel == coarsestFillLevel) {
      assert(tempCoarseBox.ok());
cout << "Resizing coarse fab to " << tempCoarseBox << NL;
      tempCoarseDestFab.resize(tempCoarseBox, nComp);
      tempCoarseDestFab.setVal(1.e30);
      coarseDestFabPtr = &tempCoarseDestFab;
    } else {
      assert(fineDestFabPtr != NULL);
      coarseDestFabPtr = fineDestFabPtr;
    }
    assert(coarseDestFabPtr != NULL);
      // get the state data

/* linInterp */
    cout << "linInterp on coarse.box() = " << coarseDestFabPtr->box() << NL;
      currentState.linInterpFillFab(multiFabCopyDesc,
                            stateDataMFId[currentLevel],
                            fillBoxId[currentIndex][currentLevel][currentBox],
                            *coarseDestFabPtr,
                            interpTime, srcComp, destComp, nComp);

    const Real *dx = amrLevels[currentLevel].geom.CellSize();
    const RealBox &realProbDomain = amrLevels[currentLevel].geom.ProbDomain();

/* FillBoundary */
    cout << "FillBoundary on coarse.box() = " << coarseDestFabPtr->box() 
	 << "  outside boundary " << realProbDomain << NL;
    currentState.FillBoundary(*coarseDestFabPtr, interpTime, dx,
                              realProbDomain, destComp, srcComp, nComp);

      const Box &currentPDomain = currentState.getDomain();

      Box intersectDestBox(savedFineBox[currentIndex][currentLevel][currentBox]);

       assert(intersectDestBox.ok());
       //if(intersectDestBox.ok()) {
        // get boundary conditions for this patch
        Array<BCRec> bcCoarse(nComp);
        const StateDescriptor &desc = amrLevels[currentLevel].desc_lst[stateIndex];
        setBC(intersectDestBox, currentPDomain, srcComp, 0, nComp,
              desc.getBCs(), bcCoarse);

      // interpolate up to fine patch
        const BoxArray &filledBoxes =
              fillBoxId[currentIndex][currentLevel][currentBox][0].FilledBoxes();
        BoxArray fboxes(filledBoxes);
        fboxes.refine(ivRefRatio);
        assert(fboxes.length() == 1);
        int iFillBox = 0;
          Box srcdestBox(fboxes[iFillBox]);
          srcdestBox &= currentFillPatchedFab.box();
          srcdestBox &= intersectDestBox;
      if((currentLevel + 1) == amrLevel.level) {
        fineDestFabPtr = &currentFillPatchedFab;
      } else {
        tempFineDestFab.resize(fillBoxId[currentIndex][currentLevel+1][currentBox][0
].box(), nComp);
        fineDestFabPtr = &tempFineDestFab;
      }

/* map->interp */
cout << "map->interp coarse.box() = " << coarseDestFabPtr->box() << " to finer box =
 " << fineDestFabPtr->box() << "  on int_region = " << intersectDestBox << NL;
cout << "crse0_geom = " << amrLevels[currentLevel].geom << NL;
cout << "crse_geom  = " << amrLevels[currentLevel + 1].geom  << NL;
for(int ibc=0; ibc < nComp; ++ibc) {
  cout << "bc_crse[" << ibc << "] = " << bcCoarse[ibc] << NL;
}
          map[currentLevel]->interp(*coarseDestFabPtr, 0, *fineDestFabPtr,
                                    destComp, nComp, intersectDestBox,
                                    ivRefRatio,
                                    amrLevels[currentLevel].geom,
                                    amrLevels[currentLevel + 1].geom,
                                    bcCoarse);
        //}
       //}
  }  // end for(currentLevel...)

currentLevel = amrLevel.level;
int currentBox = 0;
cout << "linInterp on currentFillPatched.box() = " << currentFillPatchedFab.box() <<
 NL;
  StateData &currentState = amrLevel.state[stateIndex];
      currentState.linInterpFillFab(multiFabCopyDesc,
                            stateDataMFId[currentLevel],
                            fillBoxId[currentIndex][currentLevel][currentBox],
                            currentFillPatchedFab,
                            interpTime, srcComp, destComp, nComp);

  // do non-periodic BCs on the finest level
  const Box &p_domain = amrLevel.state[stateIndex].getDomain();
  if( ! (p_domain.contains(destBox))) {
    const Real *dx = amrLevel.geom.CellSize();
    const RealBox &realProbDomain = amrLevel.geom.ProbDomain();

/* FillBoundary */
cout << "FillBoundary on dest.box() = " << currentFillPatchedFab.box() << "  outside
 boundary " << realProbDomain << NL;
    currentState.FillBoundary(currentFillPatchedFab, interpTime, dx,
                              realProbDomain,
                              destComp, srcComp, nComp);
  }

//cout << "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]] _out isValid()" << NL;
//cout << NL;
  return true;
}  // end FillPatchIterator::isValid()
#endif




// -------------------------------------------------------------
FillPatchIterator::~FillPatchIterator() {
}



#if (USEUNRAVELEDFILLPATCH == 1)
// -------------------------------------------------------------
// -------------------------------------------------------------
// -------------------------------------------------------------
// -------------------------------------------------------------
// old filPatch  (unraveled) vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// old FillPatch (unraveled) vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

void AmrLevel::FillPatch(FArrayBox &dest, int dest_comp, Real time,
                         int stateIndex, int src_comp, int ncomp,
                         Interpolater *mapper)
{
cout << "\n_in old FillPatch"
     << "\ndest.box() = " << dest.box() << NL;
    dest.setVal(1.e30);
    Box dbox(dest.box());
    Box truncdbox(dbox);
    const StateDescriptor &desc = desc_lst[stateIndex];
    const RealBox &prob_domain = geom.ProbDomain();
    const Box &p_domain = state[stateIndex].getDomain();

    int inside = p_domain.contains(dbox); // does grid intersect boundary
    if( ! inside ) truncdbox &= p_domain;

    Box unfilled_region;
    BoxDomain fd(dbox.ixType());
    fd.add(truncdbox);

    Box enclosing_box = fd.minimalBox();
    const BoxArray &grds = state[stateIndex].boxArray();
    int i;
    for(i = 0; i < grds.length(); i++) {
        const Box& gbox = grds[i];
        if(enclosing_box.intersects(gbox)) fd.rmBox(gbox);
    }
    unfilled_region = fd.minimalBox();
    fd.clear();

    FArrayBox crse;
    if (unfilled_region.ok()) {
      AmrLevel &crse_lev = parent->getLevel(level-1);
      const Geometry& crse_geom = crse_lev.geom;
      // must fill on this region on crse level and interpolate
      assert( level != 0 );

      Interpolater *map = mapper;
      if (map == 0) map = desc.interp();

      // intersect unfilled_region with the domain, this is necessary
      // because the unfilled_region may contain stuff carried by
      // periodic BC to be outside the domain of dest
      Box int_region = unfilled_region & dbox;
      if( int_region.ok() ) {
cout << "unfilled box on level " << level << " = " << int_region << NL;
        // coarsen unfilled region and widen if necessary
        Box crse_reg(map->CoarseBox(int_region,crse_ratio));
cout << "mapped->CoarseBox = " << crse_reg << NL;
cout << "Resizing crse fab to " << crse_reg << NL;
        crse.resize(crse_reg,ncomp);         // alloc patch for crse level

        //crse_lev.filPatch(crse,0,time,stateIndex,src_comp,ncomp,mapper);
        // ****************************************************************
        crse.setVal(1.e30);
        Box crse_dbox(crse.box());
        Box crse_truncdbox(crse_dbox);
        const RealBox &crse_prob_domain = crse_geom.ProbDomain();
        const Box &crse_p_domain = crse_lev.state[stateIndex].getDomain();
        int crse_inside = crse_p_domain.contains(crse_dbox);

        if( ! inside ) crse_truncdbox &= crse_p_domain;

        Box crse_unfilled_region;
        BoxDomain crse_fd(crse_dbox.ixType());
        crse_fd.add(crse_truncdbox);

        Box crse_enclosing_box = crse_fd.minimalBox();
        const BoxArray &crse_grds = crse_lev.state[stateIndex].boxArray();
        for(i = 0; i < crse_grds.length(); i++) {
            const Box& crse_gbox = crse_grds[i];
            if(crse_enclosing_box.intersects(crse_gbox)) crse_fd.rmBox(crse_gbox);
        }
        crse_unfilled_region = crse_fd.minimalBox();
        crse_fd.clear();

        FArrayBox crse0;
        if (crse_unfilled_region.ok()) {
          AmrLevel &crse0_lev = parent->getLevel(level-2);  // should be 0 here
          const Geometry& crse0_geom = crse0_lev.geom;
          // must fill on this region on crse0 level and interpolate

          Box crse_int_region = crse_unfilled_region & crse_dbox;
          if( crse_int_region.ok() ){
cout << "unfilled box on level " << level-1 << " = " << crse_int_region << NL;
            // coarsen unfilled region and widen if necessary
            int crse0_ratio = 2;  // for testing
            Box crse0_reg(map->CoarseBox(crse_int_region,crse0_ratio));
cout << "mapped->CoarseBox = " << crse0_reg << NL;
cout << "Resizing crse0 fab to " << crse0_reg << NL;
            crse0.resize(crse0_reg,ncomp);         // alloc patch for crse0 level

            //crse0_lev.filPatch(crse0,0,time,stateIndex,src_comp,ncomp,mapper);
            // ================================================================
            crse0.setVal(1.e30);
            Box crse0_dbox(crse0.box());
            Box crse0_truncdbox(crse0_dbox);

            const RealBox &crse0_prob_domain = crse0_geom.ProbDomain();
            const Box &crse0_p_domain = crse0_lev.state[stateIndex].getDomain();
            int crse0_inside = crse0_p_domain.contains(crse0_dbox);

            if( ! crse0_inside ) crse0_truncdbox &= crse0_p_domain;
cout << "linInterp on crse0.box() = " << crse0.box() << NL;
            crse0_lev.state[stateIndex].linInterp(crse0,crse0.box(),time,
                                                 src_comp,0,ncomp);
            if( ! crse0_inside) {
              const Real* crse0_dx = crse0_geom.CellSize();
cout << "FillBoundary on crse0.box() = " << crse0.box() << "  outside boundary " <<
crse0_prob_domain << NL;
              crse0_lev.state[stateIndex].FillBoundary(crse0, time, crse0_dx,
                                      crse0_prob_domain, 0, src_comp, ncomp);
            }
            // ================================================================
            Array<BCRec> bc_crse(ncomp); // get bndry conditions for this patch
            setBC(crse_int_region,crse_p_domain,src_comp,0,
                     ncomp,desc.getBCs(),bc_crse);

            // interpolate up to fine patch
cout << "map->interp crse0.box() = " << crse0.box() << " to crse.box() = " << crse.b
ox() << " on crse_int_region = " << crse_int_region << NL;
cout << "crse0_geom = " << crse0_geom << NL;
cout << "crse_geom  = " << crse_geom  << NL;
for(int ibc=0; ibc < ncomp; ++ibc) {
  cout << "bc_crse[" << ibc << "] = " << bc_crse[ibc] << NL;
}
            map->interp(crse0,0,crse,dest_comp,ncomp,crse_int_region,
                        crse_ratio,crse0_geom,geom,bc_crse);
          }
        }

cout << "linInterp on crse.box() = " << crse.box() << NL;
        crse_lev.state[stateIndex].linInterp(crse,crse.box(),time,
                                             src_comp,dest_comp,ncomp);

        if( ! inside) {              // do non-periodic BC's on this level
          const Real* crse_dx = crse_geom.CellSize();
cout << "FillBoundary on crse.box() = " << crse.box() << "  outside boundary " << cr
se_prob_domain << NL;
          crse_lev.state[stateIndex].FillBoundary(crse,time,crse_dx,
                                                  crse_prob_domain,
                                                  dest_comp,src_comp,ncomp);
        }
        // ****************************************************************
        Array<BCRec> bc_crse(ncomp); // get bndry conditions for this patch
        setBC(int_region,p_domain,src_comp,0,ncomp,desc.getBCs(),bc_crse);

        // interpolate up to fine patch
cout << "map->interp crse.box() = " << crse.box() << " to dest.box() = " << dest.box
() << " on int_region = " << int_region << NL;
cout << "crse_geom  = " << crse_geom  << NL;
cout << "geom       = " << geom       << NL;
for(int ibc=0; ibc < ncomp; ++ibc) {
  cout << "bc_crse[" << ibc << "] = " << bc_crse[ibc] << NL;
}
        map->interp(crse,0,dest,dest_comp,ncomp,int_region,
                    crse_ratio,crse_geom,geom,bc_crse);
      }
    }

  state[stateIndex].linInterp(dest,dest.box(),time,src_comp,dest_comp,ncomp);

  if( ! inside) {              // do non-periodic BC's on this level
    const Real* dx = geom.CellSize();
cout << "FillBoundary on dest.box() = " << dest.box() << "  outside boundary " << pr
ob_domain << NL;
    state[stateIndex].FillBoundary(dest,time,dx,prob_domain,
                                   dest_comp,src_comp,ncomp);
  }
cout << "_out old FillPatch" << NL;
cout << NL;
}  // end filPatch
#endif

#if (USEUNRAVELEDFILLPATCH == 0)
// -------------------------------------------------------------
// -------------------------------------------------------------
// -------------------------------------------------------------
// -------------------------------------------------------------
// old filPatch  (recursive) vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// old FillPatch (recursive) vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
void
AmrLevel::FillPatch(FArrayBox &dest, int dest_comp, Real time,
                   int state_indx, int src_comp, int ncomp,
                   Interpolater *mapper)
{
//cout << NL;
//cout << "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]] _in old FillPatch" << NL;
//cout << "currentLevel     = " << this->Level() << NL;
    int is_periodic = geom.isAnyPeriodic();
    if(is_periodic) {
      ParallelDescriptor::Abort("This FillPatch does not do periodic");
    }
    if(ParallelDescriptor::NProcs() > 1) {
      ParallelDescriptor::Abort("FillPatch() not implemented in parallel");
    } else {
      cerr << "FillPatch() not implemented in parallel" << NL;
    }


    dest.setVal(1.e30);
    Box dbox(dest.box());
//cout << "box to FillPatch = " << dest.box() << NL;
    Box truncdbox(dbox);
    int nv = dest.nComp();
    int ndesc = desc_lst.length();
    const StateDescriptor &desc = desc_lst[state_indx];
    const RealBox& prob_domain = geom.ProbDomain();
    const Box& p_domain = state[state_indx].getDomain();

    int inside = p_domain.contains(dbox);
    if( ! inside ) truncdbox &= p_domain;

    Box unfilled_region;
    BoxDomain fd(dbox.ixType());
    fd.add(truncdbox);

    Box enclosing_box = fd.minimalBox();
    // step 2: take away stuff that can be gotten from this level of refinement
    const BoxArray &grds = state[state_indx].boxArray();
    for(int i = 0; i < grds.length(); i++) {
        if(enclosing_box.intersects(grds[i])) fd.rmBox(grds[i]);
    }
    unfilled_region = fd.minimalBox();
    fd.clear();

    FArrayBox crse;
    if (unfilled_region.ok()) {
      AmrLevel &crse_lev = parent->getLevel(level-1);
      const Geometry& crse_geom = crse_lev.geom;
      // must fill on this region on crse level and interpolate
      assert( level != 0 );

      Interpolater *map = mapper;
      if (map == 0) map = desc.interp();

      Box int_region = unfilled_region & dbox;
      if( int_region.ok() ){
        // coarsen unfilled region and widen if necessary
        Box crse_reg(map->CoarseBox(int_region,crse_ratio));

        // alloc patch for crse level
        crse.resize(crse_reg,ncomp);

        // fill patch at lower level
        crse_lev.FillPatch(crse,0,time,state_indx,src_comp,ncomp,mapper);

        // get bndry conditions for this patch
        Array<BCRec> bc_crse(ncomp);
        setBC(int_region,p_domain,src_comp,0,ncomp,
              desc.getBCs(),bc_crse);

//cout << "]]----------- about to map->interp:" << NL;
//cout << "]]----------- crse.box()       = " << crse.box() << NL;
//cout << "]]----------- dest.box()       = " << dest.box() << NL;
//cout << "]]----------- intersectDestBox = " << int_region << NL;

        // interpolate up to fine patch
        map->interp(crse,0,dest,dest_comp,ncomp,int_region,
                    crse_ratio,crse_geom,geom,bc_crse);
      }
    }

//cout << "]]=========== about to linInterp:" << NL;
//cout << "]]=========== tempCoarseDestFab.box() = " << dest.box() << NL;

    // copy from data on this level
    state[state_indx].linInterp(dest,dest.box(),time,src_comp,dest_comp,ncomp);

    // do non-periodic BC's on this level
    if (!inside) {
        const Real* dx = geom.CellSize();
        state[state_indx].FillBoundary(dest,time,dx,prob_domain,
                                   dest_comp,src_comp,ncomp);
    }

/*
static int fabcount = 0;
char fabsoutchar[256];
cout << "]]]]]]]]]]] after FillBoundary:  currentFillPatchedFab.minmax(0) = " << des
t.min(0) << "  " << dest.max(0) << NL;
ostrstream fabsout(fabsoutchar, sizeof(fabsoutchar)); fabsout << "currentFillPatched
Fab.lev" << this->Level() << ".nStep" << this->nStep() << "." << fabcount << ".fab"
<< ends;
cout << "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]] writing:  " << fabsoutchar << NL;
cout << "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]] fabcount = " << fabcount << NL;
//if(fabcount > 16) {
  //Real *dp = currentFillPatchedFab.dataPtr();
  //cout << "_here 0" << NL;
//}
++fabcount;
ofstream fabout(fabsout.str());
dest.writeOn(fabout);
fabout.close();
*/

//cout << "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]] _out old FillPatch" << NL;
//cout << NL;
}



// -------------------------------------------------------------
void
AmrLevel::FillPatch(FArrayBox &dest, int dest_comp, Real time,
                   int state_indx, int src_comp, int ncomp,
                   const Box& unfilled_region,
                   Interpolater *mapper)
{
    int is_periodic = geom.isAnyPeriodic();

    if (is_periodic) {
       cerr << "AmrLevel::FillPatch(BoxA..) not implemented for periodic\n";
       exit(1);
    }

    if(ParallelDescriptor::NProcs() > 1) {
      ParallelDescriptor::Abort("FillPatch(..., Box) not implemented in parallel");
    } else {
      cerr << "FillPatch(..., Box) not implemented in parallel\n";
    }


    // this can only be called from the top level.
    // In this version we never fill outside physical domain

    dest.setVal(1.e30);
    Box dbox(dest.box());

    int nv = dest.nComp();
    int ndesc = desc_lst.length();

    assert( (0<=state_indx) && (state_indx<ndesc) );
    const StateDescriptor &desc = desc_lst[state_indx];

    assert( dbox.ixType() == desc.getType() );
    assert( desc.inRange(src_comp, ncomp) );
    assert( ncomp <= (nv-dest_comp) );

    const RealBox& prob_domain = geom.ProbDomain();

    const Box& p_domain = state[state_indx].getDomain();

      // does grid intersect domain exterior?
    int inside = p_domain.contains(dbox);

      // Intersect with problem domain at this level
    dbox &= p_domain;

    if (unfilled_region.ok()) {
          // must fill on this region on crse level and interpolate
        if (level == 0) {
            BoxLib::Error("FillPatch: unfilled region at level 0");
        }

        Interpolater *map = mapper;
        if (map == 0) map = desc.interp();

          // coarsen unfilled region and widen if necessary
        Box crse_reg(map->CoarseBox(unfilled_region,crse_ratio));

          // alloc patch for crse level
        FArrayBox crse(crse_reg,ncomp);

          // fill patch at lower level
        AmrLevel &crse_lev = parent->getLevel(level-1);
        crse_lev.FillPatch(crse,0,time,state_indx,src_comp,ncomp,mapper);

      // get BndryTypes for interpolation
        Array<BCRec> bc_crse(ncomp);
        setBC(unfilled_region,p_domain,src_comp,0,ncomp,
              desc.getBCs(),bc_crse);

          // interpolate up to fine patch
        const Geometry& crse_geom = crse_lev.geom;
        map->interp(crse,0,dest,dest_comp,ncomp,unfilled_region,
                    crse_ratio,crse_geom,geom,bc_crse);

    }

    state[state_indx].linInterp(dest,dest.box(),time,
                                src_comp,dest_comp,ncomp);

    if (!inside) {
        const Real* dx = geom.CellSize();
        state[state_indx].FillBoundary(dest,time,dx,prob_domain,
                                   dest_comp,src_comp,ncomp);
    }
}  // end old FillPatch 2


#endif



// -------------------------------------------------------------
// -------------------------------------------------------------
// -------------------------------------------------------------
// -------------------------------------------------------------
// old filPatch with periodic  vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// old FillPatch with periodic vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
/*
void
AmrLevel::filPatch(FArrayBox &dest, int dest_comp, Real time,
		   int state_indx, int src_comp, int ncomp,
		   Interpolater *mapper)
{
    dest.setVal(1.e30);
    Box dbox(dest.box());
    Box truncdbox(dbox);

    int nv = dest.nComp();
    int ndesc = desc_lst.length();

    assert( (0<=state_indx) && (state_indx<ndesc) );
    const StateDescriptor &desc = desc_lst[state_indx];

    assert( dbox.ixType() == desc.getType() );
    assert( desc.inRange(src_comp, ncomp) );
    assert( ncomp <= (nv-dest_comp) );

    const RealBox& prob_domain = geom.ProbDomain();
    int is_periodic = geom.isAnyPeriodic();

    const Box& p_domain = state[state_indx].getDomain();

      // does grid intersect domain exterior?
    int inside = p_domain.contains(dbox);

      // Intersect with problem domain at this level
    if( ! inside ) truncdbox &= p_domain;

      // create a FIDIL domain to keep track of what can be filled
      // at this level
    // step 1: make BoxDomain of everything needing filling
    Box unfilled_region;
    BoxDomain fd(dbox.ixType());
    fd.add(truncdbox);
    if( (!inside) && is_periodic ){
      Array<IntVect> pshifts(27);
      geom.periodicShift( p_domain, dbox, pshifts );
      for( int iiv=0; iiv<pshifts.length(); iiv++ ){
	IntVect iv = pshifts[iiv];
	Box shbox(dbox);
	D_TERM( shbox.shift(0,iv[0]);,
		shbox.shift(1,iv[1]);,
		shbox.shift(2,iv[2]); )
	shbox &= p_domain;
	if( shbox.ok() ){
	  fd.add(shbox);
	}
      }
    }
    // making an enclosing box can speed up with fast check, avoid rmBox
    // where not required
    Box enclosing_box = fd.minimalBox();
    // step 2: take away stuff that can be gotten from this level of refinement
    const BoxArray &grds = state[state_indx].boxArray();
    int i;
    for (i = 0; i < grds.length(); i++) {
	const Box& gbox = grds[i];
	if (enclosing_box.intersects(gbox)) fd.rmBox(gbox);
    }
    // step 3: take minimal box containing what's left
    // THIS NEEDS IMPROVEMENT -> MAY BE INEFFICIENT
    unfilled_region = fd.minimalBox();
    fd.clear();

    FArrayBox crse;
    if (unfilled_region.ok()) {
      AmrLevel &crse_lev = parent->getLevel(level-1);
      const Geometry& crse_geom = crse_lev.geom;
      // must fill on this region on crse level and interpolate
      assert( level != 0 );
      
      Interpolater *map = mapper;
      if (map == 0) map = desc.interp();
      
      // intersect unfilled_region with the domain, this is necessary
      // because the unfilled_region may contain stuff carried by periodic 
      // BC to be outside the domain of dest
      Box int_region = unfilled_region & dbox;
      if( int_region.ok() ){
	// coarsen unfilled region and widen if necessary
	Box crse_reg(map->CoarseBox(int_region,crse_ratio));
	
	// alloc patch for crse level
	crse.resize(crse_reg,ncomp);
	
	// fill patch at lower level
	crse_lev.filPatch(crse,0,time,state_indx,src_comp,ncomp,mapper);
	
	// get bndry conditions for this patch
	Array<BCRec> bc_crse(ncomp);
	setBC(int_region,p_domain,src_comp,0,ncomp,
	      desc.getBCs(),bc_crse);
	
	// interpolate up to fine patch
	map->interp(crse,0,dest,dest_comp,ncomp,int_region,
		    crse_ratio,crse_geom,geom,bc_crse);
      }
      // if periodic, copy into periodic translates of dest
      if( (!inside) && is_periodic ){
	Array<IntVect> pshifts(27);
	geom.periodicShift( p_domain, dest.box(), pshifts);
	for( int iiv=0; iiv<pshifts.length(); iiv++ ){
	  IntVect iv = pshifts[iiv];
	  dest.shift(iv);
	  int_region = unfilled_region & dest.box();
	  if( int_region.ok() ){
	    Box crse_reg(map->CoarseBox(int_region,crse_ratio));
	    crse.resize(crse_reg,ncomp);
	    crse_lev.filPatch(crse,0,time,state_indx,src_comp,
			      ncomp,mapper);
	    Array<BCRec> bc_crse(ncomp);
	    setBC(int_region,p_domain,src_comp,0,ncomp,
		  desc.getBCs(),bc_crse);

	    map->interp(crse,0,dest,dest_comp,ncomp,int_region,
			crse_ratio,crse_geom,geom,bc_crse);
	  }
	  dest.shift(-iv);
	}
      }
    }

    // copy from data on this level
    state[state_indx].linInterp(dest,dest.box(),time,src_comp,dest_comp,ncomp);
    // if periodic, copy into periodic translates of dest
    if( (!inside) && is_periodic ){
      Array<IntVect> pshifts(27);
      geom.periodicShift( p_domain, dest.box(), pshifts);
      for( int iiv=0; iiv<pshifts.length(); iiv++){
	IntVect iv=pshifts[iiv];
	dest.shift(iv);
	if( dest.box().intersects(p_domain) ){
	  state[state_indx].linInterp(dest,dest.box(),time,
				      src_comp,dest_comp,ncomp);
	}
	dest.shift(-iv);
      }
    }

    // do non-periodic BC's on this level
    if (!inside) {
	const Real* dx = geom.CellSize();
	state[state_indx].FillBoundary(dest,time,dx,prob_domain,
				   dest_comp,src_comp,ncomp);
    }
}
*/
// -------------------------------------------------------------
// -------------------------------------------------------------
// -------------------------------------------------------------
// -------------------------------------------------------------






// -------------------------------------------------------------
const Box &FillPatchIterator::UngrownBox() const {
  return validbox();
}


// -------------------------------------------------------------
void
AmrLevel::FillCoarsePatch(FArrayBox &dest,
                          int dest_comp, Real time,
                          int state_indx, int src_comp, int ncomp,
                          Interpolater *mapper)
{
    ParallelDescriptor::Abort("FillCoarsePatch not implemented in parallel.");

      // must fill this region on crse level and interpolate
    assert(level != 0);
    assert( ncomp <= (dest.nComp() - dest_comp) );
    assert( (0 <= state_indx) && (state_indx < desc_lst.length()) );

    Box dbox(dest.box());
    const StateDescriptor &desc = desc_lst[state_indx];

    assert( dbox.ixType() == desc.getType() );
    assert( desc.inRange(src_comp, ncomp) );

    const RealBox &prob_domain = geom.ProbDomain();
    const Box &p_domain = state[state_indx].getDomain();

    int inside = p_domain.contains(dbox);   // does grid intersect domain exterior?

    dbox &= p_domain;                // Intersect with problem domain at this level

    Interpolater *map = mapper;
    if(map == 0) map = desc.interp();

      // coarsen unfilled region and widen by interpolater stencil width
    Box crse_reg(map->CoarseBox(dbox,crse_ratio));

    FArrayBox crse(crse_reg,ncomp);                   // alloc patch for crse level

      // fill patch at lower level
    AmrLevel &crse_lev = parent->getLevel(level-1);
    crse_lev.FillPatch(crse,0,time,state_indx,src_comp,ncomp,mapper);

      // get bndry conditions for this patch
    Array<BCRec> bc_crse(ncomp);
    setBC(dbox,p_domain,src_comp,0,ncomp,desc.getBCs(),bc_crse);

      // interpolate up to fine patch
    const Geometry &crse_geom = crse_lev.geom;
    map->interp(crse,0,dest,dest_comp,ncomp,dbox,
                crse_ratio,crse_geom,geom,bc_crse);

    if( ! inside) {
        const Real *dx = geom.CellSize();
        state[state_indx].FillBoundary(dest,time,dx,prob_domain,
                                   dest_comp,src_comp,ncomp);
    }
}


// -------------------------------------------------------------
void
AmrLevel::FillCoarsePatch(MultiFab &mfdest,
		          int dest_comp, Real time,
		          int state_indx, int src_comp, int ncomp,
		          Interpolater *mapper)
{
  // must fill this region on crse level and interpolate
  assert(level != 0);
  assert( ncomp <= (mfdest.nComp() - dest_comp) );
  assert( (0 <= state_indx) && (state_indx < desc_lst.length()) );

  const StateDescriptor &desc = desc_lst[state_indx];
  assert( desc.inRange(src_comp, ncomp) );
  Interpolater *map = mapper;
  if(map == 0) {
    map = desc.interp();
  }
  const RealBox &prob_domain = geom.ProbDomain();
  const Box &p_domain = state[state_indx].getDomain();
  AmrLevel &crse_lev = parent->getLevel(level-1);

  // build a properly coarsened boxarray
  BoxArray mfdestBoxArray(mfdest.boxArray());
  BoxArray crse_regBoxArray(mfdestBoxArray.length());
  for(int ibox = 0; ibox < mfdestBoxArray.length(); ++ibox) {
    //assert(mfdest.fabbox(ibox) == mfdest[ibox].box());
    Box dbox(mfdest.fabbox(ibox));
    assert( dbox.ixType() == desc.getType() );

    dbox &= p_domain;                // Intersect with problem domain at this level

    // coarsen unfilled region and widen by interpolater stencil width
    Box crse_reg(map->CoarseBox(dbox,crse_ratio));
    crse_regBoxArray.set(ibox, crse_reg);
  }

  int boxGrow = 0;
  MultiFab mf_crse_reg(crse_regBoxArray, ncomp, boxGrow);

  for( FillPatchIterator fpi(crse_lev, mf_crse_reg, boxGrow, dest_comp,
                             time, state_indx, src_comp, ncomp, mapper);
                         fpi.isValid();
                         ++fpi)
  {
    DependentMultiFabIterator mfdest_mfi(fpi, mfdest);
    assert(mfdest_mfi.fabbox() == mfdest_mfi().box());
    Box dbox(mfdest_mfi().box());
    dbox &= p_domain;                // Intersect with problem domain at this level

    FArrayBox &crse = fpi();
    FArrayBox &dest = mfdest_mfi();

      // get bndry conditions for this patch
    Array<BCRec> bc_crse(ncomp);
    setBC(dbox,p_domain,src_comp,0,ncomp,desc.getBCs(),bc_crse);

      // interpolate up to fine patch
    const Geometry &crse_geom = crse_lev.geom;
    map->interp(crse,0,dest,dest_comp,ncomp,dbox,
		crse_ratio,crse_geom,geom,bc_crse);

    if( ! p_domain.contains(dbox)) 
    {
	const Real *dx = geom.CellSize();
	state[state_indx].FillBoundary(dest,time,dx,prob_domain,
				       dest_comp,src_comp,ncomp);
    }
  }
}  // end FillCoarsePatch(...)


// -------------------------------------------------------------
PArray<FArrayBox>*
AmrLevel::derive(const aString &name, Real time)
{
    if(ParallelDescriptor::NProcs() > 1) {
      cerr << "PArray *AmrLevel::derive(...) not implemented in parallel." << NL;
      ParallelDescriptor::Abort("Exiting.");
    } else {
      cerr << "AmrLevel::derive(returning PArray *) not implemented in parallel." << NL;
    }
    int state_indx, src_comp;
    if (isStateVariable(name,state_indx,src_comp)) {
	const StateDescriptor &desc = desc_lst[state_indx];
	int nc = desc.nComp();
	const BoxArray& grds = state[state_indx].boxArray();
	PArray<FArrayBox> *df = new PArray<FArrayBox>(grids.length(),
						      PArrayManage);
        int i;
	for (i = 0; i < grds.length(); i++) {
	    FArrayBox *dest = new FArrayBox(grds[i],1);
	    state[state_indx].linInterp(*dest,grds[i],time,src_comp,0,1);
	    df->set(i,dest);
	}
	return df;
    }

      // can quantity be derived?
    const DeriveRec* d;
    if (d = derive_lst.get(name)) {
	PArray<FArrayBox> *df = new PArray<FArrayBox>(grids.length(),
						      PArrayManage);

	const Real* dx = geom.CellSize();
	int state_indx, src_comp, num_comp;
	d->getRange(0,state_indx,src_comp,num_comp);
	const BoxArray& grds = state[state_indx].boxArray();
	int n_state = d->numState();
	int n_der = d->numDerive();
	int nsr = d->numRange();
	IndexType der_typ = d->deriveType();
	  // can do special fill
        int i;
	for (i = 0; i < grds.length(); i++) {
	      // build destination FAB and install
	    Box dbox(grids[i]);
	    dbox.convert(der_typ);
	    FArrayBox *dest = new FArrayBox(dbox,n_der);
	    df->set(i,dest);

	      // build src fab and fill with component state data
	    Box sbox(d->boxMap()(dbox));
	    FArrayBox src(sbox,n_state);
	    int dc = 0;
            int k;
	    for (k = 0; k < nsr; k++) {
		d->getRange(k,state_indx,src_comp,num_comp); 
                const StateDescriptor &desc = desc_lst[state_indx];
		if (grds[i].contains(sbox)) {
		      // can do copy
		    state[state_indx].linInterp(src,sbox,time,
						src_comp,dc,num_comp);
		} else {
		      // must filpatch
		    FillPatch(src,dc,time,state_indx,src_comp,num_comp);
		}
		dc += num_comp;
	    }

	      // call deriving function
	    Real *ddat = dest->dataPtr();
	    const int* dlo = dest->loVect();
	    const int* dhi = dest->hiVect();
	    Real *cdat = src.dataPtr();
	    const int* clo = src.loVect();
	    const int* chi = src.hiVect();
	    const int* dom_lo = state[state_indx].getDomain().loVect();
	    const int* dom_hi = state[state_indx].getDomain().hiVect();
	    const int* bcr = d->getBC();
	    const Real* xlo = grid_loc[i].lo();
	    d->derFunc()(ddat,ARLIM(dlo),ARLIM(dhi),&n_der,
                         cdat,ARLIM(clo),ARLIM(chi),&n_state,
			 dlo,dhi,dom_lo,dom_hi,dx,xlo,&time,bcr,
                         &level,&i);
	}
	return df;
    }

      // if we got here, cannot derive given name
    cerr << "AmrLevel::derive (PArray): unknown variable: " << name << NL;
    ParallelDescriptor::Abort("Exiting.");

    // Just to keep the compiler happy
    return 0;
}


// -------------------------------------------------------------
FArrayBox*
AmrLevel::derive(const Box& b, const aString &name, Real time)
{

if(ParallelDescriptor::NProcs() > 1) {
  cerr << "AmrLevel::derive(box, name, time) not implemented in parallel." << NL;
  ParallelDescriptor::Abort("Exiting.");
} else {
  cerr << "AmrLevel::derive(box, name, time) not implemented in parallel." << NL;
}

    int state_indx, src_comp;

      // is it a state variable?
    if (isStateVariable(name,state_indx,src_comp)) {
	FArrayBox *dest = new FArrayBox(b,1);
	FillPatch(*dest,0,time,state_indx,src_comp,1);
	return dest;
    }

    const DeriveRec* d;
    if (d = derive_lst.get(name)) {

        cerr << "AmrLevel::derive:  about to FillDerive on var =  " << name << NL;
	cerr << "FillDerive not implemented in parallel.\n";
        if(ParallelDescriptor::NProcs() > 1) {
          ParallelDescriptor::Abort("Exiting.");
	}

	FArrayBox *dest = new FArrayBox(b,d->numDerive());
	FillDerive(*dest,b,name,time);
	return dest;
    }

      // if we got here, cannot derive given name
    cerr << "AmrLevel::derive (Fab): unknown variable: " << name << NL;
    ParallelDescriptor::Abort("Exiting.");

    // Just to keep the compiler happy
    return 0;
}

// -------------------------------------------------------------
void
AmrLevel::FillDerive(FArrayBox &dest, const Box& subbox,
		     const aString &name, Real time)
{
    const DeriveRec* d = derive_lst.get(name);
    assert (d != 0);

      // get domain and BoxArray
    IndexType der_typ = d->deriveType();
    int n_der = d->numDerive();
    int cell_centered = der_typ.cellCentered();

    Box dom(geom.Domain());
    if (!cell_centered) dom.convert(der_typ);

      // only fill portion on interior of domain
    Box dbox(subbox);
    dbox &= dom;

      // create a FIDIL domain to keep track of what can be filled
      // at this level
    Box unfilled_region;
    BoxDomain fd(dbox.ixType());
    fd.add(dbox);
    int i;
    for (i = 0; i < grids.length(); i++) {
	Box gbox(grids[i]);
	if (!cell_centered) gbox.convert(der_typ);
	if (dbox.intersects(gbox)) fd.rmBox(gbox);
    }
    unfilled_region = fd.minimalBox();
    fd.clear();

    if (unfilled_region.ok()) {
	  // must fill on this region on crse level and interpolate
	if (level == 0) {
	    cerr << "FillPatch: unfilled region at level 0\n";
	    ParallelDescriptor::Abort("Exiting.");
	}

	  // coarsen unfilled region and widen if necessary
	Interpolater *mapper = d->interp();
	Box crse_reg(mapper->CoarseBox(unfilled_region,crse_ratio));

	  // alloc patch for crse level
	FArrayBox crse(crse_reg,n_der);

	  // fill patch at lower level
	AmrLevel &crse_lev = parent->getLevel(level-1);
	crse_lev.FillDerive(crse,crse_reg,name,time);

	  // get bncry conditions for this patch
	Array<BCRec> bc_crse;

	  // interpolate up to fine patch
	const Geometry& crse_geom = crse_lev.geom;
	mapper->interp(crse,0,dest,0,n_der,unfilled_region,
		       crse_ratio,crse_geom,geom,bc_crse);
    }

      // walk through grids, deriving on intersect
    int n_state = d->numState();
    int nsr = d->numRange();
    const Real* dx = geom.CellSize();
    for (i = 0; i < grids.length(); i++) {
	Box g(grids[i]);
	if (!cell_centered) g.convert(der_typ);
	g &= dbox;
	if (g.ok()) {
	    Box sbox(d->boxMap()(g));
	    FArrayBox src(sbox,n_state);
	    int dc = 0;
	    int state_indx, sc, nc;
	    for(int k = 0; k < nsr; k++) {
		d->getRange(k,state_indx,sc,nc);
		const Box& sg = state[state_indx].boxArray()[i];
		if (sg.contains(sbox)) {
		      // can do copy
		    state[state_indx].linInterp(src,sbox,time,sc,dc,nc);
		} else {
		      // must filpatch
		    FillPatch(src,dc,time,state_indx,sc,nc);
		}
		dc += nc;
	    }

	      // now derive
	    Real *ddat = dest.dataPtr();
	    const int* dlo = dest.loVect();
	    const int* dhi = dest.hiVect();
	    const int* lo = g.loVect();
	    const int* hi = g.hiVect();
	    Real *cdat = src.dataPtr();
	    const int* clo = src.loVect();
	    const int* chi = src.hiVect();
	    const int* dom_lo = dom.loVect();
	    const int* dom_hi = dom.hiVect();
	    const int* bcr = d->getBC();
	    Real xlo[BL_SPACEDIM];
	    geom.LoNode(dest.smallEnd(),xlo);
	    d->derFunc()(ddat,ARLIM(dlo),ARLIM(dhi),&n_der,
                         cdat,ARLIM(clo),ARLIM(chi),&n_state,
			 lo,hi,dom_lo,dom_hi,dx,xlo,&time,bcr,
                         &level,&i);
	}
    }
}

// ---------------------------------------------------------------
int* 
AmrLevel::getBCArray(int State_Type, int gridno, int strt_comp, int num_comp)
{
    int* bc = new int[2*BL_SPACEDIM*num_comp];
    assert(bc);
    int* b = bc;
    int n;
    for (n = 0; n < num_comp; n++) {
	const int* b_rec = 
	state[State_Type].getBC(strt_comp+n,gridno).vect();
        int m;
	for (m = 0; m < 2*BL_SPACEDIM; m++) *b++ = b_rec[m];
    }
    return bc;
}


// -------------------------------------------------------------

void
AmrLevel::probe(ostream &os, IntVect iv, int rad, Real time,
		int state_indx, int src_comp, int num_comp)
{
    Box bx(iv,iv);
    bx.grow(rad);
    FArrayBox fab(bx,num_comp);
    FillPatch(fab,0,time,state_indx,src_comp,num_comp);
    os << "\n >>>> PROBE of:";
    const StateDescriptor &desc = desc_lst[state_indx];
    desc.dumpNames(os,src_comp,num_comp);
    os << NL;
    IntVect lo = bx.smallEnd();
    IntVect hi = bx.bigEnd();
    IntVect point;
    char buf[80];
    for( point=lo; point <= hi; bx.next(point) ){
	os << point;
        int k;
	for (k = 0; k < num_comp; k++) {
	    sprintf(buf,"    %20.14f",fab(point,k));
	    os << buf;
	}
	os << NL;
    }
    os << "\n----------------------------------------------------\n";
}
