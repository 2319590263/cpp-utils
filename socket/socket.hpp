#pragma once

#include <iostream>
#include <atomic>
#include <winsock2.h>

using namespace std;

namespace ForestSavage {

    static atomic<int> WSA_count{0};

    void WSAStart(WORD wVersionRequested,WSADATA* wsa){
        if (WSAStartup(wVersionRequested, wsa)) {
            cerr << "³õÊ¼»¯Ê§°Ü" << endl;
            throw;
        }
        WSA_count.fetch_add(1);
    }

    void WSAClose(){
        WSA_count.fetch_sub(1);
        if(WSA_count.load() <= 0){
            WSACleanup();
        }

    }

    sockaddr_in make_sockaddr_in(char const *ip, int port) {
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.S_un.S_addr = inet_addr(ip);
        return addr;
    }

    class ServerSocket {

        WSADATA wsa;
        SOCKET sock;

    public:

        ServerSocket() : ServerSocket(MAKEWORD(2, 2), AF_INET, SOCK_STREAM) {}

        ServerSocket(WORD wVersionRequested, int af, int type) {
            WSAStart(wVersionRequested,&wsa);
            sock = socket(af, type, 0);
            if (sock == -1) {
                cerr << "Ì×½Ó×Ö´´½ÝÊ§°Ü" << endl;
                throw;
            }
        }

        int bind(const char *ip, int port) {
            sockaddr_in addr = make_sockaddr_in(ip, port);
            return ::bind(sock, (sockaddr *) &addr, sizeof(addr));
        }

        int listen(int backlog) {
            return ::listen(sock, backlog);
        }

        pair<SOCKET, sockaddr> accept() {
            sockaddr addrCli;
            int len = sizeof(addrCli);
            SOCKET sockCli = ::accept(sock, &addrCli, &len);
            return make_pair(sockCli, addrCli);
        }

        int send(SOCKET sock, char *buf, int len) {
            return ::send(sock, buf, len, 0);
        }

        int recv(SOCKET sock, char *buf, int len) {
            return ::recv(sock, buf, len, 0);
        }

        ~ServerSocket() {
            WSAClose();
        }

    };

    class Socket {

        WSADATA wsa;
        SOCKET sock;

    public:
        Socket() : Socket(MAKEWORD(2, 2), AF_INET, SOCK_STREAM) {}

        Socket(WORD wVersionRequested, int af, int type) {
            WSAStart(wVersionRequested,&wsa);
            sock = socket(af, type, 0);
            if (sock == -1) {
                cerr << "Ì×½Ó×Ö´´½ÝÊ§°Ü" << endl;
                throw;
            }
        }

        int connect(char const *ip, int port) {
            sockaddr_in addr = make_sockaddr_in(ip, port);
            return ::connect(sock, (sockaddr *) &addr, sizeof(addr));
        }

        int send(char *buf, int len) {
            return ::send(sock, buf, len, 0);
        }

        int recv(char *buf, int len) {
            return ::recv(sock, buf, len, 0);
        }

        ~Socket() {
            WSAClose();
        }

    };


    namespace Convenient {
        using ForestSavage::Socket;
        using ForestSavage::ServerSocket;

    }
}
