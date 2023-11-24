#pragma once

#include "rdma_common.h"

int client_receive(IBRes &ib_res);

int run_rdma_client(size_t file_size, std::string server_name, std::string sock_port, std::string &recv_file);