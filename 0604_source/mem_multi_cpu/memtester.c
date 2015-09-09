/*
 * memtester version 4
 *
 * Very simple but very effective user-space memory tester.
 * Originally by Simon Kirby <sim@stormix.com> <sim@neato.org>
 * Version 2 by Charles Cazabon <charlesc-memtester@pyropus.ca>
 * Version 3 not publicly released.
 * Version 4 rewrite:
 * Copyright (C) 2004-2012 Charles Cazabon <charlesc-memtester@pyropus.ca>
 * Licensed under the terms of the GNU General Public License version 2 (only).
 * See the file COPYING for details.
 *
 */

#define __version__ "4.3.0"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#define __USE_GNU
#include <limits.h>
#include <sched.h>
#include <signal.h>

#include "types.h"
#include "sizes.h"
#include "tests.h"
#include "help.h"
#define EXIT_FAIL_NONSTARTER    0x01
#define EXIT_FAIL_ADDRESSLINES  0x02
#define EXIT_FAIL_OTHERTEST     0x04
#define THREAD_TOTAL_NUM 24

struct test tests[] = {
    { "Random Value", test_random_value },
    { "Compare XOR", test_xor_comparison },
    { "Compare SUB", test_sub_comparison },
    { "Compare MUL", test_mul_comparison },
    { "Compare DIV",test_div_comparison },
    { "Compare OR", test_or_comparison },
    { "Compare AND", test_and_comparison },
    { "Sequential Increment", test_seqinc_comparison },
    { "Solid Bits", test_solidbits_comparison },
    { "Block Sequential", test_blockseq_comparison },
    { "Checkerboard", test_checkerboard_comparison },
    { "Bit Spread", test_bitspread_comparison },
    { "Bit Flip", test_bitflip_comparison },
    { "Walking Ones", test_walkbits1_comparison },
    { "Walking Zeroes", test_walkbits0_comparison },
#ifdef TEST_NARROW_WRITES    
    { "8-bit Writes", test_8bit_wide_random },
    { "16-bit Writes", test_16bit_wide_random },
#endif
    { NULL, NULL }
};

/* Sanity checks and portability helper macros. */
#ifdef _SC_VERSION
void check_posix_system(void) {
    if (sysconf(_SC_VERSION) < 198808L) {
        fprintf(stderr, "A POSIX system is required.  Don't be surprised if "
            "this craps out.\n");
        fprintf(stderr, "_SC_VERSION is %lu\n", sysconf(_SC_VERSION));
    }
}
#else
#define check_posix_system()
#endif

#ifdef _SC_PAGE_SIZE
int memtester_pagesize(void) {
    int pagesize = sysconf(_SC_PAGE_SIZE);
    if (pagesize == -1) {
        perror("get page size failed");
        exit(EXIT_FAIL_NONSTARTER);
    }
    printf("pagesize is %ld\n", (long) pagesize);
    return pagesize;
}
#else
int memtester_pagesize(void) {
    printf("sysconf(_SC_PAGE_SIZE) not supported; using pagesize of 8192\n");
    return 8192;
}
#endif
struct task_type
{
   ptrdiff_t pagesizemask;
   size_t wantbytes;
   size_t wantbytes_orig;
   size_t pagesize;
   size_t loops;
   pthread_t id;
   int bind;
   size_t time;
   size_t interval;
   size_t cpu;
};

