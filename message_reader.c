#include "message_slot.h"

#include <fcntl.h>     /* open */
#include <unistd.h>    /* exit */
#include <sys/ioctl.h> /* ioctl */
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

int main(int args, char **argv)
{

    if (args != 3)
    {
        printf("The number of valid arguments should be: 2, but the actual number\n");
        printf("of arguments that provided is: %d", (args - 1));
        exit(EXIT_FAILURE);
    }

    int file_desc, ret_val;
    ssize_t read_size;
    char *file_path = argv[1];
    int channel_id = atoi(argv[2]);
    char buffer[BUF_LEN];

    file_desc = open(file_path, O_RDWR);
    if (file_desc < 0)
    {
        perror("Can't open device file: ");
        exit(EXIT_FAILURE);
    }

    ret_val = ioctl(file_desc, MSG_SLOT_CHANNEL, channel_id);
    if (ret_val < 0)
    {
        perror("There was a problem with the channel id: ");
        exit(EXIT_FAILURE);
    }

    read_size = read(file_desc, buffer, BUF_LEN);

    if (read_size < 0)
    {
        perror("There was a problem with read the message,\n One possible option is that there isn't channel with that id: ");
        exit(EXIT_FAILURE);
    }

    ret_val = close(file_desc);
    if (ret_val < 0)
    {
        perror("There was a problem with close the file: ");
        exit(EXIT_FAILURE);
    }

    ret_val = write(1, buffer, read_size);
    if (ret_val != read_size)
    {
        perror("There was a problem with stdout: ");
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}
