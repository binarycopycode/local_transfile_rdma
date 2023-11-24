#include "rdma_common.h"

/* socket communication --------------------------------------------------------- */

ssize_t sock_read(int sock_fd, void *buffer, size_t len)
{
    ssize_t nr, tot_read;
    char *buf = static_cast<char *>(buffer); // avoid pointer arithmetic on void pointer
    tot_read = 0;

    while (len != 0 && (nr = read(sock_fd, buf, len)) != 0)
    {
        if (nr < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else
            {
                return -1;
            }
        }
        len -= nr;
        buf += nr;
        tot_read += nr;
    }

    return tot_read;
}

ssize_t sock_write(int sock_fd, void *buffer, size_t len)
{
    ssize_t nw, tot_written;
    const char *buf = static_cast<const char *>(buffer); // avoid pointer arithmetic on void pointer

    for (tot_written = 0; tot_written < len;)
    {
        nw = write(sock_fd, buf, len - tot_written);

        if (nw <= 0)
        {
            if (nw == -1 && errno == EINTR)
            {
                continue;
            }
            else
            {
                return -1;
            }
        }

        tot_written += nw;
        buf += nw;
    }
    return tot_written;
}

int sock_create_bind(const char *port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sock_fd = -1, ret = 0;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;

    // ret = getaddrinfo(NULL, port, &hints, &result);
    ret = getaddrinfo(nullptr, port, &hints, &result);
    // check(ret==0, "getaddrinfo error.");
    if (ret != 0)
    {
        rdma_error("getaddrinfo error in sock create bind");
        if (result)
            freeaddrinfo(result);
        if (sock_fd > 0)
            close(sock_fd);
        return -1;
    }

    for (rp = result; rp != nullptr; rp = rp->ai_next)
    {
        sock_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock_fd < 0)
        {
            continue;
        }

        //20231124 re-use same port
        int optval = 1;
        setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
        setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

        ret = bind(sock_fd, rp->ai_addr, rp->ai_addrlen);
        if (ret == 0)
        {
            /* bind success */
            break;
        }

        close(sock_fd);
        sock_fd = -1;
    }

    // check(rp != NULL, "creating socket.");
    if (rp == nullptr)
    {
        rdma_error("creating socket error in sock create bind");
        if (result)
            freeaddrinfo(result);
        if (sock_fd > 0)
            close(sock_fd);
        return -1;
    }

    freeaddrinfo(result);
    return sock_fd;

    //  error:
    //     if (result) {
    //         freeaddrinfo(result);
    //     }
    //     if (sock_fd > 0) {
    //         close(sock_fd);
    //     }
    //     return -1;
}

int sock_create_connect(const char *server_name, const char *port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sock_fd = -1, ret = 0;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;

    ret = getaddrinfo(server_name, port, &hints, &result);
    // check(ret==0, "[ERROR] %s", gai_strerror(ret));
    if (ret != 0)
    {
        rdma_error("error in cock create connect  get addrinfo ret=0");
        if (result)
            freeaddrinfo(result);
        if (sock_fd != -1)
            close(sock_fd);
        return -1;
    }

    for (rp = result; rp != nullptr; rp = rp->ai_next)
    {
        sock_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock_fd == -1)
        {
            continue;
        }

        ret = connect(sock_fd, rp->ai_addr, rp->ai_addrlen);
        if (ret == 0)
        {
            /* connection success */
            break;
        }

        close(sock_fd);
        sock_fd = -1;
    }

    // check(rp!=NULL, "could not connect.");
    if (rp == nullptr)
    {
        rdma_error("error in cock create connect rp = nullptr");
        if (result)
            freeaddrinfo(result);
        if (sock_fd != -1)
            close(sock_fd);
        return -1;
    }

    freeaddrinfo(result);
    return sock_fd;

    //  error:
    //     if (result) {
    //         freeaddrinfo(result);
    //     }
    //     if (sock_fd != -1) {
    //         close(sock_fd);
    //     }
    //     return -1;
}

