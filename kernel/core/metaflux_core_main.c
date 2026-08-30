#include <linux/fs.h>
#include <linux/eventfd.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>

#include <metaflux/shared/device.h>
#include <metaflux/uapi/transport.h>

#define MF_CDEV_RING_CAPACITY UINT32_C(256)
#define MF_CDEV_SUBMISSION_QUEUE_ID UINT64_C(1)
#define MF_CDEV_COMPLETION_QUEUE_ID UINT64_C(2)
#define MF_CDEV_GENERATION UINT64_C(1)
#define MF_CDEV_DAEMON_INCARNATION UINT64_C(1)
#define MF_CDEV_VIEW_SERIAL UINT64_C(1)

#define MF_CDEV_SINGLE_MAPPING_SIZE                                                   \
	(sizeof(mf_ring_header_v1) +                                                   \
	 (sizeof(mf_ring_descriptor_v1) * (size_t)MF_CDEV_RING_CAPACITY))
#define MF_CDEV_MAPPING_SIZE (MF_CDEV_SINGLE_MAPPING_SIZE * (size_t)2)
#define MF_CDEV_PAYLOAD_PGOFF_V0 2UL
#define MF_CDEV_PAYLOAD_MAX_SIZE (UINT64_C(67108864))

struct mf_cdev_file {
	bool control;
	bool negotiated;
	bool queue_created;
	bool lease;
	bool memory_allocated;
	struct eventfd_ctx *submission_eventfd;
	struct eventfd_ctx *completion_eventfd;
};

struct mf_cdev_queue {
	void *mapping;
	size_t allocation_size;
	u64 mapping_size;
	struct mf_registry_view_id_v1 view_id;
	u64 generation;
	struct mf_cdev_file *queue_owner;
	struct mf_cdev_file *lease_owner;
	struct mf_cdev_file *eventfd_owner;
	struct eventfd_ctx *submission_eventfd;
	struct eventfd_ctx *completion_eventfd;
	atomic_t vma_refs;
	bool online;
	wait_queue_head_t wait;
};

struct mf_cdev_memory {
	void *mapping;
	size_t allocation_size;
	u64 byte_count;
	u64 handle;
	u64 generation;
	struct mf_cdev_file *owner;
	atomic_t vma_refs;
	bool online;
};

static DEFINE_MUTEX(mf_cdev_lock);
static struct mf_cdev_queue mf_cdev_queue;
static struct mf_cdev_memory mf_cdev_payload;

static bool mf_cdev_bytes_zero(const u8 *bytes, size_t count)
{
	size_t index;

	for (index = 0; index < count; ++index) {
		if (bytes[index] != 0U)
			return false;
	}
	return true;
}

static void mf_cdev_init_ring(struct mf_ring_header_v1 *header, u64 queue_id)
{
	mf_ring_descriptor_v1 *descriptors;
	u32 index;

	memset(header, 0, sizeof(*header));
	header->metadata.magic = MF_SHARED_RING_MAGIC;
	header->metadata.abi_version = MF_SHARED_DEVICE_ABI_VERSION_1;
	header->metadata.header_size = sizeof(*header);
	header->metadata.descriptor_size = sizeof(mf_ring_descriptor_v1);
	header->metadata.capacity = MF_CDEV_RING_CAPACITY;
	header->metadata.mapping_size = MF_CDEV_SINGLE_MAPPING_SIZE;
	header->metadata.registry_view_id = mf_cdev_queue.view_id;
	header->metadata.queue_id = queue_id;
	header->metadata.queue_generation = mf_cdev_queue.generation;

	descriptors = (mf_ring_descriptor_v1 *)((u8 *)header + sizeof(*header));
	for (index = 0; index < MF_CDEV_RING_CAPACITY; ++index)
		WRITE_ONCE(descriptors[index].sequence, index);
}

static int mf_cdev_validate_size(u32 struct_size, size_t expected)
{
	return struct_size == expected ? 0 : -EINVAL;
}

