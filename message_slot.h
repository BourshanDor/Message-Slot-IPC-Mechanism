#ifndef MESSAGE_SLOT_H
#define MESSAGE_SLOT_H

#include <linux/ioctl.h>
#include <stddef.h>

#define MAJOR_NUM 235

typedef struct Node
{
    unsigned long channel_number;
    char *buffer;
    size_t nbytes;
    struct Node *next;
} ChannelNode;

typedef struct List
{
    ChannelNode *head;

} ChannelList;

// Set the message of the device driver
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned long)
#define DEVICE_RANGE_NAME "slot"
#define BUF_LEN 128
#define SUCCESS 0
#define FAIL 1
#define EXISTS 2

#endif
