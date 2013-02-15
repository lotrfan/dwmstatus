/* made by profil 2011-12-29.
 **
 ** Compile with:
 ** gcc -Wall -pedantic -std=c99 -lX11 status.c
 */

// TODO: switch to using *n functions for strings (that take the number of char's as arg)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <strings.h>
#define __USE_BSD 1 // for strdup
#include <string.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <errno.h>

#include <sys/types.h>
#include <dirent.h>

#include <sys/socket.h>
#include <linux/socket.h>
#include <linux/wireless.h>
#include <linux/if.h>
#include <iwlib.h>

#define _GNU_SOURCE
#include <pulse/pulseaudio.h>

#define COLOR_NORMAL 0
#define COLOR_CRITICAL 5
#define COLOR_WARNING 4

#define _BSTART " "
#define _BEND ""
#define _BSEP " "
#define BSTART "\x0B" _BSTART
#define BEND _BEND "\x01"

#define WIFI_33 ""
#define WIFI_66 ""
#define WIFI_100 ""
#define WIRED ""
#define NET_UP ""
#define NET_DOWN ""
#define RAM ""
#define VOL_MUTE ""
#define VOL_UNMUTE ""
#define BATT_25 ""
#define BATT_50 ""
#define BATT_75 ""
#define BATT_100 ""
#define BATT_CHARGING ""
#define BATT_AC ""
#define XAUTOLOCK ""

static Display *dpy;

enum {BattFull, BattCharging, BattDischarging};
struct BatteryInfo {
    int percent;
    int ac;
    int status;
    int hours;
    int minutes;
    int seconds;
};

struct CPUStat {
    long unsigned int lastcols[4];
    int usage;
};

struct NetSpeed {
    float wiredUp;
    float wiredDown;
    long int wired_rx;
    long int wired_tx;
    float wirelessUp;
    float wirelessDown;
    long int wireless_rx;
    long int wireless_tx;
};

// BEGIN PULSE

#define UNUSED __attribute__((unused))

struct pulseaudio_t {
    pa_context *cxt;
    pa_mainloop *mainloop;

    char *default_sink;
};

static void server_info_cb(pa_context UNUSED *c, const pa_server_info *i, void *raw)
{
    struct pulseaudio_t *pulse = raw;
    pulse->default_sink = strdup(i->default_sink_name);
}

static void connect_state_cb(pa_context *cxt, void *raw)
{
    enum pa_context_state *state = raw;
    *state = pa_context_get_state(cxt);
}


static void pulse_async_wait(struct pulseaudio_t *pulse, pa_operation *op)
{
    while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
        pa_mainloop_iterate(pulse->mainloop, 1, NULL);
}

static void get_default_sink_volume_cb(pa_context UNUSED *c, const pa_sink_info *i, int eol,
        void *raw) {
    if (eol)
        return;


    if (i->mute) {
        (*(int*)raw) = -1;
    } else {
        (*(int*)raw) = (int)(((double)pa_cvolume_avg(&(i->volume)) * 100)
                / PA_VOLUME_NORM);
    }
}
int get_default_sink_volume(struct pulseaudio_t *pulse) {
    pa_operation *op;
    int volume = 0;

    op = pa_context_get_sink_info_by_name(pulse->cxt, pulse->default_sink, get_default_sink_volume_cb, &volume);

    pulse_async_wait(pulse, op);
    pa_operation_unref(op);

    return volume;
}

static int pulse_init(struct pulseaudio_t *pulse)
{
    pa_operation *op;
    enum pa_context_state state = PA_CONTEXT_CONNECTING;

    pulse->mainloop = pa_mainloop_new();
    pulse->cxt = pa_context_new(pa_mainloop_get_api(pulse->mainloop), "bestpony");
    pulse->default_sink = NULL;

    pa_context_set_state_callback(pulse->cxt, connect_state_cb, &state);
    pa_context_connect(pulse->cxt, NULL, PA_CONTEXT_NOFLAGS, NULL);
    while (state != PA_CONTEXT_READY && state != PA_CONTEXT_FAILED)
        pa_mainloop_iterate(pulse->mainloop, 1, NULL);

    if (state != PA_CONTEXT_READY) {
        fprintf(stderr, "failed to connect to pulse daemon: %s\n",
                pa_strerror(pa_context_errno(pulse->cxt)));
        return 1;
    }

    op = pa_context_get_server_info(pulse->cxt, server_info_cb, pulse);
    pulse_async_wait(pulse, op);
    pa_operation_unref(op);
    return 0;
}

