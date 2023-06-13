#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kernel.h>  /* We're doing kernel work */
#include <linux/module.h>  /* Specifically, a module */
#include <linux/fs.h>      /* For register_chrdev */
#include <linux/uaccess.h> /* For get_user and put_user */
#include <linux/string.h>  /* For memset. NOTE - not string.h!*/
#include "message_slot.h"  /* Our custom definitions of IOCTL operations*/

MODULE_LICENSE("GPL");

/*
On each entry of that array we will chained a linked list.
Every node on the linked list will implement the functionality
of id cannel.
*/

//================================================================
//================================================================

/*
https://elixir.bootlin.com/linux/latest/source/include/linux/fs.h#L2403
When use register_chrdev() we actually call __register_chrdev() with :
1. baseminor = 0 : Which is the first of the requested range of minor numbers.
2. count = 256   : Which is the number of minor numbers required

Because in __init we call to register_chrdev(), We can create an array of 256
channel list that will manage the minor number of the device.
*/
static ChannelList minor_list[256];
int check_errors_write(ChannelNode *channel, char *buffer_channel, size_t length, struct file *file);
int check_validity(struct file *file, ChannelNode **channel, int minor_number, size_t length);
void enter_channel_to_linked_list(int minor_number, ChannelNode **channel, char *buffer_channel);
void copy_string(char *to, char *from, size_t bytes);

//================== DEVICE FUNCTIONS ===========================
static int device_open(struct inode *inode,
                       struct file *file)
{

  // Should not check for iminor because we already create an object (minor_list) that represent the minor
  printk("Invoking device_open(%p)\n", file);
  return SUCCESS;
}

//================================================================
//================================================================

static int device_release(struct inode *inode,
                          struct file *file)
{

  printk("Invoking device_release(%p,%p)\n", inode, file);
  kfree(file->private_data);
  return SUCCESS;
}

//================================================================
//================================================================

/*
Here We will set the private_data of the file to the channel number.
The idea behind it, is that the flow of the request is something like:
      open -> ioctl -> write/read -> close
and in the call to ioctl channel number provided.
*/
static long device_ioctl(struct file *file,
                         unsigned int ioctl_command_id,
                         unsigned long ioctl_param)
{
  unsigned long *id;
  printk("Invoking ioctl, The channel id is : %ld \n", ioctl_param);

  if (MSG_SLOT_CHANNEL != ioctl_command_id || ioctl_param == 0)
  {
    if (MSG_SLOT_CHANNEL != ioctl_command_id)
    {
      printk("The number that provide to ioctl is incorrect, you should pass %ld:", MSG_SLOT_CHANNEL);
    }
    else
    {
      printk("The channel number should be positive number ");
    }
    return -EINVAL;
  }

  id = (unsigned long *)kmalloc(sizeof(unsigned long), GFP_KERNEL);
  if (id == NULL)
  {
    printk("problem with alloc in ioctl");
    return -ENOMEM;
  }

  *id = ioctl_param;
  file->private_data = id;

  printk("The channel number is %lu:", *((unsigned long *)(file->private_data)));

  return SUCCESS;
}

//================================================================
//================================================================

