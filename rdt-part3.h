/**************************************************************
rdt-part3.h
Student name: Gao Bo
Student No. : 3035211402
Date and version:
Development platform: Ubuntu 13.10
Development language: C
Compilation:
  Can be compiled with gcc version 4.8.1 (Ubuntu/Linaro 4.8.1-10ubuntu9)
 *****************************************************************/

#ifndef RDT3_H
#define RDT3_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>

#define LOCAL_HOST "127.0.0.1"
#define U32_SIZE 4
#define U16_SIZE 2
#define HEADER_LEN (U32_SIZE * 2 + U16_SIZE)
#define PAYLOAD_LEN 2048

#define TIMEOUT 50000 //50 milliseconds
#define TWAIT 10 * TIMEOUT //Each peer keeps an eye on the receiving
//end for TWAIT time units before closing
//For retransmission of missing last ACK
#define W 9

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))

//----- Type defines ----------------------------------------------------------
typedef unsigned char u8b_t; // a char
typedef unsigned short u16b_t; // 16-bit word
typedef unsigned int u32b_t; // 32-bit word

extern float LOSS_RATE, ERR_RATE;
static int revSeq = 0, sendSeq = 0;

// int revSeq = 0;
// int sendSeq = 0;

struct timeval tv1, tv2;
fd_set readfds;

/* this function is for simulating packet loss or corruption in an unreliable channel */
/***
Assume we have registered the target peer address with the UDP socket by the connect()
function, udt_send() uses send() function (instead of sendto() function) to send
a UDP datagram.
***/
int udt_send(int fd, void* pkt, int pktLen, unsigned int flags) {
    double randomNum = 0.0;

    /* simulate packet loss */
    //randomly generate a number between 0 and 1
    randomNum = (double)rand() / RAND_MAX;
    if (randomNum < LOSS_RATE) {
        //simulate packet loss of unreliable send
        printf("WARNING: udt_send: Packet lost in unreliable layer!!!!!!\n");
        return pktLen;
    }

    /* simulate packet corruption */
    //randomly generate a number between 0 and 1
    randomNum = (double)rand() / RAND_MAX;
    if (randomNum < ERR_RATE) {
        //clone the packet
        u8b_t errmsg[pktLen];
        memcpy(errmsg, pkt, pktLen);
        //change a char of the packet
        int position = rand() % pktLen;
        if (errmsg[position] > 1) {
            errmsg[position] -= 2;
        } else {
            errmsg[position] = 254;
        }
        printf("WARNING: udt_send: Packet corrupted in unreliable layer!!!!!!\n");
        return send(fd, errmsg, pktLen, 0);
    } else {
        // transmit original packet
        return send(fd, pkt, pktLen, 0);
    }
}

/* this function is for calculating the 16-bit checksum of a message */
/***
Source: UNIX Network Programming, Vol 1 (by W.R. Stevens et. al)
***/
u16b_t checksum(u8b_t* msg, u16b_t bytecount) {
    u32b_t sum = 0;
    u16b_t* addr = (u16b_t*)msg;
    u16b_t word = 0;

    // add 16-bit by 16-bit
    while (bytecount > 1) {
        sum += *addr++;
        bytecount -= 2;
    }

    // Add left-over byte, if any
    if (bytecount > 0) {
        *(u8b_t*)(&word) = *(u8b_t*)addr;
        sum += word;
    }

    // Fold 32-bit sum to 16 bits
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    word = ~sum;

    return word;
}

//----- Type defines ----------------------------------------------------------

// define your data structures and global variables in here

/* pack the header and the payload together, including the calculation of
checksum.
ack = 0 means it's data, ack = 1 means it's ACK.
Returns pktLen.
*/
int make_pkt(char* packed_pkt, char* payLoad, int payLen, int ack, int seq) {
    char* ptr = packed_pkt;
    u32b_t seq_t = htonl(seq);
    u32b_t ack_t = htonl(ack);
    u16b_t checkSum = htons(0);

    memcpy(ptr, &ack_t, U32_SIZE);
    ptr += U32_SIZE;
    memcpy(ptr, &seq_t, U32_SIZE);
    ptr += U32_SIZE;
    memcpy(ptr, &checkSum, U16_SIZE);
    ptr += U16_SIZE;

    if (payLoad != NULL && payLen > 0) {
        memcpy(ptr, payLoad, payLen);
    }

    /* First I pack all the fields needed including the checksum field into the
    packed_pkt, then I perform checksum() on this whole pkt, and re-pack the
    new checksum field into the packed_pkt.
    */

    int pktLen = payLen + HEADER_LEN;
    checkSum = checksum((u8b_t*)packed_pkt, (u16b_t)pktLen);
    ptr -= U16_SIZE;
    memcpy(ptr, &checkSum, U16_SIZE);

    // printf("Size of pkt: %d, pktLen: %d\n", length(packed_pkt), pktLen);

    return pktLen;
}

