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

#ifndef _WIN32
typedef int SOCKET;
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#define INVALID_SOCKET (int)(~0)
#define SOCKET_ERROR -1
#define SD_SEND 0x01
#endif // !_WIN32

struct SocketCombo
{
	SOCKET soc;
	sockaddr_in AdressInfo;
};

struct MySocket
{
	SOCKET soc;
	std::string name;
	sockaddr_in MainSocket,RecieveSocket;
	std::function<void(int, char*, int)> ResponseFunction = [](int, char*, int) {};
	std::function<void(int, char*, int)> WelcomeFunction = [](int, char*, int) {};
	std::atomic<bool> Close;
	SocketCombo ChildSockets[8];
};

struct ThreadSocket
{
	std::shared_ptr<MySocket> Socket;
	std::thread thr;
	std::string name;
};

enum SocketType
{
	Client = 0,
	Server
};

enum SocketFunctionTypes
{
	Welcome = 0,
	Response
};

enum SocketStatus
{
	Initial=0,
	HasAddresInfo,
	Binded,
	Connected,
	Listening,
};
class SocketWrap;

class SocketInstance
{
public:
	SocketInstance()
	{
		this->_Name = "";
		this->_Soc = 0;
		this->_Port = 0;
		this->_Address = nullptr;
		this->_Status = SocketStatus::Initial;
		this->_SocType = SocketType::Client;
		this->_Reference = nullptr;
		this->WelcomeFunction = [](std::shared_ptr<SocketInstance>, char*, int) {};
		this->ResponseFunction = [](std::shared_ptr<SocketInstance>, char*, int) {};
	};
	SocketInstance(std::string Name, SocketWrap* ref) :SocketInstance()
	{
		this->_Reference = ref;
		this->_Name = Name;
	};
	SocketInstance(const SocketInstance& soc)
	{
		this->_Port = soc._Port;
		this->_Address = soc._Address;
		this->_Soc = soc._Soc;
		this->_SocType = soc._SocType;
		this->_Status = soc._Status;
		this->WelcomeFunction = soc.WelcomeFunction;
		this->ResponseFunction = soc.ResponseFunction;
		this->_Reference = soc._Reference;
		this->_Name = std::string(soc._Name);
	}
	SocketInstance(std::string Name, int socket,SocketWrap* ref) :SocketInstance(Name,ref)
	{
		this->_Soc = socket;
	};
	~SocketInstance();
	//Server things
	void SetupServer(uint32_t port);
	void CreateListeningThread(int maxClients);

	//Client things
	void SetupClient(std::string addr,uint32_t port);
	void ConnectToServer();
	void SendTCPClient(void* data, size_t size);
	void RecieveTCPClient(void* dataBuffer, size_t bufferSize);
	void CreateRecieveThread();


	//Universal
	std::string GetName();
	SOCKET GetSocket();
	SocketStatus GetStatus();
	void SetStatus(SocketStatus stat);
	void BindSocketFunction(std::function<void(std::shared_ptr<SocketInstance>, char*, int)> func, SocketFunctionTypes type);
	void StopListeningThread();
	void CloseConnection();


private:
	void RecieveFunc();
	void ListenServerFunc(int maxClients);

	SocketWrap* _Reference;
	std::atomic<bool> isListening;
	std::vector<std::shared_ptr<SocketInstance>> ServerSockets;
	SOCKET _Soc;
	std::string _Name;
	struct addrinfo* _Address;
	SocketStatus _Status;
	SocketType _SocType;
	std::function<void(std::shared_ptr<SocketInstance>, char*, int)> WelcomeFunction;
	std::function<void(std::shared_ptr<SocketInstance>, char*, int)> ResponseFunction;
	std::thread ListeningThread;
	uint32_t _Port;
};

class SocketWrap
{
public:
	SocketWrap();
	~SocketWrap();

#ifndef _WIN32
	std::shared_ptr<SocketInstance> CreateSocket(std::string name, int proto);
#else
	std::shared_ptr<SocketInstance> CreateSocket(std::string name, IPPROTO proto);
#endif // !_WIN32
	std::shared_ptr<SocketInstance> CreateEmptySocket(std::string name);
	std::shared_ptr<SocketInstance> GetSocketByName(std::string name);
	void CloseSocket(std::string name);


private:
#ifdef _WIN32
	WSAData _WsaData;
#endif // !_WIN32
	std::vector<std::shared_ptr<SocketInstance>> _Sockets;
};
