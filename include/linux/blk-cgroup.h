#ifndef _BLK_CGROUP_H
#define _BLK_CGROUP_H
/*
 * Common Block IO controller cgroup interface
 *
 * Based on ideas and code from CFQ, CFS and BFQ:
 * Copyright (C) 2003 Jens Axboe <axboe@kernel.dk>
 *
 * Copyright (C) 2008 Fabio Checconi <fabio@gandalf.sssup.it>
 *		      Paolo Valente <paolo.valente@unimore.it>
 *
 * Copyright (C) 2009 Vivek Goyal <vgoyal@redhat.com>
 * 	              Nauman Rafique <nauman@google.com>
 */

#include <linux/cgroup.h>
#include <linux/u64_stats_sync.h>
#include <linux/blkdev.h>

enum blkio_policy_id {
	BLKIO_POLICY_PROP = 0,		/* Proportional Bandwidth division */
	BLKIO_POLICY_THROTL,		/* Throttling */
	BLKIO_POLICY_TPPS,
	BLKIO_POLICY_CACHE,		/* Cache Quota */
};

/* Max limits for throttle policy */
#define THROTL_IOPS_MAX		UINT_MAX

#ifdef CONFIG_BLK_CGROUP

enum stat_type {
	/* Total time spent (in ns) between request dispatch to the driver and
	 * request completion for IOs doen by this cgroup. This may not be
	 * accurate when NCQ is turned on. */
	BLKIO_STAT_SERVICE_TIME = 0,
	/* Total time spent waiting in scheduler queue in ns */
	BLKIO_STAT_WAIT_TIME,
	/* Number of IOs merged */
	BLKIO_STAT_MERGED,
	/* Number of IOs throttled */
	BLKIO_STAT_THROTTLED,
	/* Number of IOs queued up */
	BLKIO_STAT_QUEUED,
	/* All the single valued stats go below this */
	BLKIO_STAT_TIME,
#ifdef CONFIG_DEBUG_BLK_CGROUP
	BLKIO_STAT_AVG_QUEUE_SIZE,
	BLKIO_STAT_IDLE_TIME,
	BLKIO_STAT_EMPTY_TIME,
	BLKIO_STAT_GROUP_WAIT_TIME,
	BLKIO_STAT_DEQUEUE
#endif
};

/* Per cpu stats */
enum stat_type_cpu {
	BLKIO_STAT_CPU_SECTORS,
	/* Total bytes transferred */
	BLKIO_STAT_CPU_SERVICE_BYTES,
	/* Total IOs serviced, post merge */
	BLKIO_STAT_CPU_SERVICED,
	BLKIO_STAT_CPU_NR
};

enum stat_sub_type {
	BLKIO_STAT_READ = 0,
	BLKIO_STAT_WRITE,
	BLKIO_STAT_SYNC,
	BLKIO_STAT_ASYNC,
	BLKIO_STAT_TOTAL
};

/* blkg state flags */
enum blkg_state_flags {
	BLKG_waiting = 0,
	BLKG_idling,
	BLKG_empty,
};

/* cgroup files owned by proportional weight policy */
enum blkcg_file_name_prop {
	BLKIO_PROP_weight = 1,
	BLKIO_PROP_weight_device,
	BLKIO_PROP_io_service_bytes,
	BLKIO_PROP_io_serviced,
	BLKIO_PROP_time,
	BLKIO_PROP_sectors,
	BLKIO_PROP_io_service_time,
	BLKIO_PROP_io_wait_time,
	BLKIO_PROP_io_merged,
	BLKIO_PROP_io_queued,
	BLKIO_PROP_avg_queue_size,
	BLKIO_PROP_group_wait_time,
	BLKIO_PROP_idle_time,
	BLKIO_PROP_empty_time,
	BLKIO_PROP_dequeue,
};

/* cgroup files owned by throttle policy */
enum blkcg_file_name_throtl {
	BLKIO_THROTL_read_bps_device,
	BLKIO_THROTL_write_bps_device,
	BLKIO_THROTL_read_iops_device,
	BLKIO_THROTL_write_iops_device,
	BLKIO_THROTL_seq_bios_device,
	BLKIO_THROTL_io_service_bytes,
	BLKIO_THROTL_io_serviced,
	BLKIO_THROTL_io_queued,
	BLKIO_THROTL_async_write_bps,
};

struct blkio_cgroup {
	struct cgroup_subsys_state css;
	unsigned int weight;
	spinlock_t lock;
	struct hlist_head blkg_list;
	struct list_head policy_list; /* list of blkio_policy_node */
	struct percpu_counter nr_dirtied;
	unsigned long bw_time_stamp;
	unsigned long dirtied_stamp;
	unsigned long dirty_ratelimit;
	unsigned long long async_write_bps;
};

