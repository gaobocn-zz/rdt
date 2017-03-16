# Go-Back-N
Bo Gao, bg447

## How to Run

```
make
./run-server.sh <LOSS_RATE> <ERR_RATE> <CLIENT_HOST_NAME>
./run-client.sh <LOSS_RATE> <ERR_RATE> <SERVER_HOST_NAME> <filename>
```

For example, to run both servers and client on localhost with `LOSS_RATE` of 0.1 and `ERR_RATE` of 0.1:<br/>
`./run-server.sh 0.1 0.1 localhost`<br/>
`./run-client.sh 0.1 0.1 localhost filename`<br/>
Then a file with the same file name will appear in the `store` dir at server's location.

## Implementation

### Checksumming
I first pack all data (including header fields and payload) and leave the checksum field empty, then I do a checksumming of the whole packet and fill it in. So the checksum of the whole packet should be 0.
### Sender
The sender sends out a window of packets, then sets a timer and waits for ACKs. If received in order ACKs, move the sending window forward, if received out of order ACKs or corrupted packets or timeout, resend the remaining unACKed packets and reset the timer and wait for ACKs again.
### Receiver
If received in order packets, send out ACKs and move receiving window forward. Otherwise send out duplicate ACKs.
### Start of Transmission
The sender will first send over the file name as well as the length of the file. Then the receiver knows about the number of packets to be sent.
### Connection Teardown
The side sending out the close signal set a timer and waits for the other side's packets, it drops any ACKs (since everything is already ACKed), and for any datagram coming resends an ACK and resets timer. The timer should be longer than normal RTT, because we need to make sure that every packets sent by the other side is ACKed, otherwise the other side will be left hanging.
### Congestion Control
I modified the old code to support congestion control. The sender switches between fast (n = 8) and slow (n = 4) mode.

## Inefficiencies of this Protocol
### Performance
In fact this protocol is full of inefficiencies, with the most obvious one being that every time a packet / ACK gets lost / corrupted, we need to resend all the remaining unACKed packets. And the solution is to use Selective Repeat, where receiver buffers all the packets in the window and sender keeps one timer for each packet and resend the single packet when it's timeout. There are some cons in selective repeat as well, e.g. retransmissions can only be triggered by timeout. In Go-Back-N when we received corrupted or out of order packets we can use duplicate ACKs to trigger fast retransmission, but in selective repeat we have to wait for each packets to timeout. The solution is to apply the cumulative ACKs and fast retransmission mechanism in TCP.

### Malicious Client
Suppose we have a malicious client sending wrong SNs and it tries to occupy all server time. I think this can be solved by rate-limiting mechanism, like token bucket or leaky bucket. We can set up a rate limit for our server, e.g. 5 packets of 2048 Bytes per second, and any packets coming at a higher rate will simply get dropped without processing. It is possible though that the client is sending so fast that it overwhelms the network, which is basically a denial-of-service attack. I don't think we can solve that in the transport layer protocol, but we can use upstream filtering or link layer mechanism to block the malicious IP.

As for invalid SNs as well as out-of-range SN, I don't see that as a problem because we can just set a rule for SNs and keep waiting for the next in order SN, any packets not complying will be dropped.
