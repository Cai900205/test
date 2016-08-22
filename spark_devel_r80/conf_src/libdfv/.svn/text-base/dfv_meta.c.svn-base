#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include "dfv.h"
#include "fica_opt.h"

dfv_fmeta_t* dfv_fmeta_get(dfv_repo_t* repo, int slot_id)
{
    dfv_rmeta_t* rmeta = dfv_rmeta_get(repo);
    return(rmeta->fmeta_tbl[slot_id]);
}

dfv_rmeta_t* dfv_rmeta_get(dfv_repo_t* repo)
{
    return(&repo->rmeta);
}

void dfv_rmeta_reset(dfv_rmeta_t* rmeta)
{
    for(int i=0; i<DFV_MAX_SLOTS; i++) {
        SAFE_RELEASE(rmeta->fmeta_tbl[i]);
    }
    memset(rmeta, 0, sizeof(dfv_rmeta_t));
    
    rmeta->version = DFV_META_VERSION;
    zlog_info(dfv_zc, "reset meta with default settings");
    return;
}

void dfv_rmeta_dump(zlog_level ll, dfv_rmeta_t* rmeta)
{
    char    outstr[1024];

    DFV_LOG(ll, "dump rmeta@%p", rmeta);
    DFV_LOG(ll, "  - version : %d", rmeta->version);
    for(int i=0; i<DFV_MAX_SLOTS; i++) {
        dfv_fmeta_t* fmeta = rmeta->fmeta_tbl[i];
        if (fmeta) {
            struct tm* local_time = localtime((time_t*)&fmeta->file_time);
            char tmstr[256];
            strftime(tmstr, sizeof(tmstr), "%Y/%m/%d-%H:%M:%S", local_time);  
            sprintf(outstr, "#%02d : slices=%d, slice_sz=%zu, slot_sz=%lu, time=\'%s\'",
                        fmeta->slot_id,
                        fmeta->slice_num,
                        fmeta->slice_sz,
                        fmeta->slot_sz,
                        tmstr);
            DFV_LOG(ll, "  - %s", outstr);
        }
    }
    DFV_LOG(ll, "dump rmeta@%p done", rmeta);
    return;
}

int dfv_rmeta_load(dfv_rmeta_t* rmeta, const char* path)
{
    FILE*   fd = NULL;
    char    instr[1024];
    
    fd = fopen(path, "r");
    if (!fd) {
        zlog_warn(dfv_zc, "failed to open rmeta file: path=%s, errmsg=%s",
                    path, strerror(errno));
        return (SPKERR_BADRES);
    }
    
    dfv_rmeta_reset(rmeta);
    int line_cnt = 0;
    memset(rmeta, 0, sizeof(dfv_rmeta_t));
    while(fgets(instr, 1024, fd)) {
        char* key;
        char* value;
        line_cnt++;
        if (fica_shift_option(instr, &key, &value)) {
            if (!strcmp(key, "version")) {
                rmeta->version = atoi(value);
            } else if (!strcmp(key, "filemeta")) {
                dfv_fmeta_t i_fmeta;
                memset(&i_fmeta, 0, sizeof(dfv_fmeta_t));
                sscanf(value, "%d,%d,%lu,%lu,%lu",
                        &i_fmeta.slot_id,
                        &i_fmeta.slice_num,
                        &i_fmeta.slice_sz,
                        &i_fmeta.slot_sz,
                        &i_fmeta.file_time);
                if (i_fmeta.slot_id < 0 || i_fmeta.slot_id >= DFV_MAX_SLOTS) {
                    zlog_error(dfv_zc, "Illegal slot_id in line %d, skip: id=%d",
                                 line_cnt, i_fmeta.slot_id);
                    continue;
                }
                dfv_fmeta_t* fmeta = rmeta->fmeta_tbl[i_fmeta.slot_id];
                if (fmeta) {
                    zlog_error(dfv_zc, "Duplicate slot_id in line %d, skip: id=%d",
                                 line_cnt, i_fmeta.slot_id);
                    continue;
                }
                fmeta = malloc(sizeof(dfv_fmeta_t));
                memcpy(fmeta, &i_fmeta, sizeof(dfv_fmeta_t));
                pthread_mutex_init(&fmeta->open_cnt_lock, NULL);
                rmeta->fmeta_tbl[i_fmeta.slot_id] = fmeta;
            } else {
                zlog_error(dfv_zc, "Syntax error in line %d, skip", line_cnt);
            }
        }
    }

    fclose(fd);
    zlog_notice(dfv_zc, "rmeta loaded: path=%s",
                 path);
    return(SPK_SUCCESS);
}

int dfv_rmeta_save(dfv_rmeta_t* rmeta, const char* path)
{
    FILE* fd = NULL;
    char  outstr[SPK_MAX_PATHNAME];
    
    fd = fopen(path, "w");
    if (!fd) {
        zlog_fatal(dfv_zc, "Failed to open rmeta file: path=%s, errmsg=%s",
                    path, strerror(errno));
        return (SPKERR_BADRES);
    }
    
    sprintf(outstr, "version=%d\n", rmeta->version);
    fputs(outstr, fd);
    for (int i=0; i<DFV_MAX_SLOTS; i++) {
        dfv_fmeta_t* fmeta = rmeta->fmeta_tbl[i];
        if (fmeta) {
            sprintf(outstr, "filemeta=%d,%d,%lu,%lu,%lu\n",
                        fmeta->slot_id,
                        fmeta->slice_num,
                        fmeta->slice_sz,
                        fmeta->slot_sz,
                        fmeta->file_time);
            fputs(outstr, fd);
        }
    }
    
    fclose(fd);

    zlog_notice(dfv_zc, "rmeta saved: path=%s", path);
    return(SPK_SUCCESS);
}
