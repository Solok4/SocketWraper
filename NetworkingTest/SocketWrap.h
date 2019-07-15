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
struct SocketCombo
{
	SOCKET soc;
	sockaddr_in AdressInfo;
};

struct MySocket
{
	SOCKET soc;
	const char* name;
	sockaddr_in MainSocket,RecieveSocket;
	std::function<void(int, char*, int)> ResponseFunction = [](int, char*, int) {};
	std::function<void(int, char*, int)> WelcomeFunction = [](int, char*, int) {};
	std::atomic<bool> Close = false;
	SocketCombo ChildSockets[8];
};

struct ThreadSocket
{
	std::shared_ptr<MySocket> Socket;
	std::thread thr;
	const char* name;
};
#endif

enum SocketFunctionTypes
{
	Welcome = 0,
	Response
};

class SocketWrap
{
public:
	SocketWrap();
	~SocketWrap();
	void Initalize();
	// WINDOWS
	void CreateSocket(const char* name, int proto);
	void ConnectionData(const char* name, const char* ip, int port);
	std::shared_ptr<MySocket> GetSocketByName(const char* name);
	//CLIENT
	void ConnectWin(const char* name);
	void CloseConnection(const char*name);
	int SendTCP(const char* name,const char* sendBuff);
	int SendTCP(int soc, const char* sendBuff);
	void CreateListeningClient(const char* SocketName,const char* ThreadName,IPPROTO type);
	void ListenClient(std::shared_ptr<MySocket> sock,IPPROTO type);
	int RecvTCPServer(const char* name,int index, char* recvBuff);
	int SendUDP(const char* name, const char* sendBuff);
	//SERVER
	void BindSocket(const char* name);
	void BindSocketFunction(const char* name,SocketFunctionTypes type, std::function<void(int,char*, int)> fun);
	void ListenServer(std::shared_ptr<MySocket> sock, int numberOfClients);

	void CreateListeningServerThread(const char* ThreadName, const char* SocketName);
	void StopThread(const char* ThreadName);

	void CloseSocket(const char* name);
	void Shutdown();




#ifdef _WIN32
	std::vector<std::shared_ptr<MySocket>> _Sockets;
	std::vector<std::shared_ptr<ThreadSocket>> _ThreadSockets;
	WSAData _WsaData;
#endif
};