static int mf_cdev_eventfd_get(s32 descriptor, struct eventfd_ctx **out_context)
{
	struct eventfd_ctx *context;

	if (out_context == NULL)
		return -EINVAL;
	*out_context = NULL;
	if (descriptor < 0)
		return 0;
	context = eventfd_ctx_fdget(descriptor);
	if (IS_ERR(context))
		return PTR_ERR(context);
	*out_context = context;
	return 0;
}

static void mf_cdev_eventfd_put(struct eventfd_ctx **context)
{
	if (context != NULL && *context != NULL) {
		eventfd_ctx_put(*context);
		*context = NULL;
	}
}

static void mf_cdev_payload_reap_locked(void)
{
	if (!mf_cdev_payload.online && atomic_read(&mf_cdev_payload.vma_refs) == 0 &&
	    mf_cdev_payload.mapping != NULL) {
		vfree(mf_cdev_payload.mapping);
		mf_cdev_payload.mapping = NULL;
		mf_cdev_payload.allocation_size = 0;
		mf_cdev_payload.byte_count = 0;
		mf_cdev_payload.handle = 0;
		mf_cdev_payload.generation = 0;
	}
}

static int mf_cdev_memory_alloc(struct mf_cdev_file *file, void __user *argument)
{
	mf_uapi_memory_v0 request;
	void *mapping;
	size_t allocation_size;
	int result;

	if (file == NULL || file->control || !file->negotiated)
		return -EPERM;
	if (copy_from_user(&request, argument, sizeof(request)) != 0)
		return -EFAULT;
	if (mf_cdev_validate_size(request.struct_size, sizeof(request)) != 0 || request.flags != 0U ||
	    request.handle != 0U || request.byte_count == 0U ||
	    (request.generation != 0U && request.generation != mf_cdev_queue.generation) ||
	    request.byte_count > MF_CDEV_PAYLOAD_MAX_SIZE || request.alignment < PAGE_SIZE ||
	    (request.alignment & (request.alignment - 1U)) != 0U || request.offset != 0U ||
	    request.fd != -1 || !mf_cdev_bytes_zero(request.reserved, sizeof(request.reserved)))
		return -EINVAL;

	allocation_size = PAGE_ALIGN((size_t)request.byte_count);
	if (allocation_size < request.byte_count || allocation_size == 0U)
		return -EOVERFLOW;
	mapping = vmalloc_user(allocation_size);
	if (mapping == NULL)
		return -ENOMEM;

	mutex_lock(&mf_cdev_lock);
	if (!mf_cdev_queue.online) {
		mutex_unlock(&mf_cdev_lock);
		vfree(mapping);
		return -ENODEV;
	}
	if (mf_cdev_payload.online || mf_cdev_payload.mapping != NULL) {
		mutex_unlock(&mf_cdev_lock);
		vfree(mapping);
		return -EBUSY;
	}
	mf_cdev_payload.mapping = mapping;
	mf_cdev_payload.allocation_size = allocation_size;
	mf_cdev_payload.byte_count = allocation_size;
	mf_cdev_payload.handle = MF_CDEV_SUBMISSION_QUEUE_ID;
	mf_cdev_payload.generation = mf_cdev_queue.generation;
	mf_cdev_payload.owner = file;
	atomic_set(&mf_cdev_payload.vma_refs, 0);
	mf_cdev_payload.online = true;
	file->memory_allocated = true;
	request.handle = mf_cdev_payload.handle;
	request.generation = mf_cdev_payload.generation;
	request.byte_count = mf_cdev_payload.byte_count;
	request.alignment = max_t(u64, request.alignment, PAGE_SIZE);
	request.offset = (u64)MF_CDEV_PAYLOAD_PGOFF_V0 * (u64)PAGE_SIZE;
	request.fd = -1;
	mutex_unlock(&mf_cdev_lock);

	result = copy_to_user(argument, &request, sizeof(request));
	if (result != 0) {
		mutex_lock(&mf_cdev_lock);
		if (mf_cdev_payload.owner == file) {
			mf_cdev_payload.owner = NULL;
			mf_cdev_payload.online = false;
			file->memory_allocated = false;
			mf_cdev_payload_reap_locked();
		}
		mutex_unlock(&mf_cdev_lock);
		return -EFAULT;
	}
	return 0;
}

