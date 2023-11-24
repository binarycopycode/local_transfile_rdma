#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <iostream>
#include <string>
#include <malloc.h>

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <byteswap.h>

#include <infiniband/verbs.h>

/* Error Macro*/
inline void rdma_error(std::string msg)
{
    std::cerr << "rdma error: " << msg << std::endl;
}

struct IBRes
{
    struct ibv_context *ctx;
    struct ibv_pd *pd;
    struct ibv_mr *mr;
    struct ibv_cq *cq;
    struct ibv_qp *qp;
    struct ibv_port_attr port_attr;
    struct ibv_device_attr dev_attr;
    union ibv_gid gid;

    char *ib_buf;
    size_t ib_buf_size;
    size_t file_data_size;
    size_t recv_size;

    uint32_t rkey;
    uint64_t raddr;
};

/* Queue Pair meta exchange*/
struct QPInfo
{
    uint16_t lid;
    uint32_t qp_num;
    union ibv_gid gid;
    uint32_t rkey;
    uint64_t raddr;
} __attribute__((packed));

enum MsgType {
    MSG_WRITE_FINISH = 1,
};

#define IB_MTU IBV_MTU_4096
#define IB_PORT 1
#define IB_SL 0
#define IB_WR_ID_STOP 0xE000000000000000
#define NUM_WARMING_UP_OPS 500000
#define TOT_NUM_OPS 10000000
#define SIG_INTERVAL 1000

#define SOCK_SYNC_MSG "sync"

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t htonll(uint64_t x) { return bswap_64(x); }
static inline uint64_t ntohll(uint64_t x) { return bswap_64(x); }
#elif __BYTE_ORDER == __BIG_ENDIAN
static inline uint64_t htonll(uint64_t x) { return x; }
static inline uint64_t ntohll(uint64_t x) { return x; }
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif

ssize_t sock_read(int sock_fd, void *buffer, size_t len);
ssize_t sock_write(int sock_fd, void *buffer, size_t len);
int sock_create_bind(const char *port);
int sock_create_connect(const char *server_name, const char *port);
int sock_set_qp_info(int sock_fd, struct QPInfo *qp_info);
int sock_get_qp_info(int sock_fd, struct QPInfo *qp_info);

int modify_qp_to_rts(struct ibv_qp *qp, uint32_t qp_num, uint16_t lid, union ibv_gid r_gid);
// fill all of IBRES for client(is_server:false) and server(is_server:true)
int setup_ib(IBRes &ib_res, size_t file_size, bool is_server);
void close_ib_connection(IBRes &ib_res);

int connect_qp_client(IBRes &ib_res, std::string server_name, std::string sock_port);
int connect_qp_server(IBRes &ib_res, std::string sock_port);

int post_write_signaled(uint32_t req_size, uint32_t lkey, uint64_t wr_id,
                        struct ibv_qp *qp, char *buf,
                        uint64_t raddr, uint32_t rkey);
int post_write_imm_signaled(uint32_t req_size, uint32_t lkey, uint64_t wr_id,
                            struct ibv_qp *qp, char *buf,
                            uint64_t raddr, uint32_t rkey);
int post_recv(uint32_t req_size, uint32_t lkey, uint64_t wr_id,
              struct ibv_qp *qp, char *buf);