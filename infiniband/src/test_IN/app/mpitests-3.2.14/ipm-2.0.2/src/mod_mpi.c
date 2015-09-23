
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#include "ipm.h"
#include "report.h"
#include "mod_mpi.h"
#include "ipm_debug.h"
#include "ipm_modules.h"
#include "GEN.calltable_mpi.h"

char* ipm_mpi_op[MAXNUM_MPI_OPS];
char* ipm_mpi_type[MAXNUM_MPI_TYPES];

mpidata_t mpidata[MAXNUM_REGIONS];

MPI_Group ipm_world_group;

int mod_mpi_xml(ipm_mod_t* mod, void *ptr, struct region *reg);
int mod_mpi_region(ipm_mod_t*mod, int op, struct region *reg);

// 
// Optionally add barrier so we can measure performance of collective operations without noise introduced by application imbalance
//
int ipm_add_barrier_to_reduce = 0;
int ipm_add_barrier_to_allreduce = 0;
int ipm_add_barrier_to_gather = 0;
int ipm_add_barrier_to_allgather = 0;
int ipm_add_barrier_to_alltoall = 0;
int ipm_add_barrier_to_alltoallv = 0;
int ipm_add_barrier_to_bcast = 0;
int ipm_add_barrier_to_scatter = 0;
int ipm_add_barrier_to_scatterv = 0;
int ipm_add_barrier_to_gatherv = 0;
int ipm_add_barrier_to_allgatherv = 0;
int ipm_add_barrier_to_reduce_scatter = 0;

int ipm_mpi_get_val(const char* entry)
{
    int val = 0;

    char* str = getenv(entry);
    if (str != NULL) {
        char c = toupper(str[0]);
        if ((c == '1') || (c == 'T') || (c == 'Y')) {
            val = 1;
            IPMDBG0("%s\n", entry);
        }
    }

    return val;
}

void ipm_mpi_get_env()
{
    IPMDBG0("Reading MPI-specific env variables for IPM\n");
    ipm_add_barrier_to_reduce         = ipm_mpi_get_val("IPM_ADD_BARRIER_TO_REDUCE");
    ipm_add_barrier_to_allreduce      = ipm_mpi_get_val("IPM_ADD_BARRIER_TO_ALLREDUCE");
    ipm_add_barrier_to_gather         = ipm_mpi_get_val("IPM_ADD_BARRIER_TO_GATHER");
    ipm_add_barrier_to_allgather      = ipm_mpi_get_val("IPM_ADD_BARRIER_TO_ALL_GATHER");
    ipm_add_barrier_to_alltoall       = ipm_mpi_get_val("IPM_ADD_BARRIER_TO_ALLTOALL");
    ipm_add_barrier_to_alltoallv      = ipm_mpi_get_val("IPM_ADD_BARRIER_TO_ALLTOALLV");
    ipm_add_barrier_to_bcast          = ipm_mpi_get_val("IPM_ADD_BARRIER_TO_BROADCAST");
    ipm_add_barrier_to_scatter        = ipm_mpi_get_val("IPM_ADD_BARRIER_TO_SCATTER");
    ipm_add_barrier_to_scatterv       = ipm_mpi_get_val("IPM_ADD_BARRIER_TO_SCATTERV");
    ipm_add_barrier_to_gatherv        = ipm_mpi_get_val("IPM_ADD_BARRIER_TO_GATHERV");
    ipm_add_barrier_to_allgatherv     = ipm_mpi_get_val("IPM_ADD_BARRIER_TO_ALLGATHERV");
    ipm_add_barrier_to_reduce_scatter = ipm_mpi_get_val("IPM_ADD_BARRIER_TO_REDUCE_SCATTER");
}