static int mf_cdev_negotiate(struct mf_cdev_file *file, void __user *argument)
{
	mf_uapi_negotiate_v0 request;
	u64 supported = MF_UAPI_FEATURE_QUEUE_MMAP_V0 | MF_UAPI_FEATURE_EVENTFD_V0 |
			MF_UAPI_FEATURE_WORKER_BROKER_V0;

	if (copy_from_user(&request, argument, sizeof(request)) != 0)
		return -EFAULT;
	if (mf_cdev_validate_size(request.struct_size, sizeof(request)) != 0 || request.flags != 0U ||
	    !mf_cdev_bytes_zero(request.reserved, sizeof(request.reserved)))
		return -EINVAL;
	if (request.version != MF_UAPI_VERSION_V0 || (request.required_features & ~supported) != 0U)
		return -EOPNOTSUPP;

	mutex_lock(&mf_cdev_lock);
	if (!mf_cdev_queue.online) {
		mutex_unlock(&mf_cdev_lock);
		return -ENODEV;
	}
	request.required_features &= supported;
	request.optional_features &= supported;
	request.registry_view_daemon = mf_cdev_queue.view_id.daemon_incarnation;
	request.registry_view_serial = mf_cdev_queue.view_id.view_serial;
	request.device_generation = mf_cdev_queue.generation;
	request.descriptor_size = sizeof(mf_ring_descriptor_v1);
	request.ring_order = ilog2(MF_CDEV_RING_CAPACITY);
	request.max_queues = 1U;
	request.max_regions = 0U;
	request.max_inflight = MF_CDEV_RING_CAPACITY;
	request.dma_width = 64U;
	request.dma_alignment = PAGE_SIZE;
	file->negotiated = true;
	mutex_unlock(&mf_cdev_lock);

	return copy_to_user(argument, &request, sizeof(request)) == 0 ? 0 : -EFAULT;
}

static int mf_cdev_queue_create(struct mf_cdev_file *file, void __user *argument)
{
	mf_uapi_queue_v0 request;
	struct eventfd_ctx *submission_eventfd = NULL;
	struct eventfd_ctx *completion_eventfd = NULL;
	s32 submission_descriptor;
	s32 completion_descriptor;
	int result;

	if (file->control || !file->negotiated)
		return -EPERM;
	if (copy_from_user(&request, argument, sizeof(request)) != 0)
		return -EFAULT;
	if (mf_cdev_validate_size(request.struct_size, sizeof(request)) != 0 || request.flags != 0U ||
	    request.queue_id != 0U || request.submission_eventfd < -1 ||
	    request.completion_eventfd < -1 ||
	    (request.submission_eventfd < 0) != (request.completion_eventfd < 0) ||
	    !mf_cdev_bytes_zero(request.reserved, sizeof(request.reserved)))
		return -EINVAL;
	if (file->queue_created)
		return -EBUSY;
	submission_descriptor = request.submission_eventfd;
	completion_descriptor = request.completion_eventfd;
	result = mf_cdev_eventfd_get(request.submission_eventfd, &submission_eventfd);
	if (result != 0)
		return result == -EBADF ? -EINVAL : result;
	result = mf_cdev_eventfd_get(request.completion_eventfd, &completion_eventfd);
	if (result != 0) {
		mf_cdev_eventfd_put(&submission_eventfd);
		return result == -EBADF ? -EINVAL : result;
	}

	mutex_lock(&mf_cdev_lock);
	if (!mf_cdev_queue.online) {
		mutex_unlock(&mf_cdev_lock);
		mf_cdev_eventfd_put(&submission_eventfd);
		mf_cdev_eventfd_put(&completion_eventfd);
		return -ENODEV;
	}
	if (mf_cdev_queue.queue_owner != NULL && mf_cdev_queue.queue_owner != file) {
		mutex_unlock(&mf_cdev_lock);
		mf_cdev_eventfd_put(&submission_eventfd);
		mf_cdev_eventfd_put(&completion_eventfd);
		return -EBUSY;
	}
	if (mf_cdev_queue.eventfd_owner != NULL) {
		mutex_unlock(&mf_cdev_lock);
		mf_cdev_eventfd_put(&submission_eventfd);
		mf_cdev_eventfd_put(&completion_eventfd);
		return -EBUSY;
	}
	request.queue_id = MF_CDEV_SUBMISSION_QUEUE_ID;
	request.queue_generation = mf_cdev_queue.generation;
	request.mmap_offset = 0U;
	request.mapping_size = mf_cdev_queue.mapping_size;
	request.capacity = MF_CDEV_RING_CAPACITY;
	request.descriptor_size = sizeof(mf_ring_descriptor_v1);
	request.submission_eventfd = submission_descriptor;
	request.completion_eventfd = completion_descriptor;
	if (submission_eventfd != NULL || completion_eventfd != NULL) {
		mf_cdev_queue.submission_eventfd = submission_eventfd;
		mf_cdev_queue.completion_eventfd = completion_eventfd;
		mf_cdev_queue.eventfd_owner = file;
		file->submission_eventfd = submission_eventfd;
		file->completion_eventfd = completion_eventfd;
	}
	mf_cdev_queue.queue_owner = file;
	file->queue_created = true;
	mutex_unlock(&mf_cdev_lock);

	if (copy_to_user(argument, &request, sizeof(request)) != 0) {
		mutex_lock(&mf_cdev_lock);
		if (mf_cdev_queue.queue_owner == file)
			mf_cdev_queue.queue_owner = NULL;
		if (mf_cdev_queue.eventfd_owner == file) {
			mf_cdev_queue.eventfd_owner = NULL;
			mf_cdev_queue.submission_eventfd = NULL;
			mf_cdev_queue.completion_eventfd = NULL;
			file->submission_eventfd = NULL;
			file->completion_eventfd = NULL;
			mf_cdev_eventfd_put(&submission_eventfd);
			mf_cdev_eventfd_put(&completion_eventfd);
		}
		file->queue_created = false;
		mutex_unlock(&mf_cdev_lock);
		return -EFAULT;
	}
	return 0;
}

