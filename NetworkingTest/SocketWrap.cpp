#include "pch.h"
#include "SocketWrap.h"


SocketWrap::SocketWrap()
{
	SocketWrap::Initalize();
}


SocketWrap::~SocketWrap()
{
}

void SocketWrap::Initalize()
{
#ifdef WIN32
	int error = WSAStartup(MAKEWORD(2, 2), &this->_WsaData);
	if (error != NO_ERROR)
	{
		Clog::PrintLog(LogTag::Error, "Failed to initalize WSA");
		WSACleanup();
		return;
	}
#endif // WIN
}


void SocketWrap::CreateSocket(const char* name, int proto)
{
	std::shared_ptr<MySocket> mySocket(new MySocket);
	SOCKET soc;
	if (proto == IPPROTO_TCP)
		soc = socket(AF_INET, SOCK_STREAM, proto);
	else// if (proto == IPPROTO_UDP)
		soc = socket(AF_INET, SOCK_DGRAM, proto);
	if (soc == INVALID_SOCKET)
	{
		Clog::PrintLog(LogTag::Error, "Failed to create a socket. Error: %d",WSAGetLastError());
		return;
	}
	mySocket->name = name;
	mySocket->soc = soc;
	mySocket->MainSocket.sin_family = AF_INET;
	mySocket->RecieveSocket.sin_family = AF_INET;
	if (proto == IPPROTO_UDP)
	{
		mySocket->RecieveSocket.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	}
	_Sockets.push_back(mySocket);
}

void SocketWrap::ConnectionData(const char* name,const char * ip, int port)
{
	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
	if (sock == nullptr)
	{
		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found Error: %d",name,WSAGetLastError());
		return;
	}
	if (sock->RecieveSocket.sin_addr.S_un.S_addr != htonl(INADDR_ANY))
	{
		sock->RecieveSocket.sin_addr.S_un.S_addr = inet_addr(ip);
	}
	sock->RecieveSocket.sin_port = htons(port);

	sock->MainSocket.sin_addr.S_un.S_addr = inet_addr(ip);
	sock->MainSocket.sin_port = htons(port);
}

std::shared_ptr<MySocket> SocketWrap::GetSocketByName(const char * name)
{
	for (auto x: _Sockets)
	{
		if (strcmp(x->name, name) == 0)
		{
			return x;
		}
	}
	return nullptr;
}

void SocketWrap::ConnectWin(const char * name)
{
	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
	if (sock == nullptr)
	{
		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found  Error: %d",  name,WSAGetLastError() );
		return;
	}
	int result = connect(sock->soc, (struct sockaddr*)&sock->MainSocket, sizeof(sock->MainSocket));
	if (result == SOCKET_ERROR)
	{
		Clog::PrintLog(LogTag::Error, "Failed to connect with socket named %s - Error: %d", name,WSAGetLastError());
		return;
	}
}

void SocketWrap::CloseConnection(const char * name)
{
	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
	if (sock == nullptr)
	{
		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found. Error: %d", name, WSAGetLastError());
		return;
	}
	int result = shutdown(sock->soc,SD_SEND);
	if (result == SOCKET_ERROR)
	{
		Clog::PrintLog(LogTag::Error, "Failed to disconnect with socket named %s Error: %d", name, WSAGetLastError());
		return;
	}
}

int SocketWrap::SendTCP(const char * name,const char * sendBuff)
{
	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
	if (sock == nullptr)
	{
		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found Error: %d", name, WSAGetLastError());
		return -1;
	}
	int result = send(sock->soc, sendBuff, (int)strlen(sendBuff), 0);
	if (result == SOCKET_ERROR)
	{
		Clog::PrintLog(LogTag::Error, "Failed to send with socket named %s Error: %d", name, WSAGetLastError());
		return -1;
	}
	printf("%d bytes sent\n", result);
	return result;
}

int SocketWrap::SendTCP(int soc, const char* sendBuff)
{
	int result = send(soc, sendBuff, (int)strlen(sendBuff), 0);
	if (result == SOCKET_ERROR)
	{
		Clog::PrintLog(LogTag::Error, "Failed to send with socket num %d Error: %d", soc, WSAGetLastError());
		return -1;
	}
	printf("%d bytes sent\n", result);
	return result;
}

void SocketWrap::CreateListeningClient(const char* SocketName,const char* ThreadName, IPPROTO type)
{
	std::shared_ptr<ThreadSocket> ThrSock(new ThreadSocket);
	for (auto x : _ThreadSockets)
	{
		if (strcmp(x->name, ThreadName) == 0)
		{
			Clog::PrintLog(LogTag::Error, "Thread named %s already exists", ThreadName);
			return;
		}
	}
	ThrSock->name = ThreadName;
	std::shared_ptr<MySocket> MySock = SocketWrap::GetSocketByName(SocketName);
	if (MySock == nullptr)
	{
		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found", SocketName);
		return;
	}
	ThrSock->Socket = MySock;
	ThrSock->thr = std::thread(&SocketWrap::ListenClient, this, ThrSock->Socket, type);
	_ThreadSockets.push_back(ThrSock);

	
}

