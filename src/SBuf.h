/*
 * SBuf.h (C) 2008 Francesco Chemolli <kinkie@squid-cache.org>
 *
 * SQUID Web Proxy Cache          http://www.squid-cache.org/
 * ----------------------------------------------------------
 *
 *  Squid is the result of efforts by numerous individuals from
 *  the Internet community; see the CONTRIBUTORS file for full
 *  details.   Many organizations have provided support for Squid's
 *  development; see the SPONSORS file for full details.  Squid is
 *  Copyrighted (C) 2001 by the Regents of the University of
 *  California; see the COPYRIGHT file for full details.  Squid
 *  incorporates software developed and/or copyrighted by other
 *  sources; see the CREDITS file for full details.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 */

#ifndef SQUID_SBUF_H
#define SQUID_SBUF_H

#include "base/InstanceId.h"
#include "Debug.h"
#include "MemBlob.h"
#include "SquidString.h"

#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_STDARG_H
#include <stdarg.h>
#endif
#if HAVE_IOSFWD
#include <iosfwd>
#endif

/* squid string placeholder (for printf) */
#ifndef SQUIDSBUFPH
#define SQUIDSBUFPH "%.*s"
#define SQUIDSBUFPRINT(s) (s).plength(),(s).rawContent()
#endif /* SQUIDSBUFPH */

typedef enum {
    caseSensitive,
    caseInsensitive
} SBufCaseSensitive;

/**
 * Container for various SBuf class-wide statistics.
 *
 * The stats are not completely accurate; they're mostly meant to
 * understand whether Squid is leaking resources
 * and whether SBuf is paying off the expected gains.
 */
class SBufStats
{
public:
    u_int64_t alloc; ///<number of calls to SBuf constructors
    u_int64_t allocCopy; ///<number of calls to SBuf copy-constructor
    u_int64_t allocFromString; ///<number of copy-allocations from Strings
    u_int64_t allocFromCString; ///<number of copy-allocations from c-strings
    u_int64_t assignFast; ///<number of no-copy assignment operations
    u_int64_t clear; ///<number of clear operations
    u_int64_t append; ///<number of append operations
    u_int64_t toStream;  ///<number of write operations to ostreams
    u_int64_t setChar; ///<number of calls to setAt
    u_int64_t getChar; ///<number of calls to at() and operator[]
    u_int64_t compareSlow; ///<number of comparison operations requiring data scan
    u_int64_t compareFast; ///<number of comparison operations not requiring data scan
    u_int64_t copyOut; ///<number of data-copies to other forms of buffers
    u_int64_t rawAccess; ///<number of accesses to raw contents
    u_int64_t chop;  ///<number of chop operations
    u_int64_t trim;  ///<number of trim operations
    u_int64_t find;  ///<number of find operations
    u_int64_t scanf;  ///<number of scanf operations
    u_int64_t caseChange; ///<number of toUpper and toLower operations
    u_int64_t cowFast; ///<number of cow operations not actually requiring a copy
    u_int64_t cowSlow; ///<number of cow operations requiring a copy
    u_int64_t live;  ///<number of currently-allocated SBuf

    /**
     * Dump statistics to an ostream.
     */
    std::ostream& dump(std::ostream &os) const;
    SBufStats();

    SBufStats& operator +=(const SBufStats&);
};

/**
 * A String or Buffer.
 * Features: refcounted backing store, cheap copy and sub-stringing
 * operations, copy-on-write to isolate change operations to each instance.
 * Where possible, we're trying to mimic std::string's interface.
 */
class SBuf
{
public:
    typedef int32_t size_type;
    static const size_type npos = -1;

    /// Maximum size of a SBuf. By design it MUST be < MAX(size_type)/2. Currently 256Mb.
    static const size_type maxSize = 0xfffffff;

    /// create an empty (zero-size) SBuf
    SBuf();
    SBuf(const SBuf &S);

