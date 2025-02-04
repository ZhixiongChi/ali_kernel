#undef TRACE_SYSTEM
#define TRACE_SYSTEM writeback

#if !defined(_TRACE_WRITEBACK_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_WRITEBACK_H

#include <linux/backing-dev.h>
#include <linux/device.h>
#include <linux/writeback.h>

struct wb_writeback_work;

DECLARE_EVENT_CLASS(writeback_work_class,
	TP_PROTO(struct backing_dev_info *bdi, struct wb_writeback_work *work),
	TP_ARGS(bdi, work),
	TP_STRUCT__entry(
		__array(char, name, 32)
		__field(long, nr_pages)
		__field(dev_t, sb_dev)
		__field(int, sync_mode)
		__field(int, for_kupdate)
		__field(int, range_cyclic)
		__field(int, for_background)
	),
	TP_fast_assign(
		strncpy(__entry->name, dev_name(bdi->dev), 32);
		__entry->nr_pages = work->nr_pages;
		__entry->sb_dev = work->sb ? work->sb->s_dev : 0;
		__entry->sync_mode = work->sync_mode;
		__entry->for_kupdate = work->for_kupdate;
		__entry->range_cyclic = work->range_cyclic;
		__entry->for_background	= work->for_background;
	),
	TP_printk("bdi %s: sb_dev %d:%d nr_pages=%ld sync_mode=%d "
		  "kupdate=%d range_cyclic=%d background=%d",
		  __entry->name,
		  MAJOR(__entry->sb_dev), MINOR(__entry->sb_dev),
		  __entry->nr_pages,
		  __entry->sync_mode,
		  __entry->for_kupdate,
		  __entry->range_cyclic,
		  __entry->for_background
	)
);
#define DEFINE_WRITEBACK_WORK_EVENT(name) \
DEFINE_EVENT(writeback_work_class, name, \
	TP_PROTO(struct backing_dev_info *bdi, struct wb_writeback_work *work), \
	TP_ARGS(bdi, work))
DEFINE_WRITEBACK_WORK_EVENT(writeback_nothread);
DEFINE_WRITEBACK_WORK_EVENT(writeback_queue);
DEFINE_WRITEBACK_WORK_EVENT(writeback_exec);

TRACE_EVENT(writeback_pages_written,
	TP_PROTO(long pages_written),
	TP_ARGS(pages_written),
	TP_STRUCT__entry(
		__field(long,		pages)
	),
	TP_fast_assign(
		__entry->pages		= pages_written;
	),
	TP_printk("%ld", __entry->pages)
);

DECLARE_EVENT_CLASS(writeback_class,
	TP_PROTO(struct backing_dev_info *bdi),
	TP_ARGS(bdi),
	TP_STRUCT__entry(
		__array(char, name, 32)
	),
	TP_fast_assign(
		strncpy(__entry->name, dev_name(bdi->dev), 32);
	),
	TP_printk("bdi %s",
		  __entry->name
	)
);
#define DEFINE_WRITEBACK_EVENT(name) \
DEFINE_EVENT(writeback_class, name, \
	TP_PROTO(struct backing_dev_info *bdi), \
	TP_ARGS(bdi))

DEFINE_WRITEBACK_EVENT(writeback_nowork);
DEFINE_WRITEBACK_EVENT(writeback_bdi_register);
DEFINE_WRITEBACK_EVENT(writeback_bdi_unregister);
DEFINE_WRITEBACK_EVENT(writeback_task_start);
DEFINE_WRITEBACK_EVENT(writeback_task_stop);

