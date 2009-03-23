/* $Id: healthd.c 2306 2008-11-20 13:19:07Z tomazb $ */

//! \file healthd.c 
//! Implements Libera Health (temperature) monitor daemon.

/*
LIBERA HEALTH DAEMON - Libera GNU/Linux daemons
Copyright (C) 2007 Instrumentation Technologies

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
or visit http://www.gnu.org

*/

#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <syslog.h>
#include <stdbool.h>

#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/select.h>

#include <math.h>

#include "libera.h"

#include "libera_health.h"
#include "debug.h"



//--------------------------------------------------------------------------
// Globals.

/** Pointer to the application file name. */
const char *argv0 = 0;


/** Reference temperature */
double ref_temp;

/** Reference temperature input flag */
int ref_temp_in = FALSE;

/** Maximal allowed temperature */
double max_temp;

/** Maximal temperature input flag */
int max_temp_in = FALSE;

/** Default reference fan revolution */
int ref_rpm = HEALTHD_DEFAULT_RPM;

/** Sampling time */
unsigned int t_s = HEALTHD_DEFAULT_SAMP_TIME;

/** Debug filename */
char *debug_fname = "/tmp/healthd_debug.dat";

/** Debug file pointer */
FILE *f_debug = NULL;

/** Debug flag */
int debug = FALSE;


/** Libera type flag */
int l_type;



/** Whether to detach from stdio as a daemon: set to false for debug. */
static int daemon_mode = TRUE;

/** Whether to execute system HW reset at temperature too high. */
static int do_hw_reset = TRUE;




//--------------------------------------------------------------------------

/** Print diagnostic message and exit.
 *  @param function Function name.
 *  @param line Line number.
 *  @param what Error message.
 */
void die(const char *function, int line, const char *what)
{
    syslog(LOG_CRIT,
           "System error in function `%s': line %d: `%s' -- %s.",
           function, line, what, (errno ? strerror(errno) : "(n/a)"));

    exit(EXIT_FAILURE);
}


//--------------------------------------------------------------------------

/* Sensor detection. */

#define I2C_DEVICE "/sys/class/i2c-adapter/i2c-0/device/"

const char * temp_electron   = I2C_DEVICE "0-0029/temp1_input";
const char * temp_brilliance = I2C_DEVICE "0-0018/temp1_input";
const char * fan_front_tach  = I2C_DEVICE "0-004b/fan1_input";
const char * fan_front_speed = I2C_DEVICE "0-004b/speed";
const char * fan_back_tach   = I2C_DEVICE "0-0048/fan1_input";
const char * fan_back_speed  = I2C_DEVICE "0-0048/speed";

const char * temp_sensor;   // Either electron or brilliance sensor


bool read_sensor(const char *sensor, int *value)
{
    bool Ok = false;
    FILE * input = fopen(sensor, "r");
    if (input == NULL)
        _LOG_CRIT("Unable to read sensor %s (%d)", sensor, errno);
    else
    {
        Ok = fscanf(input, "%d", value) == 1;
        fclose(input);
    }
    return Ok;
}


bool write_sensor(const char *sensor, int value)
{
    bool Ok = false;
    FILE * output = fopen(sensor, "w");
    if (output == NULL)
        _LOG_CRIT("Unable to write sensor %s (%d)", sensor, errno);
    else
    {
        Ok = fprintf(output, "%d", value) > 0;
        fclose(output);
    }
    return Ok;
}



//--------------------------------------------------------------------------

/** Find if a process exists.
 *  Returns 1 (exists) or 0 (does not exist).
 *  @param fname Pointer to pid filename.
 */
int find_instance(const char *fname)
{
    FILE *fp = fopen(fname, "r");
    if (!fp) 
    {

        if (ENOENT != errno)
            EXIT("fopen");
        return 0;
    }
    _LOG_WARNING("found existing pid file %s", fname);

    int rc = 0;
    char *line = 0;
    size_t size = 0;

    if (-1 != getline(&line, &size, fp)) 
    {
        const pid_t pid = atol(line);
        const int no_signal = 0;

        if (0 == kill(pid, no_signal))
            rc = 1;
        else if (errno != ESRCH)
            EXIT("kill");
    }

    if (line)
        free(line);

    if (0 != fclose(fp))
        EXIT("fclose");

    return rc;
}


