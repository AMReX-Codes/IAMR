#define _TagBox_C_ "%W% %G%"

#include <TagBox.H>
#include <Misc.H>
#include <Assert.H>
#include <Geometry.H>
#include <ParallelDescriptor.H>

//FabFunctorRegistry<int> BaseFab<int>::ffRegistry;

extern  void inspectTAGArray( const TagBoxArray& tba );
extern  void inspectTAG( const TAGBOX& tb, int n);
extern  void inspectFAB( FARRAYBOX& unfab, int n );

struct TagBoxMergeDesc {
  bool destLocal;
  int mergeIndexSrc;
  int mergeIndexDest;
  int nOverlap;
  Box overlapBox;
  FillBoxId fillBoxId;
};

TAGBOX::TAGBOX() {
}

TAGBOX::TAGBOX(const BOX &bx, int n)
       : BaseFab<int>(bx,n)
{
  setVal(CLEAR);
}

TAGBOX::~TAGBOX() {
}



TAGBOX*
TAGBOX::coarsen(const IntVect & ratio)
{
   BOX cbx(domain);
   cbx.coarsen(ratio);
   TAGBOX* crse = new TAGBOX(cbx);
   const BOX& cbox = crse->box();
   BOX b1(::refine(cbox,ratio));

   const int* flo = domain.loVect();
   const int* fhi = domain.hiVect();
   const int* flen = domain.length();
   int* fdat = dataPtr();
   const int* clo = cbox.loVect();
   const int* chi = cbox.hiVect();
   const int* clen = cbox.length();
   int* cdat = crse->dataPtr();

   const int* lo  = b1.loVect();
   const int* hi  = b1.hiVect();
   const int* len = b1.length();
   int longlen, dir;
   longlen = b1.longside(dir);

   int *t = new int[longlen];
   assert(t);
   int i;
   for (i = 0; i < longlen; i++) t[i] = TAGBOX::CLEAR;

   int klo,khi,jlo,jhi,ilo,ihi;
   jlo = jhi = klo = khi = 0;
   D_TERM(ilo=flo[0]; ihi=fhi[0]; ,
          jlo=flo[1]; jhi=fhi[1]; ,
          klo=flo[2]; khi=fhi[2];)

#define IXPROJ(i,r) (((i)+(r)*Abs(i))/(r) - Abs(i))
#define IOFF(j,k,lo,len) D_TERM(0, +(j-lo[1])*len[0], +(k-lo[2])*len[0]*len[1])
#define JOFF(i,k,lo,len) D_TERM(i-lo[0], +0, +(k-lo[2])*len[0]*len[1])
#define KOFF(i,j,lo,len) D_TERM(i-lo[0], +(j-lo[1])*len[0], +0)

// hack
       dir = 0;
   
   int ratiox = 1;
   int ratioy = 1;
   int ratioz = 1;
   D_TERM( ratiox = ratio[0];,
           ratioy = ratio[1];,
	   ratioz = ratio[2];)

   int dummy_ratio = 1;

   if (dir ==0) {
      int k;
      for (k = klo; k <= khi; k++) {
      int kc = IXPROJ(k,ratioz);
      int j;
      for (j = jlo; j <= jhi; j++) {
         int jc = IXPROJ(j,ratioy);
	 int* c = cdat + IOFF(jc,kc,clo,clen);
	 const int* f = fdat + IOFF(j,k,flo,flen);

         // copy fine grid row of values into tmp array
	 for (i = ilo; i <= ihi; i++) t[i-lo[0]] = f[i-ilo];

         int off;
	 for (off = 0; off < ratiox; off++) {
         int ic;
	 for (ic = 0; ic < clen[0]; ic++) {
	    i = ic*ratiox + off;
	    c[ic] = Max(c[ic],t[i]);
	 }
	 }
      }
      }
   } else if (dir == 1) {
      int k;
      for (k = klo; k <= khi; k++) {
      int kc = IXPROJ(k,dummy_ratio);
      int i;
      for (i = ilo; i <= ihi; i++) {
         int ic = IXPROJ(i,dummy_ratio);
	 int* c = cdat + JOFF(ic,kc,clo,clen);
	 const int* f = fdat + JOFF(i,k,flo,flen);

         // copy fine grid row of values into tmp array
	 int strd = flen[0];
	 int j;
	 for (j=jlo; j <= jhi; j++) t[j-lo[1]] = f[(j-jlo)*strd];

	 int off;
	 for (off = 0; off < dummy_ratio; off++) {
	 int jc = 0;
	 strd = clen[0];
	 int jcnt;
	 for (jcnt = 0; jcnt < clen[1]; jcnt++) {
	    j = jcnt*dummy_ratio + off;
	    c[jc] = Max(c[jc],t[j]);
	    jc += strd;
	 }
	 }
      }
      }
   } else {
      int j;
      for (j = jlo; j <= jhi; j++) {
      int jc = IXPROJ(j,dummy_ratio);
      int i;
      for (i = ilo; i <= ihi; i++) {
         int ic = IXPROJ(i,dummy_ratio);
	 int* c = cdat + KOFF(ic,jc,clo,clen);
	 const int* f = fdat + KOFF(i,j,flo,flen);

         // copy fine grid row of values into tmp array
	 int strd = flen[0]*flen[1];
	 int k;
	 for (k = klo; k <= khi; k++) t[k-lo[2]] = f[(k-klo)*strd];

         int off;
	 for (off = 0; off < dummy_ratio; off++) {
	 int kc = 0;
	 strd = clen[0]*clen[1];
         int kcnt;
	 for (kcnt = 0; kcnt < clen[2]; kcnt++) {
	    k = kcnt*dummy_ratio + off;
	    c[kc] = Max(c[kc],t[k]);
	    kc += strd;
	 }
	 }
      }
      }
   }

   delete t;

   return crse;

#undef ABS
#undef IXPROJ
#undef IOFF
#undef JOFF
#undef KOFF
}

