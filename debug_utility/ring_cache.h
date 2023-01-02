#ifndef _RING_CACHE_H_
#define _RING_CACHE_H_

struct ring_cache {
	/* addr where ring_cache buffer starts */
	void *base;
	/* addr storing the next writable pos, guaranteed to be >= read except when write overflow, but it's ok. */
	unsigned int *write;
	/* addr storing the next readable pos, except when read == write as buffer empty */
	unsigned int *read;
	/* must be power of 2 */
	unsigned int max_size;
};

struct ring_cache_segment {
	/* addr points into ring_cache buffer for read/write */
	void *ring_cache_pt;
	/* size to read/write */
	unsigned int sz;
	/* pos in external data buffer to read/write */
	unsigned int data_pos;
	/* the size to be read/write after this segment completed */
	unsigned int remain;
};

void ring_cache_init(void *base, unsigned int max_size, unsigned int *read,
	unsigned int *write, struct ring_cache *ring_cache);
unsigned int ring_cache_read_prepare(unsigned int sz, struct ring_cache_segment *seg, struct ring_cache *ring_cache);
#define ring_cache_read_all_prepare(seg, ring_cache)  ring_cache_read_prepare((ring_cache)->max_size, seg, ring_cache)
unsigned int ring_cache_write_prepare(unsigned int sz, struct ring_cache_segment *seg, struct ring_cache *ring_cache);

/* making sure max_size is power of 2 */
#define RING_CACHE_VALIDATE_SIZE(max_size) WARN_ON(!max_size || (max_size & (max_size - 1)))

#define RING_CACHE_EMPTY(ring_cache) (*((ring_cache)->read) == *((ring_cache)->write))
/* equation works even when write overflow */
#define RING_CACHE_SIZE(ring_cache) (*((ring_cache)->write) - *((ring_cache)->read))
#define RING_CACHE_FULL(ring_cache) (RING_CACHE_SIZE(ring_cache) == (ring_cache)->max_size)
#define RING_CACHE_WRITE_REMAIN_SIZE(ring_cache) ((ring_cache)->max_size - RING_CACHE_SIZE(ring_cache))

#define RING_CACHE_READ_FOR_EACH(_sz, _seg, _ring_cache) \
	for (_ring_cache_segment_prepare(*((_ring_cache)->read), &(_seg), (_ring_cache)); \
		(_seg).sz > 0; \
		_ring_cache_read_commit(&(_seg), (_ring_cache)), _ring_cache_segment_prepare(*((_ring_cache)->read), \
			&(_seg), (_ring_cache)))

#define RING_CACHE_READ_ALL_FOR_EACH(seg, ring_cache) RING_CACHE_READ_FOR_EACH((ring_cache)->max_size, seg, ring_cache)

#define RING_CACHE_WRITE_FOR_EACH(_sz, _seg, _ring_cache) \
	for (_ring_cache_segment_prepare(*((_ring_cache)->write), &(_seg), (_ring_cache)); \
		(_seg).sz > 0; \
		_ring_cache_write_commit(&(_seg), (_ring_cache)), _ring_cache_segment_prepare(*((_ring_cache)->write), \
			&(_seg), (_ring_cache)))

void ring_cache_dump(const char *title, struct ring_cache *ring_cache);
void ring_cache_dump_segment(const char *title, struct ring_cache_segment *seg);


/* ring_cache Buffer Internal API */
void _ring_cache_segment_prepare(unsigned int from, struct ring_cache_segment *seg, struct ring_cache *ring_cache);
void _ring_cache_read_commit(struct ring_cache_segment *seg, struct ring_cache *ring_cache);
void _ring_cache_write_commit(struct ring_cache_segment *seg, struct ring_cache *ring_cache);

#endif