/* unpack the received pkt, and extract the payload, ack, seq form the pkt,
also check whether the pkt has been corrupted by performing checksum().
Returns -1 for corrupted pkt, dataLen otherwise.
*/
int unpack_pkt(char* packed_pkt, char* payLoad, int pktLen, int* ack, int* seq) {
    char* ptr = packed_pkt;
    u32b_t seq_t, ack_t;
    int msgLen = pktLen - HEADER_LEN;

    memcpy(&ack_t, ptr, U32_SIZE);
    ptr += U32_SIZE;
    memcpy(&seq_t, ptr, U32_SIZE);
    ptr += U32_SIZE + U16_SIZE;
    *ack = ntohl(ack_t);
    *seq = ntohl(seq_t);

    if (payLoad != NULL && msgLen > 0) {
        memcpy(payLoad, ptr, msgLen);
    }

    if (checksum((u8b_t*)packed_pkt, pktLen) != (u16b_t)0)
        return -1;

    return (msgLen);
}

/* Wait for ACK with seq# of 'seq',
return:
-1 for error
0 for time out
1 for ACK
3 for corrupted pkt
4 for in order datagram
5 for out of order datagram
*/
char buf[HEADER_LEN + PAYLOAD_LEN];

int time_out(int fd, int seq, char* msg, int* msgLen, int& rSeq, struct timeval& tv) {

    FD_ZERO(&readfds);

    // add our descriptors to the set
    FD_SET(fd, &readfds);

    // wait until either socket has data ready to be recv()d (timeout 10.5 secs)
    int rv = -1;
    while (rv == -1) {
        rv = select(fd + 1, &readfds, NULL, NULL, &tv);
        if (rv == -1) {
            perror("select"); // error occurred in select()
        } else if (rv == 0) {
            printf("time_out: Timeout occurred!\n");
        } else {
            // one or both of the descriptors have data
            if (FD_ISSET(fd, &readfds)) {
                int pktLen = recv(fd, buf, sizeof buf, 0);
                if (pktLen == -1)
                    return -1;
                int r_ack, r_seq, r_msgLen;
                if ((r_msgLen = unpack_pkt(buf, msg, pktLen, &r_ack, &r_seq) == -1)) {
                    printf("time_out: received corrupted pkt.\n");
                    return 3;
                }
                if (r_ack == 0) {
                    if (r_seq == revSeq) {
                        printf("time_out: Received in order datagram, seq#: %d\n", r_seq);
                        if (msgLen != NULL)
                             *msgLen = r_msgLen;
                        return 4;
                    } else {
                        printf("time_out: Received out of order datagram, seq#: %d\n", r_seq);
                        printf("time_out: Send (expected#-1) ACK, with seq: %d\n", revSeq - 1);
                        char ack_pkt[HEADER_LEN];
                        make_pkt(ack_pkt, NULL, 0, 1, revSeq - 1);
                        udt_send(fd, ack_pkt, HEADER_LEN, 0);
                        return 5;
                    }
                } else {
                    rSeq = r_seq;
                    return 1;
                }
            } else {
                rv = -1;
            }
        }
    }

    return rv;
}

int rdt_socket();
int rdt_bind(int fd, u16b_t port);
int rdt_target(int fd, char* peer_name, u16b_t peer_port);
int rdt_send(int fd, char* msg, int length);
int rdt_recv(int fd, char* msg, int length);
int rdt_close(int fd);

/* Application process calls this function to create the RDT socket.
   return -> the socket descriptor on success, -1 on error
*/
int rdt_socket() {
    tv1.tv_sec = 0;
    tv1.tv_usec = TIMEOUT;
    tv2.tv_sec = 0;
    tv2.tv_usec = TWAIT;
    return socket(AF_INET, SOCK_DGRAM, 0);
}

