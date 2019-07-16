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

enum SocketType
{
	Unspec =0,
	Client,
	Server
};

enum SocketFunctionTypes
{
	Welcome = 0,
	Response
};

enum SocketStatus
{
	Created=0,
	HasAddresInfo,
	Binded,
	Connected,
	Listening,
};
class SocketWrap;

class SocketInstance
{
public:
	SocketInstance() : _Soc(0), _Name("")
	{
		_Address = nullptr;
		_Status = SocketStatus::Created;
		_SocType = Unspec;
		this->_Reference = nullptr;
		this->WelcomeFunction = [](int, char*, int) {};
		this->ResponseFunction = [](int, char*, int) {};
	};
	SocketInstance(const char* Name, SocketWrap* ref) :_Soc(0), _Name(Name), _Reference(ref)
	{
		_Address = nullptr;
		_Status = SocketStatus::Created;
		_SocType = Unspec;
		this->WelcomeFunction = [](int, char*, int) {};
		this->ResponseFunction = [](int, char*, int) {};
	};
	SocketInstance(const char* Name, int socket,SocketWrap* ref) :_Soc(socket), _Name(Name), _Reference(ref)
	{
		_Address = nullptr;
		_Status = SocketStatus::Created;
		_SocType = Unspec;
		this->WelcomeFunction = [](int,char*,int) {};
		this->ResponseFunction = [](int,char*,int) {};
	};
	~SocketInstance();
	//Server things
	void SetupServer(const char* port);
	void CreateListeningThread(int maxClients);

	//Client things
	void SetupClient(const char* addr,const char* port);
	void ConnectToServer();
	void SendTCPClient(void* data, size_t size);
	void RecieveTCPClient(void* dataBuffer, size_t bufferSize);
	void CreateRecieveThread();


	//Universal
	const char* GetName();
	SOCKET GetSocket();
	SocketStatus GetStatus();
	void SetStatus(SocketStatus stat);
	void BindSocketFunction(std::function<void(int, char*, int)> func, SocketFunctionTypes type);
	void StopListeningThread();
	void CloseConnection();


private:
	void RecieveFunc();
	void ListenServerFunc(int maxClients);

	SocketWrap* _Reference;
	std::atomic<bool> isListening;
	std::vector<std::shared_ptr<SocketInstance>> ServerSockets;
	SOCKET _Soc;
	const char* _Name;
	struct addrinfo* _Address;
	SocketStatus _Status;
	SocketType _SocType;
	std::function<void(int, char*, int)> WelcomeFunction;
	std::function<void(int, char*, int)> ResponseFunction;
	std::thread ListeningThread;
};

class SocketWrap
{
public:
	SocketWrap();
	~SocketWrap();

	std::shared_ptr<SocketInstance> CreateSocket(const char* name, IPPROTO proto);
	std::shared_ptr<SocketInstance> CreateEmptySocket(const char* name);
	std::shared_ptr<SocketInstance> GetSocketByName(const char* name);
	void CloseSocket(const char* name);


private:
	WSAData _WsaData;
	std::vector<std::shared_ptr<SocketInstance>> _Sockets;
};

//class SocketWrap
//{
//public:
//	SocketWrap();
//	~SocketWrap();
//	void Initalize();
//	// WINDOWS
//	void CreateSocket(const char* name, int proto);
//	void ConnectionData(const char* name, const char* ip, int port);
//	std::shared_ptr<MySocket> GetSocketByName(const char* name);
//	//CLIENT
//	void ConnectWin(const char* name);
//	void CloseConnection(const char*name);
//	int SendTCP(const char* name,const char* sendBuff);
//	int SendTCP(int soc, const char* sendBuff);
//	void CreateListeningClient(const char* SocketName,const char* ThreadName,IPPROTO type);
//	void ListenClient(std::shared_ptr<MySocket> sock,IPPROTO type);
//	int RecvTCPServer(const char* name,int index, char* recvBuff);
//	int SendUDP(const char* name, const char* sendBuff);
//	//SERVER
//	void BindSocket(const char* name);
//	void BindSocketFunction(const char* name,SocketFunctionTypes type, std::function<void(int,char*, int)> fun);
//	void ListenServer(std::shared_ptr<MySocket> sock, int numberOfClients);
//
//	void CreateListeningServerThread(const char* ThreadName, const char* SocketName);
//	void StopThread(const char* ThreadName);
//
//	void CloseSocket(const char* name);
//	void Shutdown();
//
//#ifdef _WIN32
//	std::vector<std::shared_ptr<MySocket>> _Sockets;
//	std::vector<std::shared_ptr<ThreadSocket>> _ThreadSockets;
//	WSAData _WsaData;
//#endif
//};