struct blkio_group_stats {
	/* total disk time and nr sectors dispatched by this group */
	uint64_t time;
	uint64_t stat_arr[BLKIO_STAT_QUEUED + 1][BLKIO_STAT_TOTAL];
#ifdef CONFIG_DEBUG_BLK_CGROUP
	/* Sum of number of IOs queued across all samples */
	uint64_t avg_queue_size_sum;
	/* Count of samples taken for average */
	uint64_t avg_queue_size_samples;
	/* How many times this group has been removed from service tree */
	unsigned long dequeue;

	/* Total time spent waiting for it to be assigned a timeslice. */
	uint64_t group_wait_time;
	uint64_t start_group_wait_time;

	/* Time spent idling for this blkio_group */
	uint64_t idle_time;
	uint64_t start_idle_time;
	/*
	 * Total time when we have requests queued and do not contain the
	 * current active queue.
	 */
	uint64_t empty_time;
	uint64_t start_empty_time;
	uint16_t flags;
#endif
};

/* Per cpu blkio group stats */
struct blkio_group_stats_cpu {
	uint64_t sectors;
	uint64_t stat_arr_cpu[BLKIO_STAT_CPU_NR][BLKIO_STAT_TOTAL];
	struct u64_stats_sync syncp;
};

struct blkio_group {
	/* Pointer to the associated request_queue, RCU protected */
	struct request_queue *q;
	struct list_head q_node;
	struct hlist_node blkcg_node;
	/* request allocation list for this blkcg-q pair */
	struct request_list		rl;
	unsigned short blkcg_id;
	/* Store cgroup path */
	char path[128];
	/* The device MKDEV(major, minor), this group has been created for */
	dev_t dev;
	/* policy which owns this blk group */
	enum blkio_policy_id plid;

	/* Need to serialize the stats in the case of reset/update */
	spinlock_t stats_lock;
	struct blkio_group_stats stats;
	/* Per cpu stats pointer */
	struct blkio_group_stats_cpu __percpu *stats_cpu;
};

struct blkio_policy_node {
	struct list_head node;
	dev_t dev;
	/* This node belongs to max bw policy or porportional weight policy */
	enum blkio_policy_id plid;
	/* cgroup file to which this rule belongs to */
	int fileid;

	union {
		unsigned int weight;
		/*
		 * Rate read/write in terms of byptes per second
		 * Whether this rate represents read or write is determined
		 * by file type "fileid".
		 */
		u64 bps;
		unsigned int iops;
		unsigned int seq_bios;
	} val;
};

extern unsigned int blkcg_get_weight(struct blkio_cgroup *blkcg,
				     dev_t dev, enum blkio_policy_id plid);
extern uint64_t blkcg_get_read_bps(struct blkio_cgroup *blkcg,
				     dev_t dev);
extern uint64_t blkcg_get_write_bps(struct blkio_cgroup *blkcg,
				     dev_t dev);
extern unsigned int blkcg_get_read_iops(struct blkio_cgroup *blkcg,
				     dev_t dev);
extern unsigned int blkcg_get_write_iops(struct blkio_cgroup *blkcg,
				     dev_t dev);

typedef void (blkio_unlink_group_fn) (struct request_queue *q,
			struct blkio_group *blkg);

typedef void (blkio_update_group_weight_fn) (struct request_queue *q,
			struct blkio_group *blkg, unsigned int weight);
typedef void (blkio_update_group_read_bps_fn) (struct request_queue *q,
			struct blkio_group *blkg, u64 read_bps);
typedef void (blkio_update_group_write_bps_fn) (struct request_queue *q,
			struct blkio_group *blkg, u64 write_bps);
typedef void (blkio_update_group_read_iops_fn) (struct request_queue *q,
			struct blkio_group *blkg, unsigned int read_iops);
typedef void (blkio_update_group_write_iops_fn) (struct request_queue *q,
			struct blkio_group *blkg, unsigned int write_iops);
typedef void (blkio_update_group_seq_bios_fn) (struct request_queue *q,
			struct blkio_group *blkg, unsigned int seq_bios);

struct blkio_policy_ops {
	blkio_unlink_group_fn *blkio_unlink_group_fn;
	blkio_update_group_weight_fn *blkio_update_group_weight_fn;
	blkio_update_group_read_bps_fn *blkio_update_group_read_bps_fn;
	blkio_update_group_write_bps_fn *blkio_update_group_write_bps_fn;
	blkio_update_group_read_iops_fn *blkio_update_group_read_iops_fn;
	blkio_update_group_write_iops_fn *blkio_update_group_write_iops_fn;
	blkio_update_group_seq_bios_fn *blkio_update_group_seq_bios_fn;
};

