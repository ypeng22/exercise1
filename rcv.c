//receiving data packets from sender -window, writing to file
//changing ncp to send 15 termination packets
//change ncp to send only after receiving confirmation to accept connection
//making so that receiver can receive from multiple senders (moving 60-75 into the outer-most forloop somehow)
//add retry mechanism to sender if receiver is busy
#include "net_include.h"
#include "sendto_dbg.h"
#define NAME_LENGTH 80
#define WINDOW_SIZE 10
int gethostname(char *, size_t);

//void PromptForHostName(char *my_name, char *host_name, size_t max_len);

int main()
{
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
    char cur_window[WINDOW_SIZE][MAX_MESS_LEN];
    int cur;
    FILE *fr;
    int current_connection = 0;
    sendto_dbg_init(0);
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

    FD_ZERO(&mask);
    FD_ZERO(&write_mask);
    FD_ZERO(&excep_mask);
    FD_SET(sr, &mask);
    FD_SET((long)0, &mask); /* stdin */
    for (;;)
    {
        for (;;)
        {
            read_mask = mask;
            timeout.tv_sec = 10;
            timeout.tv_usec = 0;
            num = select(FD_SETSIZE, &read_mask, &write_mask, &excep_mask, &timeout);
            if (num > 0)
            {
                if (FD_ISSET(sr, &read_mask))
                {
                    from_len = sizeof(from_addr);
                    bytes = recvfrom(sr, mess_buf, sizeof(mess_buf), 0,
                                     (struct sockaddr *)&from_addr,
                                     &from_len);
                    mess_buf[bytes] = 0;
                    from_ip = from_addr.sin_addr.s_addr;
                    printf("bytes is %d\n", bytes);
                    if (from_ip == current_connection || current_connection == 0)
                    {
                        int packet_type;
                        memcpy(&packet_type, mess_buf, 4);
                        printf("packet type is: %d\n", packet_type);
                        if (packet_type == 0)
                        {
                            current_connection = from_ip;
                            char file_name[1400];
                            memcpy(file_name, mess_buf + 11, bytes - 11);
                            memcpy(host_name, mess_buf + 4, 7);
                            printf("file name is %s, currently talking with %s\n", file_name, host_name);
                            if ((fr = fopen(file_name, "w+")) == NULL)
                            {
                                perror("fopen");
                                exit(0);
                            }
                            //will have to move inside forloop somehow

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
                        }

                        if (packet_type == 3 && fr != NULL)
                        {
                            fclose(fr);
                            fr = NULL;
                            break;
                        }

                        if (packet_type == 1)
                        {
                            printf("got data\n");
                        }

                        printf("Received from (%d.%d.%d.%d): %s\n",
                               (htonl(from_ip) & 0xff000000) >> 24,
                               (htonl(from_ip) & 0x00ff0000) >> 16,
                               (htonl(from_ip) & 0x0000ff00) >> 8,
                               (htonl(from_ip) & 0x000000ff),
                               mess_buf);
                    }
                    else
                    {
                        printf("here is busy\n");
                        printf("AABReceived from (%d.%d.%d.%d): %s\n",
                               (htonl(from_ip) & 0xff000000) >> 24,
                               (htonl(from_ip) & 0x00ff0000) >> 16,
                               (htonl(from_ip) & 0x0000ff00) >> 8,
                               (htonl(from_ip) & 0x000000ff),
                               mess_buf);
                        fflush(0);
                        char busy[10];
                        int four = 4;
                        memcpy(busy, &four, 4);
                        printf("sending to %d from_ip is %d", from_addr.sin_addr.s_addr, from_ip);
                        int temp = sendto_dbg(sr, busy, strlen(busy), 0, (struct sockaddr *)&from_addr, sizeof(from_addr));
                        printf("sent %d\n", temp);
                    }
                }
            }
            else
            {
                printf(".");
                fflush(0);
            }
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
