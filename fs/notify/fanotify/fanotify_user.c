#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/anon_inodes.h>
#include <linux/fsnotify_backend.h>
#include <linux/init.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/poll.h>
#include <linux/security.h>
#include <linux/syscalls.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include <asm/ioctls.h>

#include "fanotify.h"

static struct kmem_cache *fanotify_mark_cache __read_mostly;

/*
 * Get an fsnotify notification event if one exists and is small
 * enough to fit in "count". Return an error pointer if the count
 * is not large enough.
 *
 * Called with the group->notification_mutex held.
 */
static struct fsnotify_event *get_one_event(struct fsnotify_group *group,
					    size_t count)
{
	BUG_ON(!mutex_is_locked(&group->notification_mutex));

	pr_debug("%s: group=%p count=%zd\n", __func__, group, count);

	if (fsnotify_notify_queue_is_empty(group))
		return NULL;

	if (FAN_EVENT_METADATA_LEN > count)
		return ERR_PTR(-EINVAL);

	/* held the notification_mutex the whole time, so this is the
	 * same event we peeked above */
	return fsnotify_remove_notify_event(group);
}

static int create_fd(struct fsnotify_group *group, struct fsnotify_event *event)
{
	int client_fd;
	struct dentry *dentry;
	struct vfsmount *mnt;
	struct file *new_file;

	pr_debug("%s: group=%p event=%p\n", __func__, group, event);

	client_fd = get_unused_fd();
	if (client_fd < 0)
		return client_fd;

	if (event->data_type != FSNOTIFY_EVENT_PATH) {
		WARN_ON(1);
		put_unused_fd(client_fd);
		return -EINVAL;
	}

	/*
	 * we need a new file handle for the userspace program so it can read even if it was
	 * originally opened O_WRONLY.
	 */
	dentry = dget(event->path.dentry);
	mnt = mntget(event->path.mnt);
	/* it's possible this event was an overflow event.  in that case dentry and mnt
	 * are NULL;  That's fine, just don't call dentry open */
	if (dentry && mnt)
		new_file = dentry_open(dentry, mnt,
				       O_RDONLY | O_LARGEFILE | FMODE_NONOTIFY,
				       current_cred());
	else
		new_file = ERR_PTR(-EOVERFLOW);
	if (IS_ERR(new_file)) {
		/*
		 * we still send an event even if we can't open the file.  this
		 * can happen when say tasks are gone and we try to open their
		 * /proc files or we try to open a WRONLY file like in sysfs
		 * we just send the errno to userspace since there isn't much
		 * else we can do.
		 */
		put_unused_fd(client_fd);
		client_fd = PTR_ERR(new_file);
	} else {
		fd_install(client_fd, new_file);
	}

	return client_fd;
}

static ssize_t fill_event_metadata(struct fsnotify_group *group,
				   struct fanotify_event_metadata *metadata,
				   struct fsnotify_event *event)
{
	pr_debug("%s: group=%p metadata=%p event=%p\n", __func__,
		 group, metadata, event);

	metadata->event_len = FAN_EVENT_METADATA_LEN;
	metadata->vers = FANOTIFY_METADATA_VERSION;
	metadata->mask = fanotify_outgoing_mask(event->mask);
	metadata->pid = pid_vnr(event->tgid);
	metadata->fd = create_fd(group, event);

	return metadata->fd;
}

static ssize_t copy_event_to_user(struct fsnotify_group *group,
				  struct fsnotify_event *event,
				  char __user *buf)
{
	struct fanotify_event_metadata fanotify_event_metadata;
	int ret;

	pr_debug("%s: group=%p event=%p\n", __func__, group, event);

	ret = fill_event_metadata(group, &fanotify_event_metadata, event);
	if (ret < 0)
		return ret;

	if (copy_to_user(buf, &fanotify_event_metadata, FAN_EVENT_METADATA_LEN))
		return -EFAULT;

	return FAN_EVENT_METADATA_LEN;
}