void 
TAGBOX::buffer(int nbuff, int nwid)
{
   // note: this routine assumes cell with SET tag are in
   // interior of tagbox (region = grow(domain,-nwid))
   BOX inside(domain);
   inside.grow(-nwid);
   const int* inlo = inside.loVect();
   const int* inhi = inside.hiVect();
   int klo,khi,jlo,jhi,ilo,ihi;
   jlo = jhi = klo = khi = 0;
   D_TERM(ilo=inlo[0]; ihi=inhi[0]; ,
          jlo=inlo[1]; jhi=inhi[1]; ,
          klo=inlo[2]; khi=inhi[2];)

   const int* len = domain.length();
   const int* lo = domain.loVect();
   int ni, nj, nk;
   ni = nj = nk = 0;
   D_TERM(ni, =nj, =nk) = nbuff;
   int *d = dataPtr();

#define OFF(i,j,k,lo,len) D_TERM(i-lo[0], +(j-lo[1])*len[0] , +(k-lo[2])*len[0]*len[1])
   
   int k;
   for (k = klo; k <= khi; k++) {
   int j;
   for (j = jlo; j <= jhi; j++) {
   int i;
   for (i = ilo; i <= ihi; i++) {
      int *d_check = d + OFF(i,j,k,lo,len);
      if (*d_check == SET) {
         int k;
	 for (k = -nk; k <= nk; k++) {
         int j;
	 for (j = -nj; j <= nj; j++) {
	 // tell CRAY compiler this is a short loop
         int i;
	 for (i = -ni; i <= ni; i++) {
	     int *dn = d_check+ D_TERM(i, +j*len[0], +k*len[0]*len[1]);
             if (*dn !=SET) *dn = BUF;
	 }
	 }
	 }
      }
   }
   }
   }
#undef off
}

void 
TAGBOX::merge(const TAGBOX& src)
{
   // compute intersections
   BOX bx(domain);
   bx &= src.domain;
   if (bx.ok()) {
      const int* dlo = domain.loVect();
      const int* dlen = domain.length();
      const int* slo = src.domain.loVect();
      const int* slen = src.domain.length();
      const int* lo = bx.loVect();
      const int* hi = bx.hiVect();
      const int* ds0 = src.dataPtr();
      int* dd0 = dataPtr();
      int klo,khi,jlo,jhi,ilo,ihi;
      jlo = jhi = klo = khi = 0;
      D_TERM(ilo=lo[0]; ihi=hi[0]; ,
             jlo=lo[1]; jhi=hi[1]; ,
	     klo=lo[2]; khi=hi[2];)

#     define OFF(i,j,k,lo,len) D_TERM(i-lo[0], +(j-lo[1])*len[0] , +(k-lo[2])*len[0]*len[1])
      
      int k;
      for (k = klo; k <= khi; k++) {
      int j;
      for (j = jlo; j <= jhi; j++) {
      int i;
      for (i = ilo; i <= ihi; i++) {
         const int *ds = ds0 + OFF(i,j,k,slo,slen);
	 if (*ds != CLEAR) {
            int *dd = dd0 + OFF(i,j,k,dlo,dlen);
	    *dd = SET;
         }	    
      }
      }
      }
   }
}