void SocketWrap::ListenClient(std::shared_ptr<MySocket> sock,IPPROTO type)
{
	fd_set readfds;
	SOCKET activity;
	timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	int Length;
	char Buffer[8192];

	while (sock->Close.load() == false)
	{
		FD_ZERO(&readfds);
		FD_SET(sock->soc, &readfds);

		activity = select(sock->soc, &readfds, NULL, NULL, &tv);
		if (activity < 0)
		{
			Clog::PrintLog(LogTag::Error, "Activity error");
		}
		if (FD_ISSET(sock->soc, &readfds))
		{
			if (type == IPPROTO_TCP)
			{
				Length = recv(sock->soc, Buffer, sizeof(Buffer), 0);
				if (Length > 0)
				{
					//Data Recieved
					sock->ResponseFunction(sock->soc, Buffer, Length);
				}
				else if (Length == 0)
				{
					//Conection closed
					Clog::PrintLog(Log, "Connection closed");
					sock->Close.store(true);
					SocketWrap::CloseConnection(sock->name);
				}
				else
				{
					//Handle Error
					Clog::PrintLog(LogTag::Error, "Failed recieving data in client listening func Error: %d",WSAGetLastError());
					sock->Close.store(true);
				}
				
			}
			else if (type == IPPROTO_UDP)
			{
				int FromLen = sizeof(sock->RecieveSocket);
				Length = recvfrom(sock->soc, Buffer, sizeof(Buffer), 0,(sockaddr*)&sock->RecieveSocket,&FromLen);
				Clog::PrintLog(LogTag::Log, "Length: %d", Length);
				if (Length > 0)
				{
					//Data Recieved
					sock->ResponseFunction(sock->soc, Buffer, Length);

				}
				else if (Length == 0)
				{
					//Conection closed
					sock->Close.store(true);
				}
				else
				{
					//Handle Error
					Clog::PrintLog(LogTag::Error, "Failed recieving data in client listening func Error: %d", WSAGetLastError());
					sock->Close.store(true);
				}
			}
		}
	}
}


int SocketWrap::RecvTCPServer(const char * name, int index, char * recvBuff)
{
	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
	if (sock == nullptr)
	{
		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found Error: %d", name, WSAGetLastError());
		return -1;
	}
	int result = recv(sock->ChildSockets[index].soc,recvBuff,(int)strlen(recvBuff),0);
	if (result == SOCKET_ERROR)
	{
		Clog::PrintLog(LogTag::Error, "Failed to recieve with socket named %s Error: %d", name, WSAGetLastError());
		return -1;
	}
	printf("%d bytes recieved\n", result);
	return result;
}

int SocketWrap::SendUDP(const char * name, const char * sendBuff)
{
	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
	if (sock == nullptr)
	{
		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found Error: %d", name, WSAGetLastError());
		return -1;
	}
	int fromlen = sizeof(sock->MainSocket);
	int result = sendto(sock->soc, sendBuff, strlen(sendBuff), 0, (sockaddr*)&sock->MainSocket, fromlen);
	if (result == SOCKET_ERROR)
	{
		Clog::PrintLog(LogTag::Error, "Failed to send with socket named %s Error: %d", name, WSAGetLastError());
		return -1;
	}
	printf("%d bytes sent\n", result);
	return result;
}

void SocketWrap::CloseSocket(const char * name)
{
	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
	if (sock == nullptr)
	{
		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found Error: %d", name, WSAGetLastError());
		return;
	}
	int result = closesocket(sock->soc);
	if (result == SOCKET_ERROR)
	{
		Clog::PrintLog(LogTag::Error, "Failed to close socket named %s Error: %d", name, WSAGetLastError());
		return;
	}
}

void SocketWrap::Shutdown()
{
	for (auto x : _Sockets)
	{
		closesocket(x->soc);
	}
	WSACleanup();
}

void SocketWrap::BindSocket(const char * name)
{
	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
	if (sock == nullptr)
	{
		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found Error: %d", name, WSAGetLastError());
		return;
	}
	int result = bind(sock->soc, (struct sockaddr*)&sock->MainSocket, sizeof(sock->MainSocket));
	if (result == SOCKET_ERROR)
	{
		Clog::PrintLog(LogTag::Error, "Failed to bind socket named %s Error: %d", name, WSAGetLastError());
		return;
	}
}


void SocketWrap::BindSocketFunction(const char * name,SocketFunctionTypes type, std::function<void(int,char*, int)> fun)
{
	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
	if (sock == nullptr)
	{
		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found Error: %d", name, WSAGetLastError());
		return;
	}
	switch (type)
	{
	case Welcome:
		sock->WelcomeFunction = fun;
		break;
	case Response:
		sock->ResponseFunction = fun;
		break;
	default:
		break;
	}
	
}