struct blkio_policy_type {
	struct list_head list;
	struct blkio_policy_ops ops;
	enum blkio_policy_id plid;
};

/* Blkio controller policy registration */
void blkio_policy_register(struct blkio_policy_type *);
void blkio_policy_unregister(struct blkio_policy_type *);

static inline char *blkg_path(struct blkio_group *blkg)
{
	return blkg->path;
}

#else

struct blkio_group {
};

struct blkio_policy_type {
};

static inline void blkio_policy_register(struct blkio_policy_type *blkiop) { }
static inline void blkio_policy_unregister(struct blkio_policy_type *blkiop) { }

static inline char *blkg_path(struct blkio_group *blkg) { return NULL; }
static inline struct request_list *blk_get_rl(struct request_queue *q,
					      struct bio *bio) { return &q->root_rl; }
static inline void blk_put_rl(struct request_list *rl) { }
static inline void blk_rq_set_rl(struct request *rq, struct request_list *rl) { }
static inline struct request_list *blk_rq_rl(struct request *rq) { return &rq->q->root_rl; }

#define blk_queue_for_each_rl(rl, q)	\
	for ((rl) = &(q)->root_rl; (rl); (rl) = NULL)

#endif

#define BLKIO_WEIGHT_MIN	100
#define BLKIO_WEIGHT_MAX	1000
#define BLKIO_WEIGHT_DEFAULT	500

#ifdef CONFIG_DEBUG_BLK_CGROUP
void blkiocg_update_avg_queue_size_stats(struct blkio_group *blkg);
void blkiocg_update_dequeue_stats(struct blkio_group *blkg,
				unsigned long dequeue);
void blkiocg_update_set_idle_time_stats(struct blkio_group *blkg);
void blkiocg_update_idle_time_stats(struct blkio_group *blkg);
void blkiocg_set_start_empty_time(struct blkio_group *blkg);

