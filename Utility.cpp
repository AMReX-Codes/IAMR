//BL_COPYRIGHT_NOTICE

//
// $Id: Utility.cpp,v 1.1 1997-07-08 23:08:07 vince Exp $
//

#include <stdlib.h>
#include <iostream.h>
#include <string.h>
#include <fstream.h>
#include <ctype.h>
#include <unistd.h>

#include <REAL.H>
#include <Misc.H>
#include <BoxLib.H>
#include <Utility.H>

#if !defined(BL_ARCH_CRAY)
//
// ------------------------------------------------------------
// Return current run-time.
// ------------------------------------------------------------
//
#include <sys/types.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/param.h>

//
// This doesn't seem to be defined on SunOS when using g++.
//
#if defined(__GNUG__) && defined(__sun) && defined(BL_SunOS)
extern "C" int gettimeofday (struct timeval*, struct timezone*);
#endif

double
Utility::second (double* t)
{
    struct tms buffer;

    times(&buffer);
    static long CyclesPerSecond = 0;
    if (CyclesPerSecond == 0)
    {
#if defined(_SC_CLK_TCK)
        CyclesPerSecond = sysconf(_SC_CLK_TCK);
        if (CyclesPerSecond == -1)
            BoxLib::Error("second(double*): sysconf() failed");
#elif defined(HZ)
        CyclesPerSecond = HZ;
#else
        CyclesPerSecond = 100;
        BoxLib::Warning("second(): sysconf(): default value of 100 for hz, worry about timings");
#endif
    }
    double dt = (buffer.tms_utime + buffer.tms_stime)/(1.0*CyclesPerSecond);
    if (t != 0)
        *t = dt;
    return dt;
}

double
Utility::wsecond (double* t)
{
    static double epoch = -1.0;
    struct timeval tp;
    if (epoch < 0.0)
    {
        if (gettimeofday(&tp, 0) != 0)
            BoxLib::Abort("wsecond(): gettimeofday() failed");
        epoch = tp.tv_sec + tp.tv_usec/1000000.0;
    }
    gettimeofday(&tp,0);
    double dt = tp.tv_sec + tp.tv_usec/1000000.0-epoch;
    if(t != 0)
        *t = dt;
    return dt;
}

#else

extern "C" double SECOND();
extern "C" double RTC();

double
Utility::second (double* t_)
{
    double t = SECOND();
    if (t_)
        *t_ = t;
    return t;
}

double
Utility::wsecond (double* t_)
{
    static double epoch = -1.0;
    if (epoch < 0.0)
    {
        epoch = RTC();
    }
    double t = RTC() - epoch;
    if (t_)
        *t_ = t;
    return t;
}
#endif /*!defined(BL_ARCH_CRAY)*/

//
// ------------------------------------------------------------
// Return true if argument is a non-zero length string of digits.
// ------------------------------------------------------------
//

bool
Utility::is_integer (const char* str)
{
    if (str == 0)
        return false;

    int len = strlen(str);

    if (len == 0)
        return false;
    else
    {
        for (int i = 0; i < len; i++)
            if (!isdigit(str[i]))
                return false;
        return true;
    }
}
