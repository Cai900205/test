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
#include <sys/time.h>

#include "dfv.h"

static void* __dfv_slice_worker(void* arg)
{
    dfv_slice_ctx_t* slice_ctx = (dfv_slice_ctx_t*)arg;
    int slice_num = slice_ctx->fmeta->slice_num;
    int slice_id = slice_ctx->slice_id;
    int cpu_base = slice_ctx->cpu_base;
    assert(slice_ctx->fd > 0);

    if (cpu_base > 0) {
        cpu_base += slice_id;
        spk_worker_set_affinity(cpu_base);
    }
    zlog_info(dfv_zc, "slice> spawned: id=%d, cpu=%d", slice_id, cpu_base);
    
    while (!slice_ctx->quit_req) {
        pthread_mutex_lock(&slice_ctx->lock);
        if(slice_ctx->wptr == slice_ctx->rptr) {
            struct timeval now;
            struct timespec outtime;
            gettimeofday(&now, NULL);
            outtime.tv_sec = now.tv_sec + 1;
            outtime.tv_nsec = now.tv_usec * 1000;
            pthread_cond_timedwait(&slice_ctx->not_empty, &slice_ctx->lock, &outtime);
        }
        if (slice_ctx->wptr == slice_ctx->rptr) {
            pthread_mutex_unlock(&slice_ctx->lock);
            continue;
        }
        assert(slice_ctx->data_chunk.flag == CHUNK_DATA_FLAG__REQ);

        size_t chunk_size = slice_ctx->data_chunk.size;
        size_t slice_sz = chunk_size / slice_num;
        ssize_t access = 0;

        // check chunk_size and slice_sz
        if ((chunk_size % slice_num) ||
            (slice_sz & (0x4000-1))) { // 16k alignment
            zlog_error(dfv_zc, "illegal chunk_sz: chunk_sz=%zu, slice_num=%d",
                        chunk_size, slice_num);
            access = SPKERR_PARAM;
            goto done;
        }

        if (slice_sz != slice_ctx->fmeta->slice_sz) {
            zlog_warn(dfv_zc, "unexpected slice size : slice_sz=%zu, expect=%zu",
                        slice_sz, slice_ctx->fmeta->slice_sz);
            // this chunk may the last in file
            // so just warn it and continue
        }

        if (slice_ctx->dir == SPK_DIR_WRITE) {
            // write
            access = write(slice_ctx->fd,
                           slice_ctx->data_chunk.buf + slice_id * slice_sz,
                           slice_sz);
        } else {
            // read
            access = read(slice_ctx->fd,
                           slice_ctx->data_chunk.buf + slice_id * slice_sz,
                           slice_sz);
        }
        if (access != slice_sz) {
            zlog_error(dfv_zc, "failed to access file: dir=%d, "
                        "slice_sz=%zu, offset=%ld, ret=%lu, errmsg=\'%s\'",
                        slice_ctx->dir, slice_sz,
                        slice_id * slice_sz, access, strerror(errno));
            access = SPKERR_EACCESS;
            goto done;
        }

done:
        if (access == slice_sz) {
            slice_ctx->data_chunk.flag = CHUNK_DATA_FLAG__DONE;
        } else {
            slice_ctx->data_chunk.flag = access;
        }
        slice_ctx->rptr++;
        pthread_cond_signal(&slice_ctx->not_full);
        pthread_mutex_unlock(&slice_ctx->lock);
    }
    zlog_info(dfv_zc, "slice> terminated: id=%d", slice_id);
    return(NULL);
}

