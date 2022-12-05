#pragma once

#include <iostream>
#include <winsock2.h>

using namespace std;

namespace ForestSavage {

    class socket_tcp_4{
        WSADATA wsa;
        SOCKET sock;

        sockaddr_in make_sockaddr_in(char* ip, int port){
            sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.S_un.S_addr = inet_addr(ip);
            return addr;
        }

    public:
        socket_tcp_4(){
            if (WSAStartup(MAKEWORD(2, 2), &wsa)) {
                cerr << "初始化失败" << endl;
                throw;
            }
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock == -1) {
                cerr << "套接字创捷失败" << endl;
                throw;
            }
        }

        int connect(char* ip, int port){
            sockaddr_in addr = make_sockaddr_in(ip,port);
            return ::connect(sock, (sockaddr *) &addr, sizeof(addr));
        }

        int bind(char* ip, int port){
            sockaddr_in addr = make_sockaddr_in(ip,port);
            return ::bind(sock, (sockaddr *) &addr, sizeof(addr));
        }
        int listen(int backlog){
            return ::listen(sock, backlog);
        }

        pair<SOCKET,sockaddr> accept(){
            sockaddr addrCli;
            int len = sizeof(addrCli);
            SOCKET sockCli = ::accept(sock, &addrCli, &len);
            return make_pair(sockCli,addrCli);
        }



        int send(SOCKET sock, char* buf, int len){
            return ::send(sock, buf, len, 0);
        }

        int send(char* buf, int len){
            return ::send(sock, buf, len, 0);
        }

        int recv(SOCKET sock, char* buf, int len){
            return ::recv(sock, buf, len, 0);
        }

        int recv(char* buf, int len){
            return ::recv(sock, buf, len, 0);
        }


        ~socket_tcp_4(){
            WSACleanup();
        }

    };

    namespace Convenient{
        using ForestSavage::socket_tcp_4;
    }
}