int 
TAGBOX::numTags() const
{
   int nt = 0;
   int len = domain.numPts();
   const int* d = dataPtr();
   int n;
   for (n = 0; n < len; n++) {
      if (d[n] != CLEAR) nt++;
   }
   return nt;
}


int
TAGBOX::numTags(const Box &b) const
{
   TAGBOX tempTagBox(b,1);
   tempTagBox.copy(*this);

   int nt = 0;
   int len = b.numPts();
   const int* d = tempTagBox.dataPtr();
   int n;
   for (n = 0; n < len; n++) {
      if (d[n] != CLEAR) nt++;
   }
   return nt;
}


int 
TAGBOX::colate(Array<INTVECT> &ar, int start) const
{
   // starting at given offset of array ar, enter location (INTVECT) of
   // each tagged cell in tagbox
   int count = 0;
   const int* len = domain.length();
   const int* lo = domain.loVect();
   const int* d = dataPtr();
   int ni, nj, nk;
   ni = nj = nk = 1;
   D_TERM( ni = len[0]; , nj = len[1]; , nk = len[2]; )
   int k;
   for (k = 0; k < nk; k++) {
   int j;
   for (j = 0; j < nj; j++) {
   int i;
   for (i = 0; i < ni; i++) {
      const int *dn = d + D_TERM(i, +j*len[0], +k*len[0]*len[1]);
      if (*dn !=CLEAR) {
        ar[start++] = INTVECT( D_DECL(lo[0]+i,lo[1]+j,lo[2]+k) );
	count++;
      }
      
   }
   }
   }
   return count;
}

// ---------------------------------------------------------------
/*
TagBoxArray::TagBoxArray(const BoxArray &ba, int _ngrow,
             PArrayPolicy _managed):
             ngrow(_ngrow), fabparray(ba.length(),_managed)
{
   // parallel loop
   int i;
   for (i = 0; i < fabparray.length(); i++) {
      BOX bx(ba[i]);
      bx.grow(ngrow);
      fabparray.set(i,new TAGBOX(bx,1));
   } 
}
*/

/*
// cant do this because it defines an invalid region in the fabarray
TagBoxArray::TagBoxArray(const BoxArray &ba, int _ngrow)
            : FabArray<int, TAGBOX>(ba, 1, _ngrow)
{
}
*/

TagBoxArray::TagBoxArray(const BoxArray &ba, int _ngrow)
            : FabArray<int, TAGBOX>(),
              border(_ngrow)
{
  BoxArray grownBoxArray(ba);
  grownBoxArray.grow(_ngrow);
  int nvars = 1;
  int growFabArray = 0;
  define(grownBoxArray, nvars, growFabArray, Fab_allocate);
}


TagBoxArray::~TagBoxArray() {
}


void 
TagBoxArray::buffer(int nbuf)
{
    if(nbuf == 0) {
      return;
    }
    assert( nbuf <= border );
    //for(int i = 0; i < fabparray.length(); i++) {
    for(FabArrayIterator<int, TAGBOX> fai(*this); fai.isValid(); ++fai) {
       fai().buffer(nbuf,border);
    } 
}

