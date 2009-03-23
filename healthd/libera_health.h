/* $Id: libera_health.h 2192 2008-10-07 09:13:06Z matejk $ */

//! \file libera_health.h
//! Declares interface for Libera HEALTH daemon.

#if !defined(_LIBERA_HEALTH_H)
#define _LIBERA_HEALTH_H


#ifdef __cplusplus
extern "C" {
#endif

/** HEALTHD process identifier (PID) pathname. */
#define HEALTHD_PID_PATHNAME    "/var/run/healthd.pid"

/** Helper macro to stringify the expanded argument. */
#define XSTR(s) STR(s)

/** Stringification macro. */
#define STR(s) #s

/** Helper macro. Print diagnostic system message and exit. */
#define EXIT(what) die( __FUNCTION__, __LINE__, what )

/** Helper macro. Return larger of a and b. */
#define MAX(a,b) ((a)>(b) ? a : b)

/** Helper macro. Return lesser of a and b. */
#define MIN(a,b) ((a)<(b) ? a : b)


/** HEALTHD defines */

/** Fan revolution */
#define HEALTHD_DEFAULT_RPM    4409
#define HEALTHD_MAX_RPM        5700
#define HEALTHD_MIN_RPM        2500
#define HEALTHD_MAX_RPM_ERROR  1000

#define HEALTHD_DEFAULT_SAMP_TIME 60

/** Brilliance */
#define HEALTHD_DEFAULT_TREF_BRILL    53
#define HEALTHD_MAX_TREF_BRILL        58
#define HEALTHD_DEFAULT_TMAX_BRILL    80

/** Electron */
#define HEALTHD_DEFAULT_TREF_ELECTR    43
#define HEALTHD_MAX_TREF_ELECTR        47
#define HEALTHD_DEFAULT_TMAX_ELECTR    70

/** Libera type */
#define BRILLIANCE    1
#define ELECTRON      0

#define TRUE     1
#define FALSE    0


//--------------------------------------------------------------------------
// Interface.


#ifdef __cplusplus
}
#endif
#endif                          // _LIBERA_HEALTH_H
