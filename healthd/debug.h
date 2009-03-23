// $Id: debug.h 2364 2008-12-18 12:56:11Z matejk $

//! \file debug.h
//! Debugging macros and declarations.

#if !defined(_DEBUG_H)
#define _DEBUG_H

#if defined( __cplusplus )
extern "C"
{
#endif
	#include <syslog.h>
	#include <errno.h>
#if defined( __cplusplus )
}
#endif

#if defined( DEBUG )
	#if defined( __cplusplus )
		#include <cassert>
	#else
		#include <assert.h>
	#endif
#endif  // DEBUG

//--------------------------------------------------------------------------
// Debugging.

#if !defined(DEBUG)
#define DEBUG 0
#endif

#if DEBUG	// defined(DEBUG) && DEBUG>0

#define ASSERT(f)		assert(f)
#define VERIFY(f)		ASSERT(f)
#define DEBUG_ONLY(f)	(f)

#define _SYSLOG( level, format, ... ) \
        syslog( LOG_ERR, "[%05ld] " format, (long int)syscall(224), ##__VA_ARGS__ )

#else	// DEBUG

#define ASSERT(f)		((void)0)
#define VERIFY(f)		((void)(f))
#define DEBUG_ONLY(f)	((void)0)

#define _SYSLOG( level, format, ... ) \
        syslog( LOG_ERR, format, ##__VA_ARGS__ )

#endif	// !DEBUG

#if DEBUG < 3
#define DEBUG_ONLY_3(f)	((void)0)
#else
#define DEBUG_ONLY_3(f)	(f)
#endif

#if DEBUG < 2
#define DEBUG_ONLY_2(f)	((void)0)
#else
#define DEBUG_ONLY_2(f)	(f)
#endif

#if DEBUG < 1
#define DEBUG_ONLY_1(f)	((void)0)
#else
#define DEBUG_ONLY_1(f)	(f)
#endif

#define DEBUG_ONLY_0(f)	(f)

// The `##' token paste operator has a special meaning when placed between
// a comma and a variable argument. If you write
// #define eprintf(format, ...) fprintf (stderr, format, ##__VA_ARGS__)
// and the variable argument is left out when the eprintf macro is used,
// then the comma before the `##' will be deleted.
// This does not happen if you pass an empty argument, nor does it happen
// if the token preceding `##' is anything other than a comma.

/** Send crtitical message to the system logger. */
#define _LOG_CRIT( format, ... ) \
        DEBUG_ONLY_0( _SYSLOG( LOG_ERR, format, ##__VA_ARGS__ ) )

/** Send error message to the system logger. */
#define _LOG_ERR( format, ... ) \
        DEBUG_ONLY_1( _SYSLOG( LOG_ERR, format, ##__VA_ARGS__ ) )

/** Send warning message to the system logger. */
#define _LOG_WARNING( format, ... ) \
        DEBUG_ONLY_1( _SYSLOG( LOG_WARNING, format, ##__VA_ARGS__ ) )

/** Send normal, but significant message to the system logger. */
#define _LOG_NOTICE( format, ... ) \
        DEBUG_ONLY_2( _SYSLOG( LOG_NOTICE, format, ##__VA_ARGS__ ) )

/** Send informational message to the system logger. */
#define _LOG_INFO( format, ... ) \
        DEBUG_ONLY_2( _SYSLOG( LOG_INFO, format, ##__VA_ARGS__ ) )

/** Send debug-level message to the system logger. */
#define _LOG_DEBUG( format, ... ) \
        DEBUG_ONLY_3( _SYSLOG( LOG_DEBUG, format, ##__VA_ARGS__ ) )

/** Dumps expression to STDERR.
 *  This macro is only available in DEBUG build.
 *  Takes a format string as used in the run-time function printf.
 */
#define TRACE( f ) DEBUG_ONLY( fprintf( stderr, f ) )

/** Same as TRACE, but takes a format string plus one argument
 *  (one variable that is dumped to STDERR).
 */
#define TRACE1( f, p ) DEBUG_ONLY( fprintf( stderr, f, p ) )

/** Same as TRACE, but takes a format string plus two arguments
 *  (two variables that are dumped to STDERR).
 */
#define TRACE2( f, p, q ) DEBUG_ONLY( fprintf( stderr, f, p, q ) )

/** Same as TRACE, but takes a format string plus three arguments
 *  (three variables that are dumped to STDERR).
 */
#define TRACE3( f, p, q, r ) DEBUG_ONLY( fprintf( stderr, f, p, q, r ) )

//--------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

//...

#ifdef __cplusplus
}
#endif
#endif	// DEBUG_H