//--------------------------------------------------------------------------

/** Cleanup function.
 *  Remove the process identification (PID) file.
 *  Set fans' speed to HEALTHD_DEFAULT_RPM.
 *  Cleanup sensors.
 */
void cleanup()
{
    // Remove pid file
    if (0 != unlink(HEALTHD_PID_PATHNAME)) 
        _LOG_ERR("Failed to unlink %s: %s.",
                 HEALTHD_PID_PATHNAME, strerror(errno));
    else
        _LOG_DEBUG("Removed PID file %s.", HEALTHD_PID_PATHNAME);

    /* Set fans to their default speeds. */
    write_sensor(fan_front_speed, HEALTHD_DEFAULT_RPM);
    write_sensor(fan_back_speed,  HEALTHD_DEFAULT_RPM);

    // Close debug file
    if (f_debug)
        fclose(f_debug);
}


//--------------------------------------------------------------------------

volatile sig_atomic_t termination_in_progress = 0;

/** Signal handler.
 *  Handle SIGINT (Ctrl-C) and other termination signals to allow the
 *  application to terminate gracefully (after cleanup).
 *  @param signo Signal number.
 */
void signal_handler(int signo)
{
    // Since this handler is established for more than one kind of signal,
    // it might still get invoked recursively by delivery of some other kind
    // of signal. Use a static variable to keep track of that.
    if (termination_in_progress)
        raise(signo);
    termination_in_progress = 1;

    // Do not use a _LOG_NOTICE macro!
    // We want the following logged regardless of the current log level.
    syslog(LOG_NOTICE, "Caught signal %d, shutting down.", signo);

    // Now do the cleanup.
    cleanup();

    // Next, restore the signal's default handling and reraise the signal to
    // terminate the process.
    _LOG_INFO("Re-raising signal %d.", signo);

    signal(signo, SIG_DFL);
    raise(signo);
}




//--------------------------------------------------------------------------

/** Check hardware (Brilliance or Electron)
 *  Returns 0.
 */
int hw_check()
{
    /* Check for presence of extra sensor on Brilliance board. */
    int temp;
    if (read_sensor(temp_brilliance, &temp))
    {
        _LOG_CRIT("Brilliance: %d", temp);
        l_type = BRILLIANCE;
    }
    else
    {
        _LOG_CRIT("Electron");
        l_type = ELECTRON;
    }
    
#if 0
    int rc = 0;
    static int config_fd = -1;
    libera_cfg_request_t req;

    config_fd = open(LIBERA_CONFIG_FIFO_PATHNAME, O_RDONLY);
    if (-1 == config_fd) 
    {
        _LOG_CRIT("Could not open libera config (%s). Error: %s.",
                 LIBERA_CONFIG_FIFO_PATHNAME, strerror(errno));
        return -1;
    }

    req.idx = LIBERA_CFG_FEATURE_ITECH;
    rc = ioctl(config_fd, LIBERA_IOC_GET_CFG, &req);
    if (rc)
    {
        _LOG_CRIT("Fail to send LIBERA_CFG_FEATURE_ITECH ioctl. Error: %s.",
                strerror(errno));
        return -1;
    }

    if (LIBERA_IS_BRILLIANCE(req.val))
    {
        l_type = BRILLIANCE;
    }
    else
    {
        //NOTE: Not necessary only Electron HW. It can be also BunchByBunch, Hadron, etc.
        // in general it means NOT Brilliance.
        l_type = ELECTRON;
    }

    close(config_fd);

#else
    l_type = BRILLIANCE;
#endif
    return 0;
}


//--------------------------------------------------------------------------

