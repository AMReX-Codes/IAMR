// #define ADVANCE_DEBUG 1

#include <string.h>

#include <Assert.H>
#include <AmrLevel.H>
#include <Derive.H>
#include <BoxDomain.H>
#include <ParallelDescriptor.H>
#ifdef	_MSC_VER
#include <strstrea.h>
#else
#include <strstream.h>
#endif

// -------------------------------------------------------------
// static data initialization
DescriptorList  AmrLevel::desc_lst;
DeriveList      AmrLevel::derive_lst;
// -------------------------------------------------------------

// -------------------------------------------------------------
AmrLevel::AmrLevel() {
   parent = 0;
   level = -1;
}

// -------------------------------------------------------------
AmrLevel::AmrLevel(Amr &papa, int lev, const Geometry &level_geom,
                   const BoxArray& ba, REAL time)
    : geom(level_geom),grids(ba)
{
    level = lev;
    parent = &papa;

    fine_ratio = IntVect::TheUnitVector(); fine_ratio.scale(-1);
    crse_ratio = IntVect::TheUnitVector(); crse_ratio.scale(-1);

    if (level > 0) crse_ratio = parent->refRatio(level-1);
    if (level < parent->maxLevel()) fine_ratio = parent->refRatio(level);

    REAL dt = parent->dtLevel(lev);
    int ndesc = desc_lst.length();
    state.resize(ndesc);
    const BOX& domain = geom.Domain();
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
    const REAL* prob_lo = geom.ProbLo();
    const REAL* dx = geom.CellSize();
    int i;
    for (i = 0; i < num_grids; i++) {
	grid_loc[i] = REALBOX(grids[i],dx,prob_lo);
    }
}