/* intofiy userspace file descriptor functions */
static unsigned int fanotify_poll(struct file *file, poll_table *wait)
{
	struct fsnotify_group *group = file->private_data;
	int ret = 0;

	poll_wait(file, &group->notification_waitq, wait);
	mutex_lock(&group->notification_mutex);
	if (!fsnotify_notify_queue_is_empty(group))
		ret = POLLIN | POLLRDNORM;
	mutex_unlock(&group->notification_mutex);

	return ret;
}

static ssize_t fanotify_read(struct file *file, char __user *buf,
			     size_t count, loff_t *pos)
{
	struct fsnotify_group *group;
	struct fsnotify_event *kevent;
	char __user *start;
	int ret;
	DEFINE_WAIT(wait);

	start = buf;
	group = file->private_data;

	pr_debug("%s: group=%p\n", __func__, group);

	while (1) {
		prepare_to_wait(&group->notification_waitq, &wait, TASK_INTERRUPTIBLE);

		mutex_lock(&group->notification_mutex);
		kevent = get_one_event(group, count);
		mutex_unlock(&group->notification_mutex);

		if (kevent) {
			ret = PTR_ERR(kevent);
			if (IS_ERR(kevent))
				break;
			ret = copy_event_to_user(group, kevent, buf);
			fsnotify_put_event(kevent);
			if (ret < 0)
				break;
			buf += ret;
			count -= ret;
			continue;
		}

		ret = -EAGAIN;
		if (file->f_flags & O_NONBLOCK)
			break;
		ret = -EINTR;
		if (signal_pending(current))
			break;

		if (start != buf)
			break;

		schedule();
	}

	finish_wait(&group->notification_waitq, &wait);
	if (start != buf && ret != -EFAULT)
		ret = buf - start;
	return ret;
}

static int fanotify_release(struct inode *ignored, struct file *file)
{
	struct fsnotify_group *group = file->private_data;

	pr_debug("%s: file=%p group=%p\n", __func__, file, group);

	/* matches the fanotify_init->fsnotify_alloc_group */
	fsnotify_put_group(group);

	return 0;
}

static long fanotify_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct fsnotify_group *group;
	struct fsnotify_event_holder *holder;
	void __user *p;
	int ret = -ENOTTY;
	size_t send_len = 0;

	group = file->private_data;

	p = (void __user *) arg;

	switch (cmd) {
	case FIONREAD:
		mutex_lock(&group->notification_mutex);
		list_for_each_entry(holder, &group->notification_list, event_list)
			send_len += FAN_EVENT_METADATA_LEN;
		mutex_unlock(&group->notification_mutex);
		ret = put_user(send_len, (int __user *) p);
		break;
	}

	return ret;
}

static const struct file_operations fanotify_fops = {
	.poll		= fanotify_poll,
	.read		= fanotify_read,
	.fasync		= NULL,
	.release	= fanotify_release,
	.unlocked_ioctl	= fanotify_ioctl,
	.compat_ioctl	= fanotify_ioctl,
};

static void fanotify_free_mark(struct fsnotify_mark *fsn_mark)
{
	kmem_cache_free(fanotify_mark_cache, fsn_mark);
}

static int fanotify_find_path(int dfd, const char __user *filename,
			      struct path *path, unsigned int flags)
{
	int ret;

	pr_debug("%s: dfd=%d filename=%p flags=%x\n", __func__,
		 dfd, filename, flags);