#define BLKG_FLAG_FNS(name)						\
static inline void blkio_mark_blkg_##name(				\
		struct blkio_group_stats *stats)			\
{									\
	stats->flags |= (1 << BLKG_##name);				\
}									\
static inline void blkio_clear_blkg_##name(				\
		struct blkio_group_stats *stats)			\
{									\
	stats->flags &= ~(1 << BLKG_##name);				\
}									\
static inline int blkio_blkg_##name(struct blkio_group_stats *stats)	\
{									\
	return (stats->flags & (1 << BLKG_##name)) != 0;		\
}									\

BLKG_FLAG_FNS(waiting)
BLKG_FLAG_FNS(idling)
BLKG_FLAG_FNS(empty)
#undef BLKG_FLAG_FNS
#else
static inline void blkiocg_update_avg_queue_size_stats(
						struct blkio_group *blkg) {}
static inline void blkiocg_update_dequeue_stats(struct blkio_group *blkg,
						unsigned long dequeue) {}
static inline void blkiocg_update_set_idle_time_stats(struct blkio_group *blkg)
{}
static inline void blkiocg_update_idle_time_stats(struct blkio_group *blkg) {}
static inline void blkiocg_set_start_empty_time(struct blkio_group *blkg) {}
#endif

#ifdef CONFIG_BLK_CGROUP
extern struct blkio_cgroup blkio_root_cgroup;
extern struct blkio_cgroup *cgroup_to_blkio_cgroup(struct cgroup *cgroup);
extern struct blkio_cgroup *task_blkio_cgroup(struct task_struct *tsk);
extern void blkiocg_add_blkio_group(struct blkio_cgroup *blkcg,
	struct blkio_group *blkg, struct request_queue *q, dev_t dev,
	enum blkio_policy_id plid);
extern int blkio_alloc_blkg_stats(struct blkio_group *blkg);
extern int blkiocg_del_blkio_group(struct blkio_group *blkg);
extern struct blkio_group *blkiocg_lookup_group(struct blkio_cgroup *blkcg,
						struct request_queue *q,
						enum blkio_policy_id plid);
void blkiocg_update_timeslice_used(struct blkio_group *blkg,
					unsigned long time);
void blkiocg_update_dispatch_stats(struct blkio_group *blkg, uint64_t bytes,
						bool direction, bool sync);
void blkiocg_update_completion_stats(struct blkio_group *blkg,
	uint64_t start_time, uint64_t io_start_time, bool direction, bool sync);
void blkiocg_update_io_merged_stats(struct blkio_group *blkg, bool direction,
					bool sync);
void blkiocg_update_io_add_stats(struct blkio_group *blkg,
		struct blkio_group *curr_blkg, bool direction, bool sync);
void blkiocg_update_io_remove_stats(struct blkio_group *blkg,
					bool direction, bool sync);
void blkiocg_update_io_throttled_stats(struct blkio_group *blkg,
		struct blkio_group *curr_blkg, bool direction, bool sync);
void blkiocg_update_io_throttled_remove_stats(struct blkio_group *blkg,
					      bool direction, bool sync);

/**
 * blk_get_rl - get request_list to use
 * @q: request_queue of interest
 * @bio: bio which will be attached to the allocated request (may be %NULL)
 *
 * The caller wants to allocate a request from @q to use for @bio.  Find
 * the request_list to use and obtain a reference on it.  Should be called
 * under queue_lock.  This function is guaranteed to return non-%NULL
 * request_list.
 */
static inline struct request_list *blk_get_rl(struct request_queue *q,
					      struct bio *bio)
{
	struct blkio_cgroup *blkcg;
	struct blkio_group *blkg;

	rcu_read_lock();

	blkcg = task_blkio_cgroup(current);

	/* bypass blkg lookup and use @q->root_rl directly for root */
	if (blkcg == &blkio_root_cgroup)
		goto root_rl;

	if (test_bit(QUEUE_FLAG_ELVSWITCH, &q->queue_flags))
		goto root_rl;

	/*
	 * Try to use blkg->rl.  blkg lookup may fail under memory pressure
	 * or if either the blkcg or queue is going away.  Fall back to
	 * root_rl in such cases.
	 */
	blkg = blkiocg_lookup_group(blkcg, q, BLKIO_POLICY_PROP);
	if (!blkg) {
		blkg = blkiocg_lookup_group(blkcg, q, BLKIO_POLICY_TPPS);
		if (!blkg)
			goto root_rl;
	}

	rcu_read_unlock();
	return &blkg->rl;
root_rl:
	rcu_read_unlock();
	return &q->root_rl;
}

/**
 * blk_rq_set_rl - associate a request with a request_list
 * @rq: request of interest
 * @rl: target request_list
 *
 * Associate @rq with @rl so that accounting and freeing can know the
 * request_list @rq came from.
 */
static inline void blk_rq_set_rl(struct request *rq, struct request_list *rl)
{
	rq->rl = rl;
}

/**
 * blk_rq_rl - return the request_list a request came from
 * @rq: request of interest
 *
 * Return the request_list @rq is allocated from.
 */
static inline struct request_list *blk_rq_rl(struct request *rq)
{
	return rq->rl;
}

struct request_list *__blk_queue_next_rl(struct request_list *rl,
					 struct request_queue *q);
/**
 * blk_queue_for_each_rl - iterate through all request_lists of a request_queue
 *
 * Should be used under queue_lock.
 */
#define blk_queue_for_each_rl(rl, q)	\
	for ((rl) = &(q)->root_rl; (rl); (rl) = __blk_queue_next_rl((rl), (q)))

#else
struct cgroup;
static inline struct blkio_cgroup *
cgroup_to_blkio_cgroup(struct cgroup *cgroup) { return NULL; }
static inline struct blkio_cgroup *
task_blkio_cgroup(struct task_struct *tsk) { return NULL; }

static inline void blkiocg_add_blkio_group(struct blkio_cgroup *blkcg,
		struct blkio_group *blkg, void *key, dev_t dev,
		enum blkio_policy_id plid) {}

static inline int blkio_alloc_blkg_stats(struct blkio_group *blkg) { return 0; }

static inline int
blkiocg_del_blkio_group(struct blkio_group *blkg) { return 0; }

static inline struct blkio_group *
blkiocg_lookup_group(struct blkio_cgroup *blkcg, struct request_queue *q,
		enum blkio_policy_id plid) { return NULL; }
static inline void blkiocg_update_timeslice_used(struct blkio_group *blkg,
						unsigned long time) {}
static inline void blkiocg_update_dispatch_stats(struct blkio_group *blkg,
				uint64_t bytes, bool direction, bool sync) {}
static inline void blkiocg_update_completion_stats(struct blkio_group *blkg,
		uint64_t start_time, uint64_t io_start_time, bool direction,
		bool sync) {}
static inline void blkiocg_update_io_merged_stats(struct blkio_group *blkg,
						bool direction, bool sync) {}
static inline void blkiocg_update_io_add_stats(struct blkio_group *blkg,
		struct blkio_group *curr_blkg, bool direction, bool sync) {}
static inline void blkiocg_update_io_remove_stats(struct blkio_group *blkg,
						bool direction, bool sync) {}
static inline void blkiocg_update_io_throttled_stats(struct blkio_group *blkg,
		struct blkio_group *curr_blkg, bool direction, bool sync) {}
static inline void
blkiocg_update_io_throttled_remove_stats(struct blkio_group *blkg,
		struct blkio_group *curr_blkg, bool direction, bool sync) {}
#endif
#endif /* _BLK_CGROUP_H */
