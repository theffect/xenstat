/*
 *  Copyright (C) International Business Machines  Corp., 2005
 *  Author(s): Judy Fischbach <jfisch@cs.pdx.edu>
 *             David Hendricks <cro_marmot@comcast.net>
 *             Josh Triplett <josh@kernel.org>
 *    based on code from Anthony Liguori <aliguori@us.ibm.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#if defined(__linux__)
#include <linux/kdev_t.h>
#endif

#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>

#define FALSE 0
#define TRUE 1

#include <xenstat.h>

#define XENTOP_VERSION "1.0"
#define IDENTIFIER_MAXOPTS 3

#define XENTOP_DISCLAIMER \
"Copyright (C) 2005  International Business Machines  Corp\n"\
"This is free software; see the source for copying conditions.There is NO\n"\
"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"

#define XENSTAT_BUGSTO "Report bugs to <shimon.zadok@gmail.com>.\n"

#define _GNU_SOURCE
#include <getopt.h>

#if !defined(__GNUC__) && !defined(__GNUG__)
#define __attribute__(arg) /* empty */
#endif

#define KEY_ESCAPE '\x1B'

#ifdef HOST_SunOS
/* Old curses library on Solaris takes non-const strings. Also, ERR interferes
 * with curse's definition.
 */
#undef ERR
#define ERR (-1)
#define curses_str_t char *
#else
#define curses_str_t const char *
#endif

// JSON output type
#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>

// Generate status check for each call
#define GEN_OR_FAIL(func)											  \
  {																	  \
    yajl_gen_status __stat = func;									  \
    if (__stat != yajl_gen_status_ok) 								  \
		print("error creating json");					  			  \
  }

/*
 * Function prototypes
 */
/* Utility functions */
static void usage(const char *);
static void version(void);
static void cleanup(void);
static void fail(const char *);
static void print(const char *, ...) __attribute__((format(printf,1,2)));
static void set_interval(char *value);
static int compare(unsigned long long, unsigned long long);
static int compare_domains(xenstat_domain **, xenstat_domain **);
static unsigned long long tot_net_bytes( xenstat_domain *, int);
static unsigned long long tot_vbd_reqs( xenstat_domain *, int);

/* Field functions */
static int compare_state(xenstat_domain *domain1, xenstat_domain *domain2);
static void print_state(xenstat_domain *domain);
static void get_state(xenstat_domain *domain, char *buf, int *len);

static int compare_cpu(xenstat_domain *domain1, xenstat_domain *domain2);
static void print_cpu(xenstat_domain *domain);
static void get_cpu(xenstat_domain *domain, char *buf, int *len);

static int compare_cpu_pct(xenstat_domain *domain1, xenstat_domain *domain2);
static void print_cpu_pct(xenstat_domain *domain);
static void get_cpu_pct(xenstat_domain *domain, char *buf, int *len);

static int compare_mem(xenstat_domain *domain1, xenstat_domain *domain2);
static void print_mem(xenstat_domain *domain);
static void get_mem(xenstat_domain *domain, char *buf, int *len);
static void print_mem_pct(xenstat_domain *domain);
static void get_mem_pct(xenstat_domain *domain, char *buf, int *len);

static int compare_maxmem(xenstat_domain *domain1, xenstat_domain *domain2);
static void print_maxmem(xenstat_domain *domain);
static void get_maxmem(xenstat_domain *domain, char *buf, int *len);
static void print_max_pct(xenstat_domain *domain);
static void get_max_pct(xenstat_domain *domain, char *buf, int *len);

static int compare_vcpus(xenstat_domain *domain1, xenstat_domain *domain2);
static void print_vcpus(xenstat_domain *domain);
static void get_vcpus(xenstat_domain *domain, char *buf, int *len);

static int compare_nets(xenstat_domain *domain1, xenstat_domain *domain2);
static void print_nets(xenstat_domain *domain);
static void get_nets(xenstat_domain *domain, char *buf, int *len);

static int compare_net_tx(xenstat_domain *domain1, xenstat_domain *domain2);
static void print_net_tx(xenstat_domain *domain);
static void get_net_tx(xenstat_domain *domain, char *buf, int *len);

static int compare_net_rx(xenstat_domain *domain1, xenstat_domain *domain2);
static void print_net_rx(xenstat_domain *domain);
static void get_net_rx(xenstat_domain *domain, char *buf, int *len);

static int compare_ssid(xenstat_domain *domain1, xenstat_domain *domain2);
static void print_ssid(xenstat_domain *domain);
static void get_ssid(xenstat_domain *domain, char *buf, int *len);

static int compare_name(xenstat_domain *domain1, xenstat_domain *domain2);
static void print_ident(xenstat_domain *domain);
static void print_name(xenstat_domain *domain);
static void print_fullname(xenstat_domain *domain);
static void print_domainid(xenstat_domain *domain);
static void get_ident(xenstat_domain *domain, char *buf, int *len);

static int compare_vbds(xenstat_domain *domain1, xenstat_domain *domain2);
static void print_vbds(xenstat_domain *domain);
static void get_vbds(xenstat_domain *domain, char *buf, int *len);

static int compare_vbd_oo(xenstat_domain *domain1, xenstat_domain *domain2);
static void print_vbd_oo(xenstat_domain *domain);
static void get_vbd_oo(xenstat_domain *domain, char *buf, int *len);

static int compare_vbd_rd(xenstat_domain *domain1, xenstat_domain *domain2);
static void print_vbd_rd(xenstat_domain *domain);
static void get_vbd_rd(xenstat_domain *domain, char *buf, int *len);

static int compare_vbd_wr(xenstat_domain *domain1, xenstat_domain *domain2);
static void print_vbd_wr(xenstat_domain *domain);
static void get_vbd_wr(xenstat_domain *domain, char *buf, int *len);

static int compare_vbd_rsect(xenstat_domain *domain1, xenstat_domain *domain2);
static void print_vbd_rsect(xenstat_domain *domain);
static void get_vbd_rsect(xenstat_domain *domain, char *buf, int *len);

static int compare_vbd_wsect(xenstat_domain *domain1, xenstat_domain *domain2);
static void print_vbd_wsect(xenstat_domain *domain);
static void get_vbd_wsect(xenstat_domain *domain, char *buf, int *len);


/* Section printing functions */
static void do_summary(void);
static void do_header(void);
static void do_domain(xenstat_domain *);
static void do_vcpu(xenstat_domain *);
static void do_network(xenstat_domain *);
static void do_vbd(xenstat_domain *);
static void top(void);

/* Field types */
typedef enum field_id {
	FIELD_DOMID,
	FIELD_NAME,
	FIELD_STATE,
	FIELD_CPU,
	FIELD_CPU_PCT,
	FIELD_MEM,
	FIELD_MEM_PCT,
	FIELD_MAXMEM,
	FIELD_MAX_PCT,
	FIELD_VCPUS,
	FIELD_NETS,
	FIELD_NET_TX,
	FIELD_NET_RX,
	FIELD_VBDS,
	FIELD_VBD_OO,
	FIELD_VBD_RD,
	FIELD_VBD_WR,
	FIELD_VBD_RSECT,
	FIELD_VBD_WSECT,
	FIELD_SSID
} field_id;

typedef struct field {
	field_id num;
	const char *header;
	unsigned int default_width;
	int (*compare)(xenstat_domain *domain1, xenstat_domain *domain2);
	void (*print)(xenstat_domain *domain);
	void (*get)(xenstat_domain *domain, char *buf, int *len);
} field;