	if (filename == NULL) {
		struct file *file;
		int fput_needed;

		ret = -EBADF;
		file = fget_light(dfd, &fput_needed);
		if (!file)
			goto out;

		ret = -ENOTDIR;
		if ((flags & FAN_MARK_ONLYDIR) &&
		    !(S_ISDIR(file->f_path.dentry->d_inode->i_mode))) {
			fput_light(file, fput_needed);
			goto out;
		}

		*path = file->f_path;
		path_get(path);
		fput_light(file, fput_needed);
	} else {
		unsigned int lookup_flags = 0;

		if (!(flags & FAN_MARK_DONT_FOLLOW))
			lookup_flags |= LOOKUP_FOLLOW;
		if (flags & FAN_MARK_ONLYDIR)
			lookup_flags |= LOOKUP_DIRECTORY;

		ret = user_path_at(dfd, filename, lookup_flags, path);
		if (ret)
			goto out;
	}

	/* you can only watch an inode if you have read permissions on it */
	ret = inode_permission(path->dentry->d_inode, MAY_READ);
	if (ret)
		path_put(path);
out:
	return ret;
}

static void fanotify_update_object_mask(struct fsnotify_group *group,
					struct inode *inode,
					struct vfsmount *mnt,
					struct fsnotify_mark *fsn_mark,
					unsigned int flags,
					__u32 mask)
{
	__u32 old_mask, new_mask;

	pr_debug("%s: group=%p inode=%p mnt=%p fsn_mark=%p flags=%x mask=%x\n",
		 __func__, group, inode, mnt, fsn_mark, flags, mask);

	spin_lock(&fsn_mark->lock);
	old_mask = fsn_mark->mask;
	if (flags & FAN_MARK_ADD)
		fsn_mark->mask |= mask;
	else if (flags & FAN_MARK_REMOVE)
		fsn_mark->mask &= ~mask;
	else
		BUG();
	new_mask = fsn_mark->mask;
	spin_unlock(&fsn_mark->lock);

	if (!new_mask)
		fsnotify_destroy_mark(fsn_mark);

	/* we made changes to a mask, update the group mask and the object mask
	 * so things happen quickly. */
	if (old_mask != new_mask) {
		__u32 dropped, do_object, do_group;

		/* more bits in old than in new? */
		dropped = (old_mask & ~new_mask);
		/* more bits in this fsn_mark than the group? */
		do_group = (new_mask & ~group->mask);

		if (inode) {
			/* more bits in this fsn_mark than the object's mask? */
			do_object = (new_mask & ~inode->i_fsnotify_mask);
			/* update the object with this new fsn_mark */
			if (dropped || do_object)
				fsnotify_recalc_inode_mask(inode);
		} else if (mnt) {
			/* more bits in this fsn_mark than the object's mask? */
			do_object = (new_mask & ~mnt->mnt_fsnotify_mask);
			/* update the object with this new fsn_mark */
			if (dropped || do_object)
				fsnotify_recalc_vfsmount_mask(mnt);
		} else {
			BUG();
		}

		/* update the group mask with the new mask */
		if (dropped || do_group)
			fsnotify_recalc_group_mask(group);
	}
}

static __u32 fanotify_mark_remove_from_mask(struct fsnotify_mark *fsn_mark, __u32 mask)
{
	__u32 oldmask;

	spin_lock(&fsn_mark->lock);
	oldmask = fsn_mark->mask;
	fsn_mark->mask = oldmask & ~mask;
	spin_unlock(&fsn_mark->lock);

	if (!(oldmask & ~mask))
		fsnotify_destroy_mark(fsn_mark);

	return mask & oldmask;
}

static int fanotify_remove_mark(struct fsnotify_group *group, struct inode *inode,
				struct vfsmount *mnt, __u32 mask)
{
	struct fsnotify_mark *fsn_mark = NULL;
	__u32 removed;

	BUG_ON(inode && mnt);
	BUG_ON(!inode && !mnt);

	if (inode)
		fsn_mark = fsnotify_find_inode_mark(group, inode);
	else if (mnt)
		fsn_mark = fsnotify_find_vfsmount_mark(group, mnt);
	else
		BUG();

	if (!fsn_mark)
		return -ENOENT;

	removed = fanotify_mark_remove_from_mask(fsn_mark, mask);
	/* matches the fsnotify_find_inode_mark() */
	fsnotify_put_mark(fsn_mark);