DECLARE_EVENT_CLASS(wbc_class,
	TP_PROTO(struct writeback_control *wbc, struct backing_dev_info *bdi),
	TP_ARGS(wbc, bdi),
	TP_STRUCT__entry(
		__array(char, name, 32)
		__field(long, nr_to_write)
		__field(long, pages_skipped)
		__field(int, sync_mode)
		__field(int, nonblocking)
		__field(int, encountered_congestion)
		__field(int, for_kupdate)
		__field(int, for_background)
		__field(int, for_reclaim)
		__field(int, range_cyclic)
		__field(int, more_io)
		__field(unsigned long, older_than_this)
		__field(long, range_start)
		__field(long, range_end)
	),

	TP_fast_assign(
		strncpy(__entry->name, dev_name(bdi->dev), 32);
		__entry->nr_to_write	= wbc->nr_to_write;
		__entry->pages_skipped	= wbc->pages_skipped;
		__entry->sync_mode	= wbc->sync_mode;
		__entry->for_kupdate	= wbc->for_kupdate;
		__entry->for_background	= wbc->for_background;
		__entry->for_reclaim	= wbc->for_reclaim;
		__entry->range_cyclic	= wbc->range_cyclic;
		__entry->more_io	= wbc->more_io;
		__entry->older_than_this = wbc->older_than_this ?
						*wbc->older_than_this : 0;
		__entry->range_start	= (long)wbc->range_start;
		__entry->range_end	= (long)wbc->range_end;
	),

	TP_printk("bdi %s: towrt=%ld skip=%ld mode=%d kupd=%d "
		"bgrd=%d reclm=%d cyclic=%d more=%d older=0x%lx "
		"start=0x%lx end=0x%lx",
		__entry->name,
		__entry->nr_to_write,
		__entry->pages_skipped,
		__entry->sync_mode,
		__entry->for_kupdate,
		__entry->for_background,
		__entry->for_reclaim,
		__entry->range_cyclic,
		__entry->more_io,
		__entry->older_than_this,
		__entry->range_start,
		__entry->range_end)
)

#define DEFINE_WBC_EVENT(name) \
DEFINE_EVENT(wbc_class, name, \
	TP_PROTO(struct writeback_control *wbc, struct backing_dev_info *bdi), \
	TP_ARGS(wbc, bdi))
DEFINE_WBC_EVENT(wbc_writeback_start);
DEFINE_WBC_EVENT(wbc_writeback_written);
DEFINE_WBC_EVENT(wbc_writeback_wait);
DEFINE_WBC_EVENT(wbc_writepage);

#define KBps(x)			((x) << (PAGE_SHIFT - 10))

TRACE_EVENT(blkcg_dirty_ratelimit,

	TP_PROTO(unsigned long bps,
		 unsigned long dirty_rate,
		 unsigned long dirty_ratelimit,
		 unsigned long balanced_dirty_ratelimit),

	TP_ARGS(bps, dirty_rate, dirty_ratelimit, balanced_dirty_ratelimit),

	TP_STRUCT__entry(
		__field(unsigned long,	kbps)
		__field(unsigned long,	dirty_rate)
		__field(unsigned long,	dirty_ratelimit)
		__field(unsigned long,	balanced_dirty_ratelimit)
	),

	TP_fast_assign(
		__entry->kbps = bps >> 10;
		__entry->dirty_rate = KBps(dirty_rate);
		__entry->dirty_ratelimit = KBps(dirty_ratelimit);
		__entry->balanced_dirty_ratelimit =
					  KBps(balanced_dirty_ratelimit);
	),

	TP_printk("kbps=%lu dirty_rate=%lu "
		  "dirty_ratelimit=%lu "
		  "balanced_dirty_ratelimit=%lu",
		  __entry->kbps,
		  __entry->dirty_rate,
		  __entry->dirty_ratelimit,
		  __entry->balanced_dirty_ratelimit
	)
);

TRACE_EVENT(bdi_dirty_ratelimit,

	TP_PROTO(struct backing_dev_info *bdi,
		 unsigned long dirty_rate,
		 unsigned long task_ratelimit),

	TP_ARGS(bdi, dirty_rate, task_ratelimit),

	TP_STRUCT__entry(
		__array(char,		bdi, 32)
		__field(unsigned long,	write_bw)
		__field(unsigned long,	avg_write_bw)
		__field(unsigned long,	dirty_rate)
		__field(unsigned long,	dirty_ratelimit)
		__field(unsigned long,	task_ratelimit)
		__field(unsigned long,	balanced_dirty_ratelimit)
	),

	TP_fast_assign(
		strlcpy(__entry->bdi, dev_name(bdi->dev), 32);
		__entry->write_bw	= KBps(bdi->write_bandwidth);
		__entry->avg_write_bw	= KBps(bdi->avg_write_bandwidth);
		__entry->dirty_rate	= KBps(dirty_rate);
		__entry->dirty_ratelimit = KBps(bdi->dirty_ratelimit);
		__entry->task_ratelimit	= KBps(task_ratelimit);
		__entry->balanced_dirty_ratelimit =
					  KBps(bdi->balanced_dirty_ratelimit);
	),

	TP_printk("bdi %s: "
		  "write_bw=%lu awrite_bw=%lu dirty_rate=%lu "
		  "dirty_ratelimit=%lu task_ratelimit=%lu "
		  "balanced_dirty_ratelimit=%lu",
		  __entry->bdi,
		  __entry->write_bw,		/* write bandwidth */
		  __entry->avg_write_bw,	/* avg write bandwidth */
		  __entry->dirty_rate,		/* bdi dirty rate */
		  __entry->dirty_ratelimit,	/* base ratelimit */
		  __entry->task_ratelimit, /* ratelimit with position control */
		  __entry->balanced_dirty_ratelimit /* the balanced ratelimit */
	)
);

