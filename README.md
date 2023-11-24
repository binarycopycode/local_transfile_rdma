# local_transfile_rdma

We test on `20230323.npcbuf`

## Npcbuf Data Preparation

For testing, download the data in issue and extract the data to the directory `/local_transfile_rdma/npcbuf`.

## RDMA RoCE preparation

```shell
sudo rdma link add rxe_name type rxe netdev xxx
```

`xxx` is your netdev's name

## Dependencies

- ibverbs

## Compile

```shell
mkdir build
cd build
cmake ..
make
cd ..
```

## Run

In one terminal:

```
./server_test 2333
```

In another terminal:

```shell
./client_test 172.31.50.173 2333
```

`server_ip` is your RoCE device ip

 Success info:

```
client receive imm_data
3875312
写入文件成功
```

then you can use `diff` to judge whether `20230323.npcbuf` is the same as `20230323-recv.npcbuf`.