/* Application process calls this function to specify the IP address
   and port number used by itself and assigns them to the RDT socket.
   return -> 0 on success, -1 on error
*/
int rdt_bind(int fd, u16b_t port) {
    printf("rdt_bind: fd: %d\n", fd);
    struct sockaddr_in my_addr;
    my_addr.sin_family = AF_INET; // host byte order
    my_addr.sin_port = htons(port); // u_int16_t, network byte order
    my_addr.sin_addr.s_addr = inet_addr(LOCAL_HOST); // u_int32_t, network byte order
    memset(&(my_addr.sin_zero), '\0', 8); // zero the rest of the struct
    // don't forget your error checking for bind():
    return bind(fd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr_in));
}

/* Application process calls this function to specify the IP address
   and port number used by remote process and associates them to the
   RDT socket.
   return -> 0 on success, -1 on error
*/
int rdt_target(int fd, char* peer_name, u16b_t peer_port) {
    struct sockaddr_in remote_addr;
    remote_addr.sin_family = AF_INET; // host byte order
    remote_addr.sin_port = htons(peer_port); // u_int16_t, network byte order
    remote_addr.sin_addr.s_addr = inet_addr(LOCAL_HOST); // u_int32_t, network byte order
    memset(&(remote_addr.sin_zero), '\0', 8); // zero the rest of the struct
    return connect(fd, (struct sockaddr*)&remote_addr, sizeof(struct sockaddr_in));
}

/* Application process calls this function to transmit a message to
   target (rdt_target) remote process through RDT socket.
   msg    -> pointer to the application's send buffer
   length -> length of application message
   return -> size of data sent on success, -1 on error
*/

char pkt[W][HEADER_LEN + PAYLOAD_LEN];
int pktLen[W];
int rdt_send(int fd, char* msg, int length) {
    //must use the udt_send() function to send data via the unreliable layer
    int unpktedLen = length;
    char* ptrM = msg;
    int l = 0, r = 0, i = 0;
    // first send out the message in several pkts
    for (r = 0; r < W && 0 < unpktedLen; ++r) {
        // pkt[r] = new char[HEADER_LEN+PAYLOAD_LEN];
        int packLen = MIN(PAYLOAD_LEN, unpktedLen);
        pktLen[r] = make_pkt(pkt[r], ptrM, packLen, 0, sendSeq + r);
        ptrM += packLen;
        unpktedLen -= packLen;
    } // after this loop, the pkt window is [l, r).

    // make_pkt(char *packed_pkt, char *payLoad, int payLen, int ack, int seq)

    // the pkt window is [l, r)
    while (1) {
        for (i = l; i < r; ++i) {
            printf("rdt_send: Send pkt of size: %d, with seq#: %d\n", pktLen[i], sendSeq + i);
            udt_send(fd, pkt[i], pktLen[i], 0);
        }

        int rSeq, temp;
        struct timeval tv = tv1; // set one timer for all the pkts sent in one round
        for (i = l; i < r; ++i) // allow the timer to receive W ACK in one round
            if ((temp = time_out(fd, sendSeq, NULL, NULL, rSeq, tv)) == 1) {
                printf("rdt_send: received ACK with seq#: %d (%d-%d)\n", rSeq, sendSeq, (sendSeq + r - 1));
                l = MAX(l, rSeq - sendSeq + 1);
            }
            else if (temp == 0) { // up to l-1 have been ACKed
                break; // break this round if timeout occurs
            }
        if (l == r) {
            break;
        }
    }
    printf("rdt_send: pkts up to %d have been ACKed\n", sendSeq + l - 1);
    sendSeq += (r);
    return length;
}