static int mf_cdev_worker_lease(struct mf_cdev_file *file, void __user *argument)
{
	mf_uapi_worker_lease_v0 request;
	struct eventfd_ctx *kick_eventfd = NULL;
	struct eventfd_ctx *completion_eventfd = NULL;
	bool claimed = false;
	int result;

	if (!file->control)
		return -EPERM;
	if (file->lease)
		return -EBUSY;
	if (copy_from_user(&request, argument, sizeof(request)) != 0)
		return -EFAULT;
	if (mf_cdev_validate_size(request.struct_size, sizeof(request)) != 0 || request.flags != 0U ||
	    request.kick_eventfd < -1 || request.completion_eventfd < -1 ||
	    (request.kick_eventfd < 0) != (request.completion_eventfd < 0) ||
	    !mf_cdev_bytes_zero(request.reserved, sizeof(request.reserved)))
		return -EINVAL;
	result = mf_cdev_eventfd_get(request.kick_eventfd, &kick_eventfd);
	if (result != 0)
		return result == -EBADF ? -EINVAL : result;
	result = mf_cdev_eventfd_get(request.completion_eventfd, &completion_eventfd);
	if (result != 0) {
		mf_cdev_eventfd_put(&kick_eventfd);
		return result == -EBADF ? -EINVAL : result;
	}

	mutex_lock(&mf_cdev_lock);
	if (!mf_cdev_queue.online) {
		mutex_unlock(&mf_cdev_lock);
		mf_cdev_eventfd_put(&kick_eventfd);
		mf_cdev_eventfd_put(&completion_eventfd);
		return -ENODEV;
	}
	if (mf_cdev_queue.lease_owner != NULL && mf_cdev_queue.lease_owner != file) {
		mutex_unlock(&mf_cdev_lock);
		mf_cdev_eventfd_put(&kick_eventfd);
		mf_cdev_eventfd_put(&completion_eventfd);
		return -EBUSY;
	}
	if (mf_cdev_queue.eventfd_owner != NULL && mf_cdev_queue.eventfd_owner != file) {
		mutex_unlock(&mf_cdev_lock);
		mf_cdev_eventfd_put(&kick_eventfd);
		mf_cdev_eventfd_put(&completion_eventfd);
		return -EBUSY;
	}
	if ((request.daemon_incarnation != 0U &&
	     request.daemon_incarnation != mf_cdev_queue.view_id.daemon_incarnation) ||
	    (request.registry_view_serial != 0U &&
	     request.registry_view_serial != mf_cdev_queue.view_id.view_serial) ||
	    (request.device_generation != 0U && request.device_generation != mf_cdev_queue.generation)) {
		mutex_unlock(&mf_cdev_lock);
		mf_cdev_eventfd_put(&kick_eventfd);
		mf_cdev_eventfd_put(&completion_eventfd);
		return -ESTALE;
	}
	request.daemon_incarnation = mf_cdev_queue.view_id.daemon_incarnation;
	request.registry_view_serial = mf_cdev_queue.view_id.view_serial;
	request.identity_record_id = MF_KERNEL_PRIMARY_ENTRY_ID;
	request.device_generation = mf_cdev_queue.generation;
	request.lease_id = MF_CDEV_SUBMISSION_QUEUE_ID;
	request.queue_mmap_offset = 0U;
	request.queue_mapping_size = mf_cdev_queue.mapping_size;
	if (kick_eventfd != NULL || completion_eventfd != NULL) {
		mf_cdev_queue.submission_eventfd = kick_eventfd;
		mf_cdev_queue.completion_eventfd = completion_eventfd;
		mf_cdev_queue.eventfd_owner = file;
		file->submission_eventfd = kick_eventfd;
		file->completion_eventfd = completion_eventfd;
	}
	mf_cdev_queue.lease_owner = file;
	file->lease = true;
	claimed = true;
	mutex_unlock(&mf_cdev_lock);

	if (copy_to_user(argument, &request, sizeof(request)) != 0) {
		if (claimed) {
			mutex_lock(&mf_cdev_lock);
			if (mf_cdev_queue.lease_owner == file)
				mf_cdev_queue.lease_owner = NULL;
			if (mf_cdev_queue.eventfd_owner == file) {
				mf_cdev_queue.eventfd_owner = NULL;
				mf_cdev_queue.submission_eventfd = NULL;
				mf_cdev_queue.completion_eventfd = NULL;
				file->submission_eventfd = NULL;
				file->completion_eventfd = NULL;
				mf_cdev_eventfd_put(&kick_eventfd);
				mf_cdev_eventfd_put(&completion_eventfd);
			}
			file->lease = false;
			mutex_unlock(&mf_cdev_lock);
		}
		return -EFAULT;
	}
	return 0;
}

