# Kernel Programming - Message Slot IPC Mechanism
## Introduction
The Kernel Programming Project - Message Slot IPC Mechanism provided me with valuable experience in kernel programming and deepened my understanding of inter-process communication (IPC), kernel modules, and drivers. The project focused on designing and implementing a new IPC mechanism called a message slot, which facilitated communication between processes through a character device file.
## Objective

The main objective of this project was to develop a kernel module that would offer a message slot IPC mechanism, enabling efficient communication between processes. The implementation involved defining the necessary file operations, such as ```device_open```, ```device_ioctl```, ```device_read```, and ```device_write```, and ensuring adherence to the specified message slot interface.

## Key Features
1. Message Slot Device Files: The project introduced message slots as character device files managed by the message slot device driver. Multiple message channels could be active simultaneously, allowing concurrent usage by multiple processes. Each message slot file had a unique minor number for effective differentiation.

2. Semantics of Message Slot File Operations: Special semantics were implemented for ioctl, write, and read file operations. For instance, the ioctl command, MSG_SLOT_CHANNEL, allowed processes to specify the desired message channel. The write operation sent non-empty messages of up to 128 bytes to the channel, while the read operation retrieved the last written message.

## Implementation Highlights
* Utilized a hard-coded major number (235) for simplicity, although dynamic allocation is typically recommended.
* Ensured proper memory management using kmalloc() with the GFP_KERNEL flag for memory allocation.
* Supported up to 256 message slot device files.
* Achieved atomic reads and writes to ensure successful processing of the entire message.
* Deallocated memory upon closing of message slot files using the release method.


