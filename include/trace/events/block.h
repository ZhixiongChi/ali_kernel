#undef TRACE_SYSTEM
#define TRACE_SYSTEM block

#if !defined(_TRACE_BLOCK_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_BLOCK_H

#include <linux/blktrace_api.h>
#include <linux/blkdev.h>
#include <linux/tracepoint.h>

#define RWBS_LEN	8

DECLARE_EVENT_CLASS(block_rq_with_error,

	TP_PROTO(struct request_queue *q, struct request *rq),

	TP_ARGS(q, rq),

	TP_STRUCT__entry(
		__field(  dev_t,	dev			)
		__field(  sector_t,	sector			)
		__field(  unsigned int,	nr_sector		)
		__field(  int,		errors			)
		__array(  char,		rwbs,	RWBS_LEN	)
		__dynamic_array( char,	cmd,	blk_cmd_buf_len(rq)	)
	),

	TP_fast_assign(
		__entry->dev	   = rq->rq_disk ? disk_devt(rq->rq_disk) : 0;
		__entry->sector    = (rq->cmd_type == REQ_TYPE_BLOCK_PC) ?
					0 : blk_rq_pos(rq);
		__entry->nr_sector = (rq->cmd_type == REQ_TYPE_BLOCK_PC) ?
					0 : blk_rq_sectors(rq);
		__entry->errors    = rq->errors;

		blk_fill_rwbs_rq(__entry->rwbs, rq);
		blk_dump_cmd(__get_str(cmd), rq);
	),

	TP_printk("%d,%d %s (%s) %llu + %u [%d]",
		  MAJOR(__entry->dev), MINOR(__entry->dev),
		  __entry->rwbs, __get_str(cmd),
		  (unsigned long long)__entry->sector,
		  __entry->nr_sector, __entry->errors)
);

DEFINE_EVENT(block_rq_with_error, block_rq_abort,

	TP_PROTO(struct request_queue *q, struct request *rq),

	TP_ARGS(q, rq)
);

DEFINE_EVENT(block_rq_with_error, block_rq_requeue,

	TP_PROTO(struct request_queue *q, struct request *rq),

	TP_ARGS(q, rq)
);

DEFINE_EVENT(block_rq_with_error, block_rq_complete,

	TP_PROTO(struct request_queue *q, struct request *rq),

	TP_ARGS(q, rq)
);

DECLARE_EVENT_CLASS(block_rq,

	TP_PROTO(struct request_queue *q, struct request *rq),

	TP_ARGS(q, rq),

	TP_STRUCT__entry(
		__field(  dev_t,	dev			)
		__field(  sector_t,	sector			)
		__field(  unsigned int,	nr_sector		)
		__field(  unsigned int,	bytes			)
		__array(  char,		rwbs,	RWBS_LEN	)
		__array(  char,         comm,   TASK_COMM_LEN   )
		__dynamic_array( char,	cmd,	blk_cmd_buf_len(rq)	)
	),

	TP_fast_assign(
		__entry->dev	   = rq->rq_disk ? disk_devt(rq->rq_disk) : 0;
		__entry->sector    = (rq->cmd_type == REQ_TYPE_BLOCK_PC) ?
					0 : blk_rq_pos(rq);
		__entry->nr_sector = (rq->cmd_type == REQ_TYPE_BLOCK_PC) ?
					0 : blk_rq_sectors(rq);
		__entry->bytes     = (rq->cmd_type == REQ_TYPE_BLOCK_PC) ?
					blk_rq_bytes(rq) : 0;

		blk_fill_rwbs_rq(__entry->rwbs, rq);
		blk_dump_cmd(__get_str(cmd), rq);
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
	),

	TP_printk("%d,%d %s %u (%s) %llu + %u [%s]",
		  MAJOR(__entry->dev), MINOR(__entry->dev),
		  __entry->rwbs, __entry->bytes, __get_str(cmd),
		  (unsigned long long)__entry->sector,
		  __entry->nr_sector, __entry->comm)
);

DEFINE_EVENT(block_rq, block_rq_insert,

	TP_PROTO(struct request_queue *q, struct request *rq),

	TP_ARGS(q, rq)
);

DEFINE_EVENT(block_rq, block_rq_issue,

	TP_PROTO(struct request_queue *q, struct request *rq),

	TP_ARGS(q, rq)
);