// -------------------------------------------------------------
void
AmrLevel::setTimeLevel(REAL time, REAL dt)
{
    int ndesc = desc_lst.length();
    int k;
    for (k = 0; k < ndesc; k++)
	state[k].setTimeLevel(time,dt);
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
  if(ParallelDescriptor::IOProcessor()) {
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
AmrLevel::get_data(int state_indx, REAL time)
{
    REAL old_time = state[state_indx].prevTime();
    REAL new_time = state[state_indx].curTime();
    REAL eps = 0.001*(new_time - old_time);
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
			     REAL time)
{
    REAL old_time = state[state_indx].prevTime();
    REAL new_time = state[state_indx].curTime();
    REAL eps = 0.001*(new_time - old_time);
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
bool FillPatchIterator::isValid() {

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
  //currentFillPatchedFab.setVal(1.e30);

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
    const REALBOX &realProbDomain = amrLevel.geom.ProbDomain();

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
cout << "dest.box() = " << localMFBoxes[ibox] << endl;
    unfilledBoxOnThisLevel = localMFBoxes[ibox] &
                            amrLevels[amrLevel.level].state[stateIndex].getDomain();
    assert(unfilledBoxOnThisLevel.ok());
    bool needsFilling = true;

    // ________________________________________ level loop
    for(currentLevel = amrLevel.level; currentLevel >= 0 && needsFilling;
        --currentLevel)
    {
cout << endl << "currentLevel = " << currentLevel << endl;
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
cout << "currentLevel = " << currentLevel << endl;
cout << "mapped->CoarseBox = " << tempCoarseBox << "  from box " << unfilledBoxOnThi
sLevel << endl;
          }
cout << "about to linInterp with tempCoarseBox = " << tempCoarseBox << endl;


          currentState.linInterpAddBox(multiFabCopyDesc,
                               stateDataMFId[currentLevel],
                               unfillableBoxesOnThisLevel,
                               fillBoxId[ibox][currentLevel][currentBLI],
                               tempCoarseBox,
                               interpTime, srcComp, destComp, nComp);

cout << "      fillBoxId.box[]  = " << fillBoxId[ibox][currentLevel][currentBLI][0].
box()  << endl;

      unfillableBoxesOnThisLevel.intersect(currentPDomain);
      unfilledBoxOnThisLevel = unfillableBoxesOnThisLevel.minimalBox();
      unfilledBoxOnThisLevel &= currentPDomain;

      if(unfilledBoxOnThisLevel.ok()) {
cout << "unfilled box on level " << currentLevel << " = " << unfilledBoxOnThisLevel
<< endl;
      } else {
        needsFilling = false;
      }
    }  // end for(currentLevel...)
  }  // end for(ibox...)

  multiFabCopyDesc.CollectData();

}  // end FillPatchIterator(...)


// -------------------------------------------------------------
bool FillPatchIterator::isValid() {

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
//cout << endl;
//cout << "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]] _in isValid()" << endl;

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
  cout << "coarsestFillLevel = " << coarsestFillLevel << endl;
  assert(coarsestFillLevel >= 0 && coarsestFillLevel <= amrLevel.level);

  for(currentLevel = coarsestFillLevel; currentLevel < amrLevel.level;
      ++currentLevel)
  {
    if(fillBoxId[currentIndex][currentLevel].length() == 0) {
      continue;
    }
    assert(fillBoxId[currentIndex][currentLevel].length() == 1);
cout << "currentLevel     = " << currentLevel     << endl;

int ivRefRatio = 2;
    //int fineLevel = currentLevel + 1;

    int currentBox = 0;
      StateData &currentState = amrLevels[currentLevel].state[stateIndex];
      Box tempCoarseBox(fillBoxId[currentIndex][currentLevel][currentBox][0].box());

    if(currentLevel == coarsestFillLevel) {
      assert(tempCoarseBox.ok());
cout << "Resizing coarse fab to " << tempCoarseBox << endl;
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
cout << "linInterp on coarse.box() = " << coarseDestFabPtr->box() << endl;
      currentState.linInterpFillFab(multiFabCopyDesc,
                            stateDataMFId[currentLevel],
                            fillBoxId[currentIndex][currentLevel][currentBox],
                            *coarseDestFabPtr,
                            interpTime, srcComp, destComp, nComp);

    const Real *dx = amrLevels[currentLevel].geom.CellSize();
    const REALBOX &realProbDomain = amrLevels[currentLevel].geom.ProbDomain();

/* FillBoundary */
cout << "FillBoundary on coarse.box() = " << coarseDestFabPtr->box() << "  outside b
oundary " << realProbDomain << endl;
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
 " << fineDestFabPtr->box() << "  on int_region = " << intersectDestBox << endl;
cout << "crse0_geom = " << amrLevels[currentLevel].geom << endl;
cout << "crse_geom  = " << amrLevels[currentLevel + 1].geom  << endl;
for(int ibc=0; ibc < nComp; ++ibc) {
  cout << "bc_crse[" << ibc << "] = " << bcCoarse[ibc] << endl;
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
 endl;
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
    const REALBOX &realProbDomain = amrLevel.geom.ProbDomain();

/* FillBoundary */
cout << "FillBoundary on dest.box() = " << currentFillPatchedFab.box() << "  outside
 boundary " << realProbDomain << endl;
    currentState.FillBoundary(currentFillPatchedFab, interpTime, dx,
                              realProbDomain,
                              destComp, srcComp, nComp);
  }

//cout << "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]] _out isValid()" << endl;
//cout << endl;
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

void AmrLevel::FillPatch(FARRAYBOX &dest, int dest_comp, REAL time,
                         int stateIndex, int src_comp, int ncomp,
                         Interpolater *mapper)
{
cout << endl;
cout << "_in old FillPatch" << endl;
cout << "dest.box() = " << dest.box() << endl;
    dest.setVal(1.e30);
    BOX dbox(dest.box());
    BOX truncdbox(dbox);
    const StateDescriptor &desc = desc_lst[stateIndex];
    const REALBOX &prob_domain = geom.ProbDomain();
    const BOX &p_domain = state[stateIndex].getDomain();

    int inside = p_domain.contains(dbox); // does grid intersect boundary
    if( ! inside ) truncdbox &= p_domain;

    BOX unfilled_region;
    BoxDomain fd(dbox.ixType());
    fd.add(truncdbox);

    Box enclosing_box = fd.minimalBox();
    const BoxArray &grds = state[stateIndex].boxArray();
    int i;
    for(i = 0; i < grds.length(); i++) {
        const BOX& gbox = grds[i];
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
      BOX int_region = unfilled_region & dbox;
      if( int_region.ok() ) {
cout << "unfilled box on level " << level << " = " << int_region << endl;
        // coarsen unfilled region and widen if necessary
        BOX crse_reg(map->CoarseBox(int_region,crse_ratio));
cout << "mapped->CoarseBox = " << crse_reg << endl;
cout << "Resizing crse fab to " << crse_reg << endl;
        crse.resize(crse_reg,ncomp);         // alloc patch for crse level

        //crse_lev.filPatch(crse,0,time,stateIndex,src_comp,ncomp,mapper);
        // ****************************************************************
        crse.setVal(1.e30);
        BOX crse_dbox(crse.box());
        BOX crse_truncdbox(crse_dbox);
        const REALBOX &crse_prob_domain = crse_geom.ProbDomain();
        const BOX &crse_p_domain = crse_lev.state[stateIndex].getDomain();
        int crse_inside = crse_p_domain.contains(crse_dbox);

        if( ! inside ) crse_truncdbox &= crse_p_domain;

        BOX crse_unfilled_region;
        BoxDomain crse_fd(crse_dbox.ixType());
        crse_fd.add(crse_truncdbox);

        Box crse_enclosing_box = crse_fd.minimalBox();
        const BoxArray &crse_grds = crse_lev.state[stateIndex].boxArray();
        for(i = 0; i < crse_grds.length(); i++) {
            const BOX& crse_gbox = crse_grds[i];
            if(crse_enclosing_box.intersects(crse_gbox)) crse_fd.rmBox(crse_gbox);
        }
        crse_unfilled_region = crse_fd.minimalBox();
        crse_fd.clear();

        FArrayBox crse0;
        if (crse_unfilled_region.ok()) {
          AmrLevel &crse0_lev = parent->getLevel(level-2);  // should be 0 here
          const Geometry& crse0_geom = crse0_lev.geom;
          // must fill on this region on crse0 level and interpolate

          BOX crse_int_region = crse_unfilled_region & crse_dbox;
          if( crse_int_region.ok() ){
cout << "unfilled box on level " << level-1 << " = " << crse_int_region << endl;
            // coarsen unfilled region and widen if necessary
            int crse0_ratio = 2;  // for testing
            BOX crse0_reg(map->CoarseBox(crse_int_region,crse0_ratio));
cout << "mapped->CoarseBox = " << crse0_reg << endl;
cout << "Resizing crse0 fab to " << crse0_reg << endl;
            crse0.resize(crse0_reg,ncomp);         // alloc patch for crse0 level

            //crse0_lev.filPatch(crse0,0,time,stateIndex,src_comp,ncomp,mapper);
            // ================================================================
            crse0.setVal(1.e30);
            BOX crse0_dbox(crse0.box());
            BOX crse0_truncdbox(crse0_dbox);

            const REALBOX &crse0_prob_domain = crse0_geom.ProbDomain();
            const BOX &crse0_p_domain = crse0_lev.state[stateIndex].getDomain();
            int crse0_inside = crse0_p_domain.contains(crse0_dbox);

            if( ! crse0_inside ) crse0_truncdbox &= crse0_p_domain;
cout << "linInterp on crse0.box() = " << crse0.box() << endl;
            crse0_lev.state[stateIndex].linInterp(crse0,crse0.box(),time,
                                                 src_comp,0,ncomp);
            if( ! crse0_inside) {
              const REAL* crse0_dx = crse0_geom.CellSize();
cout << "FillBoundary on crse0.box() = " << crse0.box() << "  outside boundary " <<
crse0_prob_domain << endl;
              crse0_lev.state[stateIndex].FillBoundary(crse0, time, crse0_dx,
                                      crse0_prob_domain, 0, src_comp, ncomp);
            }
            // ================================================================
            Array<BCRec> bc_crse(ncomp); // get bndry conditions for this patch
            setBC(crse_int_region,crse_p_domain,src_comp,0,
                     ncomp,desc.getBCs(),bc_crse);

            // interpolate up to fine patch
cout << "map->interp crse0.box() = " << crse0.box() << " to crse.box() = " << crse.b
ox() << " on crse_int_region = " << crse_int_region << endl;
cout << "crse0_geom = " << crse0_geom << endl;
cout << "crse_geom  = " << crse_geom  << endl;
for(int ibc=0; ibc < ncomp; ++ibc) {
  cout << "bc_crse[" << ibc << "] = " << bc_crse[ibc] << endl;
}
            map->interp(crse0,0,crse,dest_comp,ncomp,crse_int_region,
                        crse_ratio,crse0_geom,geom,bc_crse);
          }
        }

cout << "linInterp on crse.box() = " << crse.box() << endl;
        crse_lev.state[stateIndex].linInterp(crse,crse.box(),time,
                                             src_comp,dest_comp,ncomp);

        if( ! inside) {              // do non-periodic BC's on this level
          const REAL* crse_dx = crse_geom.CellSize();
cout << "FillBoundary on crse.box() = " << crse.box() << "  outside boundary " << cr
se_prob_domain << endl;
          crse_lev.state[stateIndex].FillBoundary(crse,time,crse_dx,
                                                  crse_prob_domain,
                                                  dest_comp,src_comp,ncomp);
        }
        // ****************************************************************
        Array<BCRec> bc_crse(ncomp); // get bndry conditions for this patch
        setBC(int_region,p_domain,src_comp,0,ncomp,desc.getBCs(),bc_crse);

        // interpolate up to fine patch
cout << "map->interp crse.box() = " << crse.box() << " to dest.box() = " << dest.box
() << " on int_region = " << int_region << endl;
cout << "crse_geom  = " << crse_geom  << endl;
cout << "geom       = " << geom       << endl;
for(int ibc=0; ibc < ncomp; ++ibc) {
  cout << "bc_crse[" << ibc << "] = " << bc_crse[ibc] << endl;
}
        map->interp(crse,0,dest,dest_comp,ncomp,int_region,
                    crse_ratio,crse_geom,geom,bc_crse);
      }
    }

  state[stateIndex].linInterp(dest,dest.box(),time,src_comp,dest_comp,ncomp);

  if( ! inside) {              // do non-periodic BC's on this level
    const REAL* dx = geom.CellSize();
cout << "FillBoundary on dest.box() = " << dest.box() << "  outside boundary " << pr
ob_domain << endl;
    state[stateIndex].FillBoundary(dest,time,dx,prob_domain,
                                   dest_comp,src_comp,ncomp);
  }
cout << "_out old FillPatch" << endl;
cout << endl;
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
AmrLevel::FillPatch(FARRAYBOX &dest, int dest_comp, REAL time,
                   int state_indx, int src_comp, int ncomp,
                   Interpolater *mapper)
{
//cout << endl;
//cout << "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]] _in old FillPatch" << endl;
//cout << "currentLevel     = " << this->Level() << endl;
    int is_periodic = geom.isAnyPeriodic();
    if(is_periodic) {
      ParallelDescriptor::Abort("This FillPatch does not do periodic");
    }
    if(ParallelDescriptor::NProcs() > 1) {
      ParallelDescriptor::Abort("FillPatch() not implemented in parallel");
    } else {
      cerr << "FillPatch() not implemented in parallel" << endl;
    }


    dest.setVal(1.e30);
    BOX dbox(dest.box());
//cout << "box to FillPatch = " << dest.box() << endl;
    BOX truncdbox(dbox);
    int nv = dest.nComp();
    int ndesc = desc_lst.length();
    const StateDescriptor &desc = desc_lst[state_indx];
    const REALBOX& prob_domain = geom.ProbDomain();
    const BOX& p_domain = state[state_indx].getDomain();

    int inside = p_domain.contains(dbox);
    if( ! inside ) truncdbox &= p_domain;

    BOX unfilled_region;
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

      BOX int_region = unfilled_region & dbox;
      if( int_region.ok() ){
        // coarsen unfilled region and widen if necessary
        BOX crse_reg(map->CoarseBox(int_region,crse_ratio));

        // alloc patch for crse level
        crse.resize(crse_reg,ncomp);

        // fill patch at lower level
        crse_lev.FillPatch(crse,0,time,state_indx,src_comp,ncomp,mapper);

        // get bndry conditions for this patch
        Array<BCRec> bc_crse(ncomp);
        setBC(int_region,p_domain,src_comp,0,ncomp,
              desc.getBCs(),bc_crse);

//cout << "]]----------- about to map->interp:" << endl;
//cout << "]]----------- crse.box()       = " << crse.box() << endl;
//cout << "]]----------- dest.box()       = " << dest.box() << endl;
//cout << "]]----------- intersectDestBox = " << int_region << endl;

        // interpolate up to fine patch
        map->interp(crse,0,dest,dest_comp,ncomp,int_region,
                    crse_ratio,crse_geom,geom,bc_crse);
      }
    }

//cout << "]]=========== about to linInterp:" << endl;
//cout << "]]=========== tempCoarseDestFab.box() = " << dest.box() << endl;

    // copy from data on this level
    state[state_indx].linInterp(dest,dest.box(),time,src_comp,dest_comp,ncomp);

    // do non-periodic BC's on this level
    if (!inside) {
        const REAL* dx = geom.CellSize();
        state[state_indx].FillBoundary(dest,time,dx,prob_domain,
                                   dest_comp,src_comp,ncomp);
    }

/*
static int fabcount = 0;
char fabsoutchar[256];
cout << "]]]]]]]]]]] after FillBoundary:  currentFillPatchedFab.minmax(0) = " << des
t.min(0) << "  " << dest.max(0) << endl;
ostrstream fabsout(fabsoutchar, sizeof(fabsoutchar)); fabsout << "currentFillPatched
Fab.lev" << this->Level() << ".nStep" << this->nStep() << "." << fabcount << ".fab"
<< ends;
cout << "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]] writing:  " << fabsoutchar << endl;
cout << "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]] fabcount = " << fabcount << endl;
//if(fabcount > 16) {
  //Real *dp = currentFillPatchedFab.dataPtr();
  //cout << "_here 0" << endl;
//}
++fabcount;
ofstream fabout(fabsout.str());
dest.writeOn(fabout);
fabout.close();
*/

//cout << "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]] _out old FillPatch" << endl;
//cout << endl;
}



// -------------------------------------------------------------
void
AmrLevel::FillPatch(FARRAYBOX &dest, int dest_comp, REAL time,
                   int state_indx, int src_comp, int ncomp,
                   const BOX& unfilled_region,
                   Interpolater *mapper)
{
    int is_periodic = geom.isAnyPeriodic();

    if (is_periodic) {
       cerr << "AmrLevel::FillPatch(BoxA..) not implemented yet for periodic"
            << endl;
       exit(1);
    }

    if(ParallelDescriptor::NProcs() > 1) {
      ParallelDescriptor::Abort("FillPatch(..., Box) not implemented in parallel");
    } else {
      cerr << "FillPatch(..., Box) not implemented in parallel" << endl;
    }


    // this can only be called from the top level.
    // In this version we never fill outside physical domain

    dest.setVal(1.e30);
    BOX dbox(dest.box());

    int nv = dest.nComp();
    int ndesc = desc_lst.length();

    assert( (0<=state_indx) && (state_indx<ndesc) );
    const StateDescriptor &desc = desc_lst[state_indx];

    assert( dbox.ixType() == desc.getType() );
    assert( desc.inRange(src_comp, ncomp) );
    assert( ncomp <= (nv-dest_comp) );

    const REALBOX& prob_domain = geom.ProbDomain();

    const BOX& p_domain = state[state_indx].getDomain();

      // does grid intersect domain exterior?
    int inside = p_domain.contains(dbox);

      // Intersect with problem domain at this level
    dbox &= p_domain;

    if (unfilled_region.ok()) {
          // must fill on this region on crse level and interpolate
        if (level == 0) {
            cerr << "FillPatch: unfilled region at level 0" << endl;
            abort();
        }

        Interpolater *map = mapper;
        if (map == 0) map = desc.interp();

          // coarsen unfilled region and widen if necessary
        BOX crse_reg(map->CoarseBox(unfilled_region,crse_ratio));

          // alloc patch for crse level
        FARRAYBOX crse(crse_reg,ncomp);

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
        const REAL* dx = geom.CellSize();
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
AmrLevel::filPatch(FARRAYBOX &dest, int dest_comp, REAL time,
		   int state_indx, int src_comp, int ncomp,
		   Interpolater *mapper)
{
    dest.setVal(1.e30);
    BOX dbox(dest.box());
    BOX truncdbox(dbox);

    int nv = dest.nComp();
    int ndesc = desc_lst.length();

    assert( (0<=state_indx) && (state_indx<ndesc) );
    const StateDescriptor &desc = desc_lst[state_indx];

    assert( dbox.ixType() == desc.getType() );
    assert( desc.inRange(src_comp, ncomp) );
    assert( ncomp <= (nv-dest_comp) );

    const REALBOX& prob_domain = geom.ProbDomain();
    int is_periodic = geom.isAnyPeriodic();

    const BOX& p_domain = state[state_indx].getDomain();

      // does grid intersect domain exterior?
    int inside = p_domain.contains(dbox);

      // Intersect with problem domain at this level
    if( ! inside ) truncdbox &= p_domain;

      // create a FIDIL domain to keep track of what can be filled
      // at this level
    // step 1: make BoxDomain of everything needing filling
    BOX unfilled_region;
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
	const BOX& gbox = grds[i];
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
      BOX int_region = unfilled_region & dbox;
      if( int_region.ok() ){
	// coarsen unfilled region and widen if necessary
	BOX crse_reg(map->CoarseBox(int_region,crse_ratio));
	
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
	const REAL* dx = geom.CellSize();
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
AmrLevel::FillCoarsePatch(FARRAYBOX &dest,
		          int dest_comp, REAL time,
		          int state_indx, int src_comp, int ncomp,
		          Interpolater *mapper)
{
      // must fill this region on crse level and interpolate
    if (level == 0) {
	cerr << "crsePatch: called at level 0" << endl;
	ParallelDescriptor::Abort("Exiting.");
    }
    if(ParallelDescriptor::NProcs() > 1) {
      ParallelDescriptor::Abort("FillCoarsePatch not implemented in parallel.");
    } else {
      cerr << "FillCoarsePatch not implemented in parallel." << endl;
    }

    BOX dbox(dest.box());

    int nv = dest.nComp();
    int ndesc = desc_lst.length();

    assert( (0<=state_indx) && (state_indx<ndesc) );
    const StateDescriptor &desc = desc_lst[state_indx];

    assert( dbox.ixType() == desc.getType() );
    assert( desc.inRange(src_comp, ncomp) );
    assert( ncomp <= (nv-dest_comp) );

    const REALBOX& prob_domain = geom.ProbDomain();

    const BOX& p_domain = state[state_indx].getDomain();

   // does grid intersect domain exterior?
    int inside = p_domain.contains(dbox);

      // Intersect with problem domain at this level
    dbox &= p_domain;

    Interpolater *map = mapper;
    if (map == 0) map = desc.interp();

      // coarsen unfilled region and widen by interpolater stencil width
    BOX crse_reg(map->CoarseBox(dbox,crse_ratio));

      // alloc patch for crse level
    FARRAYBOX crse(crse_reg,ncomp);

      // fill patch at lower level
    AmrLevel &crse_lev = parent->getLevel(level-1);
    crse_lev.FillPatch(crse,0,time,state_indx,src_comp,ncomp,mapper);

      // get bndry conditions for this patch
    Array<BCRec> bc_crse(ncomp);
    setBC(dbox,p_domain,src_comp,0,ncomp,desc.getBCs(),bc_crse);

      // interpolate up to fine patch
    const Geometry& crse_geom = crse_lev.geom;
    map->interp(crse,0,dest,dest_comp,ncomp,dbox,
		crse_ratio,crse_geom,geom,bc_crse);

    if (!inside) {
	const REAL* dx = geom.CellSize();
	state[state_indx].FillBoundary(dest,time,dx,prob_domain,
				   dest_comp,src_comp,ncomp);
    }
}

// -------------------------------------------------------------
PArray<FARRAYBOX>*
AmrLevel::derive(const aString &name, REAL time)
{
    if(ParallelDescriptor::NProcs() > 1) {
      cerr << "AmrLevel::derive(returning PArray *) not implemented in parallel." << endl;
      ParallelDescriptor::Abort("Exiting.");
    } else {
      cerr << "AmrLevel::derive(returning PArray *) not implemented in parallel." << endl;
    }
    int state_indx, src_comp;
    if (isStateVariable(name,state_indx,src_comp)) {
	const StateDescriptor &desc = desc_lst[state_indx];
	int nc = desc.nComp();
	const BoxArray& grds = state[state_indx].boxArray();
	PArray<FARRAYBOX> *df = new PArray<FARRAYBOX>(grids.length(),
						      PArrayManage);
        int i;
	for (i = 0; i < grds.length(); i++) {
	    FARRAYBOX *dest = new FARRAYBOX(grds[i],1);
	    state[state_indx].linInterp(*dest,grds[i],time,src_comp,0,1);
	    df->set(i,dest);
	}
	return df;
    }

      // can quantity be derived?
    const DeriveRec* d;
    if (d = derive_lst.get(name)) {
	PArray<FARRAYBOX> *df = new PArray<FARRAYBOX>(grids.length(),
						      PArrayManage);

	const REAL* dx = geom.CellSize();
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
	    BOX dbox(grids[i]);
	    dbox.convert(der_typ);
	    FARRAYBOX *dest = new FARRAYBOX(dbox,n_der);
	    df->set(i,dest);

	      // build src fab and fill with component state data
	    BOX sbox(d->boxMap()(dbox));
	    FARRAYBOX src(sbox,n_state);
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
	    REAL *ddat = dest->dataPtr();
	    const int* dlo = dest->loVect();
	    const int* dhi = dest->hiVect();
	    REAL *cdat = src.dataPtr();
	    const int* clo = src.loVect();
	    const int* chi = src.hiVect();
	    const int* dom_lo = state[state_indx].getDomain().loVect();
	    const int* dom_hi = state[state_indx].getDomain().hiVect();
	    const int* bcr = d->getBC();
	    const REAL* xlo = grid_loc[i].lo();
	    d->derFunc()(ddat,ARLIM(dlo),ARLIM(dhi),&n_der,
                         cdat,ARLIM(clo),ARLIM(chi),&n_state,
			 dlo,dhi,dom_lo,dom_hi,dx,xlo,&time,bcr,
                         &level,&i);
	}
	return df;
    }

      // if we got here, cannot derive given name
    cerr << "AmrLevel::derive (PArray): unknown variable: " << name << endl;
    ParallelDescriptor::Abort("Exiting.");

    // Just to keep the compiler happy
    return 0;
}


// -------------------------------------------------------------
FARRAYBOX*
AmrLevel::derive(const BOX& b, const aString &name, REAL time)
{

if(ParallelDescriptor::NProcs() > 1) {
  cerr << "AmrLevel::derive(box, name, time) not implemented in parallel." << endl;
  ParallelDescriptor::Abort("Exiting.");
} else {
  cerr << "AmrLevel::derive(box, name, time) not implemented in parallel." << endl;
}

    int state_indx, src_comp;

      // is it a state variable?
    if (isStateVariable(name,state_indx,src_comp)) {
	FARRAYBOX *dest = new FARRAYBOX(b,1);
	FillPatch(*dest,0,time,state_indx,src_comp,1);
	return dest;
    }

    const DeriveRec* d;
    if (d = derive_lst.get(name)) {

        cerr << "AmrLevel::derive:  about to FillDerive on var =  " << name << endl;
	cerr << "FillDerive not implemented in parallel." << endl;
        if(ParallelDescriptor::NProcs() > 1) {
          ParallelDescriptor::Abort("Exiting.");
	}

	FARRAYBOX *dest = new FARRAYBOX(b,d->numDerive());
	FillDerive(*dest,b,name,time);
	return dest;
    }

      // if we got here, cannot derive given name
    cerr << "AmrLevel::derive (Fab): unknown variable: " << name << endl;
    ParallelDescriptor::Abort("Exiting.");

    // Just to keep the compiler happy
    return 0;
}

// -------------------------------------------------------------
void
AmrLevel::FillDerive(FARRAYBOX &dest, const BOX& subbox,
		     const aString &name, REAL time)
{
    const DeriveRec* d = derive_lst.get(name);
    assert (d != 0);

      // get domain and BoxArray
    IndexType der_typ = d->deriveType();
    int n_der = d->numDerive();
    int cell_centered = der_typ.cellCentered();

    BOX dom(geom.Domain());
    if (!cell_centered) dom.convert(der_typ);

      // only fill portion on interior of domain
    BOX dbox(subbox);
    dbox &= dom;

      // create a FIDIL domain to keep track of what can be filled
      // at this level
    BOX unfilled_region;
    BoxDomain fd(dbox.ixType());
    fd.add(dbox);
    int i;
    for (i = 0; i < grids.length(); i++) {
	BOX gbox(grids[i]);
	if (!cell_centered) gbox.convert(der_typ);
	if (dbox.intersects(gbox)) fd.rmBox(gbox);
    }
    unfilled_region = fd.minimalBox();
    fd.clear();

    if (unfilled_region.ok()) {
	  // must fill on this region on crse level and interpolate
	if (level == 0) {
	    cerr << "FillPatch: unfilled region at level 0" << endl;
	    ParallelDescriptor::Abort("Exiting.");
	}

	  // coarsen unfilled region and widen if necessary
	Interpolater *mapper = d->interp();
	BOX crse_reg(mapper->CoarseBox(unfilled_region,crse_ratio));

	  // alloc patch for crse level
	FARRAYBOX crse(crse_reg,n_der);

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
    const REAL* dx = geom.CellSize();
    for (i = 0; i < grids.length(); i++) {
	BOX g(grids[i]);
	if (!cell_centered) g.convert(der_typ);
	g &= dbox;
	if (g.ok()) {
	    BOX sbox(d->boxMap()(g));
	    FARRAYBOX src(sbox,n_state);
	    int dc = 0;
	    int state_indx, sc, nc;
            int k;
	    for (k = 0; k < nsr; k++) {
		d->getRange(k,state_indx,sc,nc);
		const BOX& sg = state[state_indx].boxArray()[i];
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
	    REAL *ddat = dest.dataPtr();
	    const int* dlo = dest.loVect();
	    const int* dhi = dest.hiVect();
	    const int* lo = g.loVect();
	    const int* hi = g.hiVect();
	    REAL *cdat = src.dataPtr();
	    const int* clo = src.loVect();
	    const int* chi = src.hiVect();
	    const int* dom_lo = dom.loVect();
	    const int* dom_hi = dom.hiVect();
	    const int* bcr = d->getBC();
	    REAL xlo[BL_SPACEDIM];
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
#include <stdio.h>
void
AmrLevel::probe(ostream &os, INTVECT iv, int rad, REAL time,
		int state_indx, int src_comp, int num_comp)
{
    BOX bx(iv,iv);
    bx.grow(rad);
    FARRAYBOX fab(bx,num_comp);
    FillPatch(fab,0,time,state_indx,src_comp,num_comp);
    os << "----------------------------------------------------" << endl;
    os << " >>>> PROBE of:";
    const StateDescriptor &desc = desc_lst[state_indx];
    desc.dumpNames(os,src_comp,num_comp);
    os << endl;
    INTVECT lo = bx.smallEnd();
    INTVECT hi = bx.bigEnd();
    INTVECT point;
    char buf[80];
    for( point=lo; point <= hi; bx.next(point) ){
	os << point;
        int k;
	for (k = 0; k < num_comp; k++) {
	    sprintf(buf,"    %20.14f",fab(point,k));
	    os << buf;
	}
	os << "\n";
    }
    os << endl;
    os << "----------------------------------------------------" << endl;
}