void * memtest(void *arg)
{
  int done_mem=0;
  ul loop,i;
  int ret=0;
  int do_mlock=1;
  struct task_type *send=arg;
  ul loops=send->loops;
  size_t pagesize=send->pagesize; 
  size_t wantbytes=send->wantbytes; 
  size_t wantbytes_orig=send->wantbytes_orig;
  ptrdiff_t pagesizemask=send->pagesizemask;
  size_t bufsize,halflen, count;
  void volatile *buf, *aligned;
  ulv *bufa, *bufb;
  ul testmask=0;
  int exit_code = 0;
  int result=-1;
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset); 
  if(send->bind)
  {
        if(sched_getaffinity(0,sizeof(cpuset),&cpuset) == -1)
        {
           printf("warning: cound not get cpu affinity!\n");
           return (void*)-1;
        }
	result = pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&cpuset);
        if (result)
	{
           printf("[MEMTEST]: Bind cpu  failed\n");
           fflush(stdout);
	   return (void*)-1;
	}

  }
  while (!done_mem) {
        while (!buf && wantbytes) {
            buf = (void volatile *) malloc(wantbytes);
            if (!buf) wantbytes -= pagesize;
        }
        bufsize = wantbytes;
        printf("got  %lluMB (%llu bytes)", (ull) wantbytes >> 20,
            (ull) wantbytes);
        fflush(stdout);
        if (do_mlock) {
            printf(", trying mlock ...");
            fflush(stdout);
            if ((size_t) buf % pagesize) {
                /* printf("aligning to page -- was 0x%tx\n", buf); */
                aligned = (void volatile *) ((size_t) buf & pagesizemask) + pagesize;
                /* printf("  now 0x%tx -- lost %d bytes\n", aligned,
                 *      (size_t) aligned - (size_t) buf);
                 */
                bufsize -= ((size_t) aligned - (size_t) buf);
            } else {
                aligned = buf;
            }
            /* Try mlock */
            if (mlock((void *) aligned, bufsize) < 0) {
                switch(errno) {
                    case EAGAIN: /* BSDs */
                        printf("over system/pre-process limit, reducing...\n");
                        free((void *) buf);
                        buf = NULL;
                        wantbytes -= pagesize;
                        break;
                    case ENOMEM:
                        printf("too many pages, reducing...\n");
                        free((void *) buf);
                        buf = NULL;
                        wantbytes -= pagesize;
                        break;
                    case EPERM:
                        printf("insufficient permission.\n");
                        printf("Trying again, unlocked:\n");
                        do_mlock = 0;
                        free((void *) buf);
                        buf = NULL;
                        wantbytes = wantbytes_orig;
                        break;
                    default:
                        printf("failed for unknown reason.\n");
                        do_mlock = 0;
                        done_mem = 1;
                }
            } else {
                printf("locked.\n");
                done_mem = 1;
            }
        } else {
            done_mem = 1;
            printf("\n");
        }
    }
    if (!do_mlock) fprintf(stderr, "Continuing with unlocked memory; testing "
                           "will be slower and less reliable.\n");

    halflen = bufsize / 2;
    count = halflen / sizeof(ul);
    bufa = (ulv *) aligned;
    bufb = (ulv *) ((size_t) aligned + halflen);
    struct timeval tm_start,tm_end;
    gettimeofday(&tm_start,NULL);
    for(loop=1; ((!loops) || loop <= loops); loop++) {
        printf("Loop %lu", loop);
        if (loops) {
            printf("/%lu", loops);
        }
        printf(":\n");
        printf("  %-20s: ", "Stuck Address");
        fflush(stdout);
        if (!test_stuck_address(aligned, bufsize / sizeof(ul))) {
             printf("ok\n");
        } else {
            exit_code |= EXIT_FAIL_ADDRESSLINES;
        }
        for (i=0;;i++) {
            if (!tests[i].name) break;
            /* If using a custom testmask, only run this test if the
               bit corresponding to this test was set by the user.
             */
/*            if (testmask && (!((1 << i) & testmask))) {
                continue;
            }*/
            printf("  %-20s: ", tests[i].name);
            if (!tests[i].fp(bufa, bufb, count)) {
                printf("ok\n");
            } else {
                exit_code |= EXIT_FAIL_OTHERTEST;
            } 
            fflush(stdout);
        }
        printf("\n");
        fflush(stdout);
 
        gettimeofday(&tm_end,NULL);
        float diff = ((tm_end.tv_sec -tm_start.tv_sec)*1000000+(tm_end.tv_usec-tm_start.tv_usec))/1000000.0;
        if((diff > send->time)&&(loops))
        {
            printf("MEMTEST time over!\n");
            fflush(stdout);
            break;
        }
        sleep(send->interval);
    }
    if (do_mlock) munlock((void *) aligned, bufsize);
    printf("Done.\n");
    fflush(stdout);
}



/* Some systems don't define MAP_LOCKED.  Define it to 0 here
   so it's just a no-op when ORed with other constants. */
#ifndef MAP_LOCKED
  #define MAP_LOCKED 0
#endif


/* Global vars - so tests have access to this information */
int use_phys = 0;
off_t physaddrbase = 0;