int mod_mpi_init(ipm_mod_t* mod, int flags)
{
  int i;

  ipm_mpi_get_env(); // read the MPI-specific environment variables

  mod->state    = STATE_IN_INIT;
  mod->init     = mod_mpi_init;
  mod->output   = mod_mpi_output;
  mod->finalize = mod_mpi_finalize; 
  mod->xml      = mod_mpi_xml;
  mod->regfunc  = mod_mpi_region;
  mod->name     = "MPI";
  mod->ct_offs  = MOD_MPI_OFFSET;
  mod->ct_range = MOD_MPI_RANGE;

  copy_mpi_calltable();

  for( i=0; i<MAXNUM_REGIONS; i++ ) {
    mpidata[i].mtime=0.0;
    mpidata[i].mtime_e=0.0;
  }

  for( i=0; i<MAXNUM_MPI_OPS; i++ ) {
    ipm_mpi_op[i]="";
  }
  for( i=0; i<MAXNUM_MPI_TYPES; i++ ) {
    ipm_mpi_type[i]="";
  }
  
  ipm_mpi_op[IPM_MPI_MAX]    = "MPI_MAX";
  ipm_mpi_op[IPM_MPI_MIN]    = "MPI_MIN";
  ipm_mpi_op[IPM_MPI_SUM]    = "MPI_SUM";
  ipm_mpi_op[IPM_MPI_PROD]   = "MPI_PROD";
  ipm_mpi_op[IPM_MPI_LAND]   = "MPI_LAND";
  ipm_mpi_op[IPM_MPI_BAND]   = "MPI_BAND";
  ipm_mpi_op[IPM_MPI_LOR]    = "MPI_LOR";
  ipm_mpi_op[IPM_MPI_BOR]    = "MPI_BOR";
  ipm_mpi_op[IPM_MPI_LXOR]   = "MPI_LXOR";
  ipm_mpi_op[IPM_MPI_BXOR]   = "MPI_BXOR";
  ipm_mpi_op[IPM_MPI_MINLOC] = "MPI_MINLOC";
  ipm_mpi_op[IPM_MPI_MAXLOC] = "MPI_MAXLOC";

  ipm_mpi_type[IPM_MPI_CHAR]    = "MPI_CHAR";
  ipm_mpi_type[IPM_MPI_BYTE]    = "MPI_BYTE";
  ipm_mpi_type[IPM_MPI_SHORT]   = "MPI_SHORT";
  ipm_mpi_type[IPM_MPI_INT]     = "MPI_INT";
  ipm_mpi_type[IPM_MPI_LONG]    = "MPI_LONG";
  ipm_mpi_type[IPM_MPI_FLOAT]   = "MPI_FLOAT";
  ipm_mpi_type[IPM_MPI_DOUBLE]  = "MPI_DOUBLE";
  ipm_mpi_type[IPM_MPI_UNSIGNED_CHAR]  = "MPI_UNSIGNED_CHAR";
  ipm_mpi_type[IPM_MPI_UNSIGNED_SHORT] = "MPI_UNSIGNED_SHORT";
  ipm_mpi_type[IPM_MPI_UNSIGNED]       = "MPI_UNSIGNED";
  ipm_mpi_type[IPM_MPI_UNSIGNED_LONG]  = "MPI_UNSIGNED_LONG";
  ipm_mpi_type[IPM_MPI_LONG_DOUBLE]    = "MPI_LONG_DOUBLE";
  ipm_mpi_type[IPM_MPI_LONG_LONG_INT]  = "MPI_LONG_LONG_INT";
  ipm_mpi_type[IPM_MPI_FLOAT_INT]      = "MPI_FLOAT_INT";
  ipm_mpi_type[IPM_MPI_LONG_INT]       = "MPI_LONG_INT";
  ipm_mpi_type[IPM_MPI_DOUBLE_INT]     = "MPI_DOUBLE_INT";
  ipm_mpi_type[IPM_MPI_SHORT_INT]      = "MPI_SHORT_INT";
  ipm_mpi_type[IPM_MPI_2INT]           = "MPI_2INT";
  ipm_mpi_type[IPM_MPI_LONG_DOUBLE_INT]  = "MPI_LONG_DOUBLE_INT";
  ipm_mpi_type[IPM_MPI_PACKED]   = "MPI_PACKED";
  ipm_mpi_type[IPM_MPI_UB]       = "MPI_UB";
  ipm_mpi_type[IPM_MPI_LB]       = "MPI_LB";
  ipm_mpi_type[IPM_MPI_REAL]     = "MPI_REAL";
  ipm_mpi_type[IPM_MPI_INTEGER]  = "MPI_INTEGER";
  ipm_mpi_type[IPM_MPI_LOGICAL]  = "MPI_LOGICAL";
  ipm_mpi_type[IPM_MPI_DOUBLE_PRECISION]  = "MPI_DOUBLE_PRECISION";
  ipm_mpi_type[IPM_MPI_COMPLEX]  = "MPI_COMPLEX";
  ipm_mpi_type[IPM_MPI_DOUBLE_COMPLEX]  = "MPI_DOUBLE_COMPLEX";
  ipm_mpi_type[IPM_MPI_INTEGER1] = "MPI_INTEGER1";
  ipm_mpi_type[IPM_MPI_INTEGER2] = "MPI_INTEGER2";
  ipm_mpi_type[IPM_MPI_INTEGER4] = "MPI_INTEGER4";
  ipm_mpi_type[IPM_MPI_REAL4]    = "MPI_REAL4";
  ipm_mpi_type[IPM_MPI_REAL8]    = "MPI_REAL8";
  ipm_mpi_type[IPM_MPI_2INTEGER] = "MPI_2INTEGER";
  ipm_mpi_type[IPM_MPI_2REAL]    = "MPI_2REAL";
  ipm_mpi_type[IPM_MPI_2DOUBLE_PRECISION] = "MPI_2DOUBLE_PRECISION";
  ipm_mpi_type[IPM_MPI_2COMPLEX]  = "MPI_2COMPLEX";
  ipm_mpi_type[IPM_MPI_2DOUBLE_COMPLEX] = "MPI_2DOUBLE_COMPLEX";

  mod->state    = STATE_ACTIVE;
  return IPM_OK;
}