field fields[] = {
	{ FIELD_NAME,      "NAME",      10, compare_name,      print_ident,		get_ident		},
	{ FIELD_STATE,     "STATE",      6, compare_state,     print_state,		get_state		},
	{ FIELD_CPU,       "CPU(sec)",  10, compare_cpu,       print_cpu,		get_cpu			},
	{ FIELD_CPU_PCT,   "CPU(%)",     6, compare_cpu_pct,   print_cpu_pct,	get_cpu_pct		},
	{ FIELD_MEM,       "MEM(k)",    10, compare_mem,       print_mem,		get_mem			},
	{ FIELD_MEM_PCT,   "MEM(%)",     6, compare_mem,       print_mem_pct,	get_mem_pct		},
	{ FIELD_MAXMEM,    "MAXMEM(k)", 10, compare_maxmem,    print_maxmem,	get_maxmem		},
	{ FIELD_MAX_PCT,   "MAXMEM(%)",  9, compare_maxmem,    print_max_pct,	get_max_pct		},
	{ FIELD_VCPUS,     "VCPUS",      5, compare_vcpus,     print_vcpus,		get_vcpus		},
	{ FIELD_NETS,      "NETS",       4, compare_nets,      print_nets,		get_nets		},
	{ FIELD_NET_TX,    "NETTX(k)",   8, compare_net_tx,    print_net_tx,	get_net_tx		},
	{ FIELD_NET_RX,    "NETRX(k)",   8, compare_net_rx,    print_net_rx,	get_net_rx		},
	{ FIELD_VBDS,      "VBDS",       4, compare_vbds,      print_vbds,		get_vbds		},
	{ FIELD_VBD_OO,    "VBD_OO",     8, compare_vbd_oo,    print_vbd_oo,	get_vbd_oo		},
	{ FIELD_VBD_RD,    "VBD_RD",     8, compare_vbd_rd,    print_vbd_rd,	get_vbd_rd		},
	{ FIELD_VBD_WR,    "VBD_WR",     8, compare_vbd_wr,    print_vbd_wr,	get_vbd_wr		},
	{ FIELD_VBD_RSECT, "VBD_RSECT", 10, compare_vbd_rsect, print_vbd_rsect,	get_vbd_rsect	},
	{ FIELD_VBD_WSECT, "VBD_WSECT", 10, compare_vbd_wsect, print_vbd_wsect,	get_vbd_wsect	},
	{ FIELD_SSID,      "SSID",       4, compare_ssid,      print_ssid,		get_ssid		}
};

// Although seeing that the longest header is 10, i will still keep ^2
#define MAX_HEADER_LEN 16

const unsigned int NUM_FIELDS = sizeof(fields)/sizeof(field);

/*
 * Identifier
 */
enum {
	IDENT_START_OPT	= 0,
	IDENT_NAME_OPT,
	IDENT_FULL_OPT,
	IDENT_ID_OPT,
	IDENT_END
};

const char *identifier_opts[] = {
	[IDENT_START_OPT]	= "start",
	[IDENT_NAME_OPT]	= "name",
	[IDENT_FULL_OPT]	= "full",
	[IDENT_ID_OPT]		= "id",
	[IDENT_END]			= NULL
};

/*
 * Type of output
 */
enum {
	TYPE_START_OPT	= 0,
	TYPE_ORG_OPT,
	TYPE_CSV_OPT,
	TYPE_JSON_OPT,
	TYPE_END,
};

const char *type_opts[] = {
	[TYPE_START_OPT]	= "start",
	[TYPE_ORG_OPT]		= "org",
	[TYPE_CSV_OPT]		= "csv",
	[TYPE_JSON_OPT]		= "json",
	[TYPE_END]			= NULL
};

/* Globals */
struct timeval curtime, oldtime;
xenstat_handle *xhandle = NULL;
xenstat_node *prev_node = NULL;
xenstat_node *cur_node = NULL;
field_id sort_field = FIELD_DOMID;
unsigned int first_domain_index = 0;
unsigned int interval = 1;
unsigned int loop = 1;
unsigned int iterationCount = 0;
int show_vcpus = 0;
int show_networks = 0;
int show_vbds = 0;
int show_tmem = 0;
int repeat_header = 0;
int show_full_name = 0;
int identifier = 1;
int ftype = 1;
#define PROMPT_VAL_LEN 80
char *prompt = NULL;
char prompt_val[PROMPT_VAL_LEN];
int prompt_val_len = 0;
void (*prompt_complete_func)(char *);

// YAJL JSON
yajl_gen yghandle;


/*
 * Function definitions
 */

/* Utility functions */

/* Print usage message, using given program name */
static void usage(const char *program)
{
	printf("Usage: %s [OPTION]\n"
"Displays ongoing information about xen vm resources \n\n"
"-h, --help                 display this help and exit\n"
"-V, --version              output version information and exit\n"
"-i, --interval=SECONDS     seconds between updates (default 1)\n"
"-r, --repeat-header        repeat table header before each domain\n"
"-c, --iteration-count      count of iterations before exiting\n"
"-f, --identifier           output the full domain name (not truncated) or domain id\n"
"-t, --type                 type of output, options are csv/json\n"
	       "\n" XENSTAT_BUGSTO,
	       program);
	return;
}

/* Print program version information */
static void version(void)
{
	printf("xentop " XENTOP_VERSION "\n"
	       "Written by Judy Fischbach, David Hendricks, Josh Triplett\n"
	       "\n" XENTOP_DISCLAIMER);
}

/* Clean up any open resources */
static void cleanup(void)
{
	if(prev_node != NULL)
		xenstat_free_node(prev_node);
	
	if(cur_node != NULL)
		xenstat_free_node(cur_node);
	
	if(xhandle != NULL)
		xenstat_uninit(xhandle);
	
	if (yghandle != NULL)
		// Free the json object
		yajl_gen_free(yghandle);
}

/* Display the given message and gracefully exit */
static void fail(const char *str)
{
	fprintf(stderr, "%s", str);
	exit(1);
}

/* printf-style print function which calls printw, but only if the cursor is
 * not on the last line. */
static void print(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);

}

/* Handle setting the interval from the user-supplied value in prompt_val */
static void set_interval(char *value)
{
	int new_interval;
	new_interval = atoi(value);
	if(new_interval > 0)
		interval = new_interval;
	else
		exit(1);
}

/* Compares two integers, returning -1,0,1 for <,=,> */
static int compare(unsigned long long i1, unsigned long long i2)
{
	if(i1 < i2)
		return -1;
	if(i1 > i2)
		return 1;
	return 0;
}

/* Comparison function for use with qsort.  Compares two domains using the
 * current sort field. */
static int compare_domains(xenstat_domain **domain1, xenstat_domain **domain2)
{
	return fields[sort_field].compare(*domain1, *domain2);
}

/* Field functions */

/* Compare domain names, returning -1,0,1 for <,=,> */
int compare_name(xenstat_domain *domain1, xenstat_domain *domain2)
{
	return strcasecmp(xenstat_domain_name(domain1), xenstat_domain_name(domain2));
}

void print_ident(xenstat_domain *domain) {
	switch(identifier) {
		case 1:
			print_name(domain);
			break;
		case 2:
			print_fullname(domain);
			break;
		case 3:
			print_domainid(domain);
			break;
		default:
			print("identifier resulted with invalid number: %d\n", identifier);
			exit(1);
			break;
	}
}

/* Prints domain name */
void print_name(xenstat_domain *domain)
{
	print("%-13.10s", xenstat_domain_name(domain));
}

void print_fullname(xenstat_domain *domain) {
	print("%-13s", xenstat_domain_name(domain));
}

void print_domainid(xenstat_domain *domain) {
	print("%-13d", xenstat_domain_id(domain));
}

void get_ident(xenstat_domain *domain, char *buf, int *len) {
	char *name;
	int id;
	
	switch(identifier) {
		case 1:
		case 2:
			name = xenstat_domain_name(domain);
			*len = strlen(name);
			strcpy(buf, name);
			
			break;
		case 3:
			id = xenstat_domain_id(domain);
			*len = snprintf(buf, *len, "%d", id);
			
			break;
		default:
			print("identifier resulted with invalid number: %d\n", identifier);
			fail("Impossible error! or not, ha ;-)");
			break;
	}
}

