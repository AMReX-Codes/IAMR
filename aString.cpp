//BL_COPYRIGHT_NOTICE

//
// $Id: aString.cpp,v 1.1 1997-07-08 23:08:08 vince Exp $
//

#include <ctype.h>

#include <Assert.H>
#include <aString.H>

void
StringRep::resize (int n)
{
    if (n > bufferlength)
    {
        char* ns = new char[n];
        if (ns == 0)
            BoxLib::OutOfMemory(__FILE__, __LINE__);
        ::memcpy(ns,s,bufferlength);
        bufferlength = n;
        delete [] s;
        s = ns;
    }
}

aString::aString ()
    : p(new StringRep(1)),
      len(0)
{
    if (p.isNull())
        BoxLib::OutOfMemory(__FILE__, __LINE__);
    p->s[0] = 0;
}

aString::aString (char c)
    : p(new StringRep(2)),
      len(1)
{
    if (p.isNull())
        BoxLib::OutOfMemory(__FILE__, __LINE__);
    p->s[0] = c;
    p->s[1] = 0;
    if (c == '\0')
        len = 0;
}

aString::aString (int size)
    : p(new StringRep(size+1)),
      len(0)
{
    if (p.isNull())
        BoxLib::OutOfMemory(__FILE__, __LINE__);
    ::memset(p->s,'\0',p->bufferlength);
}

aString::aString (const char* initialtext)
{
    assert(initialtext != 0);
    len = ::strlen(initialtext);
    p = new StringRep(len + 1);
    if (p.isNull())
        BoxLib::OutOfMemory(__FILE__, __LINE__);
    ::memcpy(p->s,initialtext,len+1);
}

aString::aString (const aString& initialstring)
    : p(initialstring.p),
      len(initialstring.len)
{}

aString&
aString::operator= (const aString& rhs)
{
    p   = rhs.p;
    len = rhs.len;
    return *this;
}

aString&
aString::operator+= (const aString& val)
{
    copyModify();
    int clen = length() + val.length();
    p->resize(clen+1);
    ::memcpy(&(p->s[len]),val.p->s, val.length()+1);
    len = clen;
    return *this;
}

aString&
aString::operator+= (const char* s)
{
    assert(s != 0);
    copyModify();
    int slen = ::strlen(s);
    int clen = length() + slen;
    p->resize(clen+1);
    ::memcpy(&(p->s[len]),s, slen+1);
    len = clen;
    return *this;
}

aString&
aString::operator+= (char c)
{
    if (!(c == '\0'))
    {
        copyModify();
        p->resize(len+2);
        p->s[len++] = c;
        p->s[len]   = 0;
    }
    return *this;
}

char&
aString::operator[] (int index)
{
    assert(index >= 0 && index < len);
    copyModify();
    return p->s[index];
}

void
aString::copyModify ()
{
    if (!p.unique())
    {
        StringRep* np = new StringRep(len+1);
        if (np == 0)
            BoxLib::OutOfMemory(__FILE__, __LINE__);
        ::memcpy(np->s,p->s,len+1);
        p = np;
    }
}

aString&
aString::toUpper ()
{
    copyModify();
    for (char *pp = p->s; pp != 0; pp++)
        *pp = toupper(*pp);
    return *this;
}

aString&
aString::toLower ()
{
    copyModify();
    for (char *pp = p->s; pp != 0; pp++)
        *pp = tolower(*pp);
    return *this;
}

istream&
operator>> (istream& is,
            aString& str)
{
    const int BufferSize = 128;
    char buf[BufferSize + 1];
    int index = 0;
    //
    // Nullify str.
    //
    str = "";
    //
    // Eat leading whitespace.
    //
    char c;
    do { is.get(c); } while (is.good() && isspace(c));
    buf[index++] = c;
    //
    // Read until next whitespace.
    //
    while (is.get(c) && !isspace(c))
    {
        buf[index++] = c;
        if (index == BufferSize)
        {
            buf[BufferSize] = 0;
            str += buf;
            index = 0;
        }
    }
    is.putback(c);
    buf[index] = 0;
    str += buf;
    if (is.fail())
        BoxLib::Abort("operator>>(istream&,aString&) failed");
    return is;
}

ostream&
operator<< (ostream&       out,
            const aString& str)
{
    out.write(str.c_str(), str.len);
    if (out.fail())
        BoxLib::Error("operator<<(ostream&,aString&) failed");
    return out;
}

istream&
aString::getline (istream& is)
{
    char      c;
    const int BufferSize = 256;
    char      buf[BufferSize + 1];
    int       index = 0;

    *this = "";

    //
    // Get those characters.
    //
    while (is.get(c) && c != '\n')
    {
        buf[index++] = c;
        if (index == BufferSize)
        {
            buf[BufferSize] = 0;
            *this += buf;
            index = 0;
        }
    }
    is.putback(c);
    buf[index] = 0;
    *this += buf;

    if (is.fail())
        BoxLib::Abort("aString::getline(istream&) failed");

    return is;
}
