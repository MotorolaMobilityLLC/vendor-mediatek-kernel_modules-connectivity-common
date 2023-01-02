#ifndef _RING_H_
#define _RING_H_

#include <linux/io.h>
#define ROUND_REPEAT
#define EMI_READ32(addr) (readl(addr))
#define EMI_WRITE32(addr, data) (writel(data, addr))

struct ring {
	/* addr where ring buffer starts */
	void *base;
	/* addr storing the next writable pos, guaranteed to be >= read except when write overflow, but it's ok. */
	void *write;
	/* addr storing the next readable pos, except when read == write as buffer empty */
	void *read;
	/* must be power of 2 */
	unsigned int max_size;
};

struct ring_segment {
	/* addr points into ring buffer for read/write */
	void *ring_pt;
	/* size to read/write */
	unsigned int sz;
	/* pos in external data buffer to read/write */
	unsigned int data_pos;
	/* the size to be read/write after this segment completed */
	unsigned int remain;
};

void ring_init(void *base, unsigned int max_size, void *read, void *write, struct ring *ring);
unsigned int ring_read_prepare(unsigned int sz, struct ring_segment *seg, struct ring *ring);
#define ring_read_all_prepare(seg, ring)  ring_read_prepare((ring)->max_size, seg, ring)
unsigned int ring_write_prepare(unsigned int sz, struct ring_segment *seg, struct ring *ring);

/* making sure max_size is power of 2 */
#define RING_VALIDATE_SIZE(max_size) WARN_ON(!max_size || (max_size & (max_size - 1)))

#define RING_EMPTY(ring) (EMI_READ32((ring)->read) == EMI_READ32((ring)->write))
/* equation works even when write overflow */
#define RING_SIZE(ring) (EMI_READ32((ring)->write) - EMI_READ32((ring)->read))
#ifdef ROUND_REPEAT
#define RING_FULL(ring) (((EMI_READ32((ring)->write) + 1) & ((ring)->max_size - 1)) == EMI_READ32((ring)->read))
#else
#define RING_FULL(ring) (RING_SIZE(ring) == (ring)->max_size)
#endif

#define RING_READ_FOR_EACH(_sz, _seg, _ring) \
	for (_ring_segment_prepare(EMI_READ32((_ring)->read), &(_seg), (_ring)); \
		(_seg).sz > 0; \
		_ring_read_commit(&(_seg), (_ring)), \
		_ring_segment_prepare(EMI_READ32((_ring)->read), &(_seg), (_ring)))

#define RING_READ_ALL_FOR_EACH(seg, ring) RING_READ_FOR_EACH((ring)->max_size, seg, ring)

#define RING_WRITE_FOR_EACH(_sz, _seg, _ring) \
	for (_ring_segment_prepare(EMI_READ32((_ring)->write), &(_seg), (_ring)); \
		(_seg).sz > 0; \
		_ring_write_commit(&(_seg), (_ring)), \
		_ring_segment_prepare(EMI_READ32((_ring)->write), &(_seg), (_ring)))

void ring_dump(const char *title, struct ring *ring);
void ring_dump_segment(const char *title, struct ring_segment *seg);


/* Ring Buffer Internal API */
void _ring_segment_prepare(unsigned int from, struct ring_segment *seg, struct ring *ring);
void _ring_read_commit(struct ring_segment *seg, struct ring *ring);
void _ring_write_commit(struct ring_segment *seg, struct ring *ring);

#endif