// ----------------------------------------------------------------------------
void
TagBoxArray::mergeUnique()
{

/*
   // original code vvvvvvvvvvvvvvvvvvvvvvvvvvv
   for(int i = 0; i < fabparray.length(); ++i) {
      TAGBOX &dest = fabparray[i];
      for(int j = i+1; j < fabparray.length(); j++) {
         TAGBOX& src = fabparray[j];
         Box ovlp(dest.box());
	 ovlp &= src.box();
	 if (ovlp.ok()) {
	    dest.merge(src);
	    src.setVal(TAGBOX::CLEAR,ovlp,0);
	 }
      }
   }
*/

   FabArrayCopyDescriptor<int, TAGBOX> facd(true);
   FabArrayId faid = facd.RegisterFabArray(this);
   int nOverlap = 0;
   int myproc = ParallelDescriptor::MyProc();
   List<TagBoxMergeDesc> tbmdList;
   List<FabComTag> tbmdClearList;


   // dont use FabArrayIterator here
   BoxList unfilledBoxes;  // returned by AddBox, not used
   for(int idest = 0; idest < fabparray.length(); ++idest) {
      bool destLocal = (distributionMap[idest] == myproc);
      for(int isrc = idest + 1; isrc < fabparray.length(); ++isrc) {
         Box ovlp(boxarray[idest]);
         ovlp &= boxarray[isrc];
         if(ovlp.ok()) {
           TagBoxMergeDesc tbmd;
           tbmd.destLocal      = destLocal;
           tbmd.mergeIndexSrc  = isrc;
           tbmd.mergeIndexDest = idest;
           tbmd.nOverlap       = nOverlap;
           tbmd.overlapBox     = ovlp;
           if(destLocal) {
             tbmd.fillBoxId = facd.AddBox(faid, ovlp, unfilledBoxes, isrc, 0,0,1);
           }

           tbmdList.append(tbmd);
           ++nOverlap;
         }
      }
   }
   facd.CollectData();

   int listIndex = 0;
   for(ListIterator<TagBoxMergeDesc> tbmdli(tbmdList); tbmdli; ++tbmdli) {
     const TagBoxMergeDesc &tbmd = tbmdli();
     if(tbmd.destLocal) {
       TAGBOX &dest = fabparray[tbmd.mergeIndexDest];
       TAGBOX src(tbmd.overlapBox, 1);
       facd.FillFab(faid, tbmd.fillBoxId, src);
       for(ListIterator<TagBoxMergeDesc> tbmdliprev(tbmdList);
           tbmdliprev && tbmdliprev().nOverlap <= listIndex;
           ++tbmdliprev)
       {
         Box ovlpBox(src.box());
         ovlpBox &= tbmdliprev().overlapBox;
         if(ovlpBox.ok() && tbmdliprev().mergeIndexSrc == tbmd.mergeIndexSrc) {
           if(tbmdliprev().nOverlap < listIndex) {
             src.setVal(TAGBOX::CLEAR, ovlpBox, 0);
           }
           FabComTag tbmdClear;
           tbmdClear.fabIndex = tbmd.mergeIndexSrc;
           tbmdClear.ovlpBox = tbmd.overlapBox;
           tbmdClearList.append(tbmdClear);
         }
       }
       dest.merge(src);
     }
     ++listIndex;
   }

// now send the clear list elements to the processor they belong to
   int tagSize = sizeof(FabComTag);
   ParallelDescriptor::SetMessageHeaderSize(tagSize);

   for(ListIterator<FabComTag> tbmdsendli(tbmdClearList);
       tbmdsendli;
       ++tbmdsendli)
   {
     ParallelDescriptor::SendData(distributionMap[tbmdsendli().fabIndex],
                                  tbmdsendli(), NULL, 0);
   }

   ParallelDescriptor::Synchronize();  // to guarantee messages are sent

   // now clear the overlaps in the TagBoxArray
   int dataWaitingSize;
   int *nullptr = NULL;
   FabComTag tbmdClear;
   while(ParallelDescriptor::GetMessageHeader(dataWaitingSize, &tbmdClear))
   {  // data was sent to this processor
     bool srcLocal = (distributionMap[tbmdClear.fabIndex] == myproc);
     if( ! srcLocal) {
       ParallelDescriptor::Abort("tbmdClear.fabIndex is not local");
     }
     TAGBOX &src = fabparray[tbmdClear.fabIndex];
     src.setVal(TAGBOX::CLEAR, tbmdClear.ovlpBox, 0);

     ParallelDescriptor::ReceiveData(nullptr, 0);  // to advance message header
   }
   ParallelDescriptor::Synchronize();
}  // end mergeUnique()