struct {
	unsigned int (*get)(xenstat_domain *);
	char ch;
} state_funcs[] = {
	{ xenstat_domain_dying,    'd' },
	{ xenstat_domain_shutdown, 's' },
	{ xenstat_domain_blocked,  'b' },
	{ xenstat_domain_crashed,  'c' },
	{ xenstat_domain_paused,   'p' },
	{ xenstat_domain_running,  'r' }
};
const unsigned int NUM_STATES = sizeof(state_funcs)/sizeof(*state_funcs);

/* Compare states of two domains, returning -1,0,1 for <,=,> */
static int compare_state(xenstat_domain *domain1, xenstat_domain *domain2)
{
	unsigned int i, d1s, d2s;
	for(i = 0; i < NUM_STATES; i++) {
		d1s = state_funcs[i].get(domain1);
		d2s = state_funcs[i].get(domain2);
		if(d1s && !d2s)
			return -1;
		if(d2s && !d1s)
			return 1;
	}
	return 0;
}

/* Prints domain state in abbreviated letter format */
static void print_state(xenstat_domain *domain)
{
	unsigned int i;
	char ch = '-';
	
	for(i = 0; i < NUM_STATES; i++)
		if (state_funcs[i].get(domain)) ch = state_funcs[i].ch;
	
	print("%c", ch);
}

static void get_state(xenstat_domain *domain, char *buf, int *len)
{
	unsigned int i;
	char ch = '-';
	
	for(i = 0; i < NUM_STATES; i++)
		if (state_funcs[i].get(domain)) ch = state_funcs[i].ch;
	
	*len = snprintf(buf, *len, "%c", ch);
}

/* Compares cpu usage of two domains, returning -1,0,1 for <,=,> */
static int compare_cpu(xenstat_domain *domain1, xenstat_domain *domain2)
{
	return -compare(xenstat_domain_cpu_ns(domain1),
			xenstat_domain_cpu_ns(domain2));
}

/* Prints domain cpu usage in seconds */
static void print_cpu(xenstat_domain *domain)
{
	print("%10llu", xenstat_domain_cpu_ns(domain)/1000000000);
}

static void get_cpu(xenstat_domain *domain, char *buf, int *len) {
	*len = snprintf(buf, *len, "%llu", xenstat_domain_cpu_ns(domain)/1000000000);
}

/* Computes the CPU percentage used for a specified domain */
static double calc_cpu_pct(xenstat_domain *domain)
{
	xenstat_domain *old_domain;
	double us_elapsed;

	/* Can't calculate CPU percentage without a previous sample. */
	if(prev_node == NULL)
		return 0.0;

	old_domain = xenstat_node_domain(prev_node, xenstat_domain_id(domain));
	if(old_domain == NULL)
		return 0.0;

	/* Calculate the time elapsed in microseconds */
	us_elapsed = ((curtime.tv_sec-oldtime.tv_sec)*1000000.0
		      +(curtime.tv_usec - oldtime.tv_usec));

	/* In the following, nanoseconds must be multiplied by 1000.0 to
	 * convert to microseconds, then divided by 100.0 to get a percentage,
	 * resulting in a multiplication by 10.0 */
	return ((xenstat_domain_cpu_ns(domain)
		 -xenstat_domain_cpu_ns(old_domain))/10.0)/us_elapsed;
}

static int compare_cpu_pct(xenstat_domain *domain1, xenstat_domain *domain2)
{
	return -compare(calc_cpu_pct(domain1), calc_cpu_pct(domain2));
}

/* Prints cpu percentage statistic */
static void print_cpu_pct(xenstat_domain *domain)
{
	print("%6.1f", calc_cpu_pct(domain));
}

static void get_cpu_pct(xenstat_domain *domain, char *buf, int *len) {
	*len = snprintf(buf, *len, "%.1f", calc_cpu_pct(domain));
}

/* Compares current memory of two domains, returning -1,0,1 for <,=,> */
static int compare_mem(xenstat_domain *domain1, xenstat_domain *domain2)
{
	return -compare(xenstat_domain_cur_mem(domain1),
	                xenstat_domain_cur_mem(domain2));
}

/* Prints current memory statistic */
static void print_mem(xenstat_domain *domain)
{
	print("%10llu", xenstat_domain_cur_mem(domain)/1024);
}

static void get_mem(xenstat_domain *domain, char *buf, int *len) {
	*len = snprintf(buf, *len, "%llu", xenstat_domain_cur_mem(domain)/1024);
}

/* Prints memory percentage statistic, ratio of current domain memory to total
 * node memory */
static void print_mem_pct(xenstat_domain *domain)
{
	print("%6.1f", (double)xenstat_domain_cur_mem(domain) /
	               (double)xenstat_node_tot_mem(cur_node) * 100);
}

static void get_mem_pct(xenstat_domain *domain, char *buf, int *len) {
	*len = snprintf(buf, *len, "%.1f", (double)xenstat_domain_cur_mem(domain) /
	               (double)xenstat_node_tot_mem(cur_node) * 100);
}

/* Compares maximum memory of two domains, returning -1,0,1 for <,=,> */
static int compare_maxmem(xenstat_domain *domain1, xenstat_domain *domain2)
{
	return -compare(xenstat_domain_max_mem(domain1),
	                xenstat_domain_max_mem(domain2));
}

/* Prints maximum domain memory statistic in KB */
static void print_maxmem(xenstat_domain *domain)
{
	unsigned long long max_mem = xenstat_domain_max_mem(domain);
	if(max_mem == ((unsigned long long)-1))
		print("%10s", "no limit");
	else
		print("%10llu", max_mem/1024);
}

static void get_maxmem(xenstat_domain *domain, char *buf, int *len) {
	unsigned long long max_mem = xenstat_domain_max_mem(domain);
	if(max_mem == ((unsigned long long)-1))
		*len = snprintf(buf, *len, "%s", "no limit");
	else
		*len = snprintf(buf, *len, "%llu", max_mem/1024);
}

/* Prints memory percentage statistic, ratio of current domain memory to total
 * node memory */
static void print_max_pct(xenstat_domain *domain)
{
	if (xenstat_domain_max_mem(domain) == (unsigned long long)-1)
		print("%9s", "n/a");
	else
		print("%9.1f", (double)xenstat_domain_max_mem(domain) /
		               (double)xenstat_node_tot_mem(cur_node) * 100);
}

static void get_max_pct(xenstat_domain *domain, char *buf, int *len) {
	if (xenstat_domain_max_mem(domain) == (unsigned long long)-1)
		*len = snprintf(buf, *len, "%s", "n/a");
	else
		*len = snprintf(buf, *len, "%.1f", (double)xenstat_domain_max_mem(domain) /
		               (double)xenstat_node_tot_mem(cur_node) * 100);
}

/* Compares number of virtual CPUs of two domains, returning -1,0,1 for
 * <,=,> */
static int compare_vcpus(xenstat_domain *domain1, xenstat_domain *domain2)
{
	return -compare(xenstat_domain_num_vcpus(domain1),
	                xenstat_domain_num_vcpus(domain2));
}

/* Prints number of virtual CPUs statistic */
static void print_vcpus(xenstat_domain *domain)
{
	print("%5u", xenstat_domain_num_vcpus(domain));
}

static void get_vcpus(xenstat_domain *domain, char *buf, int *len) {
	*len = snprintf(buf, *len, "%u", xenstat_domain_num_vcpus(domain));
}

/* Compares number of virtual networks of two domains, returning -1,0,1 for
 * <,=,> */
static int compare_nets(xenstat_domain *domain1, xenstat_domain *domain2)
{
	return -compare(xenstat_domain_num_networks(domain1),
	                xenstat_domain_num_networks(domain2));
}

/* Prints number of virtual networks statistic */
static void print_nets(xenstat_domain *domain)
{
	print("%4u", xenstat_domain_num_networks(domain));
}

static void get_nets(xenstat_domain *domain, char *buf, int *len) {
	*len = snprintf(buf, *len, "%u", xenstat_domain_num_networks(domain));
}