dfv_file_t* dfv_file_open(dfv_repo_t* repo, int slot_id, SPK_DIR dir, dfv_slice_def_t* slice_def, int cpu_base)
{
    char pathname[SPK_MAX_PATHNAME];
    int i;
    int fd;
    dfv_rmeta_t* rmeta = &repo->rmeta;
    dfv_fmeta_t* fmeta = NULL;
    
    assert(rmeta);
    assert(dir == SPK_DIR_READ || dir == SPK_DIR_WRITE);
    assert(slot_id >= 0 && slot_id < DFV_MAX_SLOTS);
    if (dir == SPK_DIR_WRITE) {
        assert(slice_def->num > 0 && slice_def->num <= DFV_MAX_SLICES);
    } else {
        assert(slice_def == NULL);
    }
    
    // check file exists
#if 0
    sprintf(pathname, "%s/%d", repo->root_path, slot_id);
#else    
    char mnt_path[SPK_MAX_PATHNAME];
#endif
    fmeta = rmeta->fmeta_tbl[slot_id];
    if (dir == SPK_DIR_READ) {
        if (!fmeta) {
            zlog_error(dfv_zc, "file not found: slot_id=%d", slot_id);
            return(NULL);
        }
        /*
        struct stat info;
        strcpy(mnt_path, repo->mnt_path);
        for (i=0; i<slice_def->num; i++) {
            sprintf(pathname, "/%s/%s/%s/%d", DFV_PREFIX_PATH, mnt_path, DFV_POSTFIX_PATH, slot_id);
            if (stat(pathname, &info)) {
                zlog_error(dfv_zc, "file entry not found: entry=%s", pathname);
                return(NULL);
            }
            mnt_path[2]++;
        }*/
    } else {
        // write
        if (fmeta) {
            zlog_error(dfv_zc, "file already exists: slot_id=%d", slot_id);
            return(NULL);
        }
#if 1
        strcpy(mnt_path, repo->mnt_path);
        for (i=0; i<slice_def->num; i++) {
            sprintf(pathname, "/%s/%s/%s/%d", DFV_PREFIX_PATH, mnt_path, DFV_POSTFIX_PATH, slot_id);
//            sprintf(pathname, "%s/"DFV_SLICE_FILEPATH"/%d", repo->root_path, i+1, slot_id);
            if (mkdir(pathname, 0666)) {
                zlog_error(dfv_zc, "failed to create file entry: entry=%s, errmsg=%s",
                        pathname,
                        strerror(errno));
                return(NULL);
            }
            mnt_path[2]++;
        }
#else
        if (mkdir(pathname, 0666)) {
            zlog_error(dfv_zc, "failed to create file entry: entry=%s, errmsg=%s",
                        pathname,
                        strerror(errno));
            return(NULL);
        }
#endif
        fmeta = malloc(sizeof(dfv_fmeta_t));
        memset(fmeta, 0, sizeof(dfv_fmeta_t));
        fmeta->slot_id = slot_id;
        fmeta->slice_num = slice_def->num;
        fmeta->slice_sz = slice_def->size;
        fmeta->slot_sz = 0;
        fmeta->file_time = dfv_rmeta_get_now();
        fmeta->open_cnt = 0;
        pthread_mutex_init(&fmeta->open_cnt_lock, NULL);

        rmeta->fmeta_tbl[slot_id] = fmeta;
        dfv_rmeta_save(rmeta, repo->meta_path);
    }

    assert(fmeta);
    fmeta->file_pos = 0;

    // check open count
    pthread_mutex_lock(&fmeta->open_cnt_lock);
    if (fmeta->open_cnt > 0) {
        zlog_error(dfv_zc, "file busy: slot_id=%d, open_cnt=%d",
                            slot_id, fmeta->open_cnt);
        pthread_mutex_unlock(&fmeta->open_cnt_lock);
        return(NULL);
    }
    fmeta->open_cnt++;
    pthread_mutex_unlock(&fmeta->open_cnt_lock);

    // create ctx
    dfv_file_t* file_ctx = NULL;
    file_ctx = malloc(sizeof(dfv_file_t));
    assert(file_ctx);
    memset(file_ctx, 0, sizeof(dfv_file_t));
    file_ctx->repo = repo;
    file_ctx->slot_id = slot_id;
    file_ctx->fmeta = fmeta;
    file_ctx->dir = dir;
    
    // spawn workers
    strcpy(mnt_path, repo->mnt_path);
    for (i=0; i<fmeta->slice_num; i++) {
        dfv_slice_ctx_t* slice_ctx = malloc(sizeof(dfv_slice_ctx_t));
        assert(slice_ctx);
        memset(slice_ctx, 0, sizeof(dfv_slice_ctx_t));
        slice_ctx->slot_id = slot_id;
        slice_ctx->slice_id = i;
        slice_ctx->dir = dir;
        slice_ctx->fmeta = fmeta;
        pthread_mutex_init(&slice_ctx->lock, NULL);
        pthread_cond_init(&slice_ctx->not_full, NULL);
        pthread_cond_init(&slice_ctx->not_empty, NULL);
        slice_ctx->wkr_thread = malloc(sizeof(pthread_t));

#if 1
        sprintf(pathname, "/%s/%s/%s/%d/"DFV_SLICE_FILENAME, DFV_PREFIX_PATH, mnt_path, DFV_POSTFIX_PATH, slot_id, i+1);
#else
        sprintf(pathname, "%s/%d/"DFV_SLICE_FILENAME, repo->root_path,
                             slot_id, i+1);
#endif
        fd = open(pathname, O_CREAT | O_RDWR | O_DIRECT);
        if (fd < 0) {
            zlog_error(dfv_zc, "failed to open file: file=%s, errmsg=%s",
                        pathname,
                        strerror(errno));
            assert(0);
        }
        slice_ctx->fd = fd;
        file_ctx->slice_tbl[i] = slice_ctx;
        slice_ctx->cpu_base = cpu_base;
        pthread_create(slice_ctx->wkr_thread, NULL,
                         __dfv_slice_worker, slice_ctx);
        mnt_path[2]++;
    }
    
    if (file_ctx) {
        spk_stats_reset(&file_ctx->xfer_stats);
    }

    zlog_notice(dfv_zc, "file opened: slot_id=%d, dir=%d, "
                        "slice=0x%lxx%d",
                         slot_id, dir,
                         fmeta->slice_sz, fmeta->slice_num);

    return(file_ctx);
}