// ----------------------------------------------------------------------------
void
TagBoxArray::mapPeriodic( const Geometry& geom)
{
/*  // vvvvvvvvvvvvvvvoriginal code vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
  Box domain(geom.Domain());
  TAGBOX tagtmp;
  //for( int i=0; i<fabparray.length(); i++ ){
  for(FabArrayIterator<int, TAGBOX> fai(*this); fai.isValid(); ++fai) {
    TAGBOX &src = fai();
    if( ! domain.contains( src.box() ) ){
      // src is candidate for periodic mapping
      Array<IntVect> pshifts(27);
      geom.periodicShift( domain, src.box(), pshifts );
      for( int iiv=0; iiv< pshifts.length(); iiv++ ){
	IntVect iv = pshifts[iiv];
	Box shiftbox( src.box() );
	D_TERM( shiftbox.shift(0,iv[0]);,
		shiftbox.shift(1,iv[1]);,
		shiftbox.shift(2,iv[2]); )
	// possible periodic remapping, try each tagbox
	for( int j=0; j<fabparray.length(); j++ ){
	  TAGBOX& dest = fabparray[j];
	  Box intbox = dest.box() & shiftbox;
	  if( intbox.ok() ){
	    // ok, got a hit.  But be careful if is same TagBox
	    if( i != j ){
	      src.shift(iv);
	      dest.merge(src);
	      src.shift(-iv);
	    }
	    else {
	      // is same tagbox, must be careful
	      tagtmp.resize(intbox);
	      Box shintbox(intbox);
	      IntVect tmpiv( -iv );
	      D_TERM( shintbox.shift(0,tmpiv[0]);,
		      shintbox.shift(1,tmpiv[1]);,
		      shintbox.shift(2,tmpiv[2]); )
	      tagtmp.copy(src,shintbox,0,intbox,0,1);
	      dest.merge(tagtmp);
	    }
	  }
	}
      }
    }
  }
*/

  FabArrayCopyDescriptor<int, TAGBOX> facd(true);
  FabArrayId faid = facd.RegisterFabArray(this);
  int myproc = ParallelDescriptor::MyProc();
  List<FillBoxId> fillBoxIdList;
  FillBoxId tempFillBoxId;

  Box domain(geom.Domain());
  TAGBOX tagtmp;
  int srcComp  = 0;
  int destComp = 0;
  int nComp    = n_comp;
  // this logic needs to be turned inside out to use a FabArrayIterator
  //for(FabArrayIterator<int, TAGBOX> fai(*this); fai.isValid(); ++fai) {
  for(int i = 0; i < fabparray.length(); i++) {
    //TAGBOX &src = fai();
    if( ! domain.contains( boxarray[i] ) ) {
      // src is candidate for periodic mapping
      Array<IntVect> pshifts(27);
      geom.periodicShift( domain, boxarray[i], pshifts );
      for( int iiv=0; iiv< pshifts.length(); iiv++ ){
        IntVect iv = pshifts[iiv];
        Box shiftbox( boxarray[i] );
        D_TERM( shiftbox.shift(0,iv[0]);,
                shiftbox.shift(1,iv[1]);,
                shiftbox.shift(2,iv[2]); )
        // possible periodic remapping, try each tagbox
        for(int j = 0; j < fabparray.length(); j++) {
          if(distributionMap[j] == myproc) {  // local dest fab
            // (possibly) communicate src

            TAGBOX& dest = fabparray[j];
            Box intbox = dest.box() & shiftbox;
            if(intbox.ok()) {
              BoxList unfilledBoxes(intbox.ixType());
              // ok, got a hit.  But be careful if is same TagBox
              if( i != j ){
                tempFillBoxId = facd.AddBox(faid, intbox, unfilledBoxes,
                                            srcComp, destComp, nComp);
                fillBoxIdList.append(tempFillBoxId);
              } else {
                // is same tagbox, must be careful
                Box shintbox(intbox);
                IntVect tmpiv( -iv );
                D_TERM( shintbox.shift(0,tmpiv[0]);,
                        shintbox.shift(1,tmpiv[1]);,
                        shintbox.shift(2,tmpiv[2]); )
                tempFillBoxId = facd.AddBox(faid, shintbox, unfilledBoxes,
                                            srcComp, destComp, nComp);
                fillBoxIdList.append(tempFillBoxId);
              }
            }
          }
        }
      }
    }
  }


    Array<FillBoxId> fillBoxId(fillBoxIdList.length());
    int ifbi = 0;
    for(ListIterator<FillBoxId> li(fillBoxIdList); li; ++li) {
     fillBoxId[ifbi] = li();
     ++ifbi;
    }
    fillBoxIdList.clear();

    facd.CollectData();

    int iFillBox = 0;
  // this logic needs to be turned inside out to use a FabArrayIterator
  //for(FabArrayIterator<int, TAGBOX> fai(*this); fai.isValid(); ++fai) {
  for(int i = 0; i < fabparray.length(); i++) {
    //TAGBOX &src = fai();
    if( ! domain.contains( boxarray[i] ) ) {
      // src is candidate for periodic mapping
      Array<IntVect> pshifts(27);
      geom.periodicShift( domain, boxarray[i], pshifts );
      for( int iiv=0; iiv< pshifts.length(); iiv++ ) {
        IntVect iv = pshifts[iiv];
        Box shiftbox( boxarray[i] );
        D_TERM( shiftbox.shift(0,iv[0]);,
                shiftbox.shift(1,iv[1]);,
                shiftbox.shift(2,iv[2]); )
        // possible periodic remapping, try each tagbox
        for( int j=0; j<fabparray.length(); j++ ) {
          if(distributionMap[j] == myproc) {  // local dest fab
            TAGBOX& dest = fabparray[j];
            Box intbox = dest.box() & shiftbox;
            if(intbox.ok()) {
              // ok, got a hit.  But be careful if is same TagBox

              if(i != j) {
                FillBoxId fillboxid = fillBoxId[iFillBox];
                ++iFillBox;
                TAGBOX src(fillboxid.box(), n_comp);
                facd.FillFab(faid, fillboxid, src);

                src.shift(iv);
                dest.merge(src);
                src.shift(-iv);
              } else {
                // is same tagbox, must be careful
                FillBoxId fillboxid = fillBoxId[iFillBox];
                ++iFillBox;
                TAGBOX src(fillboxid.box(), n_comp);  // this is on shintbox
                facd.FillFab(faid, fillboxid, src);

                tagtmp.resize(intbox);
                Box shintbox(intbox);
                IntVect tmpiv( -iv );
                D_TERM( shintbox.shift(0,tmpiv[0]);,
                        shintbox.shift(1,tmpiv[1]);,
                        shintbox.shift(2,tmpiv[2]); )
                assert(shintbox == fillboxid.box());
                tagtmp.copy(src,shintbox,0,intbox,0,1);
                dest.merge(tagtmp);
              }
            }
          }
        }  // end for(j...)
      }  // end for(iiv...)
    }
  }  // end for(i...)

}  // end mapPeriodic(...)

