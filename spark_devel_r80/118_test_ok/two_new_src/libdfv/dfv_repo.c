#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <fcntl.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>

#include "dfv.h"

char* dfv_repo_get_rootpath(dfv_repo_t* repo)
{
    return(repo->root_path);
}

int dfv_repo_get_id(dfv_repo_t* repo)
{
    return(repo->repo_id);
}

dfv_repo_t* dfv_repo_open(int repo_id, const char* mnt_path, const char* dev_path, int flag)
{
    struct stat info;
    int ret;
    char root_path[SPK_MAX_PATHNAME];
    
    sprintf(root_path, "/%s/%s/%s", DFV_PREFIX_PATH, mnt_path, DFV_POSTFIX_PATH);
    ret = stat(root_path, &info);
    if (ret) {
        // root path not exist
        zlog_fatal(dfv_zc, "root_path not found: path=%s", root_path);
        return NULL;
    } 
   
    dfv_repo_t* repo = malloc(sizeof(dfv_repo_t));
    assert(repo);
    memset(repo, 0, sizeof(dfv_repo_t));

    repo->repo_id = repo_id;
    strcpy(repo->root_path, root_path);
    strcpy(repo->mnt_path, mnt_path);
    if (dev_path) {
        strcpy(repo->dev_path, dev_path);
    }
    sprintf(repo->meta_path, "%s/.meta", root_path);

    ret = dfv_rmeta_load(&repo->rmeta, repo->meta_path);
    if (ret < 0) {
        zlog_warn(dfv_zc, "failed to load rmeta: path=%s, ret=%d",
                     repo->meta_path, ret);
        dfv_rmeta_reset(&repo->rmeta);
        dfv_rmeta_save(&repo->rmeta, repo->meta_path);
    }

    zlog_notice(dfv_zc, "repo opened: id=%d, path=%s",
                         repo_id, repo->root_path);
    dfv_rmeta_dump(ZLOG_LEVEL_NOTICE, &repo->rmeta);

    return(repo);
}

void dfv_repo_close(dfv_repo_t* repo)
{
    dfv_rmeta_save(&repo->rmeta, repo->meta_path);
    for (int i=0; i<DFV_MAX_SLOTS; i++) {
        dfv_fmeta_t* fmeta = repo->rmeta.fmeta_tbl[i];
        if (fmeta) {
            SAFE_RELEASE(fmeta);
        }
    }
    
    SAFE_RELEASE(repo);
    return;
}

int dfv_repo_format_common(dfv_repo_t* repo)
{
    char cmd[SPK_MAX_PATHNAME];
#if 0
    int ret;
    
    assert(repo);

    sprintf(cmd, "rm %s -Rf", repo->root_path);
    ret = spk_os_exec(cmd);
    
    ret = mkdir(repo->root_path, 0666);
    if (ret) {
        zlog_error(dfv_zc, "failed to create repo dir: path=%s, errmsg=\'%s\'\n",
                        repo->root_path, strerror(errno));
        return(SPKERR_BADRES);
    }

#else
    int i;

    char mnt_path[SPK_MAX_PATHNAME];
    strcpy(mnt_path, repo->mnt_path);
    for (i=0; i<repo->dev_num; i++) {
        sprintf(cmd, "rm /%s/%s/%s/* -Rf", DFV_PREFIX_PATH, mnt_path, DFV_POSTFIX_PATH);
      //  sprintf(cmd, "rm %s/"DFV_SLICE_FILEPATH"/*  -Rf", repo->root_path, i+1);
        mnt_path[2]++;
        spk_os_exec(cmd);
    }
#endif
    dfv_rmeta_reset(&repo->rmeta);
    dfv_rmeta_save(&repo->rmeta, repo->meta_path);

    zlog_notice(dfv_zc, "repo formatted (common): id=%d, root=%s",
                         repo->repo_id, repo->root_path);
    
    return(SPK_SUCCESS);
}