TRACE_EVENT(block_bio_bounce,

	TP_PROTO(struct request_queue *q, struct bio *bio),

	TP_ARGS(q, bio),

	TP_STRUCT__entry(
		__field( dev_t,		dev			)
		__field( sector_t,	sector			)
		__field( unsigned int,	nr_sector		)
		__array( char,		rwbs,	RWBS_LEN	)
		__array( char,		comm,	TASK_COMM_LEN	)
	),

	TP_fast_assign(
		__entry->dev		= bio->bi_bdev ?
					  bio->bi_bdev->bd_dev : 0;
		__entry->sector		= bio->bi_sector;
		__entry->nr_sector	= bio->bi_size >> 9;
		blk_fill_rwbs(__entry->rwbs, bio->bi_rw, bio->bi_size);
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
	),

	TP_printk("%d,%d %s %llu + %u [%s]",
		  MAJOR(__entry->dev), MINOR(__entry->dev), __entry->rwbs,
		  (unsigned long long)__entry->sector,
		  __entry->nr_sector, __entry->comm)
);

TRACE_EVENT(block_bio_complete,

	TP_PROTO(struct request_queue *q, struct bio *bio),

	TP_ARGS(q, bio),

	TP_STRUCT__entry(
		__field( dev_t,		dev		)
		__field( sector_t,	sector		)
		__field( unsigned,	nr_sector	)
		__field( int,		error		)
		__array( char,		rwbs,	RWBS_LEN)
	),

	TP_fast_assign(
		__entry->dev		= bio->bi_bdev->bd_dev;
		__entry->sector		= bio->bi_sector;
		__entry->nr_sector	= bio->bi_size >> 9;
		blk_fill_rwbs(__entry->rwbs, bio->bi_rw, bio->bi_size);
	),

	TP_printk("%d,%d %s %llu + %u [%d]",
		  MAJOR(__entry->dev), MINOR(__entry->dev), __entry->rwbs,
		  (unsigned long long)__entry->sector,
		  __entry->nr_sector, __entry->error)
);

DECLARE_EVENT_CLASS(block_bio,

	TP_PROTO(struct request_queue *q, struct bio *bio),

	TP_ARGS(q, bio),

	TP_STRUCT__entry(
		__field( dev_t,		dev			)
		__field( sector_t,	sector			)
		__field( unsigned int,	nr_sector		)
		__array( char,		rwbs,	RWBS_LEN	)
		__array( char,		comm,	TASK_COMM_LEN	)
	),

	TP_fast_assign(
		__entry->dev		= bio->bi_bdev->bd_dev;
		__entry->sector		= bio->bi_sector;
		__entry->nr_sector	= bio->bi_size >> 9;
		blk_fill_rwbs(__entry->rwbs, bio->bi_rw, bio->bi_size);
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
	),

	TP_printk("%d,%d %s %llu + %u [%s]",
		  MAJOR(__entry->dev), MINOR(__entry->dev), __entry->rwbs,
		  (unsigned long long)__entry->sector,
		  __entry->nr_sector, __entry->comm)
);

DEFINE_EVENT(block_bio, block_bio_backmerge,

	TP_PROTO(struct request_queue *q, struct bio *bio),

	TP_ARGS(q, bio)
);

DEFINE_EVENT(block_bio, block_bio_frontmerge,

	TP_PROTO(struct request_queue *q, struct bio *bio),

	TP_ARGS(q, bio)
);

DEFINE_EVENT(block_bio, block_bio_queue,

	TP_PROTO(struct request_queue *q, struct bio *bio),

	TP_ARGS(q, bio)
);

DECLARE_EVENT_CLASS(block_get_rq,

	TP_PROTO(struct request_queue *q, struct bio *bio, int rw),

	TP_ARGS(q, bio, rw),

	TP_STRUCT__entry(
		__field( dev_t,		dev			)
		__field( sector_t,	sector			)
		__field( unsigned int,	nr_sector		)
		__array( char,		rwbs,	RWBS_LEN	)
		__array( char,		comm,	TASK_COMM_LEN	)
        ),

	TP_fast_assign(
		__entry->dev		= bio ? bio->bi_bdev->bd_dev : 0;
		__entry->sector		= bio ? bio->bi_sector : 0;
		__entry->nr_sector	= bio ? bio->bi_size >> 9 : 0;
		blk_fill_rwbs(__entry->rwbs,
			      bio ? bio->bi_rw : 0, __entry->nr_sector);
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
        ),

	TP_printk("%d,%d %s %llu + %u [%s]",
		  MAJOR(__entry->dev), MINOR(__entry->dev), __entry->rwbs,
		  (unsigned long long)__entry->sector,
		  __entry->nr_sector, __entry->comm)
);

DEFINE_EVENT(block_get_rq, block_getrq,

	TP_PROTO(struct request_queue *q, struct bio *bio, int rw),

	TP_ARGS(q, bio, rw)
);

DEFINE_EVENT(block_get_rq, block_sleeprq,

	TP_PROTO(struct request_queue *q, struct bio *bio, int rw),

	TP_ARGS(q, bio, rw)
);