static long mf_cdev_wait(struct mf_cdev_file *file, void __user *argument)
{
	mf_uapi_wait_v0 request;
	struct mf_ring_header_v1 *completion;
	u64 observed;
	long timeout;
	long result;

	if (!file->queue_created)
		return -EPERM;
	if (copy_from_user(&request, argument, sizeof(request)) != 0)
		return -EFAULT;
	if (mf_cdev_validate_size(request.struct_size, sizeof(request)) != 0 || request.flags != 0U ||
	    request.queue_id != MF_CDEV_SUBMISSION_QUEUE_ID || request.timeline == 0U ||
	    !mf_cdev_bytes_zero(request.reserved, sizeof(request.reserved)))
		return -EINVAL;

	timeout = request.timeout_ns == ~UINT64_C(0) ? MAX_SCHEDULE_TIMEOUT :
		  (long)nsecs_to_jiffies64(request.timeout_ns);
	if (request.timeout_ns != 0U && timeout == 0)
		timeout = 1;
	if (timeout < 0 || timeout > MAX_SCHEDULE_TIMEOUT)
		timeout = MAX_SCHEDULE_TIMEOUT;

	completion = (struct mf_ring_header_v1 *)((u8 *)mf_cdev_queue.mapping +
						  MF_CDEV_SINGLE_MAPPING_SIZE);
	result = wait_event_interruptible_timeout(
		mf_cdev_queue.wait,
		(!READ_ONCE(mf_cdev_queue.online) ||
		 READ_ONCE(completion->producer.position) >= request.timeline),
		timeout);
	observed = READ_ONCE(completion->producer.position);
	request.observed_timeline = observed;
	if (copy_to_user(argument, &request, sizeof(request)) != 0)
		return -EFAULT;
	if (!READ_ONCE(mf_cdev_queue.online))
		return -ENODEV;
	if (result < 0)
		return -ERESTARTSYS;
	if (result == 0 && observed < request.timeline)
		return -ETIMEDOUT;
	return 0;
}

