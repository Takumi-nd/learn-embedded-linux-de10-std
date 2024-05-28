#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define chardev_BYTES 128 // max number of characters to read from /dev/chardev

/* This code uses the character device driver /dev/chardev. The code reads the default
 * message from the driver and then prints it. After this the code changes the message in
 * a loop by writing to the driver, and prints each new message */
int main(int argc, char *argv[])
{
    FILE *keydev_fp, *swdev_fp, *hexdev_fp, *ledrdev_fp; // file pointer
    char keydev_buffer[chardev_BYTES]; 
    char swdev_buffer[chardev_BYTES]; 
    char new_msg[128];
    int key, sw;
    int count = 0;

    if ((keydev_fp = fopen("/dev/KEY", "r+")) == NULL)
    {
        fprintf(stderr, "Error opening /dev/chardev: %s\n", strerror(errno));
        return -1;
    }

    if ((keydev_fp = fopen("/dev/SW", "r+")) == NULL)
    {
        fprintf(stderr, "Error opening /dev/chardev: %s\n", strerror(errno));
        return -1;
    }

    if ((ledrdev_fp = fopen("/dev/LEDR", "r+")) == NULL)
    {
        fprintf(stderr, "Error opening /dev/chardev: %s\n", strerror(errno));
        return -1;
    }

    if ((hexdev_fp = fopen("/dev/HEX", "r+")) == NULL)
    {
        fprintf(stderr, "Error opening /dev/chardev: %s\n", strerror(errno));
        return -1;
    }

    while(1) {
        while (fgets (keydev_buffer, chardev_BYTES, keydev_fp) != NULL);
        while (fgets (swdev_buffer, chardev_BYTES, swdev_fp) != NULL);
        sscanf(keydev_buffer, "%x\n", &key);
        sscanf(swdev_buffer, "%x\n", &sw);

        if (key != 0) { 
            count += sw;
            // if (count > 9) {
            //     count = 0;
            // }
            sprintf(new_msg, "%x\n", count);
            fputs(new_msg, hexdev_fp);
            fflush(hexdev_fp);
        }

        fputs(swdev_buffer, ledrdev_fp);
        fflush(ledrdev_fp);

        sleep (1);
    }

    fclose (ledrdev_fp);
    fclose (hexdev_fp);
    return 0;
}