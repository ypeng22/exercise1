#include "net_include.h"
#include <sys/time.h>
#include "sendto_dbg.h"
#define NAME_LENGTH 80
#define RESEND_TIME 5000
#define WINDOW_SIZE 10
int gethostname(char *, size_t);

void PromptForHostName(char *my_name, char *host_name, size_t max_len);

//cant use struct sending, format is packet_type packet_number data size, and data as a byte array
struct Packet
{
    int packet_type;
    int packet_number;
    int data_size;
    char data[MAX_MESS_LEN];
};

int main(int argc, char **argv)
{
    // INPUT ARGUMENTS
    char *loss_rate_percent = argv[0];
    char *source_file_name = argv[1];
    char *dest_file_name = argv[2]; //<dest_file_name>@<comp_name>
    printf("loss rate is %d, source file is %s, dest file is %s", loss_rate_percent, source_file_name, dest_file_name);

    struct sockaddr_in name;
    struct sockaddr_in send_addr;
    struct sockaddr_in from_addr;
    socklen_t from_len;
    struct hostent h_ent;
    struct hostent *p_h_ent;
    char host_name[NAME_LENGTH] = {'\0'};
    char my_name[NAME_LENGTH] = {'\0'};
    int host_num;
    int from_ip;
    int ss, sr;
    fd_set mask;
    fd_set read_mask, write_mask, excep_mask;
    int bytes;
    int num;
    char mess_buf[MAX_MESS_LEN];
    char input_buf[80];
    struct timeval timeout;
    int cur = 0;
    int total = 2147483640;
    struct timeval cur_time;
    // FILE I/O
    char buf[MAX_MESS_LEN];
    int nread, nwritten;
    struct Packet cur_window[WINDOW_SIZE] = {NULL};
    struct timeval cur_window_times[WINDOW_SIZE] = {0, 0}; //example usage: gettimeofday(cur_window_times[0], NULL) will put current time in array
    int cur_window_acks[WINDOW_SIZE] = {0};
    FILE *fr;
    sendto_dbg_init(0);
    /* Open the source file for reading */
    if ((fr = fopen(source_file_name, "r")) == NULL)
    {
        perror("fopen");
        exit(0);
    }
    printf("Opened %s for reading...\n", source_file_name);

    sr = socket(AF_INET, SOCK_DGRAM, 0); /* socket for receiving (udp) */
    if (sr < 0)
    {
        perror("Ucast: socket");
        exit(1);
    }

    name.sin_family = AF_INET;
    name.sin_addr.s_addr = INADDR_ANY;
    name.sin_port = htons(PORT);

    if (bind(sr, (struct sockaddr *)&name, sizeof(name)) < 0)
    {
        perror("Ucast: bind");
        exit(1);
    }

    ss = socket(AF_INET, SOCK_DGRAM, 0); /* socket for sending (udp) */
    if (ss < 0)
    {
        perror("Ucast: socket");
        exit(1);
    }

    PromptForHostName(my_name, host_name, NAME_LENGTH);

    p_h_ent = gethostbyname(host_name);
    if (p_h_ent == NULL)
    {
        printf("Ucast: gethostbyname error.\n");
        exit(1);
    }

    memcpy(&h_ent, p_h_ent, sizeof(h_ent));
    memcpy(&host_num, h_ent.h_addr_list[0], sizeof(host_num));

    send_addr.sin_family = AF_INET;
    send_addr.sin_addr.s_addr = host_num;
    send_addr.sin_port = htons(PORT);

    FD_ZERO(&mask);
    FD_ZERO(&write_mask);
    FD_ZERO(&excep_mask);
    FD_SET(sr, &mask);
    FD_SET((long)0, &mask); /* stdin */

    //start connection
    char starter[1412];
    char test2[6];
    char test3[1400];
    char myhostname[80] = {'/0'};
    gethostname(myhostname, 79);
    int one = 0;
    int test;
    memcpy(starter, &one, sizeof(one));
    //sprintf(starter, "%d", one);
    memcpy(starter + 4, myhostname, 7);
    memcpy(starter + 11, dest_file_name, sizeof(dest_file_name));

    memcpy(&test, starter, 4);
    memcpy(test2, starter + 4, 7);
    memcpy(test3, starter + 11, sizeof(dest_file_name));
    printf("Outgoing packet: %s size is %d\n", starter, sizeof(starter));
    printf("got %d and %s and %s\n", test, test2, test3);
    for (int i = 0; i < 50; i++)
    {
        printf("%c", starter[i]);
    }
    sendto_dbg(ss, starter, sizeof(starter), 0,
               (struct sockaddr *)&send_addr, sizeof(send_addr));
    for (;;)
    {
        for (int i = 0; i < WINDOW_SIZE; i++)
        {
            if (cur_window_times[i].tv_sec == 0 && !feof(fr))
            {

                /* Read in a chunk of the file */
                nread = fread(buf, 1, MAX_MESS_LEN, fr);
                printf("here\n");
                // else
                // {
                //     printf("An error occurred...\n");
                //     exit(0);
                // }

                cur_window[i].packet_type = 1;
                cur_window[i].packet_number = cur + i;
                cur_window[i].data_size = nread;
                memcpy(&cur_window[i].data, &buf, nread);
                cur_window[i].data[nread] = 0;
                gettimeofday(&cur_window_times[i], NULL);
                cur_window_times[i].tv_sec = 0;
                cur_window_times[i].tv_usec = 0;
                cur_window_acks[i] = 0;
                /* Did we reach the EOF? */
                //may have to make cur window length window_size + 1
                if (feof(fr))
                {
                    printf("Finished reading.\n");
                    cur_window[i + 1].packet_type = 2;
                    cur_window[i + 1].packet_number = cur + i + 2;
                    total = cur + i + 1;
                    break;
                }
            }
        }

        for (int i = 0; i < WINDOW_SIZE; i++)
        {
            // If packet has not been acked and the timeout period has elapsed
            gettimeofday(&cur_time, NULL);
            if (!cur_window_acks[i] && cur_time.tv_usec - cur_window_times[i].tv_usec > RESEND_TIME)
            {
                // Send Packet
                char sender[1412];
                memcpy(sender, &cur_window[i].packet_type, 4);
                memcpy(sender + 4, &cur_window[i].packet_number, 4);
                memcpy(sender + 8, &cur_window[i].data_size, 4);
                printf("%d, %d, %d \n", cur_window[i].packet_type, cur_window[i].packet_number, cur_window[i].data_size);
                memcpy(sender + 12, &cur_window[i].data, cur_window[i].data_size);
                cur_window_times[i].tv_sec = cur_time.tv_sec;
                cur_window_times[i].tv_usec = cur_time.tv_usec;
                printf("Outgoing packet1: %s\n", sender);
                sendto_dbg(ss, sender, sizeof(sender), 0,
                           (struct sockaddr *)&send_addr, sizeof(send_addr));
                if (cur_window[i].packet_type == 2)
                {
                    break;
                }
            }
        }

        read_mask = mask;
        timeout.tv_sec = 10;
        timeout.tv_usec = 0;
        num = select(FD_SETSIZE, &read_mask, &write_mask, &excep_mask, &timeout);

        if (num > 0)
        {
            printf("in num\n");
            // INCOMING PACKET FROM RECIEVER
            if (FD_ISSET(sr, &read_mask))
            {
                printf("HERE AND RECEIVING");
                fflush(0);
                from_len = sizeof(from_addr);
                bytes = recvfrom(sr, mess_buf, sizeof(mess_buf), 0,
                                 (struct sockaddr *)&from_addr,
                                 &from_len);
                mess_buf[bytes] = 0;
                from_ip = from_addr.sin_addr.s_addr;
                int ack_type;
                memcpy(&ack_type, mess_buf, 4);

                if (ack_type == 1) //assuming youre byte array as char array
                {
                    int cumulative_ack;
                    memcpy(&cumulative_ack, mess_buf + 4, 4);
                    //mark as acked
                    for (int p = cumulative_ack - cur; p < 0; p--)
                    {
                        cur_window_acks[p] = 1;
                    }
                    //cur is the next one we want to send
                    //mark some as nacked
                    int cur_pointer = 8;
                    while (cur_pointer < bytes)
                    {
                        int nack; // = mess_buf[cur_pointer] + mess_buf[cur_pointer] << 8 + mess_buf[cur_pointer] << 16 + mess_buf[cur_pointer] << 24;
                        memcpy(&nack, mess_buf + cur_pointer, 4);
                        cur_window_acks[nack - cur] = 0;
                    }

                    printf("Received ack from (%d.%d.%d.%d): %s\n",
                           (htonl(from_ip) & 0xff000000) >> 24,
                           (htonl(from_ip) & 0x00ff0000) >> 16,
                           (htonl(from_ip) & 0x0000ff00) >> 8,
                           (htonl(from_ip) & 0x000000ff),
                           mess_buf);

                    // TODO: Shift window if necessary
                    int j = 0;
                    while (cur_window_acks[j] == 1)
                    {
                        j++;
                    }
                    cur += j;
                    for (int k = 0; k < WINDOW_SIZE - j; k++)
                    {
                        cur_window[k] = cur_window[k + j];
                        cur_window_times[k] = cur_window_times[k + j];
                        cur_window_acks[k] = cur_window_acks[k + j];
                    }
                    for (int k = j; k < WINDOW_SIZE; k++)
                    {
                        *(cur_window[k].data) = 0;
                        cur_window_times[k].tv_sec = 0;
                        cur_window_times[k].tv_usec = 0;
                        cur_window_acks[k] = 0;
                    }
                }
                else if (ack_type == 4)
                {
                    printf("target host is busy\n");
                }
            }
        }
        if (cur == total)
        { //and all packets in window are acked
            break;
        }
    }

    return 0;
}

void PromptForHostName(char *my_name, char *host_name, size_t max_len)
{

    char *c;

    gethostname(my_name, max_len);
    printf("My host name is %s.\n", my_name);

    printf("\nEnter host to send to:\n");
    if (fgets(host_name, max_len, stdin) == NULL)
    {
        perror("Ucast: read_name");
        exit(1);
    }

    c = strchr(host_name, '\n');
    if (c)
        *c = '\0';
    c = strchr(host_name, '\r');
    if (c)
        *c = '\0';

    printf("Sending from %s to %s.\n", my_name, host_name);
}