    /** Constructor: import c-style string
     *
     * Create a new SBuf containing a COPY of the contents of the
     * c-string
     * \param S the c string to be copied
     * \param pos how many bytes to skip at the beginning of the c-string
     * \param n how many bytes to import into the SBuf. If it is SBuf::npos
     *              or unspecified, imports to end-of-cstring
     * \note it is the caller's responsibility not to go out of bounds
     * \note bounds is 0 <= pos < length()
     */
    explicit SBuf(const char *S, size_type pos = 0, size_type n = npos);

    /** Constructor: import SquidString, copying contents.
     *
     * This method will be removed once SquidString has gone.
     */
    SBuf(const String &S);

    ~SBuf();
    /** Explicit assignment.
     *
     * Current SBuf will share backing store with the assigned one.
     */
    SBuf& assign(const SBuf &S);
    /** Assignment operator.
     *
     * Current SBuf will share backing store with the assigned one.
     */
    _SQUID_INLINE_ SBuf& operator =(const SBuf & S);

    /** Import a c-string into a SBuf, copying the data.
     *
     * It is the caller's duty to free the imported string, if needed.
     * \param S the c string to be copied
     * \param pos how many bytes to skip at the beginning of the c-string.
     * \param n how many bytes to import into the SBuf. If it is SBuf::npos
     *              or unspecified, imports to end-of-cstring
     * \note it is the caller's responsibility not to go out of bounds
     * \note bounds is 0 <= pos < length()
     */
    SBuf& assign(const char *S, size_type pos = 0, size_type n = npos);

    /** Assignment operator. Copy a NULL-terminated c-style string into a SBuf.
     *
     * Copy a c-style string into a SBuf. Shortcut for SBuf.assign(S)
     * It is the caller's duty to free the imported string, if needed.
     */
    _SQUID_INLINE_ SBuf& operator =(const char *S);

    /** Import a std::string into a SBuf. Contents are copied.
     *
     * \param pos skip this many bytes at the beginning of string.
     *          0 is beginning-of-string
     * \param n how many bytes to copy. Default is SBuf::npos, end-of-string.
     */
    SBuf& assign(const std::string &s, size_type pos = 0, size_type n = npos);

    /** reset the SBuf as if it was just created.
     *
     * Resets the SBuf to empty, memory is freed lazily.
     */
    void clear();

    /** Append operation
     *
     * Append the supplied SBuf to the current one; extend storage as needed.
     */
    SBuf& append(const SBuf & S);

    /** Append operation for C-style strings.
     *
     * Append the supplied c-string to the SBuf; extend storage
     * as needed.
     *
     * \param S the c string to be copied. Can be NULL.
     * \param pos how many bytes to skip at the beginning of the c-string
     * \param n how many bytes to import into the SBuf. If it is SBuf::npos
     *              or unspecified, imports to end-of-cstring
     */
    SBuf& append(const char * S, size_type pos = 0, size_type n = npos);

    /** Append operation for std::string
     *
     * Append the supplied std::string to the SBuf; extend storage as needed.
     *
     * \param string the std::string to be copied.
     * \param pos how many bytes to skip at the beginning of the c-string
     * \param n how many bytes to import into the SBuf. If it is SBuf::npos
     *              or unspecified, imports to end-of-cstring
     */
    SBuf& append(const std::string &str, size_type pos = 0, size_type n = npos);

    /** Assignment operation with printf(3)-style definition
     * \note arguments may be evaluated more than once, be careful
     *       of side-effects
     */
    SBuf& Printf(const char *fmt, ...);

    /** Append operation with printf-style arguments
     * \note arguments may be evaluated more than once, be careful
     *       of side-effects
     */
    SBuf& appendf(const char *fmt, ...);
    /** Append operation, with vsprintf(3)-style arguments.
     * \note arguments may be evaluated more than once, be careful
     *       of side-effects
     */
    SBuf& vappendf(const char *fmt, va_list vargs);

    /** print a SBuf.
     */
    std::ostream& print(std::ostream &os) const;

    /** print the sbuf, debug information and stats
     *
     * Debug function, dumps to a stream informations on the current SBuf,
     * including low-level details and statistics.
     */
    std::ostream& dump(std::ostream &os) const;