static long mf_cdev_ioctl(struct file *file_pointer, unsigned int command,
				  unsigned long argument)
{
	struct mf_cdev_file *file = file_pointer->private_data;
	void __user *user_argument = (void __user *)argument;

	if (file == NULL || user_argument == NULL)
		return -EINVAL;
	switch (command) {
	case MF_UAPI_IOCTL_NEGOTIATE:
		return mf_cdev_negotiate(file, user_argument);
	case MF_UAPI_IOCTL_QUEUE_CREATE:
		return mf_cdev_queue_create(file, user_argument);
	case MF_UAPI_IOCTL_WORKER_LEASE:
		return mf_cdev_worker_lease(file, user_argument);
	case MF_UAPI_IOCTL_WAIT:
		return mf_cdev_wait(file, user_argument);
	case MF_UAPI_IOCTL_MEMORY_ALLOC:
		return mf_cdev_memory_alloc(file, user_argument);
	case MF_UAPI_IOCTL_MEMORY_REGISTER:
		return -EOPNOTSUPP;
	default:
		return -ENOTTY;
	}
}

static void mf_cdev_vma_open(struct vm_area_struct *vma)
{
	struct mf_cdev_queue *queue = vma->vm_private_data;

	if (queue != NULL)
		atomic_inc(&queue->vma_refs);
}

static void mf_cdev_vma_close(struct vm_area_struct *vma)
{
	struct mf_cdev_queue *queue = vma->vm_private_data;

	if (queue != NULL)
		atomic_dec(&queue->vma_refs);
}

static const struct vm_operations_struct mf_cdev_vm_ops = {
	.open = mf_cdev_vma_open,
	.close = mf_cdev_vma_close,
};

static void mf_cdev_memory_vma_open(struct vm_area_struct *vma)
{
	struct mf_cdev_memory *memory = vma->vm_private_data;

	if (memory != NULL)
		atomic_inc(&memory->vma_refs);
}

static void mf_cdev_memory_vma_close(struct vm_area_struct *vma)
{
	struct mf_cdev_memory *memory = vma->vm_private_data;

	if (memory != NULL) {
		mutex_lock(&mf_cdev_lock);
		atomic_dec(&memory->vma_refs);
		mf_cdev_payload_reap_locked();
		mutex_unlock(&mf_cdev_lock);
	}
}

static const struct vm_operations_struct mf_cdev_memory_vm_ops = {
	.open = mf_cdev_memory_vma_open,
	.close = mf_cdev_memory_vma_close,
};

static int mf_cdev_mmap(struct file *file_pointer, struct vm_area_struct *vma)
{
	struct mf_cdev_file *file = file_pointer->private_data;
	unsigned long length;
	int result;

	if (file == NULL || file->control || vma == NULL)
		return -EINVAL;
	length = vma->vm_end - vma->vm_start;
	if (vma->vm_pgoff == MF_CDEV_PAYLOAD_PGOFF_V0) {
		mutex_lock(&mf_cdev_lock);
		if (!mf_cdev_payload.online || mf_cdev_payload.mapping == NULL ||
		    mf_cdev_payload.owner != file || length != mf_cdev_payload.byte_count) {
			mutex_unlock(&mf_cdev_lock);
			return mf_cdev_payload.online ? -EINVAL : -ENODEV;
		}
		vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);
		vma->vm_ops = &mf_cdev_memory_vm_ops;
		vma->vm_private_data = &mf_cdev_payload;
		atomic_inc(&mf_cdev_payload.vma_refs);
		result = remap_vmalloc_range(vma, mf_cdev_payload.mapping, 0);
		if (result != 0)
			atomic_dec(&mf_cdev_payload.vma_refs);
		mutex_unlock(&mf_cdev_lock);
		return result;
	}
	if (!file->queue_created || vma->vm_pgoff != 0U)
		return -EINVAL;
	if (length != mf_cdev_queue.mapping_size)
		return -EINVAL;
	mutex_lock(&mf_cdev_lock);
	if (!mf_cdev_queue.online || mf_cdev_queue.mapping == NULL) {
		mutex_unlock(&mf_cdev_lock);
		return -ENODEV;
	}
	vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);
	vma->vm_ops = &mf_cdev_vm_ops;
	vma->vm_private_data = &mf_cdev_queue;
	atomic_inc(&mf_cdev_queue.vma_refs);
	result = remap_vmalloc_range(vma, mf_cdev_queue.mapping, 0);
	if (result != 0)
		atomic_dec(&mf_cdev_queue.vma_refs);
	mutex_unlock(&mf_cdev_lock);
	return result;
}