/** Set default values for Brilliance or Electron */
void set_default_values()
{
    if (l_type == BRILLIANCE) 
    {
        if (!ref_temp_in)
            ref_temp = HEALTHD_DEFAULT_TREF_BRILL;

        if (ref_temp > HEALTHD_MAX_TREF_BRILL) 
        {
            ref_temp = HEALTHD_MAX_TREF_BRILL;
            _LOG_CRIT("Reference temperature is set to %dC",
                      HEALTHD_MAX_TREF_BRILL);
        }

        if (!max_temp_in)
            max_temp = HEALTHD_DEFAULT_TMAX_BRILL;

        temp_sensor = temp_brilliance;
//         strcpy(temp_sensors_prefix, SENSORS_ADM1023_PREFIX);
//         temp_data_feature = SENSORS_ADM1021_TEMP;
    } 
    else 
    {
        if (!ref_temp_in)
            ref_temp = HEALTHD_DEFAULT_TREF_ELECTR;

        if (ref_temp > HEALTHD_MAX_TREF_ELECTR) 
        {
            ref_temp = HEALTHD_MAX_TREF_ELECTR;
            _LOG_CRIT("Reference temperature is set to %dC",
                      HEALTHD_MAX_TREF_ELECTR);
        }

        if (!max_temp_in)
            max_temp = HEALTHD_DEFAULT_TMAX_ELECTR;

        temp_sensor = temp_electron;
//         strcpy(temp_sensors_prefix, SENSORS_MAX1617A_PREFIX);
//         temp_data_feature = SENSORS_MAX1617A_TEMP;
    }

    // Do not allow max temp below ref. temp.
    if (max_temp < (ref_temp + 1)) 
    {
        _LOG_CRIT("Maximum temperature set to low. Ref=%2.1fC Max=%2.1fC.",
                 ref_temp, max_temp);
        max_temp = ref_temp + 1;
    }
}


//--------------------------------------------------------------------------

/** Initialize this instance -- i.e. register signal handler,
 *  atexit handler, create a process identification (PID) file and
 *  daemonize this instance.
 *  Initialize sensors.
 *  Returns 0.
 */
int init()
{
    int nochdir = 0, noclose = 0;
    int log_options = LOG_PID;


#if DEBUG                       // defined(DEBUG) && DEBUG != 0
    noclose = 1;
    log_options |= LOG_PERROR;  // Print to stderr as well.
#endif                          // DEBUG

    if (daemon_mode)
        // Deamonize this process.
        VERIFY(0 == daemon(nochdir, noclose));

    // Note: closelog() is optional and therefore not used.
    openlog(argv0, log_options, 0);

    // Install cleanup handler.
    VERIFY(0 == atexit(cleanup));

    // Setup signal handler.
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = signal_handler;

    // Handle Ctrl-C and regular termination requests.
    const int sigs[] = { SIGINT, SIGHUP, SIGTERM, SIGQUIT };

    for (unsigned int i = 0; i < sizeof(sigs) / sizeof(int); ++i)
    {
        if (0 != sigaction(sigs[i], &sa, 0))
            EXIT("sigaction");
    }

    umask(0);

    if (0 != find_instance(HEALTHD_PID_PATHNAME))
    {
        _LOG_CRIT("Cannot run more than one daemon instance.");
        exit(EXIT_FAILURE);
    }

    // Create a pid file before the blocking trigger functions.
    FILE *fp = fopen(HEALTHD_PID_PATHNAME, "w");
    if (!fp)
        EXIT("fopen");

    fprintf(fp, "%d\n", getpid());
    if (0 != fclose(fp))
        EXIT("fclose");

    _LOG_DEBUG("Created pid file %s.", HEALTHD_PID_PATHNAME);


    // Check for Brilliance.
    if (-1 == hw_check())
        return -1;

    set_default_values();


    // Debug output
    if (debug)
        f_debug = fopen(debug_fname, "w");

    return 0;
}


//--------------------------------------------------------------------------

/** Controller
 *  Returns control signal.
 */
int controller(double err, double *integ)
{
    // Controller parameters
    double kp, ki;
    if (l_type == BRILLIANCE) 
    {
        kp = -160; //-210;
        ki = -100; //-160;
    } else {
        kp = -40;
        ki = -40;
    }


    // PI controller
    *integ += err;
    int rpm = (int) (ref_rpm + err * kp + *integ * ki);


    // anti-windup
    if (rpm > HEALTHD_MAX_RPM) 
    {
        rpm = HEALTHD_MAX_RPM;
        *integ -= err;
    }

    if (rpm < HEALTHD_MIN_RPM) 
    {
        rpm = HEALTHD_MIN_RPM;
        *integ -= err;
    }

    return rpm;
}