int dfv_repo_format_sps(dfv_repo_t* repo)
{
    char cmd[SPK_MAX_PATHNAME];
    int ret;
    
#if 1
    int i;
    char devname[SPK_MAX_PATHNAME];
    char pathname[SPK_MAX_PATHNAME];
    char mnt_path[SPK_MAX_PATHNAME];
    int devlen = 0;    
    strcpy(devname, repo->dev_path);
    devlen = strlen(repo->dev_path);
    strcpy(mnt_path, repo->mnt_path);
    assert(devlen > 0);
    for (i=0; i<repo->dev_num; i++) {
        sprintf(cmd, "/dev/%s", devname);
        ret = access(cmd, W_OK);
        if (!ret) {
            sprintf(cmd, "umount /dev/%s",  devname);
            spk_os_exec(cmd);
            sprintf(cmd, "mkfs.ext4 /dev/%s",  devname);
            spk_os_exec(cmd);
            spk_os_exec(pathname);
            sprintf(cmd, "mount /dev/%s /%s/%s", mnt_path, DFV_PREFIX_PATH, mnt_path);
            mnt_path[2]++;
            spk_os_exec(cmd); 
        }
        devname[devlen-1]++;
    }
    dfv_rmeta_reset(&repo->rmeta);
    dfv_rmeta_save(&repo->rmeta, repo->meta_path);
#else
    assert(strlen(repo->dev_path) > 0);

    ret = access(DFV_SPS_FMTTOOL, X_OK);
    if (!ret) {
        sprintf(cmd, "/dev/%s", repo->dev_path);
        ret = access(cmd, W_OK);
        if (!ret) {
            sprintf(cmd, "./%s -fy /dev/%s", DFV_SPS_FMTTOOL, repo->dev_path);
            spk_os_exec(cmd);
            sprintf(cmd, "umount /dev/%s",  repo->mnt_path);
            spk_os_exec(cmd);
            sprintf(cmd, "mkfs.ext4 /dev/%s",  repo->mnt_path);
            spk_os_exec(cmd);
            sprintf(cmd, "mount /dev/%s /%s/%s", repo->mnt_path, DFV_PREFIX_PATH, repo->mnt_path);
            spk_os_exec(cmd);
            ret = mkdir(repo->root_path, 0666);
            if (ret) {
                zlog_error(dfv_zc, "failed to create repo: path=%s, errmsg=\'%s\'\n",
                                repo->root_path, strerror(errno));
                return(SPKERR_BADRES);
            }
            dfv_rmeta_reset(&repo->rmeta);
            dfv_rmeta_save(&repo->rmeta, repo->meta_path);
        }
    }

#endif
    zlog_notice(dfv_zc, "repo formatted (SPS): id=%d root=%s",
                         repo->repo_id, repo->root_path);
    
    return(SPK_SUCCESS);
}

int dfv_repo_get_freeslot(dfv_repo_t* repo)
{
    assert(repo);

    int max_slot = -1;
    for (int i=0; i<DFV_MAX_SLOTS; i++) {
        if (repo->rmeta.fmeta_tbl[i]) {
            // free
            max_slot = i;
        }
    }
    max_slot += 1;
    if (max_slot >= DFV_MAX_SLOTS)
        max_slot = -1;

    return(max_slot);
}

ssize_t dfv_repo_get_slotsize(dfv_repo_t* repo, int slot_id)
{
    assert(repo);
    assert(slot_id >= 0 && slot_id < DFV_MAX_SLOTS);

    size_t file_sz = -1;
    dfv_fmeta_t* fmeta = dfv_fmeta_get(repo, slot_id);
    
    if (!fmeta) {
        // file not found
        zlog_warn(dfv_zc, "file does not exist: id=%d, slot=%d", repo->repo_id, slot_id);
        goto error_out;
    }

    int slice_num = fmeta->slice_num;
    char mnt_path[SPK_MAX_PATHNAME];
    strcpy(mnt_path, repo->mnt_path);
    
    for (int i=0; i<slice_num; i++) {
        struct stat statbuff;
        char pathname[SPK_MAX_PATHNAME];
#if 1
        sprintf(pathname, "/%s/%s/%s/%d/"DFV_SLICE_FILENAME, DFV_PREFIX_PATH, mnt_path, DFV_POSTFIX_PATH, slot_id, i+1);
#else
        sprintf(pathname, "%s/%d/"DFV_SLICE_FILENAME, repo->root_path,
                             fmeta->slot_id, i+1);
#endif
        mnt_path[2]++;
        if(stat(pathname, &statbuff) < 0){
            // bad file
            zlog_warn(dfv_zc, "stat failed: id=%d, slot=%d", repo->repo_id, slot_id);
            file_sz = SPKERR_BADRES;
            goto error_out;
        }
        if (file_sz < 0) {
            file_sz = statbuff.st_size;
        } else {
            file_sz += MIN(file_sz, statbuff.st_size);
        }
    }
   /* if (file_sz > 0) {
        file_sz *= slice_num;
    }*/

error_out:
    zlog_info(dfv_zc, "dfv_file_getsize: slot=%d, size=%zd", slot_id, file_sz);
    return(file_sz);
}

struct dfv_fmeta* dfv_repo_get_fmeta(struct dfv_repo* repo, int slot_id)
{
    return(repo->rmeta.fmeta_tbl[slot_id]);
}

int dfv_repo_delete(struct dfv_repo* repo, int slot_id)
{
    dfv_fmeta_t* fmeta = dfv_fmeta_get(repo, slot_id);
    dfv_rmeta_t* rmeta = dfv_rmeta_get(repo);
    if (!fmeta) {
        return(SPKERR_EACCESS);
    }
    
    pthread_mutex_lock(&fmeta->open_cnt_lock);
    if (fmeta->open_cnt > 0) {
        zlog_error(dfv_zc, "file busy: id=%d, slot=%d, open_cnt=%d",
                            repo->repo_id, slot_id, fmeta->open_cnt);
        pthread_mutex_unlock(&fmeta->open_cnt_lock);
        return(SPKERR_EAGAIN);
    }
    char pathname[SPK_MAX_PATHNAME];
#if 1
    int i;

    char mnt_path[SPK_MAX_PATHNAME];
    strcpy(mnt_path, repo->mnt_path);
    for (i=0; i<fmeta->slice_num; i++) {
        sprintf(pathname, " rm /%s/%s/%s/%d -Rf", DFV_PREFIX_PATH, mnt_path, DFV_POSTFIX_PATH, slot_id);
        spk_os_exec(pathname);
        mnt_path[2]++;
    }
#else
    sprintf(pathname, "rm %s/%d -Rf", repo->root_path, slot_id);
    spk_os_exec(pathname);
    
#endif
    rmeta->fmeta_tbl[slot_id] = NULL;

    pthread_mutex_unlock(&fmeta->open_cnt_lock);
    SAFE_RELEASE(fmeta);
    
    zlog_notice(dfv_zc, "file deleted: id=%d, slot=%d", repo->repo_id, slot_id);
    return(SPK_SUCCESS);
}