static void pulse_deinit(struct pulseaudio_t *pulse)
{
    pa_context_disconnect(pulse->cxt);
    pa_mainloop_free(pulse->mainloop);
    free(pulse->default_sink);
}

// END PULSE

void setstatus(char *str) {
    XStoreName(dpy, DefaultRootWindow(dpy), str);
    XSync(dpy, False);
}
void readfilef(char* filename, float *var) {
    FILE *fd;
    fd = fopen(filename, "r");
    if(fd == NULL) {
        fprintf(stderr, "Error opening %s: %s.\n", filename, strerror(errno));
        return;
    }
    fscanf(fd, "%f", var);
    fclose(fd);
}
void readfileli(char* filename, long int *var) {
    FILE *fd;
    fd = fopen(filename, "r");
    if(fd == NULL) {
        fprintf(stderr, "Error opening %s: %s.\n", filename, strerror(errno));
        return;
    }
    fscanf(fd, "%ld", var);
    fclose(fd);
}
void readfilei(char* filename, int *var) {
    FILE *fd;
    fd = fopen(filename, "r");
    if(fd == NULL) {
        fprintf(stderr, "Error opening %s: %s.\n", filename, strerror(errno));
        return;
    }
    fscanf(fd, "%d", var);
    fclose(fd);
}
void readfiles(char* filename, char var[]) {
    FILE *fd;
    fd = fopen(filename, "r");
    if(fd == NULL) {
        fprintf(stderr, "Error opening %s: %s.\n", filename, strerror(errno));
        return;
    }
    fscanf(fd, "%s", var);
    fclose(fd);
}

pid_t pidof(const char* pname) {
//pid_t proc_find(const char* name) 
//{
    DIR* dir;
    struct dirent* ent;
    char* endptr;
    char buf[100];
    
    if (!(dir = opendir("/proc"))) {
        perror("opendir");
        return -1;
    }

    long lpid = -1;

    while((ent = readdir(dir)) != NULL) {
        /* if endptr is not a null character, the directory is not
         * entirely numeric, so ignore it */
        lpid = strtol(ent->d_name, &endptr, 10);
        if (*endptr != '\0') {
            continue;
        }
        if (lpid < 1000) {
            // Ignore kernel threads
            continue;
        }

        /* try to open the cmdline file */
        snprintf(buf, sizeof(buf), "/proc/%ld/status", lpid);
        FILE* fp = fopen(buf, "r");
        
        if (fp) {
            if (fscanf(fp, "Name: %s ", buf)) {
                if (strcmp(buf, pname) == 0) {
                    fclose(fp);
                    closedir(dir);
                    return (pid_t)lpid;
                }
            }
            fclose(fp);
        }
        
    }
    
    closedir(dir);
    return -1;
}

int wireless_init() {
    int skfd;         /* generic raw socket desc. */

    if((skfd = iw_sockets_open()) < 0) {
        fprintf(stderr, "Cannot open socket\n");
        return -1;
    }
    return skfd;
}
void wireless_close(int skfd) {
    if (skfd == -1) {
        return;
    }
    iw_sockets_close(skfd);
}

int getwireless_essid(int skfd, char essid[]) {
    struct iwreq wrq;

    if (skfd == -1) {
        return 1;
    }

    /* Make sure ESSID is always NULL terminated */
    memset(essid, 0, sizeof(essid));

    /* Get ESSID */
    //wrq.u.essid.pointer = (caddr_t)essid;
    wrq.u.essid.pointer = essid;
    wrq.u.essid.length = IW_ESSID_MAX_SIZE + 1;
    wrq.u.essid.flags = 0;
    if(iw_get_ext(skfd, "wlan0", SIOCGIWESSID, &wrq) < 0) {
        perror("iw_get_ex");
        return 1;
    }

    return 0; // succeeded
}

