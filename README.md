# Go-Back-N

## How to Run

```
make
./run-server.sh <LOSS_RATE> <ERR_RATE> <HOST_NAME>
./run-client.sh <LOSS_RATE> <ERR_RATE> <HOST_NAME> <filename>
```

For example, to run both servers on localhost with `LOSS_RATE` of 0.1 and `ERR_RATE` of 0.1:<br/>
`./run-server.sh 0.1 0.1 localhost`<br/>
`./run-client.sh 0.1 0.1 localhost filename`<br/>

Then a
