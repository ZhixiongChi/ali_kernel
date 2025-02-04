/*
 * cgroup_freezer.c -  control group freezer subsystem
 *
 * Copyright IBM Corporation, 2007
 *
 * Author : Cedric Le Goater <clg@fr.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/module.h>
#include <linux/cgroup.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/freezer.h>
#include <linux/seq_file.h>

enum freezer_state {
	CGROUP_THAWED = 0,
	CGROUP_FREEZING,
	CGROUP_FROZEN,
};

struct freezer {
	struct cgroup_subsys_state css;
	enum freezer_state state;
	spinlock_t lock; /* protects _writes_ to state */
};

static inline struct freezer *cgroup_freezer(
		struct cgroup *cgroup)
{
	return container_of(
		cgroup_subsys_state(cgroup, freezer_subsys_id),
		struct freezer, css);
}

static inline struct freezer *task_freezer(struct task_struct *task)
{
	return container_of(task_subsys_state(task, freezer_subsys_id),
			    struct freezer, css);
}

static inline int __cgroup_freezing_or_frozen(struct task_struct *task)
{
	enum freezer_state state = task_freezer(task)->state;
	return (state == CGROUP_FREEZING) || (state == CGROUP_FROZEN);
}

int cgroup_freezing_or_frozen(struct task_struct *task)
{
	int result;
	task_lock(task);
	result = __cgroup_freezing_or_frozen(task);
	task_unlock(task);
	return result;
}

/*
 * cgroups_write_string() limits the size of freezer state strings to
 * CGROUP_LOCAL_BUFFER_SIZE
 */
static const char *freezer_state_strs[] = {
	"THAWED",
	"FREEZING",
	"FROZEN",
};

/*
 * State diagram
 * Transitions are caused by userspace writes to the freezer.state file.
 * The values in parenthesis are state labels. The rest are edge labels.
 *
 * (THAWED) --FROZEN--> (FREEZING) --FROZEN--> (FROZEN)
 *    ^ ^                    |                     |
 *    | \_______THAWED_______/                     |
 *    \__________________________THAWED____________/
 */

struct cgroup_subsys freezer_subsys;

/* Locks taken and their ordering
 * ------------------------------
 * cgroup_mutex (AKA cgroup_lock)
 * freezer->lock
 * css_set_lock
 * task->alloc_lock (AKA task_lock)
 * task->sighand->siglock
 *
 * cgroup code forces css_set_lock to be taken before task->alloc_lock
 *
 * freezer_create(), freezer_destroy():
 * cgroup_mutex [ by cgroup core ]
 *
 * freezer_can_attach():
 * cgroup_mutex (held by caller of can_attach)
 *
 * cgroup_freezing_or_frozen():
 * task->alloc_lock (to get task's cgroup)
 *
 * freezer_fork() (preserving fork() performance means can't take cgroup_mutex):
 * freezer->lock
 *  sighand->siglock (if the cgroup is freezing)
 *
 * freezer_read():
 * cgroup_mutex
 *  freezer->lock
 *   write_lock css_set_lock (cgroup iterator start)
 *    task->alloc_lock
 *   read_lock css_set_lock (cgroup iterator start)
 *
 * freezer_write() (freeze):
 * cgroup_mutex
 *  freezer->lock
 *   write_lock css_set_lock (cgroup iterator start)
 *    task->alloc_lock
 *   read_lock css_set_lock (cgroup iterator start)
 *    sighand->siglock (fake signal delivery inside freeze_task())
 *
 * freezer_write() (unfreeze):
 * cgroup_mutex
 *  freezer->lock
 *   write_lock css_set_lock (cgroup iterator start)
 *    task->alloc_lock
 *   read_lock css_set_lock (cgroup iterator start)
 *    task->alloc_lock (inside thaw_process(), prevents race with refrigerator())
 *     sighand->siglock
 */
static struct cgroup_subsys_state *freezer_create(struct cgroup_subsys *ss,
						  struct cgroup *cgroup)
{
	struct freezer *freezer;

	freezer = kzalloc(sizeof(struct freezer), GFP_KERNEL);
	if (!freezer)
		return ERR_PTR(-ENOMEM);

	spin_lock_init(&freezer->lock);
	freezer->state = CGROUP_THAWED;
	return &freezer->css;
}

static void freezer_destroy(struct cgroup_subsys *ss,
			    struct cgroup *cgroup)
{
	kfree(cgroup_freezer(cgroup));
}

/* task is frozen or will freeze immediately when next it gets woken */
static bool is_task_frozen_enough(struct task_struct *task)
{
	return frozen(task) ||
		(task_is_stopped_or_traced(task) && freezing(task));
}

/*
 * The call to cgroup_lock() in the freezer.state write method prevents
 * a write to that file racing against an attach, and hence the
 * can_attach() result will remain valid until the attach completes.
 */
static int freezer_can_attach(struct cgroup_subsys *ss,
			      struct cgroup *new_cgroup,
			      struct task_struct *task)
{
	struct freezer *freezer;

	/*
	 * Anything frozen can't move or be moved to/from.
	 */

	freezer = cgroup_freezer(new_cgroup);
	if (freezer->state != CGROUP_THAWED)
		return -EBUSY;

	return 0;
}

static int freezer_can_attach_task(struct cgroup *cgrp, struct task_struct *tsk)
{
	rcu_read_lock();
	if (__cgroup_freezing_or_frozen(tsk)) {
		rcu_read_unlock();
		return -EBUSY;
	}
	rcu_read_unlock();
	return 0;
}

