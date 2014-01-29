#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/slab.h>

MODULE_LICENSE("Dual BSD/GPL");

#define MINOR_NUMBERS_TO_ALLOCATE 10
#define MY_BUFFER_SIZE 131076

static dev_t my_device_numbers;
static char *device_name = "my_buffer_device";
static struct cdev *my_cdev;

int buffer_open(struct inode *inode, struct file *filp);
int buffer_release(struct inode *inode, struct file *filp);
ssize_t buffer_read(struct file *filp, char __user *buff, size_t count, loff_t *offp);
ssize_t buffer_write(struct file *filp, const char __user *buff, size_t count, loff_t *offp);

static struct file_operations buffer_fops = {
	.owner = THIS_MODULE,
	.read = buffer_read,
	.write = buffer_write,
	.open = buffer_open,
	.release = buffer_release
};

struct my_buffer {
	unsigned int buffer_length;
	char my_mem[MY_BUFFER_SIZE];
};

static struct my_buffer *buffer;
struct my_buffer* create_buffer(void);
void destroy_buffer(struct my_buffer *buf); 

int buffer_open(struct inode *inode, struct file *filp) {
	printk(KERN_ALERT "+ buffer_open");
	filp->private_data  = buffer; 
	return 0;
}

int buffer_release(struct inode *inode, struct file *filp) {
	printk(KERN_ALERT "+ buffer_release");
	return 0;
} 

ssize_t buffer_read(struct file *filp, char __user *buff, size_t count, loff_t *offp) {
	ssize_t retval = 0;

	struct my_buffer *my_buff;
	my_buff = filp->private_data;

	if(copy_to_user(buff, my_buff->my_mem, my_buff->buffer_length)) {
		printk(KERN_ALERT "+ Not all was copied, read ");
		retval = -EFAULT;
		goto out;
	}
	*offp += my_buff->buffer_length;

out:
	printk(KERN_ALERT "+ buffer read");
	return retval;
}

ssize_t buffer_write(struct file *filp, const char __user *buff, size_t count, loff_t *offp) {
	printk(KERN_ALERT "+ buffer write");
	ssize_t retval = count;
	
	struct my_buffer *my_buff;
	my_buff = filp->private_data;
	printk(KERN_ALERT "+ buffer address %p", my_buff);

	unsigned int copy_size;
	if(count > MY_BUFFER_SIZE) {
		copy_size = MY_BUFFER_SIZE;
	} else {
		copy_size = count;
	}
	
	printk(KERN_ALERT "+ copy size %d", copy_size);
	if(copy_from_user(my_buff->my_mem, buff, copy_size)) {
		printk(KERN_ALERT "+ Not all was copied, write ");
		retval = -EFAULT;
		goto out;
	}
	my_buff->buffer_length = copy_size;
	*offp += my_buff->buffer_length;

	printk(KERN_ALERT "+ buffer size %d", my_buff->buffer_length);
	printk(KERN_ALERT "+ buffer %*ph", (void *)my_buff->my_mem);
	print_hex_dump_bytes("+", DUMP_PREFIX_ADDRESS, my_buff->my_mem, my_buff->buffer_length);

out:
	return retval;
}

struct my_buffer* create_buffer(void) {
	struct my_buffer *buffer;
	buffer = kmalloc(sizeof(struct my_buffer), GFP_KERNEL);
	buffer->buffer_length = 0;
	return buffer;
}

void destroy_buffer(struct my_buffer *buf) {
	kfree(buf);
}

void unregister_device(struct cdev *device) {
	cdev_del(device);
}

void unregister_numbers(dev_t allocated_numbers, int count) {
	unregister_chrdev_region(my_device_numbers, count);
}


static int __init buffer_init(void)
{
	printk(KERN_ALERT "+ Hello, world\n");
	int err;

	buffer = create_buffer();
	printk(KERN_ALERT "+ buffer address %p", buffer);

	err = alloc_chrdev_region(&my_device_numbers, 0, MINOR_NUMBERS_TO_ALLOCATE, device_name);
	if(err) goto alloc_number_fails;
	
	my_cdev = cdev_alloc();
	my_cdev->ops = &buffer_fops;
	my_cdev->owner = THIS_MODULE;
	err = cdev_add(my_cdev, my_device_numbers, MINOR_NUMBERS_TO_ALLOCATE); 
	if(err) goto alloc_add_fails;

	printk(KERN_ALERT "+ Major number is: %d", MAJOR(my_device_numbers));
	printk(KERN_ALERT "+ Minor number is: %d", MINOR(my_device_numbers));

	printk(KERN_ALERT "+ open is: %p", buffer_fops.open);
	printk(KERN_ALERT "+ close is: %p", buffer_fops.release);
	printk(KERN_ALERT "+ read is: %p", buffer_fops.read);
	printk(KERN_ALERT "+ write is: %p", buffer_fops.write);
	return 0;
alloc_add_fails:
	unregister_numbers(my_device_numbers, MINOR_NUMBERS_TO_ALLOCATE);
alloc_number_fails:
	printk(KERN_ALERT "+ Something went wrong: %d", err);
	return err;
}

static void __exit buffer_exit(void)
{
	destroy_buffer(buffer);
	unregister_device(my_cdev);
	unregister_numbers(my_device_numbers, MINOR_NUMBERS_TO_ALLOCATE);
	printk(KERN_ALERT "+ Unregister device\n");
}

module_init(buffer_init);
module_exit(buffer_exit);
