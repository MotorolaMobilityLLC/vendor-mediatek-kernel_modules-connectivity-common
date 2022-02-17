#include "ring.h"


void ring_init(void *base, unsigned int max_size, void *read, void *write, struct ring *ring)
{
	WARN_ON(!base || !read || !write);

	/* making sure max_size is power of 2 */
	WARN_ON(!max_size || (max_size & (max_size - 1)));

	/* making sure read & write pointers are 4 bytes aligned */
	WARN_ON(((long)read & 0x3) != 0 || ((long)write & 0x3) != 0);

	ring->base = base;
	ring->read = read;
	ring->write = write;
	EMI_WRITE32(ring->write, 0);
	EMI_WRITE32(ring->read, 0);
	ring->max_size = max_size;
	pr_info("base: %p, read: %p, write: %p, max_size: %d\n", base, read, write, max_size);
}

void ring_dump(const char *title, struct ring *ring)
{
	pr_info("[%s] ring:{base=0x%p, write=%d, read=%d, max_size=%d}\n",
			title, ring->base, EMI_READ32(ring->write), EMI_READ32(ring->read), ring->max_size);
}

void ring_dump_segment(const char *title, struct ring_segment *seg)
{
	pr_info("[%s] seg:{ring_pt=0x%p, data_pos=%d, sz=%d, remain=%d}\n",
			title, seg->ring_pt, seg->data_pos, seg->sz, seg->remain);
}

/*
 * Function prepares the ring_segment and returns the number of valid bytes for read.
 */
unsigned int ring_read_prepare(unsigned int sz, struct ring_segment *seg, struct ring *ring)
{
	unsigned int wt = EMI_READ32(ring->write);
	unsigned int rd = EMI_READ32(ring->read);

	memset(seg, 0, sizeof(struct ring_segment));
#ifdef ROUND_REPEAT
	if (wt >= rd) {
		if (sz > wt - rd)
			sz = wt - rd;
		seg->remain = sz;
	} else {
		if (sz > ring->max_size - (rd - wt))
			sz = ring->max_size - (rd - wt);
		seg->remain = sz;
	}
#else
	if (sz > wt - rd)
		sz = wt - rd;
	seg->remain = sz;
#endif
	/* ring_dump(__func__, ring); */
	/* ring_dump_segment(__func__, seg); */
	return seg->remain;
}

/*
 * Function prepares the ring_segment and returns the number of bytes available for write.
 */
unsigned int ring_write_prepare(unsigned int sz, struct ring_segment *seg, struct ring *ring)
{
	unsigned int wt = EMI_READ32(ring->write);
	unsigned int rd = EMI_READ32(ring->read);

	memset(seg, 0, sizeof(struct ring_segment));
#ifdef ROUND_REPEAT
	if (wt >= rd)
		seg->remain = ring->max_size - (wt - rd + 1);
	else
		seg->remain = ring->max_size - (rd - wt + 1);

	if (sz <= seg->remain)
		seg->remain = sz;
#else
	if (sz > ring->max_size - (wt - rd))
		sz = ring->max_size - (wt - rd);
	seg->remain = sz;
#endif
	/* ring_dump(__func__, ring); */
	/* ring_dump_segment(__func__, seg); */
	return seg->remain;
}

void _ring_segment_prepare(unsigned int from, struct ring_segment *seg, struct ring *ring)
{
#ifndef ROUND_REPEAT
	unsigned int ring_pos = from & (ring->max_size - 1);

	seg->ring_pt = ring->base + ring_pos;
#else
	seg->ring_pt = ring->base + from;
#endif
	seg->data_pos = (seg->sz ? seg->data_pos + seg->sz : 0);
	if (from + seg->remain <= ring->max_size)
		seg->sz = seg->remain;
	else
		seg->sz = ring->max_size - from;
	seg->remain -= seg->sz;
	/* ring_dump(__func__, ring); */
	/* ring_dump_segment(__func__, seg); */
}

void _ring_read_commit(struct ring_segment *seg, struct ring *ring)
{
#ifdef ROUND_REPEAT
	EMI_WRITE32(ring->read, (EMI_READ32(ring->read) + seg->sz) & (ring->max_size - 1));
#else
	EMI_WRITE32(ring->read, EMI_READ32(ring->read) + seg->sz);
#endif
	/* *(ring->read) += seg->sz; */
	/* ring_dump(__func__, ring); */
	/* ring_dump_segment(__func__, seg); */
}
void _ring_write_commit(struct ring_segment *seg, struct ring *ring)
{
#ifdef ROUND_REPEAT
	EMI_WRITE32(ring->write, (EMI_READ32(ring->write) + seg->sz) & (ring->max_size - 1));
#else
	EMI_WRITE32(ring->write, EMI_READ32(ring->write) + seg->sz);
#endif
	/* ring_dump(__func__, ring); */
	/* ring_dump_segment(__func__, seg); */
}