static __poll_t mf_cdev_poll(struct file *file_pointer, poll_table *wait)
{
	struct mf_cdev_file *file = file_pointer->private_data;
	struct mf_ring_header_v1 *completion;
	__poll_t mask = 0;

	if (file == NULL || file->control || !file->queue_created)
		return EPOLLERR;
	poll_wait(file_pointer, &mf_cdev_queue.wait, wait);
	mutex_lock(&mf_cdev_lock);
	if (!mf_cdev_queue.online) {
		mask = EPOLLHUP | EPOLLERR;
	} else {
		completion = (struct mf_ring_header_v1 *)((u8 *)mf_cdev_queue.mapping +
							  MF_CDEV_SINGLE_MAPPING_SIZE);
		if (READ_ONCE(completion->producer.position) !=
		    READ_ONCE(completion->consumer.position))
			mask = EPOLLIN | EPOLLRDNORM;
	}
	mutex_unlock(&mf_cdev_lock);
	return mask;
}

static int mf_cdev_open_common(struct inode *inode, struct file *file_pointer, bool control)
{
	struct mf_cdev_file *file;

	file = kzalloc(sizeof(*file), GFP_KERNEL);
	if (file == NULL)
		return -ENOMEM;
	file->control = control;
	file_pointer->private_data = file;
	return 0;
}

static int mf_cdev_open_control(struct inode *inode, struct file *file_pointer)
{
	return mf_cdev_open_common(inode, file_pointer, true);
}

static int mf_cdev_open_data(struct inode *inode, struct file *file_pointer)
{
	return mf_cdev_open_common(inode, file_pointer, false);
}

static int mf_cdev_release(struct inode *inode, struct file *file_pointer)
{
	struct mf_cdev_file *file = file_pointer->private_data;

	if (file == NULL)
		return 0;
	mutex_lock(&mf_cdev_lock);
	if (mf_cdev_queue.queue_owner == file)
		mf_cdev_queue.queue_owner = NULL;
	if (mf_cdev_queue.lease_owner == file) {
		mf_cdev_queue.lease_owner = NULL;
		wake_up_all(&mf_cdev_queue.wait);
	}
	if (mf_cdev_queue.eventfd_owner == file) {
		mf_cdev_queue.eventfd_owner = NULL;
		mf_cdev_queue.submission_eventfd = NULL;
		mf_cdev_queue.completion_eventfd = NULL;
		mf_cdev_eventfd_put(&file->submission_eventfd);
		mf_cdev_eventfd_put(&file->completion_eventfd);
	}
	if (mf_cdev_payload.owner == file) {
		mf_cdev_payload.owner = NULL;
		mf_cdev_payload.online = false;
		file->memory_allocated = false;
		mf_cdev_payload_reap_locked();
	}
	mutex_unlock(&mf_cdev_lock);
	kfree(file);
	return 0;
}

static const struct file_operations mf_cdev_control_fops = {
	.owner = THIS_MODULE,
	.open = mf_cdev_open_control,
	.release = mf_cdev_release,
	.unlocked_ioctl = mf_cdev_ioctl,
	.compat_ioctl = mf_cdev_ioctl,
};

static const struct file_operations mf_cdev_data_fops = {
	.owner = THIS_MODULE,
	.open = mf_cdev_open_data,
	.release = mf_cdev_release,
	.unlocked_ioctl = mf_cdev_ioctl,
	.compat_ioctl = mf_cdev_ioctl,
	.mmap = mf_cdev_mmap,
	.poll = mf_cdev_poll,
};

