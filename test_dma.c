#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <usdpaa/compat.h>
#include <usdpaa/of.h>
#include <usdpaa/dma_mem.h>
#include <usdpaa/fsl_dma.h>
#include <usdpaa/fsl_srio.h>
#include <error.h>
#include <atb_clock.h>
#include <readline.h>
#include <assert.h>
#include <inttypes.h>
#include <unistd.h>


typedef struct fvl_dma_pool {
    dma_addr_t dma_phys_base;
    void      *dma_virt_base;
} fvl_dma_pool_t;

int dma_usmem_init(fvl_dma_pool_t *pool,uint64_t pool_size,uint64_t dma_size)
{
	int err;

	dma_mem_generic = dma_mem_create(DMA_MAP_FLAG_ALLOC,
					NULL, pool_size);
	if (!dma_mem_generic) {
		err = -EINVAL;
		error(0, -err, "%s(): dma_mem_create()", __func__);
		return err;
	}

	pool->dma_virt_base = __dma_mem_memalign(4096, dma_size);
	if (!pool->dma_virt_base) {
		err = -EINVAL;
		error(0, -err, "%s(): __dma_mem_memalign()", __func__);
		return err;
	}
	pool->dma_phys_base = __dma_mem_vtop(pool->dma_virt_base);

	return 0;
}

int dma_pool_init(fvl_dma_pool_t **pool,uint64_t pool_size,uint64_t dma_size)
{
	fvl_dma_pool_t *dma_pool;
	int err;

	dma_pool = malloc(sizeof(*dma_pool));
	if (!dma_pool) {
		error(0, errno, "%s(): DMA pool", __func__);
		return -errno;
	}
	memset(dma_pool, 0, sizeof(*dma_pool));
	*pool = dma_pool;

	err = dma_usmem_init(dma_pool,pool_size,dma_size);
	if (err < 0) {
		error(0, -err, "%s(): DMA pool", __func__);
		free(dma_pool);
		return err;
	}

	return 0;
}
int main(int argc,char **argv)
{
    fvl_dma_pool_t *dmapool[16];
    int rvl;
    int i,num;
    if(argc != 2)
    {
	printf("error cmdline!\n");
        return -1;	
    }
    num = atoi(argv[1]);
    for(i=0;i<num;i++)
    {
    	rvl = dma_pool_init(&dmapool[i],0x8000000,0x4000000);
    	if(rvl == 0)
    		printf("dma pool success!\n");
    }
    while(1);

    return 0;
}