int 
TagBoxArray::numTags() const 
{
   int ntag = 0;
   for(ConstFabArrayIterator<int, TAGBOX> fai(*this); fai.isValid(); ++fai) {
      ntag += fai().numTags();
   } 
   ParallelDescriptor::ReduceIntSum(ntag);
   return ntag;
}

// ------------------------------------------------------------------------
Array<INTVECT>* 
TagBoxArray::colate() const
{
   int myproc = ParallelDescriptor::MyProc();
   int nGrids = fabparray.length();
   int *startOffset = new int[nGrids];  // start locations per grid
   int *sharedNTags = new int[nGrids];  // shared numTags  per grid
   for(int isn = 0; isn < nGrids; ++isn) {
     sharedNTags[isn] = -1;  // a bad value
   }

   int len = numTags();
   Array<INTVECT> *ar = new Array<INTVECT>(len);
   for(ConstFabArrayIterator<int, TAGBOX> fai(*this); fai.isValid(); ++fai) {
      sharedNTags[fai.index()] = fai().numTags();
   }

        // communicate number of local tags for each grid
        int iProc, iPnt;
        int nProcs = ParallelDescriptor::NProcs();
        ParallelDescriptor::ShareVar(sharedNTags, nGrids * sizeof(int));
        ParallelDescriptor::Synchronize();  // for ShareVar
        for(int iGrid = 0; iGrid < nGrids; ++iGrid) {
          if(sharedNTags[iGrid] != -1) {
            for(iProc = 0; iProc < nProcs; ++iProc) {
              if(iProc != myproc) {
                ParallelDescriptor::WriteData(iProc, &(sharedNTags[iGrid]),
                                              sharedNTags,
                                              iGrid * sizeof(int), sizeof(int));
              }
            }
          }
        }
        ParallelDescriptor::Synchronize();  // need this sync after the put
        ParallelDescriptor::UnshareVar(sharedNTags);
        startOffset[0] = 0;
        for(int iGrid = 1; iGrid < nGrids; ++iGrid) {
          startOffset[iGrid] = startOffset[iGrid - 1] + sharedNTags[iGrid - 1];
        }

        // communicate all local points so all procs have the same global set

        // need a 1d array for contiguous parallel copies
        int *tmpPts = new int[len * BL_SPACEDIM];
        size_t ivSize = BL_SPACEDIM * sizeof(int);

        // copy the local IntVects into the shared array
        int *ivDest, *ivDestBase;
        const int *ivSrc;
   //for(ConstFabArrayIterator<int, TAGBOX> fai(*this); fai.isValid(); ++fai) {
      //int start = fai().colate(*ar, startOffset[fai.index()]);
   //}

   for(ConstFabArrayIterator<int, TAGBOX> fai(*this); fai.isValid(); ++fai) {
      int start = fai().colate(*ar, startOffset[fai.index()]);
      ivDestBase = tmpPts + (startOffset[fai.index()] * BL_SPACEDIM);
      for(iPnt = 0; iPnt < sharedNTags[fai.index()]; ++iPnt) {
        ivDest = ivDestBase + (iPnt * BL_SPACEDIM);
        ivSrc  = ((*ar)[startOffset[fai.index()] + iPnt]).getVect();
        memcpy(ivDest, ivSrc, ivSize);
      }
   }


        // now copy the the local IntVects to all other processors
        ParallelDescriptor::ShareVar(tmpPts, len * ivSize);
        ParallelDescriptor::Synchronize();  // for ShareVar

   for(ConstFabArrayIterator<int, TAGBOX> fai(*this); fai.isValid(); ++fai) {
        ivDestBase = tmpPts + (startOffset[fai.index()] * BL_SPACEDIM);
        for(iProc = 0; iProc < nProcs; ++iProc) {
          if(iProc != myproc) {
            if(sharedNTags[fai.index()] != 0) {
              ParallelDescriptor::WriteData(iProc, ivDestBase, tmpPts,
                      startOffset[fai.index()] * ivSize,   // offset
                      sharedNTags[fai.index()] * ivSize);  // nbytes
            }
          }
        }
   }
        ParallelDescriptor::Synchronize();  // need this sync after the put
        ParallelDescriptor::UnshareVar(tmpPts);

        for(iPnt = 0; iPnt < len; ++iPnt) {
          for(int iiv = 0; iiv < BL_SPACEDIM; ++iiv) {
            ((*ar)[iPnt])[iiv] = tmpPts[(iPnt * BL_SPACEDIM) + iiv];
          }
        }

        delete [] startOffset;
        delete [] sharedNTags;
        delete [] tmpPts;

   return ar;
}  // end colate()