int mod_mpi_output(ipm_mod_t* mod, int oflags)
{
  unsigned long reportflags;
  
  reportflags=0;

#ifdef HAVE_CLUSTERING
  reportflags|=XML_CLUSTERED;
  reportflags|=XML_RELATIVE_RANKS;
#endif  

  if( (task.flags&FLAG_LOG_TERSE) ||
      (task.flags&FLAG_LOG_FULL) ) 
    {
      report_set_filename();

      if( (task.flags&FLAG_LOGWRITER_MPIIO) )  {
	if( report_xml_mpiio(reportflags)!=IPM_OK ) 
	  {
	    IPMERR("Writing log using MPI-IO failed, trying serial\n");
	    report_xml_atroot(reportflags);
	  }
      }
      else {
	report_xml_atroot(reportflags);
      }
      
    }
  return IPM_OK;
}

int mod_mpi_finalize(ipm_mod_t* mod, int flags)
{
  return IPM_OK;
}

int mod_mpi_xml(ipm_mod_t* mod, void *ptr, struct region *reg) 
{
  struct region *tmp;
  ipm_hent_t stats;
  double time;
  int res=0;
  
  if( !reg ) {
    time = ipm_mtime();
  }
  else {
    time = mpidata[reg->id].mtime;

    if( (reg->flags)&FLAG_PRINT_EXCLUSIVE ) {
      tmp = reg->child;
      while(tmp) {
	time -= mpidata[tmp->id].mtime;
	tmp = tmp->next;
      }
    }
  }

  res+=ipm_printf(ptr, 
		  "<module name=\"%s\" time=\"%.5e\" ></module>\n",
		  mod->name, time);
  
  return res; 
}


int mod_mpi_region(ipm_mod_t *mod, int op, struct region *reg)
{
  double mtime;
  if( !reg ) return 0;

  mtime = ipm_mtime();

  switch(op) 
    {
    case -1: /* exit */
      mpidata[reg->id].mtime += (mtime - (mpidata[reg->id].mtime_e));
      break;
      
    case 1: /* enter */
      mpidata[reg->id].mtime_e=mtime;
      break;
  }

  return 0;
}