//--------------------------------------------------------------------------

/** Main daemon's function.
 *  Read sensors' values.
 *  Run control algorithm.
 *  Set fans' speeds.
 *  Report critical situations.
 *  Returns 0.
 */
int run()
{
    unsigned int i_f = 1, i_b = 1, i_t = 1, i_h = 0, i_c = 0;   // report flags

    // Do not use a _LOG_NOTICE macro!
    // We want the following logged regardless of the current log level.
    syslog(LOG_NOTICE,
           "%s %s configured -- resuming normal operations.",
           argv0, XSTR(RELEASE_VERSION));

    syslog(LOG_NOTICE,
           "Libera-%s, reference: %2.1fC, "
           "maximum %2.1fC for HW reset%s.",
           ((l_type == BRILLIANCE) ? "brilliance":"electron"), 
           ref_temp, max_temp, (do_hw_reset ? "":" no set"));

    write_sensor(fan_front_speed, ref_rpm);
    write_sensor(fan_back_speed,  ref_rpm);
    int rpm_old = ref_rpm;
    read_sensor(fan_front_tach, &ref_rpm);

    sleep(t_s);

    /* Main control loop */
    double integ = 0.0;
    while (1) 
    {

        // Read temperature and fan speed
        int temp000;
        read_sensor(temp_sensor, &temp000);
        double cur_temp = temp000 * 1e-3;

        int set_rpm, tach_f, tach_b;
        read_sensor(fan_front_speed, &set_rpm);
        read_sensor(fan_front_tach, &tach_f);
        read_sensor(fan_back_tach, &tach_b);

        double err = ref_temp - cur_temp;
        int rpm = controller(err, &integ);  // control signal

        _LOG_DEBUG("ref_temp=%2.0fC max_temp=%2.1fC cur_temp=%2.1fC "
                   "set_rpm=%4.0f tach_f=%4.0f tach_b=%4.0f.",
                   ref_temp, max_temp, cur_temp, set_rpm, tach_f, tach_b);

        // Temperature protection
        if ((cur_temp > max_temp) && (rpm < HEALTHD_MAX_RPM)) 
        {
            rpm = HEALTHD_MAX_RPM;
            integ -= err;
        }

        // Set fan speed
        if (rpm != rpm_old) 
        {
            write_sensor(fan_front_speed, rpm);
            write_sensor(fan_back_speed, rpm);
            rpm_old = rpm;
        }

        /* report */
        // Fan fault
        if ((set_rpm - tach_f >= HEALTHD_MAX_RPM_ERROR) && i_f) 
        {
            _LOG_CRIT("Front fan fault.");
            i_f = 0;
        }

        if ((set_rpm - tach_f < HEALTHD_MAX_RPM_ERROR) && !i_f) 
        {
//            _LOG_CRIT( "Front fan works properly" );
            i_f = 1;
        }

        if ((set_rpm - tach_b >= HEALTHD_MAX_RPM_ERROR) && i_b) 
        {
            _LOG_CRIT("Back fan fault.");
            i_b = 0;
        }

        if ((set_rpm - tach_b < HEALTHD_MAX_RPM_ERROR) && !i_b) {
//            _LOG_CRIT( "Back fan works properly" );
            i_b = 1;
        }

        // Exceeded max. allowed temperature
        if (cur_temp > max_temp) 
        {
            _LOG_CRIT("Current temperature (%2.0fC) is over max. Max allowed temperature (%2.0fC).",
                 cur_temp, max_temp);
            i_t = 0;

            if (do_hw_reset)
            {
                syslog(LOG_INFO, "** Rebooting the system **");
                sync();
                sleep(1);
                if (reboot(RB_AUTOBOOT))
                {
                    _LOG_CRIT("Failed in reboot system. Error=%s.",
                             strerror(errno));
                }
                sleep(10);
            }
        }

        if ((cur_temp < max_temp) && !i_t)
        {
            _LOG_CRIT("Current temperature (%2.0fC) dropped below max. Max allowed temperature (%2.0fC).",
                 cur_temp, max_temp);
            i_t = 1;
        }

        // Cannot reach ref_temp
        if ((cur_temp > ref_temp) && (rpm == HEALTHD_MAX_RPM)) 
        {
            i_h++;
            if (i_h == 5)       // after 5*t_s
                _LOG_INFO("Ref_temp %2.0fC cannot be reached. Cur temp (%2.0fC) too hot.",
                          ref_temp, cur_temp);
        }

        if ((cur_temp <= ref_temp) && (i_h >= 5))
            _LOG_INFO("Ref_temp %2.0fC was reached.", ref_temp);

        if (cur_temp <= ref_temp)
            i_h = 0;

        if ((cur_temp < ref_temp) && (rpm == HEALTHD_MIN_RPM)) 
        {
            i_c++;
            if (i_c == 5)       // after 5*t_s
                _LOG_INFO("Ref_temp %2.0fC cannot be reached. Cur temp (%2.0fC) too cold.",
                          ref_temp, cur_temp);
        }

        if ((cur_temp >= ref_temp) && (i_c >= 5))
            _LOG_INFO("Ref_temp %2.0fC was reached.",
                       ref_temp);

        if (cur_temp >= ref_temp)
            i_c = 0;

        // Debug output
        if (debug) 
        {
            fprintf(f_debug, "%d     %d     %d     %2.1f\n",
                    set_rpm, tach_f, tach_b, cur_temp);
            fflush(f_debug);
        }
        /* end of report */

        sleep(t_s);
    } // while(1)

    return 0;
}


