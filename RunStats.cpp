
#include <Utility.H>
#include <Misc.H>
#include <ParmParse.H>
#include <RunStats.H>
#include <ParallelDescriptor.H>

#ifdef BL_USE_NEW_HFILES
using std::setw;
using std::ios;
using std::setprecision;
#endif

const char SP = ' ';

List<RunStatsData> RunStats::ld;
double RunStats::total_run_time;
double RunStats::total_run_wtime;
Array<long> RunStats::cells;

RunStats::RunStats(const char *_name, int _level)
    : name(_name), level(_level)
{
    gentry = find(_name, -1);
    entry = find(_name, _level);
    entry->is_on = true;
}

void
RunStats::addCells(int lev, long count)
{
    if (lev >= cells.length()) {
	cells.resize(lev+1);
	cells[lev] = 0;
    }
    cells[lev] += count;
}

long
RunStats::getCells(int lev)
{
    return cells[lev];
}

RunStatsData *
RunStats::find(aString _name, int _level)
{
    ListIterator<RunStatsData> ldi(ld);
    while(ldi) {
	if(ldi().level == _level && ldi().name == _name) {
	    return &ld[ldi];
	}
	++ldi;
    }
    ld.append(RunStatsData(_name, _level));
    return &ld.lastElement();
}

void
RunStats::turnOn(const char *s, int _level)
{
    RunStatsData *e = find(s, _level);
    e->is_on = true;
}

void
RunStats::turnOff(const char *s, int _level)
{
    if(_level == -1) {
    } else {
	RunStatsData *e = find(s, _level);
	e->is_on = false;
    }
}

void
RunStats::init()
{
    ParmParse pp("RunStats");
    if (pp.contains("statvar")) {
	int n = pp.countval("statvar");
        int i; 
	for (i = 0; i < n; i++) {
	    aString nm;
	    pp.get("statvar",nm,i);
	    turnOn(nm.c_str());
	}
    }
}

//
ostream &
operator << (ostream &os, const RunStatsData &rd)
{
    os << "(RunStatsData "
       << rd.name << SP
       << rd.level << SP
       << rd.is_on <<  SP
       << rd.run_time << SP
       << rd.run_wtime << SP
       << rd.max_time << SP
       << rd.max_wtime << ")\n";

    return os;
}

istream &
operator >> (istream &is, RunStatsData &rd)
{
    is.ignore(BL_IGNORE_MAX, '(');
    aString s;
    is >> s;
    if(s != "RunStatsData") {
        cerr << "RunStatsData: unexpected token " << s << '\n';
        BoxLib::Abort();
    }
    is >> rd.name;
    is >> rd.level;
    {
        int i_is_on;
        is >> i_is_on;
        rd.is_on = i_is_on?true:false;
    }
    is >> rd.run_time;
    is >> rd.run_wtime;
    is >> rd.max_time;
    is >> rd.max_wtime;
    is.ignore(BL_IGNORE_MAX,')');

    return is;
}

// list active stats
ostream&
operator << (ostream &os, const RunStats &r)
{
    os << "(RunStats " << r.name
       << " level " << r.level
       << (r.isOn()?"":"in") << "active)" << '\n';

    return os;
}

#define mpCPUSeconds() Utility::second()
#define mpWallClockSeconds() ParallelDescriptor::second()

// report stats in formatted form to output stream
void
RunStats::report(ostream &os)
{
    double rtime = mpCPUSeconds();
    double wtime = mpWallClockSeconds();
    ParallelDescriptor::ReduceRealSum(rtime);
    ParallelDescriptor::ReduceRealMax(wtime);
    double tot_run_time = total_run_time + rtime;
    double tot_run_wtime = total_run_wtime + wtime;

    if (ParallelDescriptor::IOProcessor()) {
	  // output number of cells advanced on each level
	long tot_cells = 0;
        int i; 
	for (i = 0; i < cells.length(); i++) {
	    os << "Number of cells advanced at level " << i << " = "
	       << cells[i] << '\n';
	    tot_cells += cells[i];
	}
	os << "Total cells advanced = " << tot_cells << "\n\n";

	ListIterator<RunStatsData> ldi(ld);
	  // determine maximimum depth
	int maxlev = 0;
	while(ldi) {
	    maxlev = Max(maxlev, ldi().level);
	    ++ldi;
	}

        int lev; 

	os.setf(ios::showpoint);
	os << setprecision(4);
	for (lev = 0; lev <= maxlev; ++lev) {
	    os << "timings for level " << lev << '\n';

	    ldi.rewind();
	    while(ldi) {
		if(ldi().level == lev) {
		    ListIterator<RunStatsData> ldii(ld);
		    while(ldii) {
			if(ldii().name == ldi().name && ldii().level == -1)
			    break;
			ldii++;
		    }
		    if(ldi().is_on) {
			os << "State " << ldi().name;
			os << " time  = "
			   << setw(6) << ldi().run_time << SP
			   << setw(6) << 100.0*(ldi().run_time/tot_run_time) << "% "
			   << setw(6) << ldi().run_wtime << SP
			   << setw(6) << 100.0*(ldi().run_wtime/tot_run_wtime) << "% "
			   << setw(6) << ldi().max_time << SP
			   << setw(6) << ldi().max_wtime;
			os << '\n';
		    }
		}
		++ldi;
	    }
	}

	  // report totals
	ldi.rewind();

	os << '\n';
	while(ldi) {
	    if(ldi().level == -1) {
		if (ldi().is_on == true) {
		    os << "total " << ldi().name << " time";
		    os << "  = "
		       << setw(6) << ldi().run_time << "  "
		       << setw(6) << 100.0*(ldi().run_time/tot_run_time) << "% "
		       << setw(6) << ldi().run_wtime << "  "
		       << setw(6) << 100.0*(ldi().run_wtime/tot_run_wtime) << "% ";
		    os << '\n';
		}
	    }
	    ++ldi;
	}
	os << "total CPU time          = " << tot_run_time << '\n';
	os << "total Wall Clock time   = " << tot_run_wtime << '\n';
    }
}