float getwireless_strength(int skfd) {
    iwstats stats;
    iwrange	range;
    if(iw_get_range_info(skfd, "wlan0", &range) >= 0) {
        if(iw_get_stats(skfd, "wlan0", &stats,
                    &range, 1) >= 0) {
            return (float)stats.qual.qual / (float)range.max_qual.qual;
        }
    }
    return 0;
}

int getwired() {
    int isup;
    readfilei("/sys/class/net/eth0/carrier", &isup);
    return isup;
}
int iswifi() {
    FILE *fd;
    fd = fopen("/sys/class/net/wlan0/carrier", "r");
    if(fd == NULL) {
        return 0;
    }
    fclose(fd);
    return 1;
}
int isbonded() {
    FILE *fd;
    fd = fopen("/sys/class/net/bond0/carrier", "r");
    if(fd == NULL) {
        return 0;
    }
    fclose(fd);
    return 1;
}

/*
 * Returns in MHz
 */
float getfreqf(int cpu) {
    float freq;

    switch (cpu) {
        case 0:
            readfilef("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", &freq);
            break;
        case 1:
            readfilef("/sys/devices/system/cpu/cpu1/cpufreq/scaling_cur_freq", &freq);
            break;
        case 2:
            readfilef("/sys/devices/system/cpu/cpu2/cpufreq/scaling_cur_freq", &freq);
            break;
        case 3:
            readfilef("/sys/devices/system/cpu/cpu3/cpufreq/scaling_cur_freq", &freq);
            break;
    }

    freq /= 1000;
    freq /= 1000;

    return freq;;
}
/*
 * Returns in Hz
 */
int getfreqi(int cpu) {
    int freq;

    switch (cpu) {
        case 0:
            readfilei("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", &freq);
            break;
        case 1:
            readfilei("/sys/devices/system/cpu/cpu1/cpufreq/scaling_cur_freq", &freq);
            break;
        case 2:
            readfilei("/sys/devices/system/cpu/cpu2/cpufreq/scaling_cur_freq", &freq);
            break;
        case 3:
            readfilei("/sys/devices/system/cpu/cpu3/cpufreq/scaling_cur_freq", &freq);
            break;
    }

    freq /= 1000;

    return freq;;
}

void getdatetime(char buf[], int len) {
    time_t result;
    struct tm *resulttm;

    result = time(NULL);
    resulttm = localtime(&result);
    if(resulttm == NULL) {
        fprintf(stderr, "Error getting localtime.\n");
        exit(1);
    }
    if(!strftime(buf, len-1, "%b %d" _BSEP "%H:%M:%S", resulttm)) {
        fprintf(stderr, "strftime is 0.\n");
    }
}


struct BatteryInfo getbattery() {
    struct BatteryInfo info;
    int charge_now, charge_full, current;
    char status[30];

    readfilei("/sys/class/power_supply/BAT0/charge_now", &charge_now);
    readfilei("/sys/class/power_supply/BAT0/charge_full", &charge_full);
    readfilei("/sys/class/power_supply/BAT0/current_now", &current);
    readfiles("/sys/class/power_supply/BAT0/status", status);

    readfilei("/sys/class/power_supply/AC/online", &(info.ac));

    if (!strcasecmp(status, "discharging")) {
        // Battery is discharging
        info.status = BattDischarging;
        info.seconds = (float)3600 * (float)charge_now / (float)current;
    } else if (!strcasecmp(status, "charging")) {
        // Battery is charging
        info.status = BattCharging;
        info.seconds = (float)3600 * (float)(charge_full - (float)charge_now) / (float)current;
    } else if (!strcasecmp(status, "full")) {
        info.status = BattFull;
        info.seconds = 0;
        return info;
    } else {
        fprintf(stderr,"Unknown battery status: %s\n", status);
        // Not sure what's happening
        return info;
    }


    info.percent = ((float)charge_now) / ((float)charge_full) * (float)100;

    // Assign hours and minutes, and decrement seconds at the same time
    info.seconds -= (info.hours = info.seconds / 3600) * 3600; // integer division
    info.seconds -= (info.minutes = info.seconds / 60) * 60; // integer division

    return info;
}