//--------------------------------------------------------------------------

/** Print usage information. */
void usage()
{
    const char *format =
        "Usage: %s [OPTION]...\n"
        "\n"
        "-r temperature  Reference temperature.\n"
        "-m temperature  Max temperature for system reset.\n"
        "-t file         Test mode. Write debug signals to file.\n"
        "-w reset        Reset system if temperature too high (default = %d).\n"
        "-n              Non-daemon: do not run as daemon, debug mode.\n"
        "-h              Print this message and exit.\n"
        "-v              Print version information and exit.\n" "\n";

    fprintf(stderr, format, argv0, do_hw_reset);
}


//--------------------------------------------------------------------------

/** Print version information. */
void version()
{
    const char *format =
        "%s %s (%s %s)\n"
        "\n"
        "Copyright 2004, 2005 Instrumentation Technologies.\n"
        "This is free software; see the source for copying conditions. "
        "There is NO warranty; not even for MERCHANTABILITY or FITNESS "
        "FOR A PARTICULAR PURPOSE.\n\n";

    printf(format, argv0, XSTR(RELEASE_VERSION), __DATE__, __TIME__);
}


//--------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    // Make argv0 point to the file name part of the path name.
    argv0 = strrchr(argv[0], '/');
    if (argv0) {

        ASSERT(0 != (argv0 + 1));
        argv0 += 1;             // Skip the '/'.
    } else
        argv0 = argv[0];        // Use full pathname instead.

    int ch = -1;
    while ((ch = getopt(argc, argv, "nw:hr:m:t:v")) != -1) 
    {
        switch (ch) {

        case 'r':
            ref_temp = atof(optarg);
            ref_temp_in = TRUE;
            break;

        case 'm':
            max_temp = atof(optarg);
            max_temp_in = TRUE;
            break;

        case 'h':
            hw_check();
            set_default_values();
            usage();
            exit(EXIT_SUCCESS);

        case 'v':
            version();
            exit(EXIT_SUCCESS);

        case 't':
            debug = TRUE;
            debug_fname = optarg;
            break;

        case 'n': // non-deamon mode.
            daemon_mode = FALSE;
            break;

        case 'w':
            do_hw_reset = atoi(optarg);
            break;

            default:
            exit(EXIT_FAILURE);
        }
    }

    if (0 == init())
    {
        run();
    }
    else 
    {
        syslog(LOG_NOTICE, "Failed to initialize.");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