	if (removed & group->mask)
		fsnotify_recalc_group_mask(group);
	if (inode) {
		if (removed & inode->i_fsnotify_mask)
			fsnotify_recalc_inode_mask(inode);
	} else if (mnt) {
		if (removed & mnt->mnt_fsnotify_mask)
			fsnotify_recalc_vfsmount_mask(mnt);
	}

	return 0;
}

static struct fsnotify_mark *fanotify_add_vfsmount_mark(struct fsnotify_group *group,
							struct vfsmount *mnt)
{
	struct fsnotify_mark *fsn_mark;

	fsn_mark = fsnotify_find_vfsmount_mark(group, mnt);
	if (!fsn_mark) {
		struct fsnotify_mark *new_fsn_mark;
		int ret;

		fsn_mark = ERR_PTR(-ENOMEM);
		new_fsn_mark = kmem_cache_alloc(fanotify_mark_cache, GFP_KERNEL);
		if (!new_fsn_mark)
			goto out;

		fsnotify_init_mark(new_fsn_mark, fanotify_free_mark);
		ret = fsnotify_add_mark(new_fsn_mark, group, NULL, mnt, 0);
		if (ret) {
			fsn_mark = ERR_PTR(ret);
			fanotify_free_mark(new_fsn_mark);
			goto out;
		}

		fsn_mark = new_fsn_mark;
	}
out:
	return fsn_mark;
}

static struct fsnotify_mark *fanotify_add_inode_mark(struct fsnotify_group *group,
						     struct inode *inode)
{
	struct fsnotify_mark *fsn_mark;

	pr_debug("%s: group=%p inode=%p\n", __func__, group, inode);

	fsn_mark = fsnotify_find_inode_mark(group, inode);
	if (!fsn_mark) {
		struct fsnotify_mark *new_fsn_mark;
		int ret;

		fsn_mark = ERR_PTR(-ENOMEM);
		new_fsn_mark = kmem_cache_alloc(fanotify_mark_cache, GFP_KERNEL);
		if (!new_fsn_mark)
			goto out;

		fsnotify_init_mark(new_fsn_mark, fanotify_free_mark);
		ret = fsnotify_add_mark(new_fsn_mark, group, inode, NULL, 0);
		if (ret) {
			fsn_mark = ERR_PTR(ret);
			fanotify_free_mark(new_fsn_mark);
			goto out;
		}

		fsn_mark = new_fsn_mark;
	}
out:
	return fsn_mark;
}

static int fanotify_add_mark(struct fsnotify_group *group, struct inode *inode,
			     struct vfsmount *mnt, unsigned int flags, __u32 mask)
{
	struct fsnotify_mark *fsn_mark;

	pr_debug("%s: group=%p inode=%p mnt=%p flags=%x mask=%x\n",
		 __func__, group, inode, mnt, flags, mask);

	BUG_ON(inode && mnt);
	BUG_ON(!inode && !mnt);

	if (inode)
		fsn_mark = fanotify_add_inode_mark(group, inode);
	else if (mnt)
		fsn_mark = fanotify_add_vfsmount_mark(group, mnt);
	else
		BUG();

	if (IS_ERR(fsn_mark))
		goto out;

	fanotify_update_object_mask(group, inode, mnt, fsn_mark, flags, mask);

	/* match the init or the find.... */
	fsnotify_put_mark(fsn_mark);
out:
	return PTR_ERR(fsn_mark);
}

static bool fanotify_mark_validate_input(int flags,
					 __u32 mask)
{
	pr_debug("%s: flags=%x mask=%x\n", __func__, flags, mask);

	/* are flags valid of this operation? */
	if (!fanotify_mark_flags_valid(flags))
		return false;
	/* is the mask valid? */
	if (!fanotify_mask_valid(mask))
		return false;
	return true;
}