ssize_t dfv_repo_get_diskfree(struct dfv_repo* repo)
{
    ssize_t df = -1;
    
    assert(repo);

    struct statfs sfs;
#if 1
    int i;
    char pathname[SPK_MAX_PATHNAME];
    df =0;
    char mnt_path[SPK_MAX_PATHNAME];
    strcpy(mnt_path, repo->mnt_path);
    for(i=0; i<repo->dev_num; i++) {
        sprintf(pathname, "/%s/%s/%s", DFV_PREFIX_PATH, mnt_path, DFV_POSTFIX_PATH);
        //sprintf(pathname, "%s/"DFV_SLICE_FILEPATH, repo->root_path, i+1);
        if(!statfs(pathname, &sfs)) {
            df += sfs.f_bavail*sfs.f_bsize;
        }
        mnt_path[2]++;
    }

#else
    if(!statfs(repo->root_path, &sfs)) {
        df = sfs.f_bavail*sfs.f_bsize;
    }

#endif
    zlog_info(dfv_zc, "get repo diskfree: path=%s, df=%ld",
                 repo->root_path, df);
    return (df);
}

ssize_t dfv_repo_get_diskcap(struct dfv_repo* repo)
{
    ssize_t dc = -1;
    
    assert(repo);

    struct statfs sfs;
#if 1
    int i;
    char pathname[SPK_MAX_PATHNAME];
    dc =0;
    char mnt_path[SPK_MAX_PATHNAME];
    strcpy(mnt_path, repo->mnt_path);
    for(i=0; i<repo->dev_num; i++) {
        sprintf(pathname, "/%s/%s/%s", DFV_PREFIX_PATH, mnt_path, DFV_POSTFIX_PATH);
        //sprintf(pathname, "%s/"DFV_SLICE_FILEPATH, repo->root_path, i+1);
        if(!statfs(pathname, &sfs)) {
            dc += sfs.f_blocks*sfs.f_bsize;
        }
        zlog_notice(dfv_zc, "diskcap: id=%d, pathname=%s, dc=%ld",
                 repo->repo_id, pathname, dc);
        mnt_path[2]++;
    }
#else
    if(!statfs(repo->root_path, &sfs)) {
        dc = sfs.f_blocks*sfs.f_bsize;
    }

#endif
    zlog_notice(dfv_zc, "diskcap: id=%d, root=%s, dc=%ld",
                 repo->repo_id, repo->root_path, dc);
    return (dc);
}

int dfv_repo_sync(dfv_repo_t* repo)
{
    int fix_count = 0;

    for (int i=0; i<DFV_MAX_SLOTS; i++) {
        dfv_fmeta_t* fmeta = dfv_fmeta_get(repo, i);
        if (!fmeta) {
            struct stat statbuff;
            char pathname[SPK_MAX_PATHNAME];
            sprintf(pathname, "%s/%d/", repo->root_path, i);
            if(stat(pathname, &statbuff) >= 0){
                // file exists but meta not found
                zlog_warn(dfv_zc, "sync: slot exists while meta is NULL:"
                                    "root=%s, slot=%d",
                                     repo->root_path, i);
                char pathname[SPK_MAX_PATHNAME];
                sprintf(pathname, "rm %s/%d -Rf", repo->root_path, i);
                spk_os_exec(pathname);
                fix_count++;
            }
        } else {
            ssize_t slot_sz = dfv_repo_get_slotsize(repo, i);
            if (slot_sz < 0) {
                // meta exists while slot is empty
                // delete fmeta
                zlog_warn(dfv_zc, "sync: meta exists while slot is empty:"
                                    "root=%s, slot=%d",
                                     repo->root_path, i);
                repo->rmeta.fmeta_tbl[i] = NULL;
                SAFE_RELEASE(fmeta);
                fix_count++;
            } else {
                // size mismatch
                if (fmeta->slot_sz != slot_sz) {
                    zlog_warn(dfv_zc, "sync: slot file mismatch:"
                                        "root=%s, slot=%d",
                                         repo->root_path, i);
                    fmeta->slot_sz = slot_sz;
                    fix_count++;
                }
            }
        }
    }
 
    dfv_rmeta_save(&repo->rmeta, repo->meta_path);
    zlog_notice(dfv_zc, "repo fixed: id=%d, root=%s, fix_count=%d",
                 repo->repo_id, repo->root_path, fix_count);

    return(fix_count);   
}
