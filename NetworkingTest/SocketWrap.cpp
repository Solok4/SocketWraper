#include "pch.h"
#include "SocketWrap.h"


SocketInstance::~SocketInstance()
{
	if (this->_Soc != INVALID_SOCKET)
	{
		closesocket(this->_Soc);
	}
}

void SocketInstance::SetupServer(const char* port)
{
	this->_SocType = SocketType::Server;
		int status;
		struct addrinfo hints, * res;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_PASSIVE;
		if ((status = getaddrinfo(NULL, port, &hints, &res) != 0))
		{
			Clog::PrintLog(LogTag::Error, "GetAddrInfo error:%s", gai_strerror(status));
			return;
		}
		else
		{
			for (addrinfo* p=res;p!=NULL;p= p->ai_next)
			{
				void* addr;
				char ipstr[INET_ADDRSTRLEN];
				struct sockaddr_in* socka = (struct sockaddr_in*)p->ai_addr;
				addr = &(socka->sin_addr);
				inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
				Clog::PrintLog(LogTag::Log, "%s", ipstr);
			}
			this->_Addres = res;
			this->_Addres->ai_protocol = IPPROTO_TCP;
			this->_Status = SocketStatus::HasAddresInfo;
		}
		if (this->_Status == SocketStatus::HasAddresInfo && this->_SocType == SocketType::Server)
		{
			if (status = bind(this->_Soc, this->_Addres->ai_addr, sizeof(this->_Addres->ai_addrlen)) != 0)
			{
				Clog::PrintLog(LogTag::Error, "Server bind error %s: %d", this->_Name,WSAGetLastError());
				return;
			}
			else
			{
				Clog::PrintLog(LogTag::Error, "Server has been binded %s", this->_Name);
				this->_Status = SocketStatus::Binded;
				freeaddrinfo(res);
			}

		}

}

void SocketInstance::CreateListeningThread(int maxClients)
{
	if(this->ListeningThread.native_handle() == NULL)
	this->ListeningThread = std::thread(&SocketInstance::ListenServerFunc, this, maxClients);
	this->ListeningThread.join();
}