void
TagBoxArray::setVal(BoxDomain &bd, TAGBOX::TagVal val)
{
   for(FabArrayIterator<int, TAGBOX> fai(*this); fai.isValid(); ++fai) {
      BoxDomainIterator bdi(bd);
      while(bdi) {
         Box bx(fai.validbox());
	 bx &= bdi();
	 if(bx.ok()) {
	   fai().setVal(val,bx,0);
	 }
         ++bdi;
      }
   } 
}

void
TagBoxArray::setVal(BoxArray& ba, TAGBOX::TagVal val)
{
   for(FabArrayIterator<int, TAGBOX> fai(*this); fai.isValid(); ++fai) {
      for(int j = 0; j < ba.length(); j++) {
         Box bx(fai.validbox());
	 bx &= ba[j];
	 if(bx.ok()) {
	   fai().setVal(val,bx,0);
	 }
      }
   } 
}

void 
TagBoxArray::coarsen(const IntVect & ratio)
{
    for(FabArrayIterator<int, TAGBOX> fai(*this); fai.isValid(); ++fai) {
       TAGBOX *tfine = fabparray.remove(fai.index());
       TAGBOX *tcrse = tfine->coarsen(ratio);
       fabparray.set(fai.index(),tcrse);
       delete tfine;
    } 
    border = 0;
    n_grow = 0;
}