/* Compares number of total network tx bytes of two domains, returning -1,0,1
 * for <,=,> */
static int compare_net_tx(xenstat_domain *domain1, xenstat_domain *domain2)
{
	return -compare(tot_net_bytes(domain1, FALSE),
	                tot_net_bytes(domain2, FALSE));
}

/* Prints number of total network tx bytes statistic */
static void print_net_tx(xenstat_domain *domain)
{
	print("%8llu", tot_net_bytes(domain, FALSE)/1024);
}

static void get_net_tx(xenstat_domain *domain, char *buf, int *len) {
	*len = snprintf(buf, *len, "%llu", tot_net_bytes(domain, FALSE)/1024);
}

/* Compares number of total network rx bytes of two domains, returning -1,0,1
 * for <,=,> */
static int compare_net_rx(xenstat_domain *domain1, xenstat_domain *domain2)
{
	return -compare(tot_net_bytes(domain1, TRUE),
	                tot_net_bytes(domain2, TRUE));
}

/* Prints number of total network rx bytes statistic */
static void print_net_rx(xenstat_domain *domain)
{
	print("%8llu", tot_net_bytes(domain, TRUE)/1024);
}

static void get_net_rx(xenstat_domain *domain, char *buf, int *len) {
	*len = snprintf(buf, *len, "%llu", tot_net_bytes(domain, TRUE)/1024);
}

/* Gets number of total network bytes statistic, if rx true, then rx bytes
 * otherwise tx bytes
 */
static unsigned long long tot_net_bytes(xenstat_domain *domain, int rx_flag)
{
	int i = 0;
	xenstat_network *network;
	unsigned num_networks = 0;
	unsigned long long total = 0;

	/* How many networks? */
	num_networks = xenstat_domain_num_networks(domain);

	/* Dump information for each network */
	for (i=0; i < num_networks; i++) {
		/* Next get the network information */
		network = xenstat_domain_network(domain,i);
		if (rx_flag)
			total += xenstat_network_rbytes(network);
		else
			total += xenstat_network_tbytes(network);
	}

	return total;
}

/* Compares number of virtual block devices of two domains,
   returning -1,0,1 for * <,=,> */
static int compare_vbds(xenstat_domain *domain1, xenstat_domain *domain2)
{
	return -compare(xenstat_domain_num_vbds(domain1),
	                xenstat_domain_num_vbds(domain2));
}

/* Prints number of virtual block devices statistic */
static void print_vbds(xenstat_domain *domain)
{
	print("%4u", xenstat_domain_num_vbds(domain));
}

static void get_vbds(xenstat_domain *domain, char *buf, int *len) {
	*len = snprintf(buf, *len, "%u", xenstat_domain_num_vbds(domain));
}

/* Compares number of total VBD OO requests of two domains,
   returning -1,0,1 * for <,=,> */
static int compare_vbd_oo(xenstat_domain *domain1, xenstat_domain *domain2)
{
  return -compare(tot_vbd_reqs(domain1, FIELD_VBD_OO),
		  tot_vbd_reqs(domain2, FIELD_VBD_OO));
}

/* Prints number of total VBD OO requests statistic */
static void print_vbd_oo(xenstat_domain *domain)
{
	print("%8llu", tot_vbd_reqs(domain, FIELD_VBD_OO));
}

static void get_vbd_oo(xenstat_domain *domain, char *buf, int *len) {
	*len = snprintf(buf, *len, "%llu", tot_vbd_reqs(domain, FIELD_VBD_OO));
}

/* Compares number of total VBD READ requests of two domains,
   returning -1,0,1 * for <,=,> */
static int compare_vbd_rd(xenstat_domain *domain1, xenstat_domain *domain2)
{
	return -compare(tot_vbd_reqs(domain1, FIELD_VBD_RD),
			tot_vbd_reqs(domain2, FIELD_VBD_RD));
}

/* Prints number of total VBD READ requests statistic */
static void print_vbd_rd(xenstat_domain *domain)
{
	print("%8llu", tot_vbd_reqs(domain, FIELD_VBD_RD));
}

static void get_vbd_rd(xenstat_domain *domain, char *buf, int *len) {
	*len = snprintf(buf, *len, "%llu", tot_vbd_reqs(domain, FIELD_VBD_RD));
}

/* Compares number of total VBD WRITE requests of two domains,
   returning -1,0,1 * for <,=,> */
static int compare_vbd_wr(xenstat_domain *domain1, xenstat_domain *domain2)
{
	return -compare(tot_vbd_reqs(domain1, FIELD_VBD_WR),
			tot_vbd_reqs(domain2, FIELD_VBD_WR));
}

/* Prints number of total VBD WRITE requests statistic */
static void print_vbd_wr(xenstat_domain *domain)
{
	print("%8llu", tot_vbd_reqs(domain, FIELD_VBD_WR));
}

static void get_vbd_wr(xenstat_domain *domain, char *buf, int *len) {
	*len = snprintf(buf, *len, "%llu", tot_vbd_reqs(domain, FIELD_VBD_WR));
}

/* Compares number of total VBD READ sectors of two domains,
   returning -1,0,1 * for <,=,> */
static int compare_vbd_rsect(xenstat_domain *domain1, xenstat_domain *domain2)
{
	return -compare(tot_vbd_reqs(domain1, FIELD_VBD_RSECT),
			tot_vbd_reqs(domain2, FIELD_VBD_RSECT));
}

/* Prints number of total VBD READ sectors statistic */
static void print_vbd_rsect(xenstat_domain *domain)
{
	print("%10llu", tot_vbd_reqs(domain, FIELD_VBD_RSECT));
}

static void get_vbd_rsect(xenstat_domain *domain, char *buf, int *len) {
	*len = snprintf(buf, *len, "%llu", tot_vbd_reqs(domain, FIELD_VBD_RSECT));
}

/* Compares number of total VBD WRITE sectors of two domains,
   returning -1,0,1 * for <,=,> */
static int compare_vbd_wsect(xenstat_domain *domain1, xenstat_domain *domain2)
{
	return -compare(tot_vbd_reqs(domain1, FIELD_VBD_WSECT),
			tot_vbd_reqs(domain2, FIELD_VBD_WSECT));
}

/* Prints number of total VBD WRITE sectors statistic */
static void print_vbd_wsect(xenstat_domain *domain)
{
	print("%10llu", tot_vbd_reqs(domain, FIELD_VBD_WSECT));
}

static void get_vbd_wsect(xenstat_domain *domain, char *buf, int *len) {
	*len = snprintf(buf, *len, "%llu", tot_vbd_reqs(domain, FIELD_VBD_WSECT));
}


/* Gets number of total VBD requests statistic, 
 *   if flag is FIELD_VBD_OO, then OO requests,
 *   if flag is FIELD_VBD_RD, then READ requests,
 *   if flag is FIELD_VBD_WR, then WRITE requests,
 *   if flag is FIELD_VBD_RSECT, then READ sectors,
 *   if flag is FIELD_VBD_WSECT, then WRITE sectors.
 */
static unsigned long long tot_vbd_reqs(xenstat_domain *domain, int flag)
{
	int i = 0;
	xenstat_vbd *vbd;
	unsigned num_vbds = 0;
	unsigned long long total = 0;
	
	num_vbds = xenstat_domain_num_vbds(domain);
	
	for ( i=0 ; i < num_vbds ; i++) {
		vbd = xenstat_domain_vbd(domain,i);
		switch(flag) {
		case FIELD_VBD_OO:
			total += xenstat_vbd_oo_reqs(vbd);
			break;
		case FIELD_VBD_RD:
			total += xenstat_vbd_rd_reqs(vbd);
			break;
		case FIELD_VBD_WR:
			total += xenstat_vbd_wr_reqs(vbd);
			break;
		case FIELD_VBD_RSECT:
			total += xenstat_vbd_rd_sects(vbd);
			break;
		case FIELD_VBD_WSECT:
			total += xenstat_vbd_wr_sects(vbd);
			break;
		default:
			break;
		}
	}
	
	return total;
}

