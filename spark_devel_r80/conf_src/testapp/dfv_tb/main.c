#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <time.h>

#include <zlog/zlog.h>
#include "spark.h"
#include "fica_opt.h"
#include "dfv/dfv_intf.h"

#define DFV_SLICE_NUM   (4)
#define DFV_SLICE_SIZE  (4*1024*1024)
#define DFV_CHUNK_SIZE  (DFV_SLICE_NUM*DFV_SLICE_SIZE)
#define DEFAULT_FILE_SIZE    (32*1024*1024)

char* repo_mnt_default[] =
{
    "dfa",
    "dfb",
    NULL
};

char* repo_dev_default[] =
{
    "scta",
    "sctb",
    NULL
};

char* repo_mnt_tbl[DFV_MAX_REPOS] = {NULL};
char* repo_dev_tbl[DFV_MAX_REPOS] = {NULL};

struct dfv_vault* vault = NULL;
size_t file_size = DEFAULT_FILE_SIZE;
int slot_id = -1;

void print_usage()
{
    printf("Usage:\n");
    printf("  ./dfvtool CMD [OPTION]\n");
    printf("    CMD - flist\n");
    printf("\n");
}

int main(int argc, char* argv[])
{
    int ret;
    char* file_buffer = NULL;
    char* cmd;
    int i;
    
    if (argc < 2) {
        print_usage();
        exit(1);
    }
        
    zlog_init("./zlog.conf");
    ret = dfv_module_init(NULL);
    if (ret) {
        printf("dfv_module_init: ret=%d\n", ret);
        exit(-1);
    }
    
    cmd = argv[1];
    
    // parse args
    int parse_argc = 0;
    int repo_mnt_num = 0;
    int repo_dev_num = 0;    
    while (parse_argc+2 < argc) {
        char* parse_pos = argv[parse_argc+2];
        do {
            char* key; char* value;
            parse_pos = fica_shift_option(parse_pos, &key, &value);
            if (key) {
                if (!strcasecmp(key, "--repo-mnt")) {
                    assert(repo_mnt_num >= DFV_MAX_REPOS);
                    repo_mnt_tbl[repo_mnt_num++] = value;
                } else if (!strcasecmp(key, "--repo-dev")) {
                    assert(repo_dev_num >= DFV_MAX_REPOS);
                    repo_dev_tbl[repo_dev_num++] = value;
                } else if (!strcasecmp(key, "--file-sizek")) {
                    size_t file_sz_in_kb = strtoul(value, NULL, 0);
                    file_size = file_sz_in_kb * 1024;
                } else {
                    printf("Unknown option: key=%s\n", key);
                    exit(1);
                }
            }
        } while(parse_pos);
        parse_argc++;
    }
    
    if (repo_mnt_num != repo_dev_num) {
        printf("Invalid repo info\n");
        exit(1);
    }

    if (repo_mnt_num == 0) {
        while(repo_mnt_default[repo_mnt_num]) {
            repo_mnt_tbl[repo_mnt_num] = repo_mnt_default[repo_mnt_num];
            repo_dev_tbl[repo_mnt_num] = repo_dev_default[repo_mnt_num];
            repo_mnt_num++;
        }
    }

    file_buffer = memalign(4*1024, DFV_CHUNK_SIZE);
    assert(file_buffer);

    vault = dfv_vault_create(repo_mnt_num, repo_mnt_tbl, repo_dev_tbl, 0);
    if (!vault) {
        printf("Failed to create vault\n");
        exit(1);
    }

    for (i=0; i<vault->repo_num; i++) {
        printf("== repo#%d : %s\n", i, dfv_repo_get_rootpath(vault->repo_tbl[i]));
    }
    printf("== cmd : %s\n", cmd);

    if (!strcasecmp(cmd, "flist")) {
        for (int r=0; r<vault->repo_num; r++) {
            int count = 0;            
            struct dfv_repo* repo = vault->repo_tbl[r];
            dfv_rmeta_t* rmeta = dfv_rmeta_get(repo);
            printf("== flist repo#%d : %s\n", r, dfv_repo_get_rootpath(repo));
            printf("  - meta_version : %d\n", rmeta->version);
            for (i=0; i<DFV_MAX_SLOTS; i++) {
                dfv_fmeta_t* fmeta = dfv_fmeta_get(repo, i);
                if (fmeta) {
                    char    outstr[1024];
                    struct tm* local_time = localtime((time_t*)&fmeta->file_time);
                    char tmstr[256];
                    ssize_t slot_sz = dfv_repo_get_slotsize(repo, i);
                    strftime(tmstr, sizeof(tmstr), "%Y/%m/%d-%H:%M:%S", local_time);  
                    sprintf(outstr, "SLOT#%02d : slice=0x%lxx%d, file_sz=%ld, time=\'%s\'",
                                fmeta->slot_id,
                                fmeta->slice_sz,
                                fmeta->slice_num,
                                slot_sz,
                                tmstr);
                    printf("  - %s\n", outstr);
                    count++;
                }
            }
            printf("== file list done: f_count=%d\n", count);
        }
    } else if (!strcasecmp(cmd, "format")){
        // format
        ret = dfv_vault_format(vault);
        printf("== format done: ret=%d\n", ret);
    } else if (!strcasecmp(cmd, "df")){
        // diskfree
        ssize_t df = dfv_vault_get_diskfree(vault);
        printf("== diskfree: ret=%ld\n", df);
    } else if (!strcasecmp(cmd, "dc")){
        // diskfree
        ssize_t df = dfv_vault_get_diskcap(vault);
        printf("== diskcap: ret=%ld\n", df);
    } else if (!strcasecmp(cmd, "fcreate")){
        // fcreate
        printf("== slot : %d\n", slot_id);
        printf("== file_size : %zu\n", file_size);
        if (slot_id < 0) {
            slot_id = dfv_vault_get_freeslot(vault);
        }
        for (i=0; i<vault->repo_num; i++) {
            size_t repo_file_size = file_size / vault->repo_num;

            struct dfv_file* file_ctx = NULL;
            dfv_slice_def_t slice_def;
            slice_def.num = DFV_SLICE_NUM;
            slice_def.size = DFV_SLICE_SIZE;
            file_ctx = dfv_file_open(vault->repo_tbl[i], slot_id, SPK_DIR_WRITE, &slice_def, 0);
            assert(file_ctx);
    
            size_t xferred = 0;
            uint64_t data = 0;
            while(xferred < repo_file_size) {
                size_t xfer = MIN(DFV_CHUNK_SIZE, repo_file_size-xferred);
                for (int i=0; i<xfer/8; i++) {
                    *((uint64_t*)file_buffer+i) = data++;
                }
                ssize_t written = dfv_file_write(file_ctx, file_buffer, xfer);
                if (written < 0) {
                    printf("written = %ld\n", written);
                    goto out;
                }
                xferred += xfer;
            }
    
            ret = dfv_file_close(file_ctx);
            assert(!ret);
        }
        printf("== file created: slot=%d, ret=%d\n", slot_id, ret);
    } else {
    }
out:
    dfv_vault_fini(vault);
    SAFE_RELEASE(file_buffer);

    zlog_fini();
    return (0);
}