    /** random-access read to any char within the SBuf
     *
     * does not check access bounds. If you need that, use at()
     */
    _SQUID_INLINE_ const char operator [](size_type pos) const;

    /** random-access read to any char within the SBuf.
     *
     * \throw OutOfBoundsException when access is out of bounds
     * \note bounds is 0 <= pos < length()
     */
    _SQUID_INLINE_ const char at(size_type pos) const;

    /** direct-access set a byte at a specified operation.
     *
     * \param pos the position to be overwritten
     * \param toset the value to be written
     * \throw OutOfBoundsException when pos is of bounds
     * \note bounds is 0 <= pos < length()
     * \note performs a copy-on-write if needed.
     */
    void setAt(size_type pos, char toset);

    /** compare to other SBuf, str(case)cmp-style
     *
     * \param isCaseSensitive one of caseSensitive or caseInsensitive
     * \param n compare up to this many bytes. if npos (default), to end-of-string
     * \retval >0 argument of the call is greater than called SBuf
     * \retval <0 argument of the call is smaller than called SBuf
     * \retval 0  argument of the call has the same contents of called SBuf
     */
    int compare(const SBuf &S, SBufCaseSensitive isCaseSensitive = caseSensitive, size_type n = npos) const;

    /** check whether the entire supplied argument is a prefix of the SBuf.
     *  \param S the prefix to match against
     *  \param isCaseSensitive one of caseSensitive or caseInsensitive
     *  \retval true argument is a prefix of the SBuf
     */
    bool startsWith(const SBuf &S, SBufCaseSensitive isCaseSensitive = caseSensitive) const;

    /** equality check
     */
    bool operator ==(const SBuf & S) const;
    bool operator !=(const SBuf & S) const;
    _SQUID_INLINE_ bool operator <(const SBuf &S) const;
    _SQUID_INLINE_ bool operator >(const SBuf &S) const;
    _SQUID_INLINE_ bool operator <=(const SBuf &S) const;
    _SQUID_INLINE_ bool operator >=(const SBuf &S) const;

    /** Consume bytes at the head of the SBuf
     *
     * Consume N chars at SBuf head, or to SBuf's end,
     * whichever is shorter. If more bytes are consumed than available,
     * the SBuf is emptied
     * \param n how many bytes to remove; could be zero.
     *     SBuf::npos (or no argument) means 'to the end of SBuf'
     * \return a new SBuf containing the consumed bytes.
     */
    SBuf consume(size_type n = npos);

    /** gets global statistic informations
     *
     */
    static const SBufStats& GetStats();

    /** Copy SBuf contents into user-supplied C buffer.
     *
     * Export a copy of the SBuf's contents into the user-supplied
     * buffer, up to the user-supplied-length. No zero-termination is performed
     * \return num the number of actually-copied chars.
     */
    size_type copy(char *dest, size_type n) const;

    /** exports a pointer to the SBuf internal storage.
     * \warning ACCESSING RAW STORAGE IS DANGEROUS!
     *
     * Returns a pointer to SBuf's content. No terminating null character
     * is appended (use c_str() for that).
     * The returned value points to an internal location whose contents
     * are guaranteed to remain unchanged only until the next call
     * to a non-constant member function of the SBuf object. Such a
     * call may be implicit (e.g., when SBuf is destroyed
     * upon leaving the current context).
     * This is a very UNSAFE way of accessing the data.
     * This call never returns NULL.
     * \see c_str
     * \note the memory management system guarantees that the exported region
     *    of memory will remain valid if the caller keeps holding
     *    a valid reference to the SBuf object and does not write or append to
     *    it. For example:
     * \code
     * SBuf foo("some string");
     * const char *bar = foo.rawContent();
     * doSomething(bar); //safe
     * foo.append(" other string");
     * doSomething(bar); //unsafe
     * \endcode
     */
    const char* rawContent() const;