/* Compares security id (ssid) of two domains, returning -1,0,1 for <,=,> */
static int compare_ssid(xenstat_domain *domain1, xenstat_domain *domain2)
{
	return compare(xenstat_domain_ssid(domain1),
		       xenstat_domain_ssid(domain2));
}

/* Prints ssid statistic */
static void print_ssid(xenstat_domain *domain)
{
	print("%4u", xenstat_domain_ssid(domain));
}

static void get_ssid(xenstat_domain *domain, char *buf, int *len)
{
	*len = snprintf(buf, *len, "%u", xenstat_domain_ssid(domain));
}

/* Section printing functions */
/* Prints the top summary, above the domain table */
void do_summary(void)
{
#define TIME_STR_LEN 20
	const char *TIME_STR_FORMAT = "%D %H:%M:%S";
	char time_str[TIME_STR_LEN];
	unsigned run = 0, block = 0, pause = 0,
	         crash = 0, dying = 0, shutdown = 0;
	unsigned i, num_domains = 0;
	unsigned long long used = 0;
	long freeable_mb = 0;
	xenstat_domain *domain;
	time_t curt;

	/* Print program name, current time, and number of domains */
	curt = curtime.tv_sec;
	strftime(time_str, TIME_STR_LEN, TIME_STR_FORMAT, localtime(&curt));
	
	num_domains = xenstat_node_num_domains(cur_node);
	
	/* Tabulate what states domains are in for summary */
	for (i=0; i < num_domains; i++) {
		domain = xenstat_node_domain_by_index(cur_node,i);
		if (xenstat_domain_running(domain)) run++;
		else if (xenstat_domain_blocked(domain)) block++;
		else if (xenstat_domain_paused(domain)) pause++;
		else if (xenstat_domain_shutdown(domain)) shutdown++;
		else if (xenstat_domain_crashed(domain)) crash++;
		else if (xenstat_domain_dying(domain)) dying++;
	}

	print("Date: %s, Domains: %u, %u running, %u blocked, %u paused, "
	      "%u crashed, %u dying, %u shutdown ",
	      time_str, num_domains, run, block, pause, crash, dying, shutdown);

	used = xenstat_node_tot_mem(cur_node)-xenstat_node_free_mem(cur_node);
	freeable_mb = xenstat_node_freeable_mb(cur_node);

	/* Dump node memory and cpu information */
	if ( freeable_mb <= 0 )
	     print("Mem: %lluk total, %lluk used, %lluk free    ",
	      xenstat_node_tot_mem(cur_node)/1024, used/1024,
	      xenstat_node_free_mem(cur_node)/1024);
	else
	     print("Mem: %lluk total, %lluk used, %lluk free, %ldk freeable, ",
	      xenstat_node_tot_mem(cur_node)/1024, used/1024,
	      xenstat_node_free_mem(cur_node)/1024, freeable_mb*1024);
	print("CPUs: %u @ %lluMHz\n",
	      xenstat_node_num_cpus(cur_node),
	      xenstat_node_cpu_hz(cur_node)/1000000);
}

void do_json_summary(void) {
	
#define TIME_STR_LEN 20
	const char *TIME_STR_FORMAT = "%D %H:%M:%S";
	char time_str[TIME_STR_LEN];
	int time_len = 0;

#define MAX sizeof(unsigned)*3
	char buf[MAX] = {0};
	int len = 0;
	
	unsigned run = 0, block = 0, pause = 0,
	         crash = 0, dying = 0, shutdown = 0;
	unsigned i, num_domains = 0;
	unsigned long long used = 0;
	long freeable_mb = 0;
	xenstat_domain *domain;
	time_t curt;


	/*
	GEN_OR_FAIL(yajl_gen_array_open(yghandle));
	
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"date", 4));
	
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"domains", 7));
	
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"running", 7));
	
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"blocked", 7));
	
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"paused", 6));
	
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"dying", 5));
	
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"shutdown", 8));
	
	GEN_OR_FAIL(yajl_gen_map_open(yghandle));
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"mem", 3));
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"total", 5));
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"used", 4));
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"free", 4));
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"freeable", 8));
	GEN_OR_FAIL(yajl_gen_map_close(yghandle));
	
	
	GEN_OR_FAIL(yajl_gen_map_open(yghandle));
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"cpu", 3));
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"total", 5));
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"clock", 5));
	GEN_OR_FAIL(yajl_gen_map_close(yghandle));
	
	GEN_OR_FAIL(yajl_gen_array_close(yghandle));
	*/
	
	/* Print program name, current time, and number of domains */
	curt = curtime.tv_sec;
	strftime(time_str, TIME_STR_LEN, TIME_STR_FORMAT, localtime(&curt));
	
	num_domains = xenstat_node_num_domains(cur_node);
	
	/* Tabulate what states domains are in for summary */
	for (i=0; i < num_domains; i++) {
		domain = xenstat_node_domain_by_index(cur_node,i);
		if (xenstat_domain_running(domain)) run++;
		else if (xenstat_domain_blocked(domain)) block++;
		else if (xenstat_domain_paused(domain)) pause++;
		else if (xenstat_domain_shutdown(domain)) shutdown++;
		else if (xenstat_domain_crashed(domain)) crash++;
		else if (xenstat_domain_dying(domain)) dying++;
	}
	
	GEN_OR_FAIL(yajl_gen_array_open(yghandle));
	
	// Date time
	time_len = strlen(time_str);
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)time_str, time_len));
	
	// Number of domains
	len = snprintf(buf, MAX, "%u", num_domains);
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)buf, len));
	
	// Running
	len = snprintf(buf, MAX, "%u", run);
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)buf, len));
	
	// Blocked
	len = snprintf(buf, MAX, "%u", block);
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)buf, len));
	
	// Paused
	len = snprintf(buf, MAX, "%u", pause);
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)buf, len));
	
	// Crashed
	len = snprintf(buf, MAX, "%u", crash);
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)buf, len));
	
	// Dying
	len = snprintf(buf, MAX, "%u", dying);
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)buf, len));
	
	// Shutdown
	len = snprintf(buf, MAX, "%u", shutdown);
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)buf, len));
	
	used = xenstat_node_tot_mem(cur_node)-xenstat_node_free_mem(cur_node);
	freeable_mb = xenstat_node_freeable_mb(cur_node);

	// Dump node memory
	len = snprintf(buf, MAX, "%lluk", xenstat_node_tot_mem(cur_node)/1024);
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)buf, len));
	
	len = snprintf(buf, MAX, "%lluk", used/1024);
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)buf, len));
	
	len = snprintf(buf, MAX, "%lluk", xenstat_node_free_mem(cur_node)/1024);
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)buf, len));
	
	len = snprintf(buf, MAX, "%ldk", ((freeable_mb <=0)?0:freeable_mb*1024));
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)buf, len));
	
	// CPU information
	len = snprintf(buf, MAX, "%uk", xenstat_node_num_cpus(cur_node));
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)buf, len));
	
	len = snprintf(buf, MAX, "%lluMHz", xenstat_node_cpu_hz(cur_node)/1000000);
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)buf, len));
	
	GEN_OR_FAIL(yajl_gen_array_close(yghandle));
}

/* Display the top header for the domain table */
void do_header(void)
{
	field_id i;
	char *isSortOptions[] = {"", "(s)"};
	char *isSort;
	char separator;
	
	/* Turn on REVERSE highlight attribute for headings */
	for(i = 0; i < NUM_FIELDS; i++) {
		isSort = isSortOptions[0];
		
		/* The (s) attribute is turned on for the sort column */
		if (i == sort_field)
			isSort = isSortOptions[1];
		
		switch (ftype) {
			case TYPE_ORG_OPT:
				separator = (i<NUM_FIELDS-1)?' ':0;
				print("%-*s%s%c", fields[i].default_width, fields[i].header, isSort, separator);
			break;
			case TYPE_CSV_OPT:
				separator = (i<NUM_FIELDS-1)?',':0;
				print("%s%s%c", fields[i].header, isSort, separator);
			break;
		}
	}
	print("\n");
}

