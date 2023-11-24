#include "rdma_server.h"
#include <vector>
#include <fstream>
#include <filesystem>
#include <algorithm>

int read_data(std::vector<std::string> &datetime_list_,
                           std::vector<std::string> &npcbuf_data_list_)
{
    namespace fs = std::filesystem;

    // save all the datetime and sort them in string order
    std::string dir_path = "npcbuf/s5m/index";
    if (fs::is_directory(dir_path))
    {
        for (const auto &entry : fs::directory_iterator(dir_path))
        {
            if (entry.is_regular_file())
            {
                std::string filename = entry.path().filename().string();
                if (fs::path(filename).extension() == ".npcbuf")
                {
                    size_t dot_index = filename.rfind(".");
                    if (dot_index != std::string::npos)
                    {
                        std::string datetime = filename.substr(0, dot_index);
                        datetime_list_.emplace_back(datetime);
                    }
                }
            }
        }
    }
    else
    {
        std::cerr << "dir_path is not a directory" << std::endl;
        return -1;
    }

    std::sort(datetime_list_.begin(), datetime_list_.end());
    // vector pre reserve save the expand move time,
    npcbuf_data_list_.reserve(datetime_list_.size());

    for (const std::string &datetime : datetime_list_)
    {
        // read npcbuf file
        std::string npcbuf_file_path = dir_path + "/" + datetime + ".npcbuf";
        std::ifstream npcbuf_file(npcbuf_file_path, std::ios::binary);
        if (!npcbuf_file)
        {
            std::cerr << "can not open npcbuf file path:" << npcbuf_file_path << std::endl;
            return -1;
        }
        std::string npcbuf_content((std::istreambuf_iterator<char>(npcbuf_file)), std::istreambuf_iterator<char>());
        npcbuf_data_list_.emplace_back(npcbuf_content);
        npcbuf_file.close();

    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "USAGE: %s <port>\n", argv[0]);
        exit(1);
    }
    std::string port_str = argv[1];

    std::vector<std::string> datetime_list_;
    std::vector<std::string> npcbuf_data_list_;
    read_data(datetime_list_, npcbuf_data_list_);
    std::string date="20230323";
    int file_index=std::lower_bound(datetime_list_.begin(),datetime_list_.end(),date)-datetime_list_.begin();
    run_rdma_server(npcbuf_data_list_[file_index], npcbuf_data_list_[file_index].size(), port_str);
    return 0;
}