// unformatted write to output stream
void
RunStats::dumpStats(ofstream &os)
{
    double rtime = mpCPUSeconds();
    double wtime = mpWallClockSeconds();
    ParallelDescriptor::ReduceRealSum(rtime);
    ParallelDescriptor::ReduceRealMax(wtime);
    if (ParallelDescriptor::IOProcessor()) {
	os << "(ListRunStats " << ld.length() << '\n';
	os << rtime + total_run_time << '\n';
	os << wtime + total_run_wtime << '\n';
	ListIterator<RunStatsData> ldi(ld);
	while(ldi) {
	    os << ldi();
	    ++ldi;
	}
	int nlev = cells.length();
        os << nlev;
        int i; 
	for (i = 0; i < nlev; i++) {
	    os << SP << cells[i];
	}
	os << ")\n";
    }
}

// unformated read from input file
void
RunStats::readStats(ifstream &is)
{
    ld.clear();
    is.ignore(BL_IGNORE_MAX,'(');
    aString s;
    is >> s;
    if( s != "ListRunStats") {
	cerr << "readStats: unexpected token " << s << '\n';
        BoxLib::Abort();
    }
    int n;
    is >> n;
    is >> total_run_time;
    is >> total_run_wtime;
    while(n--) {
	RunStatsData rd;
	is >> rd;
	ld.append(rd);
    }
    int nlev;
    is >> nlev;
    cells.resize(nlev);
    int i; 
    for (i = 0; i < nlev; i++) {
	is >> cells[i];
    }
    is.ignore(BL_IGNORE_MAX,')');
}

void
RunStats::start()
{
    if (gentry->is_on && entry->is_on) {
        ParallelDescriptor::Synchronize();
	time = -mpCPUSeconds();
	wtime= -mpWallClockSeconds();
    }
}

void
RunStats::pause()
{
    if (gentry->is_on && entry->is_on) {
	time += mpCPUSeconds();
	wtime += mpWallClockSeconds();
        ParallelDescriptor::Synchronize();
    }
}

void
RunStats::resume()
{
    if (gentry->is_on && entry->is_on) {
        ParallelDescriptor::Synchronize();
	time += mpCPUSeconds();
	wtime += mpWallClockSeconds();
    }
}

void
RunStats::end()
{
    if (gentry->is_on && entry->is_on) {
	time += mpCPUSeconds();
	wtime += mpWallClockSeconds();
	if(ParallelDescriptor::NProcs() == 1) {
	    entry->run_time += time;
	    entry->run_wtime += wtime;
	    entry->max_time += time;
	    entry->max_wtime += wtime;
	    gentry->run_time += time;
	    gentry->run_wtime += wtime;
	    gentry->max_time += time;
	    gentry->max_wtime += wtime;
	} else {
	    double tmp[2];
	    tmp[0] = time;
	    tmp[1] = wtime;
	    ParallelDescriptor::ReduceRealSum(tmp[0]);
	    ParallelDescriptor::ReduceRealSum(tmp[1]);
	    entry->run_time += tmp[0];
	    entry->run_wtime += tmp[1];
	    gentry->run_time += tmp[0];
	    gentry->run_wtime += tmp[1];
	    tmp[0] = time;
	    tmp[1] = wtime;
	    ParallelDescriptor::ReduceRealMax(tmp[0]);
	    ParallelDescriptor::ReduceRealMax(tmp[1]);
	    entry->max_time += tmp[0];
	    entry->max_wtime += tmp[1];
	    gentry->max_time += tmp[0];
	    gentry->max_wtime += tmp[1];
	}
    }
}