void do_json_header(void) {
	field_id i;
	char *isSortOptions[] = {"", "(s)"};
	char *isSort;
	char buf[MAX_HEADER_LEN] = {0};
	size_t valuelen;
	
	// Specify open of array
	GEN_OR_FAIL(yajl_gen_array_open(yghandle));
	
	/* Turn on REVERSE highlight attribute for headings */
	for(i = 0; i < NUM_FIELDS; i++) {
		isSort = isSortOptions[0];
		
		/* The (s) attribute is turned on for the sort column */
		if (i == sort_field)
			isSort = isSortOptions[1];
		
		valuelen = snprintf(buf, MAX_HEADER_LEN, "%s%s", fields[i].header, isSort);
		
		GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)buf, valuelen));
	}
	
	// Specify end of array
	GEN_OR_FAIL(yajl_gen_array_close(yghandle));
}

/* Prints Domain information */
void do_domain(xenstat_domain *domain)
{
	unsigned int i;
	for (i = 0; i < NUM_FIELDS; i++) {
		if (i != 0)
			print(" ");
		fields[i].print(domain);
	}
	print("\n");
}

void do_json_domain(xenstat_domain *domain)
{
#define MAX_LEN 100
	unsigned int i;
	char info[MAX_LEN] = {0};
	int len;
	
	
	// Specify open of array
	GEN_OR_FAIL(yajl_gen_array_open(yghandle));
	
	for (i = 0; i < NUM_FIELDS; i++) {
		len = MAX_LEN;
		fields[i].get(domain, info, &len);
		GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)info, len));
	}
	
	// Specify end of array
	GEN_OR_FAIL(yajl_gen_array_close(yghandle));
}

/* Output all vcpu information */
void do_vcpu(xenstat_domain *domain)
{
	int i = 0;
	unsigned num_vcpus = 0;
	xenstat_vcpu *vcpu;
	char separator = 0;
	
	print("VCPU# VCPUs(s): ");

	num_vcpus = xenstat_domain_num_vcpus(domain);

	/* for all online vcpus dump out values */
	for (i=0; i< num_vcpus; i++) {
		vcpu = xenstat_domain_vcpu(domain,i);
		
		separator = (i+1<num_vcpus)?',':0;
		
		if (xenstat_vcpu_online(vcpu) > 0) {
			print("%2u %10llu%c", i, 
					xenstat_vcpu_ns(vcpu)/1000000000, separator);
		}
		else {
			print("%2u offline%c", i, separator);
		}
	}
	print("\n");
}

void do_json_vcpu(xenstat_domain *domain)
{
	int i = 0;
	unsigned num_vcpus = 0;
	xenstat_vcpu *vcpu;
	
// Maximum number of vcpus is limited by the num_vcpus type.
#define MAX_DIGIT_NUM_VCPUS sizeof(num_vcpus)*3
#define MAX_DIGIT_NUM_VCPUS_NSEC sizeof(unsigned long long int)*3
	char index_buf[MAX_DIGIT_NUM_VCPUS] = {0},
		vcpus_sec_buf[MAX_DIGIT_NUM_VCPUS_NSEC] = {0};
	int index_len = MAX_DIGIT_NUM_VCPUS,
		vcpus_nsec_len = MAX_DIGIT_NUM_VCPUS_NSEC;
	
	num_vcpus = xenstat_domain_num_vcpus(domain);
	
	GEN_OR_FAIL(yajl_gen_map_open(yghandle));
	
	/* for all online vcpus dump out values */
	for (i=0; i< num_vcpus; i++) {
		vcpu = xenstat_domain_vcpu(domain,i);
		
		{
			index_len = snprintf(index_buf, MAX_DIGIT_NUM_VCPUS, "%d", i);
			
			// Set key for domain
			GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)index_buf, index_len));
		}
		
		if (xenstat_vcpu_online(vcpu) > 0) {
			vcpus_nsec_len = snprintf(vcpus_sec_buf, MAX_DIGIT_NUM_VCPUS_NSEC, 
				"%llu",	xenstat_vcpu_ns(vcpu)/1000000000);
			
			GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)vcpus_sec_buf, vcpus_nsec_len));
		}
		else {
			
			GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"offline", 7));
		}
	}
	
	GEN_OR_FAIL(yajl_gen_map_close(yghandle));
}

/* Output all network information */
void do_network(xenstat_domain *domain)
{
	int i = 0;
	xenstat_network *network;
	unsigned num_networks = 0;
	
	/* How many networks? */
	num_networks = xenstat_domain_num_networks(domain);
	
	if (num_networks)
		print("IF# RX[ %12s %10s %8s %8s ] TX[ %12s %10s %8s %8s ]\n", "bytes", "pkts", "err", "drop"
				, "bytes", "pkts", "err", "drop");
	
	/* Dump information for each network */
	for (i=0; i < num_networks; i++) {
		/* Next get the network information */
		network = xenstat_domain_network(domain,i);
		
		
		
		print("%2d      %12llu %10llu %8llu %8llu",
		      i,
		      xenstat_network_rbytes(network),
		      xenstat_network_rpackets(network),
		      xenstat_network_rerrs(network),
		      xenstat_network_rdrop(network));

		print("       %12llu %10llu %8llu %8llu\n",
		      xenstat_network_tbytes(network),
		      xenstat_network_tpackets(network),
		      xenstat_network_terrs(network),
		      xenstat_network_tdrop(network));
	}
}

void do_json_network(xenstat_domain *domain)
{
	int i = 0;
	xenstat_network *network;
	unsigned num_networks = 0;

#define MAX_DIGIT_NUM_NET sizeof(unsigned long long int)*3
	char net_buf[MAX_DIGIT_NUM_NET] = {0};
	int net_len = MAX_DIGIT_NUM_NET;
	
	/* How many networks? */
	num_networks = xenstat_domain_num_networks(domain);
	
	/*
	if (num_networks)
		print("IF# RX[ %12s %10s %8s %8s ] TX[ %12s %10s %8s %8s ]\n", "bytes", "pkts", "err", "drop"
				, "bytes", "pkts", "err", "drop");
	*/
	
	GEN_OR_FAIL(yajl_gen_array_open(yghandle));
	
	/* Dump information for each network */
	for (i=0; i < num_networks; i++) {
		
		/* Next get the network information */
		network = xenstat_domain_network(domain,i);
		
		
		// Rx
		net_len = snprintf(net_buf, MAX_DIGIT_NUM_NET, "%llu", xenstat_network_rbytes(network));
		GEN_OR_FAIL(yajl_gen_number(yghandle, net_buf, net_len));
		
		net_len = snprintf(net_buf, MAX_DIGIT_NUM_NET, "%llu", xenstat_network_rpackets(network));
		GEN_OR_FAIL(yajl_gen_number(yghandle, net_buf, net_len));
		
		net_len = snprintf(net_buf, MAX_DIGIT_NUM_NET, "%llu", xenstat_network_rerrs(network));
		GEN_OR_FAIL(yajl_gen_number(yghandle, net_buf, net_len));
		
		net_len = snprintf(net_buf, MAX_DIGIT_NUM_NET, "%llu", xenstat_network_rdrop(network));
		GEN_OR_FAIL(yajl_gen_number(yghandle, net_buf, net_len));
		
		
		// Tx
		net_len = snprintf(net_buf, MAX_DIGIT_NUM_NET, "%llu", xenstat_network_tbytes(network));
		GEN_OR_FAIL(yajl_gen_number(yghandle, net_buf, net_len));
		
		net_len = snprintf(net_buf, MAX_DIGIT_NUM_NET, "%llu", xenstat_network_tpackets(network));
		GEN_OR_FAIL(yajl_gen_number(yghandle, net_buf, net_len));
		
		net_len = snprintf(net_buf, MAX_DIGIT_NUM_NET, "%llu", xenstat_network_terrs(network));
		GEN_OR_FAIL(yajl_gen_number(yghandle, net_buf, net_len));
		
		net_len = snprintf(net_buf, MAX_DIGIT_NUM_NET, "%llu", xenstat_network_tdrop(network));
		GEN_OR_FAIL(yajl_gen_number(yghandle, net_buf, net_len));
	}

	
	GEN_OR_FAIL(yajl_gen_array_close(yghandle));
}