void SocketWrap::ListenServer(std::shared_ptr<MySocket> sock, int numberOfClients)
{
	fd_set readfds;
	SOCKET maxSd=INVALID_SOCKET, sd = INVALID_SOCKET, activity,newSock= INVALID_SOCKET;
	int addrLen = sizeof(sock->MainSocket);
	char Buffer[8192];
	timeval tv;
	tv.tv_sec = 1;
	for (int i = 0; i < (sizeof(sock->ChildSockets) / sizeof(sock->ChildSockets[0])); i++)
	{
		sock->ChildSockets[i].soc = 0;
	}
	if (listen(sock->soc, numberOfClients) == SOCKET_ERROR)
	{
		Clog::PrintLog(LogTag::Error, "Socket named %s failed to listen Error: %d", sock->name, WSAGetLastError());
		return;
	}
	while(sock->Close.load()==false)
	{
		FD_ZERO(&readfds);
		FD_SET(sock->soc, &readfds);
		maxSd = sock->soc;
		for (int i = 0; i < (sizeof(sock->ChildSockets) / sizeof(sock->ChildSockets[0])); i++)
		{
			sd = sock->ChildSockets[i].soc;
			if (sd > 0)
				FD_SET(sd, &readfds);
			if (sd > maxSd)
				maxSd = sd;
		}

		activity = select(maxSd + 1, &readfds, NULL, NULL, &tv);
		if (activity < 0)
		{
			Clog::PrintLog(LogTag::Error, "Activity error");
		}
		if (FD_ISSET(sock->soc, &readfds))
		{
			Clog::PrintLog(LogTag::Log, "New connection");
			if ((newSock = accept(sock->soc, NULL, NULL))<0)
			{
				Clog::PrintLog(LogTag::Error, "Failed to accept with socket %s Error: %d\n", sock->name, WSAGetLastError());
				return;
			}

			// Welcome message
			/*if (send(newSock, "Hallo\n", strlen("Hallo\n"), 0)!=strlen("Hallo\n"))
			{
				Clog::PrintLog(LogTag::Error, "Failed to send a message to socket %s Error: %d", sock->name,WSAGetLastError());
			}*/
			sock->WelcomeFunction(newSock, nullptr, 0);

			for (int i = 0; i < (sizeof(sock->ChildSockets) / sizeof(sock->ChildSockets[0])); i++)
			{
				if (sock->ChildSockets[i].soc == 0)
				{
					sock->ChildSockets[i].soc = newSock;
					int AddressSize = sizeof(sock->ChildSockets[i].AdressInfo);
					getpeername(sock->ChildSockets[i].soc,(sockaddr*)&sock->ChildSockets[i].AdressInfo,&AddressSize);
					break;
				}
			}

		}
		else
		{
			for (int i = 0; i < (sizeof(sock->ChildSockets) / sizeof(sock->ChildSockets[0])); i++)
			{
				sd = sock->ChildSockets[i].soc;
				if (FD_ISSET(sd, &readfds))
				{
					int valread;
					if ((valread = recv(sd, Buffer, sizeof(Buffer) / sizeof(Buffer[0]), 0))==0)
					{
						//Ending connection prompt
						Clog::PrintLog(LogTag::Log, "Host disconected. IP: %s\n", inet_ntoa(sock->ChildSockets[i].AdressInfo.sin_addr));
						closesocket(sd);
						sock->ChildSockets[i].soc = 0;
					}
					else if(valread>0)
					{
						//Process recieved data
						sock->ResponseFunction(sd, Buffer, valread);					
					}
				}
			}
		}
	}
	printf("Ending thread");
}

void SocketWrap::CreateListeningServerThread(const char * ThreadName, const char * SocketName)
{
	std::shared_ptr<ThreadSocket> ThrSock(new ThreadSocket);
	for (auto x : _ThreadSockets)
	{
		if (strcmp(x->name, ThreadName) == 0)
		{
			Clog::PrintLog(LogTag::Error, "Thread named %s already exists", ThreadName);
			return;
		}
	}
	ThrSock->name = ThreadName;
	std::shared_ptr<MySocket> MySock =SocketWrap::GetSocketByName(SocketName);
	if (MySock == nullptr)
	{
		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found", SocketName);
		return;
	}
	ThrSock->Socket = MySock;
	ThrSock->thr = std::thread(&SocketWrap::ListenServer,this,ThrSock->Socket,1);
	_ThreadSockets.push_back(ThrSock);
}

void SocketWrap::StopThread(const char * ThreadName)
{
	std::shared_ptr<ThreadSocket> thr = nullptr;
	for (auto x : _ThreadSockets)
	{
		if (strcmp(x->name, ThreadName) == 0)
		{
			thr = x;
			break;
		}
	}
	if (thr == nullptr)
	{
		Clog::PrintLog(LogTag::Error, "Failed to find thread named %s", ThreadName);
		return;
	}
	thr->Socket->Close.store(true);
	thr->thr.join();
	for (uint32_t i = 0; i <= _ThreadSockets.size(); i++)
	{
		if (strcmp(_ThreadSockets[i]->name, ThreadName) == 0)
		{
			_ThreadSockets.erase(_ThreadSockets.begin() + i);
		}
	}
}