int main(int argc, char **argv) {
    ul loops=10, loop, i,workers=1;
    size_t pagesize, wantraw, wantmb, wantbytes, wantbytes_orig;
    char *memsuffix, *addrsuffix, *loopsuffix,*threadsuffix,*cpusuffix;
    ptrdiff_t pagesizemask;
    int exit_code = 0;
    int memfd, opt, memshift;
    size_t maxbytes = -1; /* addressable memory, in bytes */
    size_t maxmb = (maxbytes >> 20) + 1; /* addressable memory, in MB */
    /* Device to mmap memory from with -p, default is normal core */
    char *device_name = "/dev/mem";
    struct stat statbuf;
    int device_specified = 0;
    char *env_testmask = 0;
    ul testmask = 0;
    size_t time=10;
    size_t bind=0;
    size_t interval=1;
    

    printf("memtester version " __version__ " (%d-bit)\n", UL_LEN);
    printf("Copyright (C) 2001-2012 Charles Cazabon.\n");
    printf("Licensed under the GNU General Public License version 2 (only).\n");
    printf("\n");
    check_posix_system();
    pagesize = memtester_pagesize();
    pagesizemask = (ptrdiff_t) ~(pagesize - 1);
    printf("pagesizemask is 0x%tx\n", pagesizemask);
    

    for (i = 1; i < argc; i++)
    {
        char* arg = argv[i];
	if (!strcmp(arg, "--passes") && i + 1 < argc)
	{
		loops = strtoul(argv[++i],NULL,10);
	}
	else if (!strcmp(arg, "--workers") && i + 1 < argc)
	{
		workers = atoi(argv[++i]);
        if(workers > 24|| workers<1)
        {
                printf("workers param error!\n");
                fflush(stdout);
                return -1;
        }
	}
	else if (!strcmp(arg, "--memsize") && i + 1 < argc)
	{
                errno = 0;
                wantraw = (size_t) strtoul(argv[++i], &memsuffix, 0);
                if (errno != 0) {
                   fprintf(stderr, "failed to parse memory argument");
                   usage(argv[0]); /* doesn't return */
                }
                switch (*memsuffix) {
                   case 'G':
                   case 'g':
                      memshift = 30; /* gigabytes */
                      break;
                   case 'M':
                   case 'm':
                      memshift = 20; /* megabytes */
                      break;
                   case 'K':
                   case 'k':
                      memshift = 10; /* kilobytes */
                      break;
                   case 'B':
                   case 'b':
                      memshift = 0; /* bytes*/
                      break;
                   case '\0':  /* no suffix */
                      memshift = 20; /* megabytes */
                      break;
                   default:
                      usage(); /* doesn't return */
                }
                wantbytes_orig = wantbytes = ((size_t) wantraw << memshift);
                wantmb = (wantbytes_orig >> 20);
                if (wantmb > maxmb) {
                   fprintf(stderr, "This system can only address %llu MB.\n", (ull) maxmb);
                   exit(EXIT_FAIL_NONSTARTER);
                }
                if (wantbytes < pagesize) {
                   fprintf(stderr, "bytes %ld < pagesize %ld -- memory argument too large?\n",
                        wantbytes, pagesize);
                   exit(EXIT_FAIL_NONSTARTER);
                }
	}
	else if (!strcmp(arg, "--interval") && i + 1 < argc)
	{
		interval = atoi(argv[++i]);
	}
	else if (!strcmp(arg, "--bind"))
	{
                bind=1;
	}
	else if (!strcmp(arg, "--time") && i + 1 < argc) 
    {
                time = strtoull(argv[++i],NULL,10);
    }
	else if (!strcmp(arg, "--help")) 
    { 
                usage();
                fflush(stdout);
                return 0;
    }
	else if ( !strcmp(arg,"--version")) 
    { 
                printf("MEMTEST VERSION:POWERPC-MEMTEST-V1.00\n");
                fflush(stdout);
                return 0;
    }
    else
    {
	    printf("Unknown option: %s\n", arg);
        usage();
        fflush(stdout);
	    return -1;
	}
    }

    struct task_type task_test[50];
    int err=0;
    for(i=0;i<workers;i++)
    {

       task_test[i].wantbytes = wantbytes;
       task_test[i].wantbytes_orig = wantbytes_orig;
       task_test[i].pagesize = pagesize;
       task_test[i].pagesizemask = pagesizemask;

       task_test[i].loops=loops;
       task_test[i].bind = bind;
       task_test[i].time=time;
       task_test[i].interval=interval;
       task_test[i].cpu=i;

       err = pthread_create(&task_test[i].id,NULL,memtest,&task_test[i]);
       if (err) {
		printf("memtest create thread failed!\n");
		exit(-1);
       } 
   }
   for(i=0;i<workers;i++)
   {
	pthread_join(task_test[i].id,NULL);
   }
    exit(exit_code);
}
