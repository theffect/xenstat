xenstat
=======

xentop doesn't provide good 'iostat/vmstat' like output.
so why not just improve 'xentop' and actully name it xenstat.


Usage
=======

Usage: xenstat [OPTION]
Displays ongoing information about xen vm resources 

-h, --help           display this help and exit
-V, --version        output version information and exit
-i, --interval=SECONDS  seconds between updates (default 1)
-r, --repeat-header  repeat table header before each domain
-c, --iteration-count     count of iterations before exiting
-f, --identifier      output the full domain name (not truncated) or domain id

Report bugs to <shimon.zadok@gmail.com>.


Output example
==============

Date: 11/08/13 23:39:15 Domains: 5, 1 running, 4 blocked, 0 paused, 0 crashed, 0 dying, 0 shutdown Mem: 6291000k total, 4798800k used, 1492200k free    CPUs: 2 @ 2109MHz
      NAME(s)  STATE   CPU(sec) CPU(%)     MEM(k) MEM(%)  MAXMEM(k) MAXMEM(%) VCPUS NETS NETTX(k) NETRX(k) VBDS   VBD_OO   VBD_RD   VBD_WR  VBD_RSECT  VBD_WSECT SSID
Domain-0   r       6403    0.0     523912    8.3   no limit       n/a     2    0        0        0    0        0        0        0          0          0    0
VCPU# VCPUs(sec)
 0       4862s 1       1540s
----------------
DomU000   b       5155    0.0    2088960   33.2    2088960      33.2     1    2   165109   866210    3        0   585552  2096100   20973752   32326786    0
VCPU# VCPUs(sec)
 0       5155s
IF# RX[        bytes       pkts      err     drop ] TX[        bytes       pkts      err     drop ]
 0         811555694     571117        0        0          152125237     883765        0        0
 1          75443971      35796        0        0           16946574     146340        0        0
vbdType device details       OO RD(total) WR(total) RD(sector) WR(sector)
BlkBack  51713 [ca: 1]         0        85         1       1870          2
BlkBack  51714 [ca: 2]         0        34         4       1320         96
BlkBack  51728 [ca:10]         0    585433   2096095   20970562   32326688
----------------
DomU001   b       1191    0.0     524288    8.3     524288       8.3     1    2    36053    39705    1        0     9044   109065     310322    2148304    0
VCPU# VCPUs(sec)
 0       1191s
IF# RX[        bytes       pkts      err     drop ] TX[        bytes       pkts      err     drop ]
 0          40658161      56248        0        0           36905448     158200        0        0
 1                 0          0        0        0              13246         45        0        0
vbdType device details       OO RD(total) WR(total) RD(sector) WR(sector)
BlkBack  51713 [ca: 1]         0      9044    109065     310322    2148304
----------------
DomU002   b       9061    0.0    1044452   16.6    1044480      16.6     1    4    83194    19152    1        0        0        0          0          0    0
VCPU# VCPUs(sec)
 0       9061s
IF# RX[        bytes       pkts      err     drop ] TX[        bytes       pkts      err     drop ]
 0                 0          0        0        0                  0          0        0        0
 1           6174403      69792        0        0           14248056     153433        0        0
 2          13438183      12784        0        0           70943558     214256        0        0
 3                 0          0        0        0                  0          0        0        0
vbdType device details       OO RD(total) WR(total) RD(sector) WR(sector)
BlkBack    768 [ 3: 0]         0         0         0          0          0
----------------
DomU003  b        117    0.0     524288    8.3     524288       8.3     1    1     1541      221    2        0     3246     1858     108674      94896    0
VCPU# VCPUs(sec)
 0        117s
IF# RX[        bytes       pkts      err     drop ] TX[        bytes       pkts      err     drop ]
 0            226972       1278        0        0            1578482      10947        0        0
vbdType device details       OO RD(total) WR(total) RD(sector) WR(sector)
BlkBack  51714 [ca: 2]         0      3122      1858     107450      94896
BlkBack  51713 [ca: 1]         0       124         0       1224          0
----------------