TRACE_EVENT(balance_dirty_pages,

	TP_PROTO(struct backing_dev_info *bdi,
		 unsigned long thresh,
		 unsigned long bg_thresh,
		 unsigned long dirty,
		 unsigned long bdi_thresh,
		 unsigned long bdi_dirty,
		 unsigned long dirty_ratelimit,
		 unsigned long task_ratelimit,
		 unsigned long dirtied,
		 unsigned long period,
		 long pause,
		 unsigned long start_time),

	TP_ARGS(bdi, thresh, bg_thresh, dirty, bdi_thresh, bdi_dirty,
		dirty_ratelimit, task_ratelimit,
		dirtied, period, pause, start_time),

	TP_STRUCT__entry(
		__array(	 char,	bdi, 32)
		__field(unsigned long,	limit)
		__field(unsigned long,	setpoint)
		__field(unsigned long,	dirty)
		__field(unsigned long,	bdi_setpoint)
		__field(unsigned long,	bdi_dirty)
		__field(unsigned long,	dirty_ratelimit)
		__field(unsigned long,	task_ratelimit)
		__field(unsigned int,	dirtied)
		__field(unsigned int,	dirtied_pause)
		__field(unsigned long,	paused)
		__field(	 long,	pause)
		__field(unsigned long,	period)
		__field(	 long,	think)
	),

	TP_fast_assign(
		unsigned long freerun = (thresh + bg_thresh) / 2;
		strlcpy(__entry->bdi, dev_name(bdi->dev), 32);

		__entry->limit		= global_dirty_limit;
		__entry->setpoint	= (global_dirty_limit + freerun) / 2;
		__entry->dirty		= dirty;
		__entry->bdi_setpoint	= __entry->setpoint *
						bdi_thresh / (thresh + 1);
		__entry->bdi_dirty	= bdi_dirty;
		__entry->dirty_ratelimit = KBps(dirty_ratelimit);
		__entry->task_ratelimit	= KBps(task_ratelimit);
		__entry->dirtied	= dirtied;
		__entry->dirtied_pause	= current->nr_dirtied_pause;
		__entry->think		= current->dirty_paused_when == 0 ? 0 :
			 (long)(jiffies - current->dirty_paused_when) * 1000/HZ;
		__entry->period		= period * 1000 / HZ;
		__entry->pause		= pause * 1000 / HZ;
		__entry->paused		= (jiffies - start_time) * 1000 / HZ;
	),


	TP_printk("bdi %s: "
		  "limit=%lu setpoint=%lu dirty=%lu "
		  "bdi_setpoint=%lu bdi_dirty=%lu "
		  "dirty_ratelimit=%lu task_ratelimit=%lu "
		  "dirtied=%u dirtied_pause=%u "
		  "paused=%lu pause=%ld period=%lu think=%ld",
		  __entry->bdi,
		  __entry->limit,
		  __entry->setpoint,
		  __entry->dirty,
		  __entry->bdi_setpoint,
		  __entry->bdi_dirty,
		  __entry->dirty_ratelimit,
		  __entry->task_ratelimit,
		  __entry->dirtied,
		  __entry->dirtied_pause,
		  __entry->paused,	/* ms */
		  __entry->pause,	/* ms */
		  __entry->period,	/* ms */
		  __entry->think	/* ms */
	  )
);

#endif /* _TRACE_WRITEBACK_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