/* Output all VBD information */
void do_vbd(xenstat_domain *domain)
{
	int i = 0;
	xenstat_vbd *vbd;
	unsigned num_vbds = 0;

	const char *vbd_type[] = {
		"Unidentified",				/* number 0 */
		"BlkBack",					/* number 1 */
		"BlkTap",					/* number 2 */
	};
	
	num_vbds = xenstat_domain_num_vbds(domain);
	
	if (num_vbds)
		print("vbdType device details       OO RD(total) WR(total) RD(sector) WR(sector)\n");
	
	for (i=0 ; i< num_vbds; i++) {
		char details[20];

		vbd = xenstat_domain_vbd(domain,i);

#if !defined(__linux__)
		details[0] = '\0';
#else
		snprintf(details, 20, "[%2x:%2x] ",
			 MAJOR(xenstat_vbd_dev(vbd)),
			 MINOR(xenstat_vbd_dev(vbd)));
#endif
		
		print("%-7s %6d %7s %8llu %9llu %9llu %10llu %10llu\n",
		      vbd_type[xenstat_vbd_type(vbd)],
		      xenstat_vbd_dev(vbd), details,
		      xenstat_vbd_oo_reqs(vbd),
		      xenstat_vbd_rd_reqs(vbd),
		      xenstat_vbd_wr_reqs(vbd),
		      xenstat_vbd_rd_sects(vbd),
		      xenstat_vbd_wr_sects(vbd));
	}
}

void do_json_vbd(xenstat_domain *domain)
{
	int i = 0;
	xenstat_vbd *vbd;
	unsigned num_vbds = 0;

#define MAX_DIGIT_NUM_VBD sizeof(unsigned long long int)*3
	char vbd_buf[MAX_DIGIT_NUM_VBD] = {0};
	int vbd_len = 0;
	int vbd_type_index = 0;
	
	const char *vbd_type[] = {
		"Unidentified",				/* number 0 */
		"BlkBack",					/* number 1 */
		"BlkTap",					/* number 2 */
	};
	
	const int vbd_type_size[] = {
		12, 7, 6
	};
	
	num_vbds = xenstat_domain_num_vbds(domain);
	
	/*
	if (num_vbds)
		print("vbdType device details       OO RD(total) WR(total) RD(sector) WR(sector)\n");
	*/
	
	// Start array
	GEN_OR_FAIL(yajl_gen_array_open(yghandle));
	
	for (i=0 ; i< num_vbds; i++) {
		char details[20];

		vbd = xenstat_domain_vbd(domain,i);

#if !defined(__linux__)
		details[0] = '\0';
#else
		snprintf(details, 20, "[%2x:%2x]",
			 MAJOR(xenstat_vbd_dev(vbd)),
			 MINOR(xenstat_vbd_dev(vbd)));
#endif
		
		GEN_OR_FAIL(yajl_gen_array_open(yghandle));
		
		// VBD Type
		vbd_type_index = xenstat_vbd_type(vbd);
		GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)vbd_type[vbd_type_index], vbd_type_size[vbd_type_index]));
		
		// Major:Minor
		vbd_len = strlen(details);
		GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)details, vbd_len));
		
		//
		vbd_len = snprintf(vbd_buf, MAX_DIGIT_NUM_VBD, "%llu", xenstat_vbd_oo_reqs(vbd));
		GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)vbd_buf, vbd_len));
		
		vbd_len = snprintf(vbd_buf, MAX_DIGIT_NUM_VBD, "%llu", xenstat_vbd_rd_reqs(vbd));
		GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)vbd_buf, vbd_len));
		
		vbd_len = snprintf(vbd_buf, MAX_DIGIT_NUM_VBD, "%llu", xenstat_vbd_wr_reqs(vbd));
		GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)vbd_buf, vbd_len));
		
		vbd_len = snprintf(vbd_buf, MAX_DIGIT_NUM_VBD, "%llu", xenstat_vbd_rd_sects(vbd));
		GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)vbd_buf, vbd_len));
		
		vbd_len = snprintf(vbd_buf, MAX_DIGIT_NUM_VBD, "%llu", xenstat_vbd_wr_sects(vbd));
		GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)vbd_buf, vbd_len));
		
		GEN_OR_FAIL(yajl_gen_array_close(yghandle));
	}
	
	GEN_OR_FAIL(yajl_gen_array_close(yghandle));
}

/* Output all tmem information */
void do_tmem(xenstat_domain *domain)
{
	xenstat_tmem *tmem = xenstat_domain_tmem(domain);
	unsigned long long curr_eph_pages = xenstat_tmem_curr_eph_pages(tmem);
	unsigned long long succ_eph_gets = xenstat_tmem_succ_eph_gets(tmem);
	unsigned long long succ_pers_puts = xenstat_tmem_succ_pers_puts(tmem);
	unsigned long long succ_pers_gets = xenstat_tmem_succ_pers_gets(tmem);

	if (curr_eph_pages | succ_eph_gets | succ_pers_puts | succ_pers_gets)
		print("Tmem:  Curr eph pages: %8llu   Succ eph gets: %8llu   "
	              "Succ pers puts: %8llu   Succ pers gets: %8llu\n",
			curr_eph_pages, succ_eph_gets,
			succ_pers_puts, succ_pers_gets);

}

void do_json_tmem(xenstat_domain *domain) {
	
	xenstat_tmem *tmem = xenstat_domain_tmem(domain);
	
	unsigned long long curr_eph_pages = xenstat_tmem_curr_eph_pages(tmem);
	unsigned long long succ_eph_gets = xenstat_tmem_succ_eph_gets(tmem);
	unsigned long long succ_pers_puts = xenstat_tmem_succ_pers_puts(tmem);
	unsigned long long succ_pers_gets = xenstat_tmem_succ_pers_gets(tmem);
	
#define MAX_DIGIT_NUM_TMEM sizeof(unsigned long long)*3
	char tmem_buf[MAX_DIGIT_NUM_TMEM] = {0};
	int tmem_len = 0;
	
	/*
	GEN_OR_FAIL(yajl_gen_array_open(yghandle));
	
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"Curr eph pages", 14));
	
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"Succ eph gets", 13));
	
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"Succ pers puts", 14));
	
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"Succ pers gets", 14));
	
	GEN_OR_FAIL(yajl_gen_array_close(yghandle));
	*/
	
	GEN_OR_FAIL(yajl_gen_array_open(yghandle));
	
	tmem_len = snprintf(tmem_buf, MAX_DIGIT_NUM_TMEM, "%llu", curr_eph_pages);
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)tmem_buf, tmem_len));
	
	tmem_len = snprintf(tmem_buf, MAX_DIGIT_NUM_TMEM, "%llu", succ_eph_gets);
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)tmem_buf, tmem_len));
	
	tmem_len = snprintf(tmem_buf, MAX_DIGIT_NUM_TMEM, "%llu", succ_pers_puts);
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)tmem_buf, tmem_len));
	
	tmem_len = snprintf(tmem_buf, MAX_DIGIT_NUM_TMEM, "%llu", succ_pers_gets);
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)tmem_buf, tmem_len));
	
	GEN_OR_FAIL(yajl_gen_array_close(yghandle));
}