int dfv_file_close(dfv_file_t* file_ctx)
{
    assert(file_ctx);

    int i;
    dfv_fmeta_t* fmeta = file_ctx->fmeta;
    dfv_slice_ctx_t* slice_ctx = NULL;

    for (i=0; i<fmeta->slice_num; i++) {
        // set quit_req flag
        slice_ctx = file_ctx->slice_tbl[i];
        slice_ctx->quit_req = 1;
        pthread_cond_signal(&slice_ctx->not_empty);
    }

    for (i=0; i<fmeta->slice_num; i++) {
        // wait thread quit
        slice_ctx = file_ctx->slice_tbl[i];
        pthread_join(*slice_ctx->wkr_thread, NULL);
        
        close(slice_ctx->fd);
        SAFE_RELEASE(slice_ctx->wkr_thread);
        SAFE_RELEASE(slice_ctx);
        file_ctx->slice_tbl[i] = NULL;
    }  

    // update file_sz in meta
    int remove_slot = 0;
    if (file_ctx->dir == SPK_DIR_WRITE) {
        if (file_ctx->fmeta->file_pos > 0) {
            ssize_t slot_sz = dfv_repo_get_slotsize(file_ctx->repo, file_ctx->slot_id);
            if (slot_sz < 0) {
                zlog_warn(dfv_zc, "can not get file size: slot_id=%d, ret=%ld",
                                    file_ctx->slot_id, slot_sz);
            } else {
                fmeta->slot_sz = slot_sz;
                dfv_rmeta_save(&file_ctx->repo->rmeta, file_ctx->repo->meta_path);
            }
        } else {
            // nothing written
            // remove this file
            zlog_warn(dfv_zc, "file closed but nothing written: slot_id=%d",
                                file_ctx->slot_id);
            remove_slot = 1;
        }
    }
    
    // dec open_cnt
    pthread_mutex_lock(&fmeta->open_cnt_lock);
    fmeta->open_cnt--;
    pthread_mutex_unlock(&fmeta->open_cnt_lock);

    if (remove_slot) {
        dfv_repo_delete(file_ctx->repo, file_ctx->slot_id);
    }

    SAFE_RELEASE(file_ctx);

    return(SPK_SUCCESS);
}

int dfv_file_seek(dfv_file_t* file_ctx, uint64_t offset)
{
    dfv_fmeta_t* fmeta = file_ctx->fmeta;
    assert(fmeta);
    int slice_num = fmeta->slice_num;
    
//    assert(file_ctx->dir == SPK_DIR_READ);

    // offset must 16k*slice_num alignment
    assert(!(offset % (slice_num*(0x4000))));

    for (int i=0; i<slice_num; i++) {
        dfv_slice_ctx_t* slice_ctx = file_ctx->slice_tbl[i];
        pthread_mutex_lock(&slice_ctx->lock);
        lseek(slice_ctx->fd, offset / slice_num, SEEK_SET);
        pthread_mutex_unlock(&slice_ctx->lock);
    }
    
    file_ctx->fmeta->file_pos = offset;
    zlog_notice(dfv_zc, "file seek: offset=%lu", offset);

    return(SPK_SUCCESS);
}

