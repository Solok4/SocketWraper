#pragma once
#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib,"Ws2_32.lib")
#endif

#include <vector>
#include "Log.h"
#include <memory>
#include <thread>
#include <future>

#ifdef _WIN32
struct MySocket
{
	SOCKET soc;
	const char* name;
	sockaddr_in First,Second;
	std::function<void()> fun = []() {};
	std::atomic<bool> Close = false;
	SOCKET ChildSockets[8];
};

struct ThreadSocket
{
	std::shared_ptr<MySocket> Socket;
	std::thread thr;
	const char* name;
};
#endif

class SocketWrap
{
public:
	SocketWrap();
	~SocketWrap();
	void Initalize();
	// WINDOWS
	void InitalizeWin();
	void CreateSocketWin(const char* name, int proto);
	void ConnectionDataWin(const char* name, const char* ip, int port);
	std::shared_ptr<MySocket> GetSocketByName(const char* name);
	//CLIENT
	void ConnectWin(const char* name);
	void CloseConnectionWin(const char*name);
	int SendTCPWin(const char* name,const char* sendBuff);
	int RecvTCPWin(const char* name, char* recvBuff);
	int RecvTCPServerWin(const char* name,int index, char* recvBuff);
	int SendUDPWin(const char* name, const char* sendBuff);
	int RecvUDPWin(const char* name, char* recvBuff);
	//SERVER
	void BindSocketWin(const char* name);
	void BindListeningFunctionWin(const char* name, std::function<void()> fun);
	void ListenSocketWin(std::shared_ptr<MySocket> sock, int numberOfClients);

	void CreateListeningThread(const char* ThreadName, const char* SocketName);
	void StopThread(const char* ThreadName);

	void CloseSocketWin(const char* name);
	void ShutdownWin();




#ifdef _WIN32
	std::vector<std::shared_ptr<MySocket>> _Sockets;
	std::vector<std::shared_ptr<ThreadSocket>> _ThreadSockets;
	WSAData _WsaData;
#endif
};