static void top(void) {
	xenstat_domain **domains;
	unsigned int i, num_domains = 0;
	
	/* Now get the node information */
	if (prev_node != NULL)
		xenstat_free_node(prev_node);
	prev_node = cur_node;
	cur_node = xenstat_get_node(xhandle, XENSTAT_ALL);
	if (cur_node == NULL)
		fail("Failed to retrieve statistics from libxenstat\n");
	
	//const char *ver_str;
	//ver_str = xenstat_node_xen_version(cur_node);
	/* dump summary top information */
	do_summary();
		
	/* Count the number of domains for which to report data */
	num_domains = xenstat_node_num_domains(cur_node);

	domains = calloc(num_domains, sizeof(xenstat_domain *));
	if(domains == NULL)
		fail("Failed to allocate memory\n");

	for (i=0; i < num_domains; i++)
		domains[i] = xenstat_node_domain_by_index(cur_node, i);

	/* Sort */
	qsort(domains, num_domains, sizeof(xenstat_domain *),
	      (int(*)(const void *, const void *))compare_domains);

	if(first_domain_index >= num_domains)
		first_domain_index = num_domains-1;
		
	for (i = first_domain_index; i < num_domains; i++) {
		
		if (i == first_domain_index || repeat_header)
			do_header();
		
		do_domain(domains[i]);
		
		if (show_vcpus)
			do_vcpu(domains[i]);
		
		if (show_networks)
			do_network(domains[i]);
		
		if (show_vbds)
			do_vbd(domains[i]);
		
		if (show_tmem)
			do_tmem(domains[i]);
		
		if (i+1<num_domains) print("--\n");
	}
	
	print("----------------\n");
	
	free(domains);
}

static void json_top(void) {
	
	xenstat_domain **domains;
	unsigned int i, num_domains = 0;
	
#define NUMDOMAINS_MAX_DIGITS sizeof(unsigned int)*3 // by num_domains type
	int len;
	char buf[NUMDOMAINS_MAX_DIGITS] = {0};
	
	/* Now get the node information */
	if (prev_node != NULL)
		xenstat_free_node(prev_node);
	prev_node = cur_node;
	cur_node = xenstat_get_node(xhandle, XENSTAT_ALL);
	if (cur_node == NULL)
		fail("Failed to retrieve statistics from libxenstat\n");
	
	
	GEN_OR_FAIL(yajl_gen_map_open(yghandle));
	
	//const char *ver_str;
	//ver_str = xenstat_node_xen_version(cur_node);
	/* dump summary top information */
	GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"hypervisor", 10));
	
	do_json_summary();
		
	/* Count the number of domains for which to report data */
	num_domains = xenstat_node_num_domains(cur_node);

	domains = calloc(num_domains, sizeof(xenstat_domain *));
	if(domains == NULL)
		fail("Failed to allocate memory\n");

	for (i=0; i < num_domains; i++)
		domains[i] = xenstat_node_domain_by_index(cur_node, i);

	/* Sort */
	qsort(domains, num_domains, sizeof(xenstat_domain *),
	      (int(*)(const void *, const void *))compare_domains);

	if(first_domain_index >= num_domains)
		first_domain_index = num_domains-1;
	
		
	for (i = first_domain_index; i < num_domains; i++) {
		
		/* Do i really want to have the header always?
		if (i == first_domain_index || repeat_header) {
			
			// Set key for headers
			GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"header", 6));
			
			do_json_header();
		}
		*/
		
		// Set key for domain
		len = snprintf(buf, NUMDOMAINS_MAX_DIGITS, "%d", xenstat_domain_id(domains[i]));
		GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)buf, len));
		
		GEN_OR_FAIL(yajl_gen_map_open(yghandle));
		
		{
			GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"stat", 4));
			
			do_json_domain(domains[i]);
			
			
			if (show_vcpus) {
				
				// Set key for virtual cpus
				GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"vcpus", 5));
				
				do_json_vcpu(domains[i]);
			}
			
			if (show_networks) {
				
				// Set key for network
				GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"vifs", 4));
				
				do_json_network(domains[i]);
			}
			
			if (show_vbds) {
				// Set key for virtual blocks
				GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"vblks", 5));
				
				do_json_vbd(domains[i]);
			}
			
			if (show_tmem) {
				
				// Set key for tmem
				GEN_OR_FAIL(yajl_gen_string(yghandle, (const unsigned char *)"tmem", 4));
				
				do_json_tmem(domains[i]);
			}
		}
		GEN_OR_FAIL(yajl_gen_map_close(yghandle));
	}

	GEN_OR_FAIL(yajl_gen_map_close(yghandle));
	
	free(domains);
}

static int signal_exit;

static void signal_exit_handler(int sig)
{
	signal_exit = 1;
}

// Print the json
void print_json(yajl_gen g) {
	
	const unsigned char * buf;
	size_t len;
	
	yajl_gen_get_buf(g, &buf, &len);
	fwrite(buf, 1, len, stdout);
	yajl_gen_clear(g);
	print("\n");
}

int main(int argc, char **argv)
{
	int opt, optind = 0;
	char *subopts, *value;
	
	struct option lopts[] = {
		{ "help",				no_argument,       NULL, 'h' },
		{ "version",			no_argument,       NULL, 'V' },
		{ "repeat-header",		no_argument,       NULL, 'r' },
		{ "interval",			required_argument, NULL, 'i' },
		{ "iteration-count",	required_argument, NULL, 'c' },
		{ "identifier",			required_argument, NULL, 'f' },
		{ "type",				required_argument, NULL, 't' },
		{ 0, 0, 0, 0 },
	};
	const char *sopts = "hVri:c:f:t:";
	struct sigaction sa = {
		.sa_handler = signal_exit_handler,
		.sa_flags = 0
	};
	
	if (atexit(cleanup) != 0)
		fail("Failed to install cleanup handler.\n");

	while ((opt = getopt_long(argc, argv, sopts, lopts, &optind)) != -1) {
		switch (opt) {
			default:
				usage(argv[0]);
				exit(1);
			case '?':
			case 'h':
				usage(argv[0]);
				exit(0);
			case 'V':
				version();
				exit(0);
			case 'r':
				repeat_header = 1;
				break;
			case 'i':
				set_interval(optarg);
				break;
			case 'c':
				iterationCount = atoi(optarg);
				loop = 0;
				break;
			case 'f':
				subopts = optarg;
				while (*subopts != '\0') {
					switch (opt = getsubopt(&subopts, (char * const *)identifier_opts, &value)) {
						default:
							/* Unknown suboption. */
							print("Unknown suboption %d, `%s'\n", opt, value);
							break;
						
						case IDENT_NAME_OPT:
						case IDENT_FULL_OPT:
						case IDENT_ID_OPT:
							identifier = opt;
					}
				}
				
				break;
			case 't':
				subopts = optarg;
				while (*subopts != '\0') {
					switch (opt = getsubopt(&subopts, (char * const *)type_opts, &value)) {
						default:
							/* Unknown suboption. */
							print("Unknown suboption %d, `%s'\n", opt, value);
							break;
						
						case TYPE_ORG_OPT:
						case TYPE_CSV_OPT:
						case TYPE_JSON_OPT:
							ftype = opt;
					}
				}
				
				break;
		}
	}
	
	/* What to show */
	show_vcpus = 1;
	show_networks = 1;
	show_vbds = 1;
	show_tmem = 1;
	
	/* Get xenstat handle */
	xhandle = xenstat_init();
	if (xhandle == NULL)
		fail("Failed to initialize xenstat library\n");
	
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	do {
		gettimeofday(&curtime, NULL);
		
		
		if (ftype == TYPE_JSON_OPT) {
			// Allocate json object
			yghandle = yajl_gen_alloc(NULL);
			
			json_top();
			
			// Print json object
			print_json(yghandle);
		}
		else
			top();
		
		fflush(stdout);
		oldtime = curtime;
		if ((!loop) && !(--iterationCount))
			break;
		sleep(interval);
	} while (!signal_exit);

	/* Cleanup occurs in cleanup(), so no work to do here. */
	
	return 0;
}