    /** Exports a writable pointer to the SBuf internal storage.
     * \warning Use with EXTREME caution, this is a dangerous operation.
     *
     * Returns a pointer to the first unused byte in the SBuf's storage,
     * to be used for writing. If minsize is specified, it is guaranteed
     * that at least minsize bytes will be available for writing. Otherwise
     * it is guaranteed that at least as much storage as is currently
     * available will be available for the call. A COW will be performed
     * if necessary to ensure that a following write will not trample
     * a shared MemBlob. The returned pointer must not be stored, and will
     * become invalid at the first call to a non-const method call
     * on the SBuf.
     * This call guarantees to never return NULL
     * This call always forces a cow()
     * \throw SBufTooBigException if the user tries to allocate too big a SBuf
     */
    char *rawSpace(size_type minSize = npos);

    /** Force a SBuf's size
     * \warning use with EXTREME caution, this is a dangerous operation
     *
     * Adapt the SBuf internal state after external interference
     * such as writing into it via rawSpace.
     * \throw TextException if we
     */
    void forceSize(size_type newSize);

    /** exports a null-terminated reference to the SBuf internal storage.
     * \warning ACCESSING RAW STORAGE IS DANGEROUS! DO NOT EVER USE
     *  THE RETURNED POINTER FOR WRITING
     *
     * The returned value points to an internal location whose contents
     * are guaranteed to remain unchanged only until the next call
     * to a non-constant member function of the SBuf object. Such a
     * call may be implicit (e.g., when SBuf is destroyed
     * upon leaving the current context).
     * This is a very UNSAFE way of accessing the data.
     * This call never returns NULL.
     * \see rawContent
     * \note the memory management system guarantees that the exported region
     *    of memory will remain valid if the caller keeps holding
     *    a valid reference to the SBuf object and does not write or append to
     *    it
     */
    const char* c_str();

    /** Returns the number of bytes stored in SBuf.
     */
    _SQUID_INLINE_ size_type length() const;

    /** Get the length of the SBuf, as a signed integer
     *
     * Compatibility function for printf(3) which requires a signed int
     * \throw SBufTooBigException if the SBuf is too big for a signed integer
     */
    _SQUID_INLINE_ int plength() const;

    /** Check whether the SBuf is empty
     *
     * \return true if length() == 0
     */
    _SQUID_INLINE_ bool isEmpty() const;

    /** Request to extend the SBuf's free store space.
     *
     * After the reserveSpace request, the SBuf is guaranteed to have at
     * least minSpace bytes of append-able backing store (on top of the
     * currently-used portion).
     * \throw SBufTooBigException if the user tries to allocate too big a SBuf
     */
    void reserveSpace(size_type minSpace);

    /** Request to resize the SBuf's store
     *
     * After this method is called, the SBuf is guaranteed to have at least
     * minCapcity bytes of total space, including the currently-used portion
     * \throw SBufTooBigException if the user tries to allocate too big a SBuf
     */
    void reserveCapacity(size_type minCapacity);

    /** slicing method
     *
     * Removes SBuf prefix and suffix, leaving a sequence of <i>n</i>
     * bytes starting from position <i>pos</i> first byte is at pos 0.
     * \param pos start sub-stringing from this byte. If it is
     *      greater than the SBuf length, the SBuf is emptied and
     *      an empty SBuf is returned
     * \param n maximum number of bytes of the resulting SBuf.
     *     SBuf::npos means "to end of SBuf".
     *     if 0 returns an empty SBuf.
     */
    SBuf& chop(size_type pos, size_type n = npos);

    /** Remove characters in the toremove set at the beginning, end or both
     *
     * \param toremove characters to be removed. Stops chomping at the first
     *        found char not in the set
     * \param atBeginning if true (default), strips at the beginning of the SBuf
     * \param atEnd if true (default), strips at the end of the SBuf
     */
    SBuf& trim(const SBuf &toRemove, bool atBeginning = true, bool atEnd = true);

    /** Extract a part of the current SBuf.
     *
     * Return a fresh a fresh copy of a portion the current SBuf, which is left untouched.
     * \see trim
     */
    SBuf substr(size_type pos, size_type n = npos) const;