TRACE_EVENT(block_plug,

	TP_PROTO(struct request_queue *q),

	TP_ARGS(q),

	TP_STRUCT__entry(
		__array( char,		comm,	TASK_COMM_LEN	)
	),

	TP_fast_assign(
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
	),

	TP_printk("[%s]", __entry->comm)
);

DECLARE_EVENT_CLASS(block_unplug,

	TP_PROTO(struct request_queue *q),

	TP_ARGS(q),

	TP_STRUCT__entry(
		__field( int,		nr_rq			)
		__array( char,		comm,	TASK_COMM_LEN	)
	),

	TP_fast_assign(
		__entry->nr_rq	= q->root_rl.count[READ] + q->root_rl.count[WRITE];
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
	),

	TP_printk("[%s] %d", __entry->comm, __entry->nr_rq)
);

DEFINE_EVENT(block_unplug, block_unplug_timer,

	TP_PROTO(struct request_queue *q),

	TP_ARGS(q)
);

DEFINE_EVENT(block_unplug, block_unplug_io,

	TP_PROTO(struct request_queue *q),

	TP_ARGS(q)
);

TRACE_EVENT(block_split,

	TP_PROTO(struct request_queue *q, struct bio *bio,
		 unsigned int new_sector),

	TP_ARGS(q, bio, new_sector),

	TP_STRUCT__entry(
		__field( dev_t,		dev				)
		__field( sector_t,	sector				)
		__field( sector_t,	new_sector			)
		__array( char,		rwbs,		RWBS_LEN	)
		__array( char,		comm,		TASK_COMM_LEN	)
	),

	TP_fast_assign(
		__entry->dev		= bio->bi_bdev->bd_dev;
		__entry->sector		= bio->bi_sector;
		__entry->new_sector	= new_sector;
		blk_fill_rwbs(__entry->rwbs, bio->bi_rw, bio->bi_size);
		memcpy(__entry->comm, current->comm, TASK_COMM_LEN);
	),

	TP_printk("%d,%d %s %llu / %llu [%s]",
		  MAJOR(__entry->dev), MINOR(__entry->dev), __entry->rwbs,
		  (unsigned long long)__entry->sector,
		  (unsigned long long)__entry->new_sector,
		  __entry->comm)
);

TRACE_EVENT(block_remap,

	TP_PROTO(struct request_queue *q, struct bio *bio, dev_t dev,
		 sector_t from),

	TP_ARGS(q, bio, dev, from),

	TP_STRUCT__entry(
		__field( dev_t,		dev		)
		__field( sector_t,	sector		)
		__field( unsigned int,	nr_sector	)
		__field( dev_t,		old_dev		)
		__field( sector_t,	old_sector	)
		__array( char,		rwbs,	RWBS_LEN)
	),

	TP_fast_assign(
		__entry->dev		= bio->bi_bdev->bd_dev;
		__entry->sector		= bio->bi_sector;
		__entry->nr_sector	= bio->bi_size >> 9;
		__entry->old_dev	= dev;
		__entry->old_sector	= from;
		blk_fill_rwbs(__entry->rwbs, bio->bi_rw, bio->bi_size);
	),

	TP_printk("%d,%d %s %llu + %u <- (%d,%d) %llu",
		  MAJOR(__entry->dev), MINOR(__entry->dev), __entry->rwbs,
		  (unsigned long long)__entry->sector,
		  __entry->nr_sector,
		  MAJOR(__entry->old_dev), MINOR(__entry->old_dev),
		  (unsigned long long)__entry->old_sector)
);

TRACE_EVENT(block_rq_remap,

	TP_PROTO(struct request_queue *q, struct request *rq, dev_t dev,
		 sector_t from),

	TP_ARGS(q, rq, dev, from),

	TP_STRUCT__entry(
		__field( dev_t,		dev		)
		__field( sector_t,	sector		)
		__field( unsigned int,	nr_sector	)
		__field( dev_t,		old_dev		)
		__field( sector_t,	old_sector	)
		__array( char,		rwbs,	RWBS_LEN)
	),

	TP_fast_assign(
		__entry->dev		= disk_devt(rq->rq_disk);
		__entry->sector		= blk_rq_pos(rq);
		__entry->nr_sector	= blk_rq_sectors(rq);
		__entry->old_dev	= dev;
		__entry->old_sector	= from;
		blk_fill_rwbs_rq(__entry->rwbs, rq);
	),

	TP_printk("%d,%d %s %llu + %u <- (%d,%d) %llu",
		  MAJOR(__entry->dev), MINOR(__entry->dev), __entry->rwbs,
		  (unsigned long long)__entry->sector,
		  __entry->nr_sector,
		  MAJOR(__entry->old_dev), MINOR(__entry->old_dev),
		  (unsigned long long)__entry->old_sector)
);

#endif /* _TRACE_BLOCK_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

