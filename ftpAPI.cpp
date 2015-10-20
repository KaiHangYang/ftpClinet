#include "ftpAPI.h"
#include "ftpCode.h"

using namespace std;

SOCKET socket_connect(char * host, int port) {
    int i = 0;
    unsigned char ch[4];
    char ip[20];

    //  启动windows下异步套接字，调用WSAStartup之后才能使用socket的函数
    WSADATA wsaData;
    WORD sockVersion = MAKEWORD(2, 0);
    if (WSAStartup(sockVersion, &wsaData)) {
        cout << "Init socket dll Error!" << endl;
        exit(1);
    }

    // 解析域名
    struct hostent * server = gethostbyname(host);
    if (!server) {
        cout << "No such host!" << endl;
        return -1;
    }

    // 因为一个hostname 可能对应多个IP
    while (server->h_addr_list[i] != NULL) {
        memcpy(&ch, server->h_addr_list[i], 4);
        sprintf(ip, "%d.%d.%d.%d", ch[0], ch[1], ch[2], ch[3]);
        ++i;
    }

#ifndef NDEBUG
    cout << ip << endl;
#endif // DEBUG

    // AF_INET 是IPv4的网络协议套接字
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    if (SOCKET_ERROR == s) {
        cout << "Create Socket Error!" << endl;
        exit(1);
    }

    // 为socket添加超时
    int timeout = 3000;
    int ret = setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
    ret = setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

    // 服务器地址
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.S_un.S_addr = inet_addr(ip);
    address.sin_port = htons((unsigned short)port);
#ifndef NDEBUG
    cout << "连接的服务器的信息：" << address.sin_addr.S_un.S_addr << ":" << address.sin_port << endl;
#endif // NDEBUG
    // 链接
    if (SOCKET_ERROR == connect(s, (LPSOCKADDR)&address, sizeof(address))) {
        cout << "Can Not Connect To the Server!" << endl;
        exit(1);
    }
    else {
        cout << "Connect Success!" << endl;
    }
    return s;
}

SOCKET ftp_connect(char *host, int port) {
    SOCKET instr_socket;
    int result;
    ssize_t len;
    char buf[BUFSIZ];

    instr_socket = socket_connect(host, port);

    if (-1 == instr_socket) {
        return -1;
    }

    while ((len = recv(instr_socket, buf, BUFSIZ, 0)) > 0) {
        buf[len] = '\0';
#ifndef NDEBUG
        cout << "初连接获取的socket信息：" << buf << endl;
#endif // NDEBUG
    }

    sscanf(buf, "%d", &result);

    if (FTP_READY != result) {
        cout << "FTP is not ready, close the socket!" << endl;
        closesocket(instr_socket);
        return -1;
    }
    else {
        cout << "FTP is ready!" << endl;
    }

    return instr_socket;
}
void socket_clean(SOCKET sock) {
    WSACleanup();
}
int ftp_cmd_send(SOCKET sock, char * cmd, char *rec_buf, ssize_t *len) {
    ssize_t s_len;
    char s_buf[BUFSIZ];

    if (SOCKET_ERROR == (send(sock, cmd, strlen(cmd), 0))) {
        // 发送命令失败
        return -1;
    }

    s_len = recv(sock, s_buf, BUFSIZ, 0);

    if (s_len < 1) {
        return -1;
    }
    s_buf[s_len] = '\0';

    if (NULL != len && NULL != rec_buf) {
        *len = s_len;
        sprintf(rec_buf, "%s", s_buf);
    }
    else {
        cout << "ERROR:rec_buf or len is NULL!" << endl;
        return -1;
    }

    return 0;
}
int ftp_cmd_send(SOCKET sock, char *cmd) {
    ssize_t s_len;
    char s_buf[BUFSIZ];
    int result;

    cout << "Client: " << cmd << endl;

    result = ftp_cmd_send(sock, cmd, s_buf, &s_len);

    cout << "Server: " << s_buf << endl;

    if (0 == result) {
        sscanf(s_buf, "%d", &result);
    }

    return result;
}
int ftp_login (SOCKET sock, char *username, char *password) {
    char buf[BUFSIZ];
    int result;
    sprintf(buf, "USER %s\r\n", username);

    int timeout = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

    result = ftp_cmd_send(sock, buf);

#ifndef NDEBUG
    cout << "Login result：" << result << endl;
#endif

    if (FTP_LOGIN_SUCCESS == result) {
        // 可以直接登录 登陆成功
        return 0;
    }
    else if (FTP_PW_NEEDED == result) {
        sprintf(buf, "PASS %s\r\n", password);
        result = ftp_cmd_send(sock, buf);

        if (FTP_LOGIN_SUCCESS == result) {
            // 登陆成功
            cout << "登录成功!" << endl;
            return 0;
        }
        else {
            // 密码错误或者登录出错
            return -1;
        }
    }
    return -1;
}
int ftp_logout(SOCKET sock) {
    int result = ftp_cmd_send(sock, "QUIT\r\n");
    closesocket(sock);
    socket_clean(sock);
    return result;
}
int ftp_cwd(SOCKET sock, char *dir) {
    int result;
    char buf[BUFSIZ];

    sprintf(buf, "CWD %s\r\n", dir);
    result = ftp_cmd_send(sock, buf);
#ifndef NDEBUG
    cout << "工作目录改变结果：" << result << endl;
#endif
    if (FTP_FILE_ACT_FIN == result) {
        return 0;
    }

    return -1;
}

int ftp_list(SOCKET sock, char *dir) {
    char buf[BUFSIZ];
    int result;

    sprintf(buf, "LIST %s\r\n", dir);

    // 发送请求
    result = ftp_cmd_send(sock, buf);

#ifndef NDEBUG
    cout << "LIST Result: " << result << endl;
#endif // NDEBUG
    return 0;
}

int ftp_type(SOCKET sock, char type) {
    char buf[BUFSIZ];
    int result;

    sprintf(buf, "TYPE %c\r\n", type);

    result = ftp_cmd_send(sock, buf);

#ifndef NDEBUG
    cout << "CHANGE TYPE Result: " << result << endl;
#endif
    if (FTP_SUCCESS == result) {
        return 0;
    }
    return -1;
}
SOCKET ftp_data_pasv(SOCKET instr_sock) {

}
