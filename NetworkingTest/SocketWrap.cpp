#include "pch.h"
#include "SocketWrap.h"
#include <string>


SocketInstance::~SocketInstance()
{
	if (this->_Address != nullptr)
	{
		//freeaddrinfo(this->_Address);
	}
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
			Clog::PrintLog(LogTag::Error, "%s > GetAddrInfo error:%s",this->_Name, gai_strerror(status));
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
				//Clog::PrintLog(LogTag::Log, "%s", ipstr);
			}
			this->_Address = res;
			this->_Address->ai_protocol = IPPROTO_TCP;
			this->_Status = SocketStatus::HasAddresInfo;
		}
		if (this->_Status == SocketStatus::HasAddresInfo && this->_SocType == SocketType::Server)
		{
			if (status = bind(this->_Soc, this->_Address->ai_addr, this->_Address->ai_addrlen) != 0)
			{
				Clog::PrintLog(LogTag::Error, "%s > Server bind error: %d", this->_Name,WSAGetLastError());
				return;
			}
			else
			{
				Clog::PrintLog(LogTag::Log, "%s > Server has been binded", this->_Name);
				this->_Status = SocketStatus::Binded;
				this->_Port = port;
			}

		}

}

void SocketInstance::CreateListeningThread(int maxClients)
{
	if(this->ListeningThread.native_handle() == NULL && this->_SocType == SocketType::Server)
	this->ListeningThread = std::thread(&SocketInstance::ListenServerFunc, this, maxClients);
}

void SocketInstance::RecieveFunc()
{
	fd_set readfds;
	SOCKET activity;
	timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	int Length;
	char Buffer[8192];
	this->isListening = true;
	while (this->isListening.load() == true && this->_Status == SocketStatus::Connected)
	{
		FD_ZERO(&readfds);
		FD_SET(this->_Soc, &readfds);

		activity = select(this->_Soc, &readfds, NULL, NULL, &tv);
		if (activity < 0)
		{
			Clog::PrintLog(LogTag::Error, "%s > Activity error",this->_Name);
		}
		if (FD_ISSET(this->_Soc, &readfds))
		{
			if (this->_Address->ai_protocol == IPPROTO_TCP)
			{
				Length = recv(this->_Soc, Buffer, sizeof(Buffer), 0);
				if (Length > 0)
				{
					//Data Recieved
					this->ResponseFunction(std::make_shared<SocketInstance>(*this), Buffer, Length);
				}
				else if (Length == 0)
				{
					//Conection closed
					Clog::PrintLog(Log, "%s > Connection closed",this->_Name);
					this->isListening.store(false);
					this->_Status = SocketStatus::HasAddresInfo;
					this->CloseConnection();
				}
				else
				{
					//Handle Error
					Clog::PrintLog(LogTag::Error, "%s > Failed recieving data in client listening func Error: %d",this->_Name,WSAGetLastError());
					this->isListening.store(false);
					this->_Status = SocketStatus::HasAddresInfo;
					this->CloseConnection();
				}			
			}
			else if (this->_Address->ai_protocol == IPPROTO_UDP)
			{
				int FromLen = this->_Address->ai_addrlen;
				Length = recvfrom(this->_Soc, Buffer, sizeof(Buffer), 0,this->_Address->ai_addr,&FromLen);
				Clog::PrintLog(LogTag::Log, "Length: %d", Length);
				if (Length > 0)
				{
					//Data Recieved
					this->ResponseFunction(std::make_shared<SocketInstance>(*this), Buffer, Length);
				}
				else if (Length == 0)
				{
					//Conection closed
					this->isListening.store(false);
				}
				else
				{
					//Handle Error
					Clog::PrintLog(LogTag::Error, "%s > Failed recieving data in client listening func Error: %d",this->_Name, WSAGetLastError());
					this->isListening.store(false);
				}
			}
		}
	}
	Clog::PrintLog(LogTag::Log, "Stopping recieving thread");
}