    /** Find first occurrence of character in SBuf
     *
     * Returns the index in the SBuf of the first occurrence of char c.
     * \return SBuf::npos if the char was not found
     * \param startPos if specified, ignore any occurrences before that position
     *     if startPos is SBuf::npos, npos is always returned
     *     if startPos is < 0, it is ignored
     */
    size_type find(char c, size_type startPos = 0) const;

    /** Find first occurrence of SBuf in SBuf.
     *
     * Returns the index in the SBuf of the first occurrence of the
     * sequence contained in the str argument.
     * \param startPos if specified, ignore any occurrences before that position
     *     if startPos is SBuf::npos, npos is always returned
     *     if startPos is < 0, it is ignored
     * \return SBuf::npos if the SBuf was not found
     */
    size_type find(const SBuf & str, size_type startPos = 0) const;

    /** Find last occurrence of character in SBuf
     *
     * Returns the index in the SBuf of the last occurrence of char c.
     * \return SBuf::npos if the char was not found
     * \param endPos if specified, ignore any occurrences after that position.
     *   if unspecified or npos, the whole SBuf is considered.
     *   If < 0, npos is returned
     */
    size_type rfind(char c, size_type endPos = npos) const;

    /** Find last occurrence of SBuf in SBuf
     *
     * Returns the index in the SBuf of the last occurrence of the
     * sequence contained in the str argument.
     * \return SBuf::npos if the sequence  was not found
     * \param endPos if specified, ignore any occurrences after that position
     *   if unspecified or npos, the whole SBuf is considered
     *   if < 0, then npos is always returned
     */
    size_type rfind(const SBuf &str, size_type endPos = npos) const;

    /** Find first occurrence of character of set in SBuf
     *
     * Finds the first occurrence of ANY of the characters in the supplied set in
     * the SBuf.
     * \return SBuf::npos if no character in the set could be found
     * \param startPos if specified, ignore any occurrences before that position
     *   if SBuf::npos, then npos is always returned
     *   if <0, it is ignored.
     */
    size_type find_first_of(const SBuf &set, size_type startPos = 0) const;

    /** sscanf-alike
     *
     * sscanf re-implementation. Non-const, and not \0-clean.
     * \return same as sscanf
     * \see man sscanf(3)
     */
    int scanf(const char *format, ...);

    /** Lower-case SBuf
     *
     * Returns a lower-cased COPY of the SBuf
     * \see man tolower(3)
     */
    SBuf toLower() const;

    /** Upper-case SBuf
     *
     * Returns an upper-cased COPY of the SBuf
     * \see man toupper(3)
     */
    SBuf toUpper() const;

    /** String export function
     * converts the SBuf to a legacy String, by copy. Transitional.
     */
    String toString() const;

    /// TODO: possibly implement erase() similar to std::string's erase
    /// TODO: possibly implement a replace() call
private:

    MemBlob::Pointer store_; ///< memory block, possibly shared with other SBufs
    size_type off_; ///< our content start offset from the beginning of shared store_
    size_type len_; ///< number of our content bytes in shared store_
    static SBufStats stats; ///< class-wide statistics

    const InstanceId<SBuf> id; ///< blob identifier

    _SQUID_INLINE_ static MemBlob::Pointer GetStorePrototype();

    _SQUID_INLINE_ char * buf() const;
    _SQUID_INLINE_ char * bufEnd() const;
    _SQUID_INLINE_ const size_type estimateCapacity(size_type desired) const;
    void reAlloc(size_type newsize);

    _SQUID_INLINE_ bool cow(size_type minsize = npos);

    void checkAccessBounds(size_type pos) const;
    _SQUID_INLINE_ int commonCompareChecksPre(const SBuf &S) const;
    _SQUID_INLINE_ int commonCompareChecksPost(const SBuf &S) const;

};

/**
 * Prints a SBuf to the supplied stream, allowing for chaining
 */
std::ostream& operator <<(std::ostream &os, const SBuf &S);

#if _USE_INLINE_
#include "SBuf.cci"
#endif

#endif /* SQUID_SBUF_H */