/* fanotify syscalls */
SYSCALL_DEFINE3(fanotify_init, unsigned int, flags, unsigned int, event_f_flags,
		unsigned int, priority)
{
	struct fsnotify_group *group;
	int f_flags, fd;

	pr_debug("%s: flags=%d event_f_flags=%d priority=%d\n",
		__func__, flags, event_f_flags, priority);

	if (event_f_flags)
		return -EINVAL;
	if (priority)
		return -EINVAL;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (flags & ~FAN_ALL_INIT_FLAGS)
		return -EINVAL;

	f_flags = (O_RDONLY | FMODE_NONOTIFY);
	if (flags & FAN_CLOEXEC)
		f_flags |= O_CLOEXEC;
	if (flags & FAN_NONBLOCK)
		f_flags |= O_NONBLOCK;

	/* fsnotify_alloc_group takes a ref.  Dropped in fanotify_release */
	group = fsnotify_alloc_group(&fanotify_fsnotify_ops);
	if (IS_ERR(group))
		return PTR_ERR(group);

	fd = anon_inode_getfd("[fanotify]", &fanotify_fops, group, f_flags);
	if (fd < 0)
		goto out_put_group;

	return fd;

out_put_group:
	fsnotify_put_group(group);
	return fd;
}

SYSCALL_DEFINE(fanotify_mark)(int fanotify_fd, unsigned int flags,
			      __u64 mask, int dfd,
			      const char  __user * pathname)
{
	struct inode *inode;
	struct fsnotify_group *group;
	struct file *filp;
	struct path path;
	int ret, fput_needed;

	pr_debug("%s: fanotify_fd=%d flags=%x dfd=%d pathname=%p mask=%llx\n",
		 __func__, fanotify_fd, flags, dfd, pathname, mask);

	/* we only use the lower 32 bits as of right now. */
	if (mask & ((__u64)0xffffffff << 32))
		return -EINVAL;

	if (!fanotify_mark_validate_input(flags, mask))
		return -EINVAL;

	filp = fget_light(fanotify_fd, &fput_needed);
	if (unlikely(!filp))
		return -EBADF;

	/* verify that this is indeed an fanotify instance */
	ret = -EINVAL;
	if (unlikely(filp->f_op != &fanotify_fops))
		goto fput_and_out;

	ret = fanotify_find_path(dfd, pathname, &path, flags);
	if (ret)
		goto fput_and_out;

	/* inode held in place by reference to path; group by fget on fd */
	inode = path.dentry->d_inode;
	group = filp->private_data;

	/* create/update an inode mark */
	switch (flags & (FAN_MARK_ADD | FAN_MARK_REMOVE)) {
	case FAN_MARK_ADD:
		ret = fanotify_add_mark(group, inode, NULL, flags, mask);
		break;
	case FAN_MARK_REMOVE:
		ret = fanotify_remove_mark(group, inode, NULL, mask);
		break;
	default:
		ret = -EINVAL;
	}

	path_put(&path);
fput_and_out:
	fput_light(filp, fput_needed);
	return ret;
}

#ifdef CONFIG_HAVE_SYSCALL_WRAPPERS
asmlinkage long SyS_fanotify_mark(long fanotify_fd, long flags, __u64 mask,
				  long dfd, long pathname)
{
	return SYSC_fanotify_mark((int) fanotify_fd, (unsigned int) flags,
				  mask, (int) dfd,
				  (const char  __user *) pathname);
}
SYSCALL_ALIAS(sys_fanotify_mark, SyS_fanotify_mark);
#endif

/*
 * fanotify_user_setup - Our initialization function.  Note that we cannnot return
 * error because we have compiled-in VFS hooks.  So an (unlikely) failure here
 * must result in panic().
 */
static int __init fanotify_user_setup(void)
{
	fanotify_mark_cache = KMEM_CACHE(fsnotify_mark, SLAB_PANIC);

	return 0;
}
device_initcall(fanotify_user_setup);