void SocketInstance::ListenServerFunc(int maxClients)
{
	if (this->_SocType == SocketType::Server && this->_Status == SocketStatus::Binded)
	{
		fd_set readfds;
		SOCKET maxSd = INVALID_SOCKET, sd = INVALID_SOCKET, activity, newSock = INVALID_SOCKET;
		int addrLen = sizeof(this->_Address);
		char Buffer[8192];
		timeval tv;
		tv.tv_sec = 1;

		/*////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		Need to be fixed
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/
		const char* MasterName = this->_Name;
		std::string childname(this->_Name);
		childname += " ";
		for (int i = this->ServerSockets.size(); i < maxClients; i++)
		{
			std::string number = std::to_string(i);
			std::string Final(childname.c_str());
			Final += number;

			auto child = this->_Reference->CreateEmptySocket(Final.c_str());
			child->_SocType = SocketType::Client;
			this->ServerSockets.push_back(child);
		}
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		if (listen(this->_Soc, maxClients) == SOCKET_ERROR)
		{
			Clog::PrintLog(LogTag::Error, "%s > Socket failed to listen Error: %d", this->_Soc, WSAGetLastError());
			return;
		}

		void* addr;
		char ipstr[INET_ADDRSTRLEN];
		struct sockaddr_in* socka = (struct sockaddr_in*)this->_Address->ai_addr;
		addr = &(socka->sin_addr);
		inet_ntop(this->_Address->ai_family, addr, ipstr, sizeof(ipstr));

		Clog::PrintLog(LogTag::Log, "%s > Server listening at %s",this->_Name, ipstr);
		this->isListening = true;
		while (this->isListening.load() == true)
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
				Clog::PrintLog(LogTag::Error, "%s > Activity error",this->_Name);
			}
			if (FD_ISSET(this->_Soc, &readfds))
			{
				Clog::PrintLog(LogTag::Log, "%s > New connection",this->_Name);
				if ((newSock = accept(this->_Soc, NULL, NULL)) < 0)
				{
					Clog::PrintLog(LogTag::Error, "%s > Failed to accept Error: %d\n", this->_Name, WSAGetLastError());
					return;
				}

				// Welcome message
				/*if (send(newSock, "Hallo\n", strlen("Hallo\n"), 0)!=strlen("Hallo\n"))
				{
					Clog::PrintLog(LogTag::Error, "Failed to send a message to socket %s Error: %d", sock->name,WSAGetLastError());
				}*/


				for (int i = 0; i < this->ServerSockets.size(); i++)
				{
					if (this->ServerSockets[i]->_Soc == 0)
					{
						struct sockaddr addr;
						struct addrinfo hints, * res;

						this->ServerSockets[i]->_Soc = newSock;
						int AddressSize = sizeof(addr);
						getpeername(this->ServerSockets[i]->_Soc, (struct sockaddr*)&addr, &AddressSize);

						memset(&hints, 0, sizeof(hints));
						hints.ai_family = AF_INET;
						hints.ai_socktype = SOCK_STREAM;

						void* addres;
						char ipstr[INET_ADDRSTRLEN];
						struct sockaddr_in* socka = (struct sockaddr_in*)&addr;
						addres = &(socka->sin_addr);
						inet_ntop(AF_INET, addres, ipstr, sizeof(ipstr));

						getaddrinfo(ipstr, this->_Port, &hints, &res);

						this->ServerSockets[i]->_Address = res;
						this->ServerSockets[i]->_Address->ai_protocol = IPPROTO_TCP;
						this->ServerSockets[i]->_Status = SocketStatus::Connected;
						std::shared_ptr<SocketInstance> inst(this->ServerSockets[i]);
						this->WelcomeFunction(inst, nullptr, 0);
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
							struct sockaddr_in* socka = (struct sockaddr_in*)this->ServerSockets[i]->_Address->ai_addr;
							addr = &(socka->sin_addr);
							inet_ntop(this->ServerSockets[i]->_Address->ai_family, addr, ipstr, sizeof(ipstr));
							Clog::PrintLog(LogTag::Log, "%s > Host disconected. IP: %s\n",this->_Name, ipstr);
							closesocket(sd);
							this->ServerSockets[i]->_Soc = 0;
							this->ServerSockets[i]->_Status = SocketStatus::Created;
						}
						else if (valread > 0)
						{
							//Process recieved data
							std::shared_ptr<SocketInstance> inst(this->ServerSockets[i]);
							this->ResponseFunction(inst, Buffer, valread);
						}
					}
				}
			}
		}
		Clog::PrintLog(LogTag::Log, "%s > Stoping listening", this->_Name);
		for (auto i : this->ServerSockets)
		{

		}
	}
	else
	{
		Clog::PrintLog(LogTag::Error, "%s > Server is not prepared for listening", this->_Name);
	}
}