int getram() {
    FILE *fd;
    long int tot, free, buf, cache;
    // TODO: Only open this once...
    fd = fopen("/proc/meminfo", "r");
    if(fd == NULL) {
        fprintf(stderr, "Error opening %s: %s.\n", "/proc/meminfo", strerror(errno));
        return 0;
    }
    fscanf(fd, "MemTotal: %li kB MemFree: %li kB Buffers: %li kB Cached: %li kB", &tot, &free, &buf, &cache);
    fclose(fd);
    return (tot - free - buf - cache) / 1024;
}

struct NetSpeed getnetspeed(struct NetSpeed last, float timediff) {
    long int wired_rx, wired_tx, wireless_rx, wireless_tx;
    readfileli("/sys/class/net/eth0/statistics/rx_bytes", &wired_rx);
    readfileli("/sys/class/net/eth0/statistics/tx_bytes", &wired_tx);
    readfileli("/sys/class/net/wlan0/statistics/rx_bytes", &wireless_rx);
    readfileli("/sys/class/net/wlan0/statistics/tx_bytes", &wireless_tx);
    last.wiredDown = (float)(wired_rx - last.wired_rx)/timediff;
    last.wiredUp = (float)(wired_tx - last.wired_tx)/timediff;
    last.wirelessDown = (float)(wireless_rx - last.wireless_rx)/timediff;
    last.wirelessUp = (float)(wireless_tx - last.wireless_tx)/timediff;
    last.wired_rx = wired_rx;
    last.wired_tx = wired_tx;
    last.wireless_rx = wireless_rx;
    last.wireless_tx = wireless_tx;
    return last;
}

struct CPUStat getcpuinfo(struct CPUStat cpustat) {
    long int cols[4];
    FILE* fd;
    int i;
    fd = fopen("/proc/stat", "r");
    fscanf(fd,"cpu %ld %ld %ld %ld", &(cols[0]), &(cols[1]), &(cols[2]), &(cols[3])); //, &(cols[4]), &(cols[5]), &(cols[6]), &(cols[7]), &(cols[8]));
    fclose(fd);

    if (cols[3] != cpustat.lastcols[3]) {
        cpustat.usage = 100 * ((cols[0] + cols[1] + cols[2]) - (cpustat.lastcols[0] + cpustat.lastcols[1] + cpustat.lastcols[2])) / (cols[3] - cpustat.lastcols[3]);
    }
    for (i = 0; i < 4; i ++) {
        cpustat.lastcols[i] = cols[i];
    }
    return cpustat;
}

void appendStatuss(char * status, char * text, int color, int start, int end, int sep) {
    char snColor[2];
    char siColor[2];

    if (!strlen(text)) return;

    if (start || end) {
        snColor[0] = (color + 1); // color is zero based, but color strings are 1-based
        snColor[1] = '\0';
        siColor[0] = ( color + 11);
        siColor[1] = '\0';
    }

    if (start) {
        strcat(status, siColor);
        strcat(status, _BSTART);
    } else if (sep) {
        strcat(status, _BSEP);
    }
    strcat(status, text);
    if (end) {
        strcat(status, _BEND);
        strcat(status, snColor);
    }
}

void appendStatusi(char * status, int num, int low, int high, char pre[], char post[], int start, int end, int sep) {
    char tmp[strlen(pre) + 10 + strlen(post)];
    int color = COLOR_NORMAL;

    if (high != low) {
        float percent = (float)(num - low) / (float)high;
        high -= low;

        if (percent > 0.90) {
            color = COLOR_CRITICAL;
        } else if (percent > 0.70) {
            color = COLOR_WARNING;
        }
    }
    sprintf(tmp, "%s%i%s", pre, num, post);
    appendStatuss(status, tmp, color, start, end, sep);
}

void appendStatusf(char * status, float num, float low, float high, char pre[], char post[], int start, int end, int sep) {
    char tmp[strlen(pre) + 10 + strlen(post)];
    int color = COLOR_NORMAL;

    if (high != low) {
        float percent = (float)(num - low) / (float)high;
        high -= low;

        if (percent > 0.90) {
            color = COLOR_CRITICAL;
        } else if (percent > 0.70) {
            color = COLOR_WARNING;
        }
    }
    sprintf(tmp, "%s%.4g%s", pre, num, post);
    appendStatuss(status, tmp, color, start, end, sep);
}