static struct miscdevice mf_cdev_control_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "metafluxctl",
	.fops = &mf_cdev_control_fops,
	.mode = 0600,
};

static struct miscdevice mf_cdev_data_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "metaflux0",
	.fops = &mf_cdev_data_fops,
	.mode = 0600,
};

static int __init mf_cdev_init(void)
{
	struct mf_ring_header_v1 *submission;
	struct mf_ring_header_v1 *completion;
	int result;

	memset(&mf_cdev_queue, 0, sizeof(mf_cdev_queue));
	memset(&mf_cdev_payload, 0, sizeof(mf_cdev_payload));
	mf_cdev_queue.view_id.daemon_incarnation = MF_CDEV_DAEMON_INCARNATION;
	mf_cdev_queue.view_id.view_serial = MF_CDEV_VIEW_SERIAL;
	mf_cdev_queue.generation = MF_CDEV_GENERATION;
	mf_cdev_queue.mapping_size = MF_CDEV_MAPPING_SIZE;
	mf_cdev_queue.allocation_size = PAGE_ALIGN(MF_CDEV_MAPPING_SIZE);
	init_waitqueue_head(&mf_cdev_queue.wait);
	atomic_set(&mf_cdev_queue.vma_refs, 0);
	atomic_set(&mf_cdev_payload.vma_refs, 0);
	mf_cdev_queue.mapping = vmalloc_user(mf_cdev_queue.allocation_size);
	if (mf_cdev_queue.mapping == NULL)
		return -ENOMEM;
	submission = (struct mf_ring_header_v1 *)mf_cdev_queue.mapping;
	completion = (struct mf_ring_header_v1 *)((u8 *)mf_cdev_queue.mapping +
						  MF_CDEV_SINGLE_MAPPING_SIZE);
	mf_cdev_init_ring(submission, MF_CDEV_SUBMISSION_QUEUE_ID);
	mf_cdev_init_ring(completion, MF_CDEV_COMPLETION_QUEUE_ID);
	mf_cdev_queue.online = true;

	result = misc_register(&mf_cdev_control_device);
	if (result != 0)
		goto fail_mapping;
	result = misc_register(&mf_cdev_data_device);
	if (result != 0) {
		misc_deregister(&mf_cdev_control_device);
		goto fail_mapping;
	}
	pr_info("metaflux_core: cdev queues ready (generation %llu, mapping %llu bytes)\n",
		(unsigned long long)mf_cdev_queue.generation,
		(unsigned long long)mf_cdev_queue.mapping_size);
	return 0;

fail_mapping:
	mf_cdev_queue.online = false;
	vfree(mf_cdev_queue.mapping);
	mf_cdev_queue.mapping = NULL;
	return result;
}

static void __exit mf_cdev_exit(void)
{
	misc_deregister(&mf_cdev_data_device);
	misc_deregister(&mf_cdev_control_device);
	mutex_lock(&mf_cdev_lock);
	mf_cdev_queue.online = false;
	mf_cdev_queue.queue_owner = NULL;
	mf_cdev_queue.lease_owner = NULL;
	mf_cdev_queue.eventfd_owner = NULL;
	mf_cdev_eventfd_put(&mf_cdev_queue.submission_eventfd);
	mf_cdev_eventfd_put(&mf_cdev_queue.completion_eventfd);
	mf_cdev_payload.online = false;
	mf_cdev_payload.owner = NULL;
	wake_up_all(&mf_cdev_queue.wait);
	mf_cdev_payload_reap_locked();
	if (atomic_read(&mf_cdev_queue.vma_refs) == 0 && mf_cdev_queue.mapping != NULL) {
		vfree(mf_cdev_queue.mapping);
		mf_cdev_queue.mapping = NULL;
	}
	mutex_unlock(&mf_cdev_lock);
}

module_init(mf_cdev_init);
module_exit(mf_cdev_exit);

MODULE_DESCRIPTION("MetaFlux local character-device transport");
MODULE_LICENSE("GPL");