void SocketInstance::SetupClient(const char* addr, const char* port)
{		
	int status;
	struct addrinfo hints, * res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	this->_SocType = SocketType::Client;
	if ((status = getaddrinfo(addr, port, &hints, &res) != 0))
	{
		Clog::PrintLog(LogTag::Error, "%s > GetAddrInfo error:%s",this->_Name, gai_strerror(status));
			return;
	}
	else
	{
		void* addr;
		char ipstr[INET_ADDRSTRLEN];
		struct sockaddr_in* socka = (struct sockaddr_in*)res->ai_addr;
		addr = &(socka->sin_addr);
		inet_ntop(res->ai_family, addr, ipstr, sizeof(ipstr));
		//Clog::PrintLog(LogTag::Log, "%s", ipstr);
		this->_Address = res;
		this->_Address->ai_protocol = IPPROTO_TCP;
		this->_Status = SocketStatus::HasAddresInfo;
	}
}

void SocketInstance::ConnectToServer()
{
	if (this->_Status != SocketStatus::HasAddresInfo)
	{
		Clog::PrintLog(LogTag::Error, "%s > You are alowed to connect to server after specifing its adress",this->_Name);
		return;
	}
	else
	{

		void* addr;
		char ipstr[INET_ADDRSTRLEN];
		struct sockaddr_in* socka = (struct sockaddr_in*)this->_Address->ai_addr;
		addr = &(socka->sin_addr);
		inet_ntop(this->_Address->ai_family, addr, ipstr, sizeof(ipstr));

		int status;
		if ((status = connect(this->_Soc, this->_Address->ai_addr, this->_Address->ai_addrlen)) != 0)
		{
			Clog::PrintLog(LogTag::Error, "%s > Failed to connect to server %s",this->_Name, ipstr);
		}
		else
		{
			Clog::PrintLog(LogTag::Log, "%s > Connected to %s",this->_Name, ipstr);
		}
	}
}

void SocketInstance::SendTCPClient(void* data, size_t size)
{
	if (this->_Status == SocketStatus::Connected)
	{
		char* ptr = (char*)data;
		size_t length = size;
		size_t PartSize = 0;
		while (0 < length)
		{
			PartSize = send(this->_Soc, ptr, size, NULL);
			if (PartSize == SOCKET_ERROR)
			{
				Clog::PrintLog(LogTag::Error, "%s > Failed to send data", this->_Name);
				return;
			}
			ptr += PartSize;
			length -= PartSize;
		}
		Clog::PrintLog(LogTag::Log, "%s > Sent %d bytes", this->_Name, size);
	}
	else
	{
		Clog::PrintLog(LogTag::Error, "%s > You have to connect to server first.",this->_Name);
		return;
	}
}

void SocketInstance::RecieveTCPClient(void* dataBuffer, size_t bufferSize)
{
	int recievedBytes = recv(this->_Soc, (char*)dataBuffer, bufferSize, NULL);
	if (recievedBytes > 0)
	{
		Clog::PrintLog(LogTag::Log, "%s > Recieved %d bytes.", recievedBytes, this->_Name);
	}
	else if (recievedBytes == 0)
	{
		Clog::PrintLog(LogTag::Log, "%s > Connection closed", this->_Name);
		this->_Status = SocketStatus::HasAddresInfo;
	}
	else
	{
		Clog::PrintLog(LogTag::Error, "%s > Failed to recieve data", this->_Name);
	}
}

void SocketInstance::CreateRecieveThread()
{
	if (this->ListeningThread.native_handle() == NULL && this->_SocType == SocketType::Client)
		this->ListeningThread = std::thread(&SocketInstance::RecieveFunc, this);
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

void SocketInstance::BindSocketFunction(std::function<void(std::shared_ptr<SocketInstance>, char*, int)> func, SocketFunctionTypes type)
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
	this->ListeningThread.join();
}

void SocketInstance::CloseConnection()
{
	int result;
	if ((result = shutdown(this->_Soc, SD_SEND)) == SOCKET_ERROR)
	{
		Clog::PrintLog(LogTag::Error, "%s > Failed to close connection", this->_Name);
	}
	
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


std::shared_ptr<SocketInstance> SocketWrap::CreateSocket(const char* name, IPPROTO proto)
{
	SOCKET soc;
	if (proto == IPPROTO_TCP)
		soc = socket(AF_INET, SOCK_STREAM, proto);
	else// if (proto == IPPROTO_UDP)
		soc = socket(AF_INET, SOCK_DGRAM, proto);
	if (soc == INVALID_SOCKET)
	{
		Clog::PrintLog(LogTag::Error, "%s > Failed to create a socket. Error: %d",name,WSAGetLastError());
		return nullptr;
	}
	auto mySocket = std::make_shared<SocketInstance>(name,soc,this);
	_Sockets.push_back(mySocket);
	return mySocket;
}

std::shared_ptr<SocketInstance> SocketWrap::CreateEmptySocket(const char* name)
{
	auto mySocket = std::make_shared<SocketInstance>(name,this);
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