void SocketInstance::ListenServerFunc(int maxClients)
{
	if (this->_SocType == SocketType::Server && this->_Status == SocketStatus::Binded)
	{
		fd_set readfds;
		SOCKET maxSd = INVALID_SOCKET, sd = INVALID_SOCKET, activity, newSock = INVALID_SOCKET;
		int addrLen = sizeof(this->_Addres);
		char Buffer[8192];
		timeval tv;
		tv.tv_sec = 1;
		for (int i = this->ServerSockets.size(); i < maxClients; i++)
		{
			char childname[32];
			char number[4];
			sprintf(number, "%d", i);
			strcpy_s(childname, this->_Name);
			strcat_s(childname, " ");
			strcat_s(childname, number);
			auto child = this->_Reference->CreateEmptySocket(childname);
			this->ServerSockets.push_back(child);
		}
		if (listen(this->_Soc, maxClients) == SOCKET_ERROR)
		{
			Clog::PrintLog(LogTag::Error, "Socket named %s failed to listen Error: %d", this->_Soc, WSAGetLastError());
			return;
		}
		this->isListening = true;
		while (this->isListening.load() == false)
		{
			FD_ZERO(&readfds);
			FD_SET(this->_Soc, &readfds);
			maxSd = this->_Soc;
			for (int i = 0; i < this->ServerSockets.size(); i++)
			{
				sd = this->ServerSockets[i]->_Soc;
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
			if (FD_ISSET(this->_Soc, &readfds))
			{
				Clog::PrintLog(LogTag::Log, "New connection");
				if ((newSock = accept(this->_Soc, NULL, NULL)) < 0)
				{
					Clog::PrintLog(LogTag::Error, "Failed to accept with socket %s Error: %d\n", this->_Name, WSAGetLastError());
					return;
				}

				// Welcome message
				/*if (send(newSock, "Hallo\n", strlen("Hallo\n"), 0)!=strlen("Hallo\n"))
				{
					Clog::PrintLog(LogTag::Error, "Failed to send a message to socket %s Error: %d", sock->name,WSAGetLastError());
				}*/
				this->WelcomeFunction(newSock, nullptr, 0);

				for (int i = 0; i < this->ServerSockets.size(); i++)
				{
					if (this->ServerSockets[i]->_Soc == 0)
					{
						this->ServerSockets[i]->_Soc = newSock;
						int AddressSize = sizeof(this->ServerSockets[i]->_Addres);
						getpeername(this->ServerSockets[i]->_Soc, (sockaddr*)& this->ServerSockets[i]->_Addres, &AddressSize);
						break;
					}
				}

			}
			else
			{
				for (int i = 0; i < this->ServerSockets.size(); i++)
				{
					sd = this->ServerSockets[i]->_Soc;
					if (FD_ISSET(sd, &readfds))
					{
						int valread;
						if ((valread = recv(sd, Buffer, sizeof(Buffer) / sizeof(Buffer[0]), 0)) == 0)
						{
							//Ending connection prompt
							void* addr;
							char ipstr[INET_ADDRSTRLEN];
							struct sockaddr_in* socka = (struct sockaddr_in*)this->_Addres->ai_addr;
							addr = &(socka->sin_addr);
							inet_ntop(this->_Addres->ai_family, addr, ipstr, sizeof(ipstr));
							Clog::PrintLog(LogTag::Log, "Host disconected. IP: %s\n", ipstr);
							closesocket(sd);
							this->ServerSockets[i]->_Soc = 0;
						}
						else if (valread > 0)
						{
							//Process recieved data
							this->ResponseFunction(sd, Buffer, valread);
						}
					}
				}
			}
		}
		printf("Ending thread");
	}
	else
	{
		Clog::PrintLog(LogTag::Error, "Server %s is not prepared for listening", this->_Name);
	}
}


void SocketInstance::SetupClient(const char* addr, const char* port)
{		
	int status;
	struct addrinfo hints, * res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	if ((status = getaddrinfo(addr, port, &hints, &res) != 0))
	{
		Clog::PrintLog(LogTag::Error, "GetAddrInfo error:%s", gai_strerror(status));
			return;
	}
	else
	{
		void* addr;
		char ipstr[INET_ADDRSTRLEN];
		struct sockaddr_in* socka = (struct sockaddr_in*)res->ai_addr;
		addr = &(socka->sin_addr);
		inet_ntop(res->ai_family, addr, ipstr, sizeof(ipstr));
		Clog::PrintLog(LogTag::Log, "%s", ipstr);

		this->_Addres = res;
		freeaddrinfo(res);
	}
}

void SocketInstance::ConnectToServer()
{
	if (this->_Status != SocketStatus::HasAddresInfo)
	{
		Clog::PrintLog(LogTag::Error, "You are alowed to connect to server after specifing its adress");
		return;
	}
	else
	{
		int status;
		void* addr = &this->_Addres->ai_addr;
		char straddr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, addr, straddr, sizeof(straddr));
		if ((status = connect(this->_Soc, (struct sockaddr*) & this->_Addres, sizeof(this->_Addres))) != 0)
		{
			Clog::PrintLog(LogTag::Error, "Failed to connect to server %s", straddr);
		}
		else
		{
			Clog::PrintLog(LogTag::Log, "Connected to %s", straddr);
		}
	}
}

const char* SocketInstance::GetName()
{
	return this->_Name;
}

SOCKET SocketInstance::GetSocket()
{
	return this->_Soc;
}

SocketStatus SocketInstance::GetStatus()
{
	return this->_Status;
}

void SocketInstance::SetStatus(SocketStatus stat)
{
	this->_Status = stat;
}

void SocketInstance::BindSocketFunction(std::function<void(int, char*, int)> func, SocketFunctionTypes type)
{
	switch (type)
	{
	case Welcome:
		this->WelcomeFunction = func;
		break;
	case Response:
		this->ResponseFunction = func;
		break;
	default:
		break;
	}
}

