#include "rdma_client.h"


int client_receive(IBRes &ib_res)
{
    int ret = 0;
    int num_wc = 20;
    struct ibv_wc *wc = nullptr;
    wc = (struct ibv_wc *)calloc(num_wc, sizeof(struct ibv_wc));
    if(wc == nullptr)
    {
        rdma_error("client receive failed to allocate wc");
        return -1;
    }

    // volatile char *msg_start = ib_res.ib_buf;
    // volatile char *msg_end = msg_start + ib_res.ib_buf_size - 1;

    // loop till msg from server
    // while ((*msg_start != 'A') && (*msg_end != 'A'))
    // {
    // }

    /*pre-post recvs*/
    char *recv_buf_ptr = ib_res.ib_buf + ib_res.file_data_size;
    ret = post_recv(ib_res.recv_size, ib_res.mr->lkey, (uint64_t)recv_buf_ptr, ib_res.qp, recv_buf_ptr);

    // std::cout << "client receive start poll cq" << std::endl;
    int n=0;
    do
    {    
        n = ibv_poll_cq(ib_res.cq, num_wc, wc);
    }while(n == 0);
    if(n < 0)
    {
        rdma_error("client_receive poll cq failed");
        free(wc);
        return -1;
    }
    for (int i = 0; i < n; i++)
    {
        if (wc[i].status != IBV_WC_SUCCESS)
        {
            rdma_error("in client_receive: wc failed");
            free(wc);
            return -1;
        }
        if(wc[i].opcode == IBV_WC_RECV_RDMA_WITH_IMM)
        {
            std::cout << "client receive imm_data" << std::endl;
        }
    }

    free(wc);
    return 0;
}

int run_rdma_client(size_t file_size, std::string server_name, std::string sock_port, std::string &recv_file)
{
    int ret = 0;
    IBRes client_ibres;

    //msg_start = msg_end = 'A'
    ret = setup_ib(client_ibres, file_size + 2, true);
    if(ret != 0)
    {
        rdma_error("client setup_ib failed");
        close_ib_connection(client_ibres);
        return -1;
    }

    ret = connect_qp_client(client_ibres, server_name, sock_port);
    if(ret != 0)
    {
        rdma_error("client connect qp failed");
        close_ib_connection(client_ibres);
        return -1;
    }

    ret = client_receive(client_ibres);
    if(ret != 0)
    {
        rdma_error("client receive failed");
        close_ib_connection(client_ibres);
        return -1;
    }
    
    recv_file.assign(client_ibres.ib_buf + 1, file_size);

    close_ib_connection(client_ibres);
    return 0;
}