int sock_set_qp_info(int sock_fd, struct QPInfo *qp_info)
{
    int n;
    struct QPInfo tmp_qp_info;

    tmp_qp_info.lid = htons(qp_info->lid);
    tmp_qp_info.qp_num = htonl(qp_info->qp_num);
    tmp_qp_info.rkey = htonl(qp_info->rkey);
    tmp_qp_info.raddr = htonll(qp_info->raddr);
    memcpy(&tmp_qp_info.gid, &qp_info->gid, sizeof(union ibv_gid)); // 复制gid

    n = sock_write(sock_fd, (char *)&tmp_qp_info, sizeof(struct QPInfo));
    // check(n==sizeof(struct QPInfo), "write qp_info to socket.");
    if (n != sizeof(struct QPInfo))
    {
        rdma_error("write qp_info to socket error");
        return -1;
    }

    return 0;

    //  error:
    //     return -1;
}

int sock_get_qp_info(int sock_fd, struct QPInfo *qp_info)
{
    int n;
    struct QPInfo tmp_qp_info;

    n = sock_read(sock_fd, (char *)&tmp_qp_info, sizeof(struct QPInfo));
    // check(n==sizeof(struct QPInfo), "read qp_info from socket.");
    if (n != sizeof(struct QPInfo))
    {
        rdma_error("read qp_info from socket failed");
        return -1;
    }

    qp_info->lid = ntohs(tmp_qp_info.lid);
    qp_info->qp_num = ntohl(tmp_qp_info.qp_num);
    qp_info->rkey = ntohl(tmp_qp_info.rkey);
    qp_info->raddr = ntohll(tmp_qp_info.raddr);
    memcpy(&qp_info->gid, &tmp_qp_info.gid, sizeof(union ibv_gid)); // 复制gid

    return 0;

    //  error:
    //     return -1;
}
/* --------socket communication end---------------*/