void appendNetInfo(char * status, float speed, int up, int end) {
    int megabytes = 0;
    char tmp[15] = "";
    speed /= 1024;
    if (speed >= 1024) {
        speed /= 1024;
        megabytes = 1;
    }
    if (speed >= 1000) {
        speed = truncf(speed);
    } else {
        speed = truncf(10.*speed)/10;
    }
    if (megabytes) {
        sprintf(tmp, "%s% 2.1fM", (up ? NET_UP : NET_DOWN), speed );
    } else {
        sprintf(tmp, "%s% 5.0f", (up ? NET_UP : NET_DOWN), speed );
    }
    appendStatuss(status, tmp, COLOR_NORMAL, 0, end, 1);
}

int main(int argc, char * argv[]) {
    int freq0, freq1, freq2, freq3, freqavg;
    char status[280] = "";

    char datetime[30] = "";

    struct NetSpeed netspeed;
    netspeed = getnetspeed(netspeed, 1);

    struct BatteryInfo battinfo;
    int battcolor = COLOR_NORMAL;

    char tmp[100] = "";
    char tmp1[100] = "";
    char tmpWired[100] = "";
    char tmpWifi[100] = "";

    char essid[IW_ESSID_MAX_SIZE + 1];
    float wifi_qual;
    //    char _net[IW_ESSID_MAX_SIZE + 1 + 10] = ""; // size of essid + icon + wired

    int wifi_skfd;

    struct CPUStat cpustat;
    for (int i = 0; i < 4; i ++) {
        cpustat.lastcols[i] = 0;
    }
    cpustat = getcpuinfo(cpustat);


    struct pulseaudio_t pulse;

    int pulseready = 0;

    int vol = -1;
    char _volstr[12] = "";
    
    int runonce = 0;

    /* initialize pulse */
    if (pulse_init(&pulse) == 0)
        pulseready = 1;

    wifi_skfd = wireless_init();

    if (argc >= 2) {
        if (!strcmp(argv[1], "once")) {
            runonce = 1;
        }
    }


    // TODO: Screenlocking, Touchpad
    //

    if (!runonce) {
        if (!(dpy = XOpenDisplay(NULL))) {
            fprintf(stderr, "Cannot open display.\n");
            return 1;
        }
    }



    for ( ; ; sleep(1)) {

        if (pulseready) {
            vol = get_default_sink_volume(&pulse);
            if (vol == -1) {
                sprintf(_volstr, VOL_MUTE);
            } else {
                if (vol == 100) {
                    sprintf(_volstr, VOL_UNMUTE "%d", vol);
                } else if (vol < 10) {
                    sprintf(_volstr, VOL_UNMUTE "  %d", vol);
                } else {
                    sprintf(_volstr, VOL_UNMUTE " %d", vol);
                }
            }
        }

        netspeed = getnetspeed(netspeed, 1);

        tmpWired[0] = '\0';
        if (getwired()) {
            appendStatuss(tmpWired, WIRED, COLOR_NORMAL, 0, 0, 0);
            appendNetInfo(tmpWired, netspeed.wiredDown, 0, 0);
            appendNetInfo(tmpWired, netspeed.wiredUp, 1, 0 );
        }

        tmp[0] = '\0';
        tmpWifi[0] = '\0';
        if (!getwireless_essid(wifi_skfd, essid) && strlen(essid)) {
            wifi_qual = getwireless_strength(wifi_skfd);
            if (wifi_qual <= 0.33) {
                if (strlen(tmp)) strcat(tmp, _BSEP);
                strcat(tmp, WIFI_33 " ");
                strcat(tmp, essid);
            } else if (wifi_qual <= 0.66) {
                if (strlen(tmp)) strcat(tmp, _BSEP);
                strcat(tmp, WIFI_66 " ");
                strcat(tmp, essid);
            } else {
                if (strlen(tmp)) strcat(tmp, _BSEP);
                strcat(tmp, WIFI_100 " ");
                strcat(tmp, essid);
            }
            appendStatuss(tmpWifi, tmp, COLOR_NORMAL, 0, 0, 0);
            appendNetInfo(tmpWifi, netspeed.wirelessDown, 0, 0);
            appendNetInfo(tmpWifi, netspeed.wirelessUp, 1, 0);
        }
        // Now tmpWired and tmpWifi both have their respective info
        // Choose how to display them:
        //  * if a bond interface is detected, display together using a splitter
        //  * if bond is not active, then display then separate
        // In either case, only display what's active
        if (isbonded()) {
            if (tmpWired[0] && tmpWifi[0]) {
                appendStatuss(status, tmpWired, COLOR_NORMAL, 1, 0, 0);
                appendStatuss(status, tmpWifi,  COLOR_NORMAL, 0, 1, 1);
            } else if (tmpWired[0]) {
                appendStatuss(status, tmpWired, COLOR_NORMAL, 1, 1, 0);
            } else if (tmpWifi[0]) {
                appendStatuss(status, tmpWifi, COLOR_NORMAL, 1, 1, 0);
            }
        } else {
            if (tmpWired[0]) {
                appendStatuss(status, tmpWired, COLOR_NORMAL, 1, 1, 0);
            }
            if (tmpWifi[0]) {
                appendStatuss(status, tmpWifi, COLOR_NORMAL, 1, 1, 0);
            }
        }

        getdatetime(datetime, 30);

        battinfo = getbattery();

        appendStatuss(status, _volstr, COLOR_NORMAL, 1, 1, 0);

        if (pidof("xautolock") == -1) {
            // xautolock NOT running
            appendStatuss(status, XAUTOLOCK, COLOR_NORMAL, 1, 1, 0);
        }

        appendStatusi(status, getram(), COLOR_NORMAL, 7886, RAM, "M", 1, 1, 0);

        tmp[0] = '\0';
        // When charging, display that symbol; when fully charged, and plugged in, display that symbol (never both at the same time)
        if (battinfo.status == BattCharging) {
            strcat(tmp, BATT_CHARGING);
        } else if (battinfo.ac) {
            strcat(tmp1, BATT_AC);
        }
        if (!battinfo.ac || battinfo.status == BattCharging) {
            if (battinfo.percent < 25) {
                strcat(tmp, BATT_25);
                if (battinfo.percent < 20) battcolor = COLOR_WARNING;
                if (battinfo.percent < 11) battcolor = COLOR_CRITICAL;
            } else if (battinfo.percent < 50) {
                strcat(tmp, BATT_50);
            } else if (battinfo.percent < 75) {
                strcat(tmp, BATT_75);
            } else {
                strcat(tmp, BATT_100);
            }
            sprintf(tmp1, "%s" _BSEP "%i%%" _BSEP "%02d:%02d:%02d", tmp, battinfo.percent, battinfo.hours, battinfo.minutes, battinfo.seconds );
        }

        appendStatuss(status, tmp1, battcolor, 1, 1, 0);

        freq0 = getfreqi(0);
        freq1 = getfreqi(1);
        freq2 = getfreqi(2);
        freq3 = getfreqi(3);
        freqavg = (freq0+freq1+freq2+freq3)/4;
        cpustat = getcpuinfo(cpustat);
        //sprintf(tmp, "%4i" _BSEP "%4i" _BSEP "%4i" _BSEP "%4i" _BSEP "%3i%%", freq0, freq1, freq2, freq3, cpustat.usage);
        sprintf(tmp, "%4i" _BSEP "%3i%%", freqavg, cpustat.usage);
        appendStatuss(status, tmp, COLOR_NORMAL, 1, 1, 0);

        appendStatuss(status, datetime, 0, 1, 1, 0);


        if (runonce) {
            break;
        } else {
            setstatus(status);
        }

        // Reset variables from above
        status[0] = '\0';
        tmp[0] = '\0';
        tmp1[0] = '\0';
        battcolor = COLOR_NORMAL;


    }

    if (dpy) {
        XCloseDisplay(dpy);
    }

    if (runonce) {
        printf("%s%c", status, 0);
    }

    wireless_close(wifi_skfd);

    if (pulseready) {
        /* shut down */
        pulse_deinit(&pulse);
    }

    return 0;
}
