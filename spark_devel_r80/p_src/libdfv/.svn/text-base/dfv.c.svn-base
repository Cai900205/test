#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>

#include "dfv.h"

zlog_category_t* dfv_zc = NULL;

int dfv_module_init(const char* log_cat)
{
    if (dfv_zc) {
        fprintf(stderr, "%s: already initialized!\n", __func__);
        return (SPKERR_BADSEQ);
    }

    dfv_zc = zlog_get_category(log_cat?log_cat:"DFV");
    if (!dfv_zc) {
        fprintf(stderr, "%s: failed to initialize log system!\n", __func__);
        return(SPKERR_LOGSYS);
    }

    zlog_notice(dfv_zc, "module initialized.");
    return(SPK_SUCCESS);
}

dfv_vault_t* dfv_vault_open(int repo_num, const char* mnt_tbl[], const char* dev_tbl[], int flag)
{
    assert(repo_num > 0 && repo_num <= DFV_MAX_REPOS);
    dfv_vault_t* vault = malloc(sizeof(dfv_vault_t));
    assert(vault);
    memset(vault, 0, sizeof(dfv_vault_t));

    for (int i=0; i<repo_num; i++) {
        assert(mnt_tbl[i]);
        dfv_repo_t* repo = dfv_repo_open(i, mnt_tbl[i], dev_tbl[i], flag);
        if (!repo) {
            goto cleanup;
        }
        vault->repo_tbl[i] = repo;
    }
    vault->repo_num = repo_num;
    int ret = dfv_vault_sync(vault);
    if (ret > 0) {
        zlog_warn(dfv_zc, "vault created with warning: fix_count=%d", ret);
    }
    return(vault);

cleanup:
    if (vault) {
        for (int i=0; i<repo_num; i++){
            if (vault->repo_tbl[i]) {
                dfv_repo_close(vault->repo_tbl[i]);
                vault->repo_tbl[i] = NULL;
            }
        }
        SAFE_RELEASE(vault);
    }
    return(NULL);
}

void dfv_vault_close(dfv_vault_t* vault)
{
    if (vault) {
        for (int i=0; i<vault->repo_num; i++){
            if (vault->repo_tbl[i]) {
                dfv_repo_close(vault->repo_tbl[i]);
                vault->repo_tbl[i] = NULL;
            }
        }
        SAFE_RELEASE(vault);
    }
}

ssize_t dfv_vault_get_slotsize(dfv_vault_t* vault, int slot_id)
{
    ssize_t file_sz = -1;

    for(int i=0; i<vault->repo_num; i++) {
        ssize_t size = dfv_repo_get_slotsize(vault->repo_tbl[i], slot_id);
        if (size < 0) {
            zlog_warn(dfv_zc, "failed to get slotsize: repo_id=%d, slot_id=%d, size=%zd",
                                i, slot_id, size);
            return(-1);
        }
        if (file_sz < 0)
            file_sz = size;
        else if (file_sz != size) {
            zlog_warn(dfv_zc, "slot size in vault mismatch: slot=%d@%d, "
                                "size=%zd, expect=%zu",
                                slot_id, i, size, file_sz);
            file_sz = MIN(file_sz, size);
        }
    }
    file_sz *= vault->repo_num;

    zlog_info(dfv_zc, "dfv_vault_get_filesize: slot=%d, size=%zd", slot_id, file_sz);
    return(file_sz);
}

int dfv_vault_get_freeslot(dfv_vault_t* vault)
{
    int free_slot = -1;

    for(int i=0; i<vault->repo_num; i++) {
        int ret = dfv_repo_get_freeslot(vault->repo_tbl[i]);
        zlog_info(dfv_zc, "got freeslot: repo_id=%d, slot_id=%d",
                        i, ret);
        if (ret < 0) {
            free_slot = -1;
            break;
        }
        if (free_slot < 0) {
            free_slot = ret;
        } else if (free_slot != ret) {
            zlog_warn(dfv_zc, "free slot in valut not match: repo_id=%d, "
                                "slot_id=%d, expect=%d",
                                i, ret, free_slot);
            free_slot = MAX(free_slot, ret);
        }
    }
    if (free_slot < 0) {
        zlog_error(dfv_zc, "failed to get free slot");
    } else {
        zlog_info(dfv_zc, "got freeslot: slot_id=%d", free_slot);
    }
    return(free_slot);
}

