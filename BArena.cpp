//BL_COPYRIGHT_NOTICE

//
// $Id: BArena.cpp,v 1.1 1997-07-08 23:08:05 vince Exp $
//

#include <BArena.H>
#include <Assert.H>

void*
BArena::alloc (size_t _sz,
               void** _pt)
{
    void* pt = new char[_sz];
    if (pt == 0)
        BoxLib::OutOfMemory(__FILE__, __LINE__);
    if (_pt != 0)
        *_pt = pt;
    return pt;
}

void BArena::free (void* pt)
{
    delete [] pt;
}
