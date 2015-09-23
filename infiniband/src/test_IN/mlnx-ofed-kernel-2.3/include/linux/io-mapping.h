#ifndef _COMPAT_IO_MAPPING_H_
#define _COMPAT_IO_MAPPING_H_

#if defined(COMPAT_VMWARE)
static inline void * io_mapping_create_wc(resource_size_t area_start, resource_size_t area_len) {
	return NULL;
}
static inline void io_mapping_free(void *map) {
	/* STUB */
}

struct io_mapping;

static inline void * io_mapping_map_wc(struct io_mapping *mapping, unsigned long offset) {
	return NULL;
}
static inline void io_mapping_unmap(void *map) {
	/* STUB */
}
#else
#include_next <linux/io-mapping.h>
#endif

#endif /* _COMPAT_IO_MAPPING_H_ */