static int freezer_fork(struct cgroup_subsys *ss, struct task_struct *task)
{
	struct freezer *freezer;

	/*
	 * No lock is needed, since the task isn't on tasklist yet,
	 * so it can't be moved to another cgroup, which means the
	 * freezer won't be removed and will be valid during this
	 * function call.
	 */
	freezer = task_freezer(task);

	/*
	 * The root cgroup is non-freezable, so we can skip the
	 * following check.
	 */
	if (!freezer->css.cgroup->parent)
		return 0;

	spin_lock_irq(&freezer->lock);
	BUG_ON(freezer->state == CGROUP_FROZEN);

	/* Locking avoids race with FREEZING -> THAWED transitions. */
	if (freezer->state == CGROUP_FREEZING)
		freeze_task(task, true);
	spin_unlock_irq(&freezer->lock);

	return 0;
}

/*
 * caller must hold freezer->lock
 */
static void update_if_frozen(struct cgroup *cgroup,
				 struct freezer *freezer)
{
	struct cgroup_iter it;
	struct task_struct *task;
	unsigned int nfrozen = 0, ntotal = 0;
	enum freezer_state old_state = freezer->state;

	cgroup_iter_start(cgroup, &it);
	while ((task = cgroup_iter_next(cgroup, &it))) {
		ntotal++;
		if (is_task_frozen_enough(task))
			nfrozen++;
	}

	if (old_state == CGROUP_THAWED) {
		BUG_ON(nfrozen > 0);
	} else if (old_state == CGROUP_FREEZING) {
		if (nfrozen == ntotal)
			freezer->state = CGROUP_FROZEN;
	} else { /* old_state == CGROUP_FROZEN */
		BUG_ON(nfrozen != ntotal);
	}

	cgroup_iter_end(cgroup, &it);
}

static int freezer_read(struct cgroup *cgroup, struct cftype *cft,
			struct seq_file *m)
{
	struct freezer *freezer;
	enum freezer_state state;

	if (!cgroup_lock_live_group(cgroup))
		return -ENODEV;

	freezer = cgroup_freezer(cgroup);
	spin_lock_irq(&freezer->lock);
	state = freezer->state;
	if (state == CGROUP_FREEZING) {
		/* We change from FREEZING to FROZEN lazily if the cgroup was
		 * only partially frozen when we exitted write. */
		update_if_frozen(cgroup, freezer);
		state = freezer->state;
	}
	spin_unlock_irq(&freezer->lock);
	cgroup_unlock();

	seq_puts(m, freezer_state_strs[state]);
	seq_putc(m, '\n');
	return 0;
}

static int try_to_freeze_cgroup(struct cgroup *cgroup, struct freezer *freezer)
{
	struct cgroup_iter it;
	struct task_struct *task;
	unsigned int num_cant_freeze_now = 0;

	freezer->state = CGROUP_FREEZING;
	cgroup_iter_start(cgroup, &it);
	while ((task = cgroup_iter_next(cgroup, &it))) {
		if (!freeze_task(task, true))
			continue;
		if (is_task_frozen_enough(task))
			continue;
		if (!freezing(task) && !freezer_should_skip(task))
			num_cant_freeze_now++;
	}
	cgroup_iter_end(cgroup, &it);

	return num_cant_freeze_now ? -EBUSY : 0;
}

static void unfreeze_cgroup(struct cgroup *cgroup, struct freezer *freezer)
{
	struct cgroup_iter it;
	struct task_struct *task;

	cgroup_iter_start(cgroup, &it);
	while ((task = cgroup_iter_next(cgroup, &it))) {
		thaw_process(task);
	}
	cgroup_iter_end(cgroup, &it);

	freezer->state = CGROUP_THAWED;
}

static int freezer_change_state(struct cgroup *cgroup,
				enum freezer_state goal_state)
{
	struct freezer *freezer;
	int retval = 0;

	freezer = cgroup_freezer(cgroup);

	spin_lock_irq(&freezer->lock);

	update_if_frozen(cgroup, freezer);
	if (goal_state == freezer->state)
		goto out;

	switch (goal_state) {
	case CGROUP_THAWED:
		unfreeze_cgroup(cgroup, freezer);
		break;
	case CGROUP_FROZEN:
		retval = try_to_freeze_cgroup(cgroup, freezer);
		break;
	default:
		BUG();
	}
out:
	spin_unlock_irq(&freezer->lock);

	return retval;
}

static int freezer_write(struct cgroup *cgroup,
			 struct cftype *cft,
			 const char *buffer)
{
	int retval;
	enum freezer_state goal_state;

	if (strcmp(buffer, freezer_state_strs[CGROUP_THAWED]) == 0)
		goal_state = CGROUP_THAWED;
	else if (strcmp(buffer, freezer_state_strs[CGROUP_FROZEN]) == 0)
		goal_state = CGROUP_FROZEN;
	else
		return -EINVAL;

	if (!cgroup_lock_live_group(cgroup))
		return -ENODEV;
	retval = freezer_change_state(cgroup, goal_state);
	cgroup_unlock();
	return retval;
}

static struct cftype files[] = {
	{
		.name = "state",
		.read_seq_string = freezer_read,
		.write_string = freezer_write,
	},
};

static int freezer_populate(struct cgroup_subsys *ss, struct cgroup *cgroup)
{
	if (!cgroup->parent)
		return 0;
	return cgroup_add_files(cgroup, ss, files, ARRAY_SIZE(files));
}

struct cgroup_subsys freezer_subsys = {
	.name		= "freezer",
	.create		= freezer_create,
	.destroy	= freezer_destroy,
	.populate	= freezer_populate,
	.subsys_id	= freezer_subsys_id,
	.can_attach	= freezer_can_attach,
	.can_attach_task = freezer_can_attach_task,
	.pre_attach	= NULL,
	.attach_task	= NULL,
	.attach		= NULL,
	.fork		= freezer_fork,
	.exit		= NULL,
};
