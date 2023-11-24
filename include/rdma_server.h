#pragma once

#include "rdma_common.h"

int server_write(IBRes &ib_res);

int run_rdma_server(const std::string &npcbuf_file_str, size_t file_size, std::string sock_port);