int dfv_vault_format(dfv_vault_t* vault)
{
    int ret = -1;
    for(int i=0; i<vault->repo_num; i++) {
        dfv_repo_t* repo = vault->repo_tbl[i];
        if (strlen(repo->dev_path) > 0 && !access(DFV_SPS_FMTTOOL, X_OK)) {
            ret = dfv_repo_format_sps(repo);
        } else {
            ret = dfv_repo_format_common(repo);
        }
        if (ret < 0) {
            zlog_warn(dfv_zc, "format repo failed: repo=%s",
                        repo->root_path);
            break;
        }
    }

    zlog_notice(dfv_zc, "vault formatted: ret=%d", ret);
    return(ret);
}

int dfv_vault_del_slot(dfv_vault_t* vault, int slot_id)
{
    int ret = -1;
    for(int i=0; i<vault->repo_num; i++) {
        dfv_repo_t* repo = vault->repo_tbl[i];
        ret = dfv_repo_delete(repo, slot_id);
        if (ret < 0) {
            zlog_warn(dfv_zc, "failed to delete slot: repo=%s, slot_id=%d, ret=%d",
                            repo->root_path, slot_id, ret);
            break;
        }
    }

    zlog_notice(dfv_zc, "slot deleted: slot=%d", slot_id);
    return(ret);
}

ssize_t dfv_vault_get_diskfree(dfv_vault_t* vault)
{
    ssize_t df = 0;
    ssize_t ret;
    for(int i=0; i<vault->repo_num; i++) {
        dfv_repo_t* repo = vault->repo_tbl[i];
        ret = dfv_repo_get_diskfree(repo);
        if (ret < 0) {
            zlog_warn(dfv_zc, "failed to get diskfree: repo=%s, ret=%ld",
                repo->root_path, ret);
            df = -1;
            break;
        }
        df += ret;
    }

    zlog_info(dfv_zc, "get vault diskfree: df=%ld", df);
    return(df);
}

ssize_t dfv_vault_get_diskcap(dfv_vault_t* vault)
{
    ssize_t dc = 0;
    ssize_t ret;
    for(int i=0; i<vault->repo_num; i++) {
        dfv_repo_t* repo = vault->repo_tbl[i];
        ret = dfv_repo_get_diskcap(repo);
        if (ret < 0) {
            zlog_warn(dfv_zc, "failed to get diskfree: repo=%s, ret=%ld",
                repo->root_path, ret);
            dc = -1;
            break;
        }
        dc += ret;
    }

    zlog_notice(dfv_zc, "get vault diskcap: dc=%ld", dc);
    return(dc);
}

dfv_repo_t* dfv_vault_get_repo(dfv_vault_t* vault, int repo_id)
{
    assert(repo_id < vault->repo_num);
    return(vault->repo_tbl[repo_id]);
}

int dfv_vault_sync(dfv_vault_t* vault)
{
    int fix_count = 0;
    for(int i=0; i<vault->repo_num; i++) {
        dfv_repo_t* repo = vault->repo_tbl[i];

        fix_count += dfv_repo_sync(repo);
    }

    for(int s=0; s<DFV_MAX_SLOTS; s++) {
        dfv_fmeta_t* fmeta0 = dfv_fmeta_get(vault->repo_tbl[0], s);
        for(int i=1; i<vault->repo_num; i++) {
            dfv_fmeta_t* fmeta = dfv_fmeta_get(vault->repo_tbl[i], s);
            if ((fmeta0 && !fmeta) ||
                (!fmeta0 && fmeta)) {
                dfv_vault_del_slot(vault, s);
                fix_count++;
                zlog_fatal(dfv_zc, "meta in vault not coherent: slot_id=%d", s);
                break;
            }
        }
    }

    zlog_notice(dfv_zc, "sync repos in vault done: fix_count=%d", fix_count);
    return(fix_count);
}