ssize_t dfv_file_read(dfv_file_t* file_ctx, void* buf, size_t size)
{
    assert(file_ctx);

    dfv_slice_ctx_t* slice_ctx = NULL;
    ssize_t ret_sz = 0;
    int i;
    
    zlog_info(dfv_zc, "file read: ctx=%p, buf=%zu@%p", file_ctx, size, buf);

    dfv_fmeta_t* fmeta = file_ctx->fmeta;
    for (i=0; i<fmeta->slice_num; i++) {
        slice_ctx = file_ctx->slice_tbl[i];
        // wait for slice worker to be idle
        pthread_mutex_lock(&slice_ctx->lock);
        if(slice_ctx->wptr != slice_ctx->rptr) {
            pthread_cond_wait(&slice_ctx->not_full, &slice_ctx->lock);
        }
        assert(slice_ctx->data_chunk.flag == CHUNK_DATA_FLAG__DONE);
        // push chunk
        slice_ctx->data_chunk.buf = buf;
        slice_ctx->data_chunk.size = size;
        slice_ctx->data_chunk.flag = CHUNK_DATA_FLAG__REQ;
        // update wptr
        slice_ctx->wptr++;
        // trig slice worker
        pthread_cond_signal(&slice_ctx->not_empty);
        pthread_mutex_unlock(&slice_ctx->lock);
    }

    ret_sz = size;
    for (i=0; i<fmeta->slice_num; i++) {
        slice_ctx = file_ctx->slice_tbl[i];
        // wait for slice worker to be idle
        pthread_mutex_lock(&slice_ctx->lock);
        if(slice_ctx->wptr != slice_ctx->rptr) {
            pthread_cond_wait(&slice_ctx->not_full, &slice_ctx->lock);
        }
        if (slice_ctx->data_chunk.flag != CHUNK_DATA_FLAG__DONE) {
            // error
            slice_ctx->data_chunk.flag = CHUNK_DATA_FLAG__DONE;
            ret_sz = 0;
        }
        pthread_mutex_unlock(&slice_ctx->lock);
    }
    
    if (ret_sz > 0) {
        file_ctx->fmeta->file_pos += ret_sz;
        spk_stats_inc_xfer(&file_ctx->xfer_stats, ret_sz, 1);
    }
    return(ret_sz);
}

ssize_t dfv_file_write(dfv_file_t* file_ctx, void* buf, size_t size)
{
    assert(file_ctx);
    dfv_slice_ctx_t* slice_ctx;
    ssize_t ret_sz = 0;
    int i;

    dfv_fmeta_t* fmeta = file_ctx->fmeta;

    for (i=0; i<fmeta->slice_num; i++) {
        slice_ctx = file_ctx->slice_tbl[i];
        // wait for slice worker to be idle
        pthread_mutex_lock(&slice_ctx->lock);
        if(slice_ctx->wptr != slice_ctx->rptr) {
            pthread_cond_wait(&slice_ctx->not_full, &slice_ctx->lock);
        }
        assert(slice_ctx->data_chunk.flag == CHUNK_DATA_FLAG__DONE);
        // push chunk
        slice_ctx->data_chunk.buf = buf;
        slice_ctx->data_chunk.size = size;
        slice_ctx->data_chunk.flag = CHUNK_DATA_FLAG__REQ;
        // update wptr
        slice_ctx->wptr++;
        // trig slice worker
        pthread_cond_signal(&slice_ctx->not_empty);
        pthread_mutex_unlock(&slice_ctx->lock);
    }

    ret_sz = size;
    for (i=0; i<fmeta->slice_num; i++) {
        slice_ctx = file_ctx->slice_tbl[i];
        // wait for slice worker to be idle
        pthread_mutex_lock(&slice_ctx->lock);
        if(slice_ctx->wptr != slice_ctx->rptr) {
            pthread_cond_wait(&slice_ctx->not_full, &slice_ctx->lock);
        }
        if (slice_ctx->data_chunk.flag < 0) {
            // error
            ret_sz = slice_ctx->data_chunk.flag;
        }
        slice_ctx->data_chunk.flag = CHUNK_DATA_FLAG__DONE;
        pthread_mutex_unlock(&slice_ctx->lock);
    }
    
    if (ret_sz > 0) {
        file_ctx->fmeta->file_pos += ret_sz;
        spk_stats_inc_xfer(&file_ctx->xfer_stats, ret_sz, 1);
    }

    return(ret_sz);
}

spk_stats_t* dfv_file_get_stats(dfv_file_t* ctx)
{
    return(&ctx->xfer_stats);
}