/* Application process calls this function to wait for a message
   from the remote process; the caller will be blocked waiting for
   the arrival of the message.
   msg    -> pointer to the receiving buffer
   length -> length of receiving buffer
   return -> size of data received on success, -1 on error
*/
int rdt_recv(int fd, char* msg, int length) {
    char* pkt = new char[HEADER_LEN + PAYLOAD_LEN];
    char ack_pkt[HEADER_LEN];
    int pktLen, msgLen;
    int ack, seq;

    // first recieve one pkt
    pktLen = recv(fd, pkt, HEADER_LEN + PAYLOAD_LEN, 0);
    if (pktLen == -1) {
        msgLen = -1;
    } else {
        printf("rdt_recv: Received a packet of size: %d\n", pktLen);
        msgLen = unpack_pkt(pkt, msg, pktLen, &ack, &seq);
        if (ack == 1) {
            printf("rdt_recv: received ACK, discard.\n");
            msgLen = -1;
        } else {
            if (msgLen == -1) {
                printf("rdt_recv: Datagram corrupted and discarded.\n");
            } else if (seq != revSeq) {
                printf("rdt_recv: Received out of order datagram, with seq#: %d\n", seq);
                msgLen = -1;
            } else {
                printf("rdt_recv: Received in order datagram of size: %d\nwith seq: %d\n", msgLen, seq);
            }
        }
    }

    if (msgLen != -1) { // if the pkt meets our requirement, ACK and return
        printf("rdt_recv: Send in order ACK, with seq: %d\n", revSeq);
        make_pkt(ack_pkt, NULL, 0, 1, revSeq);
        udt_send(fd, ack_pkt, HEADER_LEN, 0);
        revSeq++;
        return msgLen;
    }

    // otherwise, enter rdt_recv_time_out phase where we accept in order pkts
    // and discard out of order pkts
    make_pkt(ack_pkt, NULL, 0, 1, revSeq - 1);

    int rv = -1;
    while (1) {
        printf("rdt_recv: Send dup ACK, with seq: %d\n", revSeq - 1);
        udt_send(fd, ack_pkt, HEADER_LEN, 0);
        // set timer
        struct timeval tv = tv1;

        FD_ZERO(&readfds);

        // add our descriptors to the set
        FD_SET(fd, &readfds);

        rv = select(fd + 1, &readfds, NULL, NULL, &tv);
        if (rv == -1) {
            perror("select"); // error occurred in select()
        } else if (rv == 0) {
            printf("rdt_recv_time_out: Timeout occurred!\n");
        } else {
            // one or both of the descriptors have data
            if (FD_ISSET(fd, &readfds)) {
                int pktLen = recv(fd, buf, sizeof buf, 0);
                if (pktLen == -1)
                    return -1;
                int r_ack, r_seq;
                if ((msgLen = unpack_pkt(buf, msg, pktLen, &r_ack, &r_seq)) == -1) {
                    printf("rdt_recv_time_out: received corrupted pkt.\n");
                    continue;
                }
                if (r_ack == 0) {
                    if (r_seq == revSeq) {
                        printf("rdt_recv_time_out: Received in order datagram, size of: %d, seq#: %d\n", msgLen, r_seq);
                        printf("rdt_recv_time_out: Send in order ACK, with seq: %d\n", revSeq);
                        make_pkt(ack_pkt, NULL, 0, 1, revSeq);
                        udt_send(fd, ack_pkt, HEADER_LEN, 0);
                        revSeq++;
                        return msgLen;
                    } else {
                        printf("rdt_recv_time_out: Received out of order datagram, with seq#: %d\n", r_seq);
                    }
                } else {
                    printf("rdt_recv_time_out: Received ACK, discard.\n");
                }
            }
        }
    }
    // all pkts up to revSeq-1 have been successfully received
    return msgLen;
}

/* Application process calls this function to close the RDT socket.
*/
int rdt_close(int fd) {
    printf("\nIn rdt_close\n");
    char* ack_pkt = new char[HEADER_LEN];
    int pktLen = make_pkt(ack_pkt, NULL, 0, 1, revSeq - 1);
    int temp, rSeq;
    struct timeval tv = tv2;
    // beware, the rdt_close phase uses TWAIT, which is longer than TIMEOUT.
    // if there are data coming in, than reACK with revSeq-1, and reset timer
    while ((temp = time_out(fd, revSeq - 1, NULL, NULL, rSeq, tv)) == 5 || (temp == 3) || temp == 4) {
        tv = tv2;
        printf("rdt_close: received retransmission, send out dup ACK\n");
        udt_send(fd, ack_pkt, pktLen, 0);
    }
    printf("rdt_close: close socket\n");
    return close(fd);
}

#endif
