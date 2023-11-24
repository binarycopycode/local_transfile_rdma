#include "rdma_client.h"
#include <fstream>

int main(int argc, char *argv[])
{

    if (argc != 3)
    {
        fprintf(stderr, "USAGE: %s <server_ip> <port>\n", argv[0]);
        exit(1);
    }
    std::string server_ip_str = argv[1];
    std::string port_str = argv[2];
    //20230323.npcbuf len = 3875312
    size_t file_size = 3875312;
    std::string ans;
    int ret = run_rdma_client(file_size, server_ip_str, port_str, ans);
    if(ret < 0)
    {
        std::cout << "run rdma client error" << std::endl;
        return 0;
    }
    std::cout << ""<< ans.size() << std::endl;

    // 打开文件流并以二进制模式写入文件
    std::ofstream file("20230323-recv.npcbuf", std::ios::binary);
    
    // 检查文件是否成功打开
    if (file.is_open()) {
        // 将字符串数据写入文件
        file.write(ans.data(), ans.size());
        
        // 关闭文件流
        file.close();
        
        std::cout << "写入文件成功！" << std::endl;
    } else {
        std::cout << "无法打开文件！" << std::endl;
    }

    return 0;
}
