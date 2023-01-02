#include "ring_cache.h"
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/bug.h>



void ring_cache_init(void *base, unsigned int max_size, unsigned int *read,
	unsigned int *write, struct ring_cache *ring_cache)
{
	WARN_ON(!base);

	/* making sure max_size is power of 2 */
	WARN_ON(!max_size || (max_size & (max_size - 1)));

	/* making sure read & write pointers are 4 bytes aligned */
	WARN_ON(((long)read & 0x3) != 0 || ((long)write & 0x3) != 0);

	ring_cache->base = base;
	ring_cache->read = read;
	ring_cache->write = write;
	ring_cache->max_size = max_size;
}

void ring_cache_dump(const char *title, struct ring_cache *ring_cache)
{
	pr_info("[%s] ring_cache:{write=%d, read=%d, max_size=%d}\n",
			title, *ring_cache->write, *ring_cache->read, ring_cache->max_size);
}

void ring_cache_dump_segment(const char *title, struct ring_cache_segment *seg)
{
	pr_info("[%s] seg:{ring_cache_pt=0x%p, data_pos=%d, sz=%d, remain=%d}\n",
			title, seg->ring_cache_pt, seg->data_pos, seg->sz, seg->remain);
}

/*
 * Function prepares the ring_cache_segment and returns the number of valid bytes for read.
 */
unsigned int ring_cache_read_prepare(unsigned int sz, struct ring_cache_segment *seg, struct ring_cache *ring_cache)
{
	unsigned int wt = *ring_cache->write;
	unsigned int rd = *ring_cache->read;

	memset(seg, 0, sizeof(struct ring_cache_segment));
	if (sz > wt - rd)
		sz = wt - rd;
	seg->remain = sz;
	/* ring_cache_dump(__func__, ring_cache); */
	/* ring_cache_dump_segment(__func__, seg); */
	return seg->remain;
}

/*
 * Function prepares the ring_cache_segment and returns the number of bytes available for write.
 */
unsigned int ring_cache_write_prepare(unsigned int sz, struct ring_cache_segment *seg, struct ring_cache *ring_cache)
{
	unsigned int wt = *ring_cache->write;
	unsigned int rd = *ring_cache->read;

	memset(seg, 0, sizeof(struct ring_cache_segment));
	pr_info("ring_cache_write_prepare: sz(%d), wt(%d), rd(%d), max_size(%d)\n", sz, wt, rd, ring_cache->max_size);
	if (sz > ring_cache->max_size - (wt - rd))
		sz = ring_cache->max_size - (wt - rd);
	seg->remain = sz;
	/* ring_cache_dump(__func__, ring_cache); */
	/* ring_cache_dump_segment(__func__, seg); */
	return seg->remain;
}

void _ring_cache_segment_prepare(unsigned int from, struct ring_cache_segment *seg, struct ring_cache *ring_cache)
{
	unsigned int ring_cache_pos = from & (ring_cache->max_size - 1);

	seg->ring_cache_pt = ring_cache->base + ring_cache_pos;
	seg->data_pos = (seg->sz ? seg->data_pos + seg->sz : 0);
	if (ring_cache_pos + seg->remain <= ring_cache->max_size)
		seg->sz = seg->remain;
	else
		seg->sz = ring_cache->max_size - ring_cache_pos;
	seg->remain -= seg->sz;
	/* ring_cache_dump(__func__, ring_cache); */
	/* ring_cache_dump_segment(__func__, seg); */
}

void _ring_cache_read_commit(struct ring_cache_segment *seg, struct ring_cache *ring_cache)
{
	*(ring_cache->read) += seg->sz;
	/* ring_cache_dump(__func__, ring_cache); */
	/* ring_cache_dump_segment(__func__, seg); */
}
void _ring_cache_write_commit(struct ring_cache_segment *seg, struct ring_cache *ring_cache)
{
	*(ring_cache->write) += seg->sz;
	/* ring_cache_dump(__func__, ring_cache); */
	/* ring_cache_dump_segment(__func__, seg); */
}