static ssize_t device_read(struct file *file,
                           char __user *buffer,
                           size_t length,
                           loff_t *offset)
{
  // https://elixir.bootlin.com/linux/latest/source/include/linux/fs.h#L892
  int minor_number = iminor(file->f_inode);
  unsigned long unread_bytes;
  ChannelNode *channel = NULL;
  int succ;
  size_t bytes_to_copy;
  size_t i;
  char *copy_buffer;

  printk("Invocing device_read(%p,%ld)\n", file, length);

  succ = check_validity(file, &channel, minor_number, length);

  if (succ != 0)
  {
    return succ;
  }

  bytes_to_copy = channel->nbytes;

  // read should be atomic, so we first make a copy of the buffer provided,
  // and then we will try to passing the message to the reader.
  // If we failed we should restore the given buffer.

  copy_buffer = (char *)kmalloc(length * sizeof(char), GFP_KERNEL);
  if (copy_buffer == NULL)
  {
    printk(KERN_ALERT "Can not insure atomic read ");
    return -EINVAL;
  }

  unread_bytes = copy_from_user(copy_buffer, buffer, bytes_to_copy);

  if (unread_bytes != 0)
  {
    printk(KERN_ALERT "Can not insure atomic read ");
    return -EINVAL;
  }

  for (i = 0; i < bytes_to_copy; ++i)
  {
    if (put_user(channel->buffer[i], &buffer[i]) < 0)
    {
      if (copy_to_user(buffer, copy_buffer, i) != 0)
      {
        printk(KERN_ALERT "Can not insure atomic read ");
      }
      kfree(copy_buffer);
      printk(KERN_ALERT "Invalid buffer location that the user provide");
      return -EINVAL;
    }
  }

  kfree(copy_buffer);

  return bytes_to_copy;
}

//================================================================

int check_validity(struct file *file, ChannelNode **channel, int minor_number, size_t length)
{

  ChannelNode *tmp;
  unsigned long id;
  if (file->private_data == NULL || *((unsigned long *)file->private_data) == 0)
  {

    printk(KERN_ALERT "No channel has been set\n");
    return -EINVAL;
  }

  id = *(unsigned long *)file->private_data;
  tmp = minor_list[minor_number].head;

  if (tmp == NULL)
  {
    printk(KERN_ALERT "There is not channel with that number\n");
    return -EWOULDBLOCK;
  }

  while (tmp != NULL)
  {

    if (tmp->channel_number == id)
    {

      *channel = tmp;
      if (tmp->nbytes <= 0)
      {
        printk(KERN_ALERT "There is nothing to read\n");
        return -EWOULDBLOCK;
      }
      if (length < tmp->nbytes)
      {
        printk(KERN_ALERT "There buffer provided is not enough  \n");
        return -ENOSPC;
      }

      return SUCCESS;
    }
    if (tmp->channel_number < id)
    {

      tmp = tmp->next;
      if (tmp == NULL)
      {
        printk(KERN_ALERT "There is not channel with that number\n");
        return -EWOULDBLOCK;
      }
    }
    else
    {
      printk(KERN_ALERT "There is not channel with that number\n");
      return -EWOULDBLOCK;
    }
  }

  return SUCCESS;
}

//================================================================
//================================================================

static ssize_t device_write(struct file *file,
                            const char __user *buffer,
                            size_t length,
                            loff_t *offset)
{

  ssize_t i;
  unsigned long id;
  int minor_number = iminor(file->f_inode);
  ChannelNode *channel = (ChannelNode *)kmalloc(sizeof(ChannelNode), GFP_KERNEL);
  char *buffer_channel = (char *)kmalloc(BUF_LEN * sizeof(char), GFP_KERNEL);
  int succ = 0;
  char *copy_channel_buffer;

  printk("Invoking device_write(%p,%ld)  \n", file, length);

  succ = check_errors_write(channel, buffer_channel, length, file);
  if (succ != 0)
  {
    return succ;
  }

  id = *((unsigned long *)file->private_data);
  channel->channel_number = id;

  enter_channel_to_linked_list(minor_number, &channel, buffer_channel);

  printk("Prepare to write at (minor : %d , channel : %lu)", minor_number, id);

  // write should be atomic, so we first make a copy of the buffer of the channel,
  // and then we will try to passing the message to the reader.
  // If we failed we should restore the given buffer.

  copy_channel_buffer = (char *)kmalloc(BUF_LEN * sizeof(char), GFP_KERNEL);
  if (copy_channel_buffer == NULL)
  {
    printk(KERN_ALERT "Can not insure atomic write ");
    return -EINVAL;
  }

  copy_string(copy_channel_buffer, channel->buffer, channel->nbytes);

  for (i = 0; i < length; ++i)
  {
    if (get_user(channel->buffer[i], &buffer[i]) < 0)
    {
      copy_string(channel->buffer, copy_channel_buffer, channel->nbytes);
      kfree(copy_channel_buffer);
      printk(KERN_ALERT "Invalid buffer location that the user provide");
      return -EINVAL;
    }
  }

  kfree(copy_channel_buffer);
  channel->nbytes = length;
  return length;
}

