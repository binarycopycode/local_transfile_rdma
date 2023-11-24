#include "rdma_server.h"

int server_write(IBRes &ib_res)
{
    int ret = 0;
    int num_wc = 20;
    struct ibv_wc *wc = nullptr;
    wc = (struct ibv_wc *)calloc(num_wc, sizeof(struct ibv_wc));
    if(wc == nullptr)
    {
        rdma_error("server write failed to allocate wc");
        return -1;
    }

    //test20231123  file_data_size + recv_size
    ret = post_write_imm_signaled(ib_res.file_data_size, ib_res.mr->lkey, MSG_WRITE_FINISH, ib_res.qp, ib_res.ib_buf,
                        ib_res.raddr, ib_res.rkey);
    if(ret != 0)
    {
        rdma_error("server write failed to post write signaled");
        free(wc);
        return -1;
    }
    
    int n = 0;
    do 
    {
        n = ibv_poll_cq(ib_res.cq, num_wc, wc);
    }while(n == 0);
    if(n < 0)
    {
        rdma_error("server_write poll cq failed");
        free(wc);
        return -1;
    }

    free(wc);
    return 0;
}

int run_rdma_server(const std::string &npcbuf_file_str, size_t file_size, std::string sock_port)
{
    int ret = 0;
    IBRes server_ibres;

    //msg_start = msg_end = 'A'
    ret = setup_ib(server_ibres, file_size + 2, true);
    if(ret != 0)
    {
        rdma_error("server setup_ib failed");
        close_ib_connection(server_ibres);
        return -1;
    }
    
    server_ibres.ib_buf[0] = 'A';
    memcpy(server_ibres.ib_buf+1, npcbuf_file_str.data(), file_size);
    server_ibres.ib_buf[file_size + 1] = 'A';

    ret = connect_qp_server(server_ibres, sock_port);
    if(ret != 0)
    {
        rdma_error("server connect qp failed");
        close_ib_connection(server_ibres);
        return -1;
    }
    
    ret = server_write(server_ibres);
    if(ret != 0)
    {
        rdma_error("server write failed");
        close_ib_connection(server_ibres);
        return -1;
    }

    close_ib_connection(server_ibres);
    return 0;
}