// 20231110 add rgid
int modify_qp_to_rts(struct ibv_qp *qp, uint32_t target_qp_num, uint16_t target_lid, union ibv_gid r_gid)
{
    int ret = 0;

    /* change QP state to INIT */
    {
        // in cpp constructor should be ordered like defination of struct 20231120
        struct ibv_qp_attr qp_attr = {
            .qp_state = IBV_QPS_INIT,
            .qp_access_flags = IBV_ACCESS_LOCAL_WRITE |
                               IBV_ACCESS_REMOTE_READ |
                               IBV_ACCESS_REMOTE_ATOMIC |
                               IBV_ACCESS_REMOTE_WRITE,
            .pkey_index = 0,
            .port_num = IB_PORT,
        };

        ret = ibv_modify_qp(qp, &qp_attr,
                            IBV_QP_STATE | IBV_QP_PKEY_INDEX |
                                IBV_QP_PORT | IBV_QP_ACCESS_FLAGS);
        // check(ret == 0, "Failed to modify qp to INIT.");
        if (ret != 0)
        {
            rdma_error("Failed to modify qp to INIT.");
            return -1;
        }
    }

    /* Change QP state to RTR */
    {
        // chained designators are nonstandard in c++ 20231120
        struct ibv_qp_attr qp_attr = {
            .qp_state = IBV_QPS_RTR,
            .path_mtu = IB_MTU,
            .rq_psn = 0,
            .dest_qp_num = target_qp_num,
            .max_dest_rd_atomic = 1,
            .min_rnr_timer = 12,
        };
        qp_attr.ah_attr.grh.dgid = r_gid,
        qp_attr.ah_attr.grh.sgid_index = 1,
        qp_attr.ah_attr.grh.hop_limit = 1,

        qp_attr.ah_attr.dlid = target_lid,
        qp_attr.ah_attr.sl = IB_SL,
        qp_attr.ah_attr.src_path_bits = 0,

        // change is_global from 0 to 1 and add  gid
            qp_attr.ah_attr.is_global = 1,
        qp_attr.ah_attr.port_num = IB_PORT;

        ret = ibv_modify_qp(qp, &qp_attr,
                            IBV_QP_STATE | IBV_QP_AV |
                                IBV_QP_PATH_MTU | IBV_QP_DEST_QPN |
                                IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC |
                                IBV_QP_MIN_RNR_TIMER);
        // check(ret == 0, "Failed to change qp to rtr.");
        if (ret != 0)
        {
            rdma_error("Failed to change qp to rtr.");
            return -1;
        }
    }

    /* Change QP state to RTS */
    {
        // in cpp constructor should be ordered like defination of struct 20231120
        struct ibv_qp_attr qp_attr = {
            .qp_state = IBV_QPS_RTS,
            .sq_psn = 0,
            .max_rd_atomic = 1,
            .timeout = 14,
            .retry_cnt = 7,
            .rnr_retry = 7,
        };

        ret = ibv_modify_qp(qp, &qp_attr,
                            IBV_QP_STATE | IBV_QP_TIMEOUT |
                                IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY |
                                IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
        // check(ret == 0, "Failed to modify qp to RTS.");
        if (ret != 0)
        {
            rdma_error("Failed tFailed to modify qp to RTS.o modify qp to RTS.");
            return -1;
        }
    }

    return 0;
    // error:
    // 	return -1;
}

int connect_qp_server(IBRes &ib_res, std::string sock_port)
{
    int ret = 0, n = 0;
    int sockfd = 0;
    int peer_sockfd = 0;
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len = sizeof(struct sockaddr_in);
    char sock_buf[64] = {'\0'};
    struct QPInfo local_qp_info, remote_qp_info;

    sockfd = sock_create_bind(sock_port.c_str());
    // check(sockfd > 0, "Failed to create server socket.");
    if (sockfd <= 0)
    {
        rdma_error("Failed to create server socket.");
        if (peer_sockfd > 0)
            close(peer_sockfd);
        if (sockfd > 0)
            close(sockfd);
        return -1;
    }

    listen(sockfd, 5);

    peer_sockfd = accept(sockfd, (struct sockaddr *)&peer_addr,
                         &peer_addr_len);
    // check(peer_sockfd > 0, "Failed to create peer_sockfd");
    if (peer_sockfd <= 0)
    {
        rdma_error("Failed to create peer_sockfd");
        if (peer_sockfd > 0)
            close(peer_sockfd);
        if (sockfd > 0)
            close(sockfd);
        return -1;
    }

    /* init local qp_info */
    local_qp_info.lid = ib_res.port_attr.lid;
    local_qp_info.qp_num = ib_res.qp->qp_num;
    local_qp_info.rkey = ib_res.mr->rkey;
    local_qp_info.raddr = (uintptr_t)ib_res.ib_buf;
    // add gid 20231110
    local_qp_info.gid = ib_res.gid;

    /* get qp_info from client */
    ret = sock_get_qp_info(peer_sockfd, &remote_qp_info);
    // check(ret == 0, "Failed to get qp_info from client");
    if (ret != 0)
    {
        rdma_error("Failed to get qp_info from client");
        if (peer_sockfd > 0)
            close(peer_sockfd);
        if (sockfd > 0)
            close(sockfd);
        return -1;
    }

    /* send qp_info to client */
    ret = sock_set_qp_info(peer_sockfd, &local_qp_info);
    // check(ret == 0, "Failed to send qp_info to client");
    if (ret != 0)
    {
        rdma_error("Failed to send qp_info to client");
        if (peer_sockfd > 0)
            close(peer_sockfd);
        if (sockfd > 0)
            close(sockfd);
        return -1;
    }

    /* store rkey and raddr info */
    ib_res.rkey = remote_qp_info.rkey;
    ib_res.raddr = remote_qp_info.raddr;

    /* change send QP state to RTS */
    ret = modify_qp_to_rts(ib_res.qp, remote_qp_info.qp_num,
                           remote_qp_info.lid, remote_qp_info.gid);
    // check(ret == 0, "Failed to modify qp to rts");
    if (ret != 0)
    {
        rdma_error("Failed to modify qp to rts");
        if (peer_sockfd > 0)
            close(peer_sockfd);
        if (sockfd > 0)
            close(sockfd);
        return -1;
    }

    /* sync with clients */
    n = sock_read(peer_sockfd, sock_buf, sizeof(SOCK_SYNC_MSG));
    // check(n == sizeof(SOCK_SYNC_MSG), "Failed to receive sync from client");
    if (n != sizeof(SOCK_SYNC_MSG))
    {
        rdma_error("Failed to receive sync from client");
        if (peer_sockfd > 0)
            close(peer_sockfd);
        if (sockfd > 0)
            close(sockfd);
        return -1;
    }

    n = sock_write(peer_sockfd, sock_buf, sizeof(SOCK_SYNC_MSG));
    // check(n == sizeof(SOCK_SYNC_MSG), "Failed to write sync to client");
    if (n != sizeof(SOCK_SYNC_MSG))
    {
        rdma_error("Failed to write sync to client");
        if (peer_sockfd > 0)
            close(peer_sockfd);
        if (sockfd > 0)
            close(sockfd);
        return -1;
    }

    close(peer_sockfd);
    close(sockfd);

    return 0;

    // error:
    //     if (peer_sockfd > 0)
    //     {
    //         close(peer_sockfd);
    //     }
    //     if (sockfd > 0)
    //     {
    //         close(sockfd);
    //     }

    //     return -1;
}

int connect_qp_client(IBRes &ib_res, std::string server_name, std::string sock_port)
{
    int ret = 0, n = 0;
    int peer_sockfd = 0;
    char sock_buf[64] = {'\0'};

    struct QPInfo local_qp_info, remote_qp_info;

    peer_sockfd = sock_create_connect(server_name.c_str(),
                                      sock_port.c_str());
    // check(peer_sockfd > 0, "Failed to create peer_sockfd");
    if (ret != 0)
    {
        rdma_error("Failed to create peer_sockfd in connect_qp_client");
        if (peer_sockfd > 0)
            close(peer_sockfd);
        return -1;
    }

    local_qp_info.lid = ib_res.port_attr.lid;
    local_qp_info.qp_num = ib_res.qp->qp_num;
    local_qp_info.rkey = ib_res.mr->rkey;
    local_qp_info.raddr = (uintptr_t)ib_res.ib_buf;
    // add gid 20231113
    local_qp_info.gid = ib_res.gid;

    /* send qp_info to server */
    ret = sock_set_qp_info(peer_sockfd, &local_qp_info);
    // check(ret == 0, "Failed to send qp_info to server");
    if (ret != 0)
    {
        rdma_error("Failed to send qp_info to server in connect_qp_client");
        if (peer_sockfd > 0)
            close(peer_sockfd);
        return -1;
    }

    /* get qp_info from server */
    ret = sock_get_qp_info(peer_sockfd, &remote_qp_info);
    // check(ret == 0, "Failed to get qp_info from server");
    if (ret != 0)
    {
        rdma_error("Failed to get qp_info from server in connect_qp_client");
        if (peer_sockfd > 0)
            close(peer_sockfd);
        return -1;
    }

    /* store rkey and raddr info */
    ib_res.rkey = remote_qp_info.rkey;
    ib_res.raddr = remote_qp_info.raddr;

    /* change QP state to RTS */
    ret = modify_qp_to_rts(ib_res.qp, remote_qp_info.qp_num,
                           remote_qp_info.lid, remote_qp_info.gid);
    // check(ret == 0, "Failed to modify qp to rts");
    if (ret != 0)
    {
        rdma_error("Failed to modify qp to rts in connect_qp_client");
        if (peer_sockfd > 0)
            close(peer_sockfd);
        return -1;
    }

    /* sync with server */
    n = sock_write(peer_sockfd, sock_buf, sizeof(SOCK_SYNC_MSG));
    // check(n == sizeof(SOCK_SYNC_MSG), "Failed to write sync to client");
    if (n != sizeof(SOCK_SYNC_MSG))
    {
        rdma_error("Failed to write sync to client in connect_qp_client");
        if (peer_sockfd > 0)
            close(peer_sockfd);
        return -1;
    }

    n = sock_read(peer_sockfd, sock_buf, sizeof(SOCK_SYNC_MSG));
    // check(n == sizeof(SOCK_SYNC_MSG), "Failed to receive sync from client");
    if (n != sizeof(SOCK_SYNC_MSG))
    {
        rdma_error("Failed to receive sync from client in connect_qp_client");
        if (peer_sockfd > 0)
            close(peer_sockfd);
        return -1;
    }

    close(peer_sockfd);
    return 0;

    // error:
    //     if (peer_sockfd > 0)
    //     {
    //         close(peer_sockfd);
    //     }

    //     return -1;
}

int setup_ib(IBRes &ib_res, size_t file_size, bool is_server)
{
    int ret = 0;
    struct ibv_device **dev_list = NULL;
    memset(&ib_res, 0, sizeof(struct IBRes));
    ib_res.ctx = nullptr;
    ib_res.pd = nullptr;
    ib_res.mr = nullptr;
    ib_res.cq = nullptr;
    ib_res.qp = nullptr;
    ib_res.ib_buf = nullptr;

    /* get IB device list */
    dev_list = ibv_get_device_list(NULL);
    // check(dev_list != NULL, "Failed to get ib device list.");
    if (!dev_list)
    {
        rdma_error("Failed to get ib device list.");
        return -1;
    }

    /* create IB context */
    ib_res.ctx = ibv_open_device(*dev_list);
    // check(ib_res.ctx != NULL, "Failed to open ib device.");
    if (!ib_res.ctx)
    {
        if (dev_list != nullptr)
            ibv_free_device_list(dev_list);
        rdma_error("Failed to open ib device.");
        return -1;
    }

    /* allocate protection domain */
    ib_res.pd = ibv_alloc_pd(ib_res.ctx);
    // check(ib_res.pd != NULL, "Failed to allocate protection domain.");
    if (!ib_res.pd)
    {
        if (dev_list != nullptr)
            ibv_free_device_list(dev_list);
        rdma_error("Failed to allocate protection domain.");
        return -1;
    }

    /* query IB port attribute */
    ret = ibv_query_port(ib_res.ctx, IB_PORT, &ib_res.port_attr);
    // check(ret == 0, "Failed to query IB port information.");
    if (ret != 0)
    {
        if (dev_list != nullptr)
            ibv_free_device_list(dev_list);
        rdma_error("Failed to query IB port information.");
        return -1;
    }

    ib_res.file_data_size = file_size;
    //20231123 test write_imm
    ib_res.recv_size = 8;
    ib_res.ib_buf_size = ib_res.file_data_size + ib_res.recv_size;
    // align 4KB alloc
    ib_res.ib_buf = static_cast<char *>(memalign(4096, ib_res.ib_buf_size));
    if (!ib_res.ib_buf)
    {
        if (dev_list != nullptr)
            ibv_free_device_list(dev_list);
        rdma_error("Failed to allocate ib_buf");
        return -1;
    }

    ib_res.mr = ibv_reg_mr(ib_res.pd,
                           static_cast<void *>(ib_res.ib_buf),
                           ib_res.ib_buf_size,
                           IBV_ACCESS_LOCAL_WRITE |
                               IBV_ACCESS_REMOTE_READ |
                               IBV_ACCESS_REMOTE_WRITE);
    // check(ib_res.mr != NULL, "Failed to register mr");
    if (!ib_res.mr)
    {
        if (dev_list != nullptr)
            ibv_free_device_list(dev_list);
        rdma_error("Failed to register mr");
        return -1;
    }

    /* reset buffer to all '0' */
    memset(ib_res.ib_buf, '\0', ib_res.ib_buf_size);

    /* query IB device attr */
    ret = ibv_query_device(ib_res.ctx, &ib_res.dev_attr);
    // check(ret == 0, "Failed to query device");
    if (ret != 0)
    {
        if (dev_list != nullptr)
            ibv_free_device_list(dev_list);
        rdma_error("Failed to query device");
        return -1;
    }

    /* add query IB gid  20231110 */
    ret = ibv_query_gid(ib_res.ctx, IB_PORT, 1, &ib_res.gid);
    // check(ret == 0, "ibv_query_gid fail");
    if (ret != 0)
    {
        if (dev_list != nullptr)
            ibv_free_device_list(dev_list);
        rdma_error("ibv_query_gid fail");
        return -1;
    }

    /* create cq */
    ib_res.cq = ibv_create_cq(ib_res.ctx, ib_res.dev_attr.max_cqe,
                              nullptr, nullptr, 0);
    // check(ib_res.cq != NULL, "Failed to create cq");
    if (!ib_res.cq)
    {
        if (dev_list != nullptr)
            ibv_free_device_list(dev_list);
        rdma_error("Failed to create cq");
        return -1;
    }

    /* create qp */
    struct ibv_qp_init_attr qp_init_attr = {
        .send_cq = ib_res.cq,
        .recv_cq = ib_res.cq,
        .cap = {
            .max_send_wr = ib_res.dev_attr.max_qp_wr,
            .max_recv_wr = ib_res.dev_attr.max_qp_wr,
            .max_send_sge = 1,
            .max_recv_sge = 1,
        },
        .qp_type = IBV_QPT_RC,
    };
    ib_res.qp = ibv_create_qp(ib_res.pd, &qp_init_attr);
    // check(ib_res.qp != NULL, "Failed to create qp");
    if (!ib_res.qp)
    {
        if (dev_list != nullptr)
            ibv_free_device_list(dev_list);
        rdma_error("Failed to create qp");
        return -1;
    }

    ibv_free_device_list(dev_list);
    return 0;
    // error:
    //     if (dev_list != NULL)
    //     {
    //         ibv_free_device_list(dev_list);
    //     }
    //     return -1;
}

void close_ib_connection(IBRes &ib_res)
{
    if (ib_res.qp != nullptr)
        ibv_destroy_qp(ib_res.qp);

    if (ib_res.cq != nullptr)
        ibv_destroy_cq(ib_res.cq);

    if (ib_res.mr != nullptr)
        ibv_dereg_mr(ib_res.mr);

    if (ib_res.pd != nullptr)
        ibv_dealloc_pd(ib_res.pd);

    if (ib_res.ctx != nullptr)
        ibv_close_device(ib_res.ctx);

    if (ib_res.ib_buf != nullptr)
        free(ib_res.ib_buf);
}

int post_write_signaled(uint32_t req_size, uint32_t lkey, uint64_t wr_id,
                        struct ibv_qp *qp, char *buf,
                        uint64_t raddr, uint32_t rkey)
{
    int ret = 0;
    struct ibv_send_wr *bad_send_wr;

    struct ibv_sge list = {
        .addr = (uintptr_t)buf,
        .length = req_size,
        .lkey = lkey};

    struct ibv_send_wr send_wr = {
        .wr_id = wr_id,
        .sg_list = &list,
        .num_sge = 1,
        .opcode = IBV_WR_RDMA_WRITE,
        .send_flags = IBV_SEND_SIGNALED,
    };
    send_wr.wr.rdma.remote_addr = raddr;
    send_wr.wr.rdma.rkey = rkey;

    ret = ibv_post_send(qp, &send_wr, &bad_send_wr);
    return ret;
}

int post_write_imm_signaled(uint32_t req_size, uint32_t lkey, uint64_t wr_id,
                            struct ibv_qp *qp, char *buf,
                            uint64_t raddr, uint32_t rkey)
{
    int ret = 0;
    struct ibv_send_wr *bad_send_wr;

    struct ibv_sge list = {
        .addr = (uintptr_t)buf,
        .length = req_size,
        .lkey = lkey};

    struct ibv_send_wr send_wr = {
        .wr_id = wr_id,
        .sg_list = &list,
        .num_sge = 1,
        // test write with imm_data
        //.opcode = IBV_WR_RDMA_WRITE,
        .opcode = IBV_WR_RDMA_WRITE_WITH_IMM,
        .send_flags = IBV_SEND_SIGNALED,
        .imm_data = htonl(0x1234),
    };
    send_wr.wr.rdma.remote_addr = raddr;
    send_wr.wr.rdma.rkey = rkey;

    ret = ibv_post_send(qp, &send_wr, &bad_send_wr);
    return ret;
}

int post_recv(uint32_t req_size, uint32_t lkey, uint64_t wr_id,
              struct ibv_qp *qp, char *buf)
{
    int ret = 0;
    struct ibv_recv_wr *bad_recv_wr;

    struct ibv_sge list = 
    {
        .addr = (uintptr_t)buf,
        .length = req_size,
        .lkey = lkey
    };

    struct ibv_recv_wr recv_wr = 
    {
        .wr_id = wr_id,
        .sg_list = &list,
        .num_sge = 1
    };

    ret = ibv_post_recv(qp, &recv_wr, &bad_recv_wr);
    return ret;
}