//================================================================

int check_errors_write(ChannelNode *channel, char *buffer_channel, size_t length, struct file *file)
{

  if (channel == NULL || buffer_channel == NULL)
  {
    printk(KERN_ALERT "Failed to allocate memory\n");
    return -ENOMEM;
  }

  if (length == 0 || length > BUF_LEN)
  {
    printk(KERN_ALERT "Invalid message length\n");
    return -EMSGSIZE;
  }

  if (file->private_data == NULL || *((unsigned long *)(file->private_data)) == 0)
  {
    printk(KERN_ALERT "No channel has been set\n");
    return -EINVAL;
  }

  return SUCCESS;
}

//================================================================

// In channel pointer we will provide a pointer to the channel we going to write into.
void enter_channel_to_linked_list(int minor_number, ChannelNode **channel, char *buffer_channel)
{

  ChannelNode *curr;
  ChannelNode *prev = NULL;
  unsigned long id = (*channel)->channel_number;

  if (minor_list[minor_number].head == NULL)
  {
    minor_list[minor_number].head = *channel;
    (*channel)->buffer = buffer_channel;
    (*channel)->next = NULL;
    (*channel)->nbytes = 0;
  }

  else
  {
    curr = minor_list[minor_number].head;

    while (curr != NULL)
    {

      if (curr->channel_number == id)
      {

        kfree(curr->buffer);

        curr->buffer = buffer_channel;
        kfree(*channel);
        *channel = curr;
        break;
      }
      if (curr->channel_number > id)
      {
        if (prev != NULL)
        {
          prev->next = *channel;
        }
        else
        {
          minor_list[minor_number].head = (*channel);
        }

        (*channel)->next = curr;
        (*channel)->buffer = buffer_channel;
        break;
      }
      prev = curr;
      curr = curr->next;

      if (curr == NULL)
      {
        prev->next = *channel;
        (*channel)->next = NULL;
        (*channel)->buffer = buffer_channel;
      }
    }
  }
}

void copy_string(char *from, char *to, size_t bytes)
{
  size_t i;
  for (i = 0; i < bytes; ++i)
  {
    to[i] = from[i];
  }
}

//===============================================================
//==================== DEVICE SETUP =============================
//===============================================================

struct file_operations Fops = {
    .owner = THIS_MODULE,
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .unlocked_ioctl = device_ioctl,
    .release = device_release,
};

//================================================================

static int __init init_message_slot(void)
{

  int rc = -1;
  int i;

  rc = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &Fops);
  if (rc < 0)
  {
    printk(KERN_ALERT "%s registraion failed for  %d\n",
           DEVICE_RANGE_NAME, MAJOR_NUM);
    return rc;
  }

  // Initialize the minor array.
  for (i = 0; i < 256; i++)
  {
    minor_list[i].head = NULL;
  }

  printk("Registeration is successful. ");
  printk("If you want to talk to the device driver,\n");
  printk("you have to create a device file:\n");
  printk("mknod /dev/%s'i' c %d 'i'\n", DEVICE_RANGE_NAME, MAJOR_NUM);
  printk("Dont forget to rm the device file and "
         "rmmod when you're done\n");

  return SUCCESS;
}

//================================================================
//================================================================
static void __exit cleanup_message_slot(void)
{
  int i;
  ChannelNode *head;
  ChannelNode *tmp;

  // Free all the allocated space
  for (i = 0; i < 256; i++)
  {
    head = minor_list[i].head;
    while (head != NULL)
    {
      tmp = head->next;
      kfree(head);
      head = tmp;
    }
  }

  unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
  printk("cleanup is successful.\n");
}
//================================================================
//================================================================

module_init(init_message_slot);
module_exit(cleanup_message_slot);

//========================= END OF FILE =========================