void SocketInstance::StopListeningThread()
{
	this->isListening = false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

SocketWrap::SocketWrap()
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


SocketWrap::~SocketWrap()
{
	WSACleanup();
}


std::shared_ptr<SocketInstance> SocketWrap::CreateSocket(const char* name, int proto)
{
	SOCKET soc;
	if (proto == IPPROTO_TCP)
		soc = socket(AF_INET, SOCK_STREAM, proto);
	else// if (proto == IPPROTO_UDP)
		soc = socket(AF_INET, SOCK_DGRAM, proto);
	if (soc == INVALID_SOCKET)
	{
		Clog::PrintLog(LogTag::Error, "Failed to create a socket. Error: %d",WSAGetLastError());
		return nullptr;
	}
	std::shared_ptr<SocketInstance> mySocket(new SocketInstance(name,soc,this));
	_Sockets.push_back(mySocket);
	return mySocket;
}

std::shared_ptr<SocketInstance> SocketWrap::CreateEmptySocket(const char* name)
{
	std::shared_ptr<SocketInstance> mySocket(new SocketInstance(name,this));
	_Sockets.push_back(mySocket);
	return mySocket;
}

std::shared_ptr<SocketInstance> SocketWrap::GetSocketByName(const char* name)
{
	for (auto a : this->_Sockets)
	{
		if (strcmp(a->GetName(), name) == 0)
		{
			return a;
		}
	}
	return nullptr;
}

void SocketWrap::CloseSocket(const char* name)
{
	bool found = false;
	int index = 0;
	for (int i=0;i<this->_Sockets.size();i++)
	{
		if (strcmp(this->_Sockets[i]->GetName(), name) == 0)
		{
			index = i;
			found = true;
			break;
		}
	}
	if (found == true)
	{
		this->_Sockets.erase(this->_Sockets.begin() + index);
	}
	else
	{
		Clog::PrintLog(LogTag::Error, "Socket named %s not found", name);
	}
}

//void SocketWrap::CreateSocket(const char* name, int proto)
//{
//	std::shared_ptr<MySocket> mySocket(new MySocket);
//	SOCKET soc;
//	if (proto == IPPROTO_TCP)
//		soc = socket(AF_INET, SOCK_STREAM, proto);
//	else// if (proto == IPPROTO_UDP)
//		soc = socket(AF_INET, SOCK_DGRAM, proto);
//	if (soc == INVALID_SOCKET)
//	{
//		Clog::PrintLog(LogTag::Error, "Failed to create a socket. Error: %d",WSAGetLastError());
//		return;
//	}
//	mySocket->name = name;
//	mySocket->soc = soc;
//	mySocket->MainSocket.sin_family = AF_INET;
//	mySocket->RecieveSocket.sin_family = AF_INET;
//	if (proto == IPPROTO_UDP)
//	{
//		mySocket->RecieveSocket.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
//	}
//	_Sockets.push_back(mySocket);
//}
//
//void SocketWrap::ConnectionData(const char* name,const char * ip, int port)
//{
//	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
//	if (sock == nullptr)
//	{
//		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found Error: %d",name,WSAGetLastError());
//		return;
//	}
//	if (sock->RecieveSocket.sin_addr.S_un.S_addr != htonl(INADDR_ANY))
//	{
//		sock->RecieveSocket.sin_addr.S_un.S_addr = inet_addr(ip);
//	}
//	sock->RecieveSocket.sin_port = htons(port);
//
//	sock->MainSocket.sin_addr.S_un.S_addr = inet_addr(ip);
//	sock->MainSocket.sin_port = htons(port);
//}
//
//std::shared_ptr<MySocket> SocketWrap::GetSocketByName(const char * name)
//{
//	for (auto x: _Sockets)
//	{
//		if (strcmp(x->name, name) == 0)
//		{
//			return x;
//		}
//	}
//	return nullptr;
//}
//
//void SocketWrap::ConnectWin(const char * name)
//{
//	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
//	if (sock == nullptr)
//	{
//		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found  Error: %d",  name,WSAGetLastError() );
//		return;
//	}
//	int result = connect(sock->soc, (struct sockaddr*)&sock->MainSocket, sizeof(sock->MainSocket));
//	if (result == SOCKET_ERROR)
//	{
//		Clog::PrintLog(LogTag::Error, "Failed to connect with socket named %s - Error: %d", name,WSAGetLastError());
//		return;
//	}
//}
//
//void SocketWrap::CloseConnection(const char * name)
//{
//	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
//	if (sock == nullptr)
//	{
//		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found. Error: %d", name, WSAGetLastError());
//		return;
//	}
//	int result = shutdown(sock->soc,SD_SEND);
//	if (result == SOCKET_ERROR)
//	{
//		Clog::PrintLog(LogTag::Error, "Failed to disconnect with socket named %s Error: %d", name, WSAGetLastError());
//		return;
//	}
//}
//
//int SocketWrap::SendTCP(const char * name,const char * sendBuff)
//{
//	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
//	if (sock == nullptr)
//	{
//		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found Error: %d", name, WSAGetLastError());
//		return -1;
//	}
//	int result = send(sock->soc, sendBuff, (int)strlen(sendBuff), 0);
//	if (result == SOCKET_ERROR)
//	{
//		Clog::PrintLog(LogTag::Error, "Failed to send with socket named %s Error: %d", name, WSAGetLastError());
//		return -1;
//	}
//	printf("%d bytes sent\n", result);
//	return result;
//}
//
//int SocketWrap::SendTCP(int soc, const char* sendBuff)
//{
//	int result = send(soc, sendBuff, (int)strlen(sendBuff), 0);
//	if (result == SOCKET_ERROR)
//	{
//		Clog::PrintLog(LogTag::Error, "Failed to send with socket num %d Error: %d", soc, WSAGetLastError());
//		return -1;
//	}
//	printf("%d bytes sent\n", result);
//	return result;
//}
//
//void SocketWrap::CreateListeningClient(const char* SocketName,const char* ThreadName, IPPROTO type)
//{
//	std::shared_ptr<ThreadSocket> ThrSock(new ThreadSocket);
//	for (auto x : _ThreadSockets)
//	{
//		if (strcmp(x->name, ThreadName) == 0)
//		{
//			Clog::PrintLog(LogTag::Error, "Thread named %s already exists", ThreadName);
//			return;
//		}
//	}
//	ThrSock->name = ThreadName;
//	std::shared_ptr<MySocket> MySock = SocketWrap::GetSocketByName(SocketName);
//	if (MySock == nullptr)
//	{
//		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found", SocketName);
//		return;
//	}
//	ThrSock->Socket = MySock;
//	ThrSock->thr = std::thread(&SocketWrap::ListenClient, this, ThrSock->Socket, type);
//	_ThreadSockets.push_back(ThrSock);
//
//	
//}
//
//void SocketWrap::ListenClient(std::shared_ptr<MySocket> sock,IPPROTO type)
//{
//	fd_set readfds;
//	SOCKET activity;
//	timeval tv;
//	tv.tv_sec = 1;
//	tv.tv_usec = 0;
//	int Length;
//	char Buffer[8192];
//
//	while (sock->Close.load() == false)
//	{
//		FD_ZERO(&readfds);
//		FD_SET(sock->soc, &readfds);
//
//		activity = select(sock->soc, &readfds, NULL, NULL, &tv);
//		if (activity < 0)
//		{
//			Clog::PrintLog(LogTag::Error, "Activity error");
//		}
//		if (FD_ISSET(sock->soc, &readfds))
//		{
//			if (type == IPPROTO_TCP)
//			{
//				Length = recv(sock->soc, Buffer, sizeof(Buffer), 0);
//				if (Length > 0)
//				{
//					//Data Recieved
//					sock->ResponseFunction(sock->soc, Buffer, Length);
//				}
//				else if (Length == 0)
//				{
//					//Conection closed
//					Clog::PrintLog(Log, "Connection closed");
//					sock->Close.store(true);
//					SocketWrap::CloseConnection(sock->name);
//				}
//				else
//				{
//					//Handle Error
//					Clog::PrintLog(LogTag::Error, "Failed recieving data in client listening func Error: %d",WSAGetLastError());
//					sock->Close.store(true);
//				}
//				
//			}
//			else if (type == IPPROTO_UDP)
//			{
//				int FromLen = sizeof(sock->RecieveSocket);
//				Length = recvfrom(sock->soc, Buffer, sizeof(Buffer), 0,(sockaddr*)&sock->RecieveSocket,&FromLen);
//				Clog::PrintLog(LogTag::Log, "Length: %d", Length);
//				if (Length > 0)
//				{
//					//Data Recieved
//					sock->ResponseFunction(sock->soc, Buffer, Length);
//
//				}
//				else if (Length == 0)
//				{
//					//Conection closed
//					sock->Close.store(true);
//				}
//				else
//				{
//					//Handle Error
//					Clog::PrintLog(LogTag::Error, "Failed recieving data in client listening func Error: %d", WSAGetLastError());
//					sock->Close.store(true);
//				}
//			}
//		}
//	}
//}
//
//
//int SocketWrap::RecvTCPServer(const char * name, int index, char * recvBuff)
//{
//	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
//	if (sock == nullptr)
//	{
//		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found Error: %d", name, WSAGetLastError());
//		return -1;
//	}
//	int result = recv(sock->ChildSockets[index].soc,recvBuff,(int)strlen(recvBuff),0);
//	if (result == SOCKET_ERROR)
//	{
//		Clog::PrintLog(LogTag::Error, "Failed to recieve with socket named %s Error: %d", name, WSAGetLastError());
//		return -1;
//	}
//	printf("%d bytes recieved\n", result);
//	return result;
//}
//
//int SocketWrap::SendUDP(const char * name, const char * sendBuff)
//{
//	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
//	if (sock == nullptr)
//	{
//		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found Error: %d", name, WSAGetLastError());
//		return -1;
//	}
//	int fromlen = sizeof(sock->MainSocket);
//	int result = sendto(sock->soc, sendBuff, strlen(sendBuff), 0, (sockaddr*)&sock->MainSocket, fromlen);
//	if (result == SOCKET_ERROR)
//	{
//		Clog::PrintLog(LogTag::Error, "Failed to send with socket named %s Error: %d", name, WSAGetLastError());
//		return -1;
//	}
//	printf("%d bytes sent\n", result);
//	return result;
//}
//
//void SocketWrap::CloseSocket(const char * name)
//{
//	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
//	if (sock == nullptr)
//	{
//		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found Error: %d", name, WSAGetLastError());
//		return;
//	}
//	int result = closesocket(sock->soc);
//	if (result == SOCKET_ERROR)
//	{
//		Clog::PrintLog(LogTag::Error, "Failed to close socket named %s Error: %d", name, WSAGetLastError());
//		return;
//	}
//}
//
//
//void SocketWrap::BindSocket(const char * name)
//{
//	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
//	if (sock == nullptr)
//	{
//		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found Error: %d", name, WSAGetLastError());
//		return;
//	}
//	int result = bind(sock->soc, (struct sockaddr*)&sock->MainSocket, sizeof(sock->MainSocket));
//	if (result == SOCKET_ERROR)
//	{
//		Clog::PrintLog(LogTag::Error, "Failed to bind socket named %s Error: %d", name, WSAGetLastError());
//		return;
//	}
//}
//
//
//void SocketWrap::BindSocketFunction(const char * name,SocketFunctionTypes type, std::function<void(int,char*, int)> fun)
//{
//	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
//	if (sock == nullptr)
//	{
//		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found Error: %d", name, WSAGetLastError());
//		return;
//	}
//	switch (type)
//	{
//	case Welcome:
//		sock->WelcomeFunction = fun;
//		break;
//	case Response:
//		sock->ResponseFunction = fun;
//		break;
//	default:
//		break;
//	}
//	
//}
//
//void SocketWrap::ListenServer(std::shared_ptr<MySocket> sock, int numberOfClients)
//{
//	fd_set readfds;
//	SOCKET maxSd=INVALID_SOCKET, sd = INVALID_SOCKET, activity,newSock= INVALID_SOCKET;
//	int addrLen = sizeof(sock->MainSocket);
//	char Buffer[8192];
//	timeval tv;
//	tv.tv_sec = 1;
//	for (int i = 0; i < (sizeof(sock->ChildSockets) / sizeof(sock->ChildSockets[0])); i++)
//	{
//		sock->ChildSockets[i].soc = 0;
//	}
//	if (listen(sock->soc, numberOfClients) == SOCKET_ERROR)
//	{
//		Clog::PrintLog(LogTag::Error, "Socket named %s failed to listen Error: %d", sock->name, WSAGetLastError());
//		return;
//	}
//	while(sock->Close.load()==false)
//	{
//		FD_ZERO(&readfds);
//		FD_SET(sock->soc, &readfds);
//		maxSd = sock->soc;
//		for (int i = 0; i < (sizeof(sock->ChildSockets) / sizeof(sock->ChildSockets[0])); i++)
//		{
//			sd = sock->ChildSockets[i].soc;
//			if (sd > 0)
//				FD_SET(sd, &readfds);
//			if (sd > maxSd)
//				maxSd = sd;
//		}
//
//		activity = select(maxSd + 1, &readfds, NULL, NULL, &tv);
//		if (activity < 0)
//		{
//			Clog::PrintLog(LogTag::Error, "Activity error");
//		}
//		if (FD_ISSET(sock->soc, &readfds))
//		{
//			Clog::PrintLog(LogTag::Log, "New connection");
//			if ((newSock = accept(sock->soc, NULL, NULL))<0)
//			{
//				Clog::PrintLog(LogTag::Error, "Failed to accept with socket %s Error: %d\n", sock->name, WSAGetLastError());
//				return;
//			}
//
//			// Welcome message
//			/*if (send(newSock, "Hallo\n", strlen("Hallo\n"), 0)!=strlen("Hallo\n"))
//			{
//				Clog::PrintLog(LogTag::Error, "Failed to send a message to socket %s Error: %d", sock->name,WSAGetLastError());
//			}*/
//			sock->WelcomeFunction(newSock, nullptr, 0);
//
//			for (int i = 0; i < (sizeof(sock->ChildSockets) / sizeof(sock->ChildSockets[0])); i++)
//			{
//				if (sock->ChildSockets[i].soc == 0)
//				{
//					sock->ChildSockets[i].soc = newSock;
//					int AddressSize = sizeof(sock->ChildSockets[i].AdressInfo);
//					getpeername(sock->ChildSockets[i].soc,(sockaddr*)&sock->ChildSockets[i].AdressInfo,&AddressSize);
//					break;
//				}
//			}
//
//		}
//		else
//		{
//			for (int i = 0; i < (sizeof(sock->ChildSockets) / sizeof(sock->ChildSockets[0])); i++)
//			{
//				sd = sock->ChildSockets[i].soc;
//				if (FD_ISSET(sd, &readfds))
//				{
//					int valread;
//					if ((valread = recv(sd, Buffer, sizeof(Buffer) / sizeof(Buffer[0]), 0))==0)
//					{
//						//Ending connection prompt
//						Clog::PrintLog(LogTag::Log, "Host disconected. IP: %s\n", inet_ntoa(sock->ChildSockets[i].AdressInfo.sin_addr));
//						closesocket(sd);
//						sock->ChildSockets[i].soc = 0;
//					}
//					else if(valread>0)
//					{
//						//Process recieved data
//						sock->ResponseFunction(sd, Buffer, valread);					
//					}
//				}
//			}
//		}
//	}
//	printf("Ending thread");
//}
//
//void SocketWrap::CreateListeningServerThread(const char * ThreadName, const char * SocketName)
//{
//	std::shared_ptr<ThreadSocket> ThrSock(new ThreadSocket);
//	for (auto x : _ThreadSockets)
//	{
//		if (strcmp(x->name, ThreadName) == 0)
//		{
//			Clog::PrintLog(LogTag::Error, "Thread named %s already exists", ThreadName);
//			return;
//		}
//	}
//	ThrSock->name = ThreadName;
//	std::shared_ptr<MySocket> MySock =SocketWrap::GetSocketByName(SocketName);
//	if (MySock == nullptr)
//	{
//		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found", SocketName);
//		return;
//	}
//	ThrSock->Socket = MySock;
//	ThrSock->thr = std::thread(&SocketWrap::ListenServer,this,ThrSock->Socket,1);
//	_ThreadSockets.push_back(ThrSock);
//}
//
//void SocketWrap::StopThread(const char * ThreadName)
//{
//	std::shared_ptr<ThreadSocket> thr = nullptr;
//	for (auto x : _ThreadSockets)
//	{
//		if (strcmp(x->name, ThreadName) == 0)
//		{
//			thr = x;
//			break;
//		}
//	}
//	if (thr == nullptr)
//	{
//		Clog::PrintLog(LogTag::Error, "Failed to find thread named %s", ThreadName);
//		return;
//	}
//	thr->Socket->Close.store(true);
//	thr->thr.join();
//	for (uint32_t i = 0; i <= _ThreadSockets.size(); i++)
//	{
//		if (strcmp(_ThreadSockets[i]->name, ThreadName) == 0)
//		{
//			_ThreadSockets.erase(_ThreadSockets.begin() + i);
//		}
//	}
//}

