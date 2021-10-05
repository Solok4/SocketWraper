#include "SocketWrap.h"
#include <string>

#ifndef _WIN32
	#include <fcntl.h>
	#include <unistd.h>
	#include <errno.h>
#endif


SocketInstance::~SocketInstance()
{
}

void SocketInstance::SetupServer(uint32_t port)
{
	this->_SocType = SocketType::Server;
	int status;
	struct addrinfo hints, * res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if ((status = getaddrinfo(NULL, std::to_string(port).c_str(), &hints, &res) != 0))
	{
		CLog::error("%s > GetAddrInfo error:%s", this->_Name.c_str(), gai_strerror(status));
		return;
	}
	else
	{
		for (addrinfo* p = res; p != NULL; p = p->ai_next)
		{
			void* addr;
			char ipstr[INET_ADDRSTRLEN];
			struct sockaddr_in* socka = (struct sockaddr_in*)p->ai_addr;
			addr = &(socka->sin_addr);
			inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
		}
		this->_Address = res;
		this->_Address->ai_protocol = IPPROTO_TCP;
		this->_Status = SocketStatus::HasAddresInfo;
	}
	if (this->_Status == SocketStatus::HasAddresInfo && this->_SocType == SocketType::Server)
	{
		if (status = bind(this->_Soc, this->_Address->ai_addr, this->_Address->ai_addrlen) != 0)
		{
#ifdef _WIN32
			CLog::error("%s > Server bind error: %d", this->_Name.c_str(), WSAGetLastError());
#else
			CLog::error("%s > Server bind error", this->_Name.c_str());
#endif // _WIN32
			return;
		}
		else
		{
			CLog::info("%s > Server has been binded", this->_Name.c_str());
			this->_Status = SocketStatus::Binded;
			this->_Port = port;
		}

	}

}

void SocketInstance::CreateListeningThread(int maxClients)
{
	if (this->ListeningThread.native_handle() == 0 && this->_SocType == SocketType::Server)
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
			CLog::error("%s > Activity error", this->_Name.c_str());
		}
		if (FD_ISSET(this->_Soc, &readfds))
		{
			if (this->_Address->ai_protocol == IPPROTO_TCP)
			{
				Length = recv(this->_Soc, Buffer, sizeof(Buffer), 0);
				if (Length > 0)
				{
					//Data Recieved
					std::shared_ptr<SocketInstance> temp = std::make_shared<SocketInstance>(*this);
					this->ResponseFunction(temp, Buffer, Length);
				}
				else if (Length == 0)
				{
					//Conection closed
					CLog::info("%s > Connection closed", this->_Name.c_str());
					this->isListening.store(false);
					this->_Status = SocketStatus::HasAddresInfo;
					this->CloseConnection();
				}
				else
				{
					//Handle Error
#ifdef _WIN32
					CLog::error("%s > Failed recieving data in client listening func Error: %d", this->_Name.c_str(), WSAGetLastError());
#else
					CLog::error("%s > Failed recieving data in client listening func Error", this->_Name.c_str());
#endif // _WIN32
					this->isListening.store(false);
					this->_Status = SocketStatus::HasAddresInfo;
					this->CloseConnection();
				}
			}
			else if (this->_Address->ai_protocol == IPPROTO_UDP)
			{
#ifdef _WIN32
				int FromLen = this->_Address->ai_addrlen;
#else
				socklen_t FromLen = this->_Address->ai_addrlen;
#endif // _WIN32
				Length = recvfrom(this->_Soc, Buffer, sizeof(Buffer), 0, this->_Address->ai_addr, &FromLen);
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
#ifdef _WIN32
					CLog::error("%s > Failed recieving data in client listening func Error: %d", this->_Name.c_str(), WSAGetLastError());
#else
					CLog::error("%s > Failed recieving data in client listening func Error", this->_Name.c_str());
#endif // _WIN32
					this->isListening.store(false);
				}
			}
		}
	}
	CLog::info("%s > Stopping recieving thread",this->_Name.c_str());
}

void SocketInstance::ListenServerFunc(int maxClients)
{
	if (this->_SocType == SocketType::Server && this->_Status == SocketStatus::Binded)
	{
		CLog::info("%s > Creating %d server listeners",this->GetName().c_str(), maxClients);
		fd_set readfds;
		SOCKET maxSd = INVALID_SOCKET, sd = INVALID_SOCKET, activity, newSock = INVALID_SOCKET;
		int addrLen = sizeof(this->_Address);
		char Buffer[8192];
		timeval tv;
		tv.tv_sec = 1;

		/*////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		Need to be fixed
		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/

		for (int i = this->ServerSockets.size(); i < maxClients; i++)
		{
			std::string finalName = this->GetName() + "-" + std::to_string(i);
			auto child = this->_Reference->CreateEmptySocket(finalName);
			child->_SocType = SocketType::Client;
			this->ServerSockets.push_back(child);
		}
		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		if (listen(this->_Soc, maxClients) == SOCKET_ERROR)
		{
#ifdef _WIN32
			CLog::error("%s > Socket failed to listen Error: %d", this->_Name.c_str(), WSAGetLastError());
#else
			CLog::error("%s > Socket failed to listen Error", this->_Name.c_str());
#endif // _WIN32
			return;
		}
		this->_Status = SocketStatus::Listening;

		void* addr;
		char ipstr[INET_ADDRSTRLEN];
		struct sockaddr_in* socka = (struct sockaddr_in*)this->_Address->ai_addr;
		addr = &(socka->sin_addr);
		inet_ntop(this->_Address->ai_family, addr, ipstr, sizeof(ipstr));

		CLog::info("%s > Server listening at %s", this->_Name.c_str(), ipstr);
		this->isListening = true;
		while (this->isListening.load() == true && this->_Status == SocketStatus::Listening)
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
				CLog::error("%s > Activity error", this->_Name.c_str());
			}
			if (FD_ISSET(this->_Soc, &readfds))
			{
				CLog::info("%s > New connection", this->_Name.c_str());
				if ((newSock = accept(this->_Soc, NULL, NULL)) < 0)
				{
#ifdef _WIN32
					CLog::error("%s > Failed to accept Error: %d\n", this->_Name.c_str(), WSAGetLastError());
#else
					CLog::error("%s > Failed to accept Error\n", this->_Name.c_str());
#endif // _WIN32
					return;
				}

				// Welcome message
				/*if (send(newSock, "Hallo\n", strlen("Hallo\n"), 0)!=strlen("Hallo\n"))
				{
					CLog:Log(LogTag::Error, "Failed to send a message to socket %s Error: %d", sock->name,WSAGetLastError());
				}*/


				for (int i = 0; i < this->ServerSockets.size(); i++)
				{
					if (this->ServerSockets[i]->_Soc == 0)
					{
						struct sockaddr addr;
						struct addrinfo hints, * res;

						this->ServerSockets[i]->_Soc = newSock;
#ifdef _WIN32
						int AddressSize = sizeof(addr);
#else
						socklen_t AddressSize = sizeof(addr);
#endif // _WIN32
						getpeername(this->ServerSockets[i]->_Soc, (struct sockaddr*) & addr, &AddressSize);

						memset(&hints, 0, sizeof(hints));
						hints.ai_family = AF_INET;
						hints.ai_socktype = SOCK_STREAM;

						void* addres;
						char ipstr[INET_ADDRSTRLEN];
						struct sockaddr_in* socka = (struct sockaddr_in*) & addr;
						addres = &(socka->sin_addr);
						inet_ntop(AF_INET, addres, ipstr, sizeof(ipstr));

						getaddrinfo(ipstr, std::to_string(this->_Port).c_str(), &hints, &res);

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
							CLog::info("%s > Host disconected. IP: %s\n", this->_Name.c_str(), ipstr);
							this->ServerSockets[i]->CloseConnection();
							this->ServerSockets[i]->_Soc = 0;
							this->ServerSockets[i]->_Status = SocketStatus::Initial;
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
		CLog::info("%s > Stoping listening", this->_Name.c_str());
		for (auto i : this->ServerSockets)
		{

		}
	}
	else
	{
		CLog::error("%s > Server is not prepared for listening", this->_Name.c_str());
	}
}


void SocketInstance::SetupClient(std::string addr, uint32_t port)
{
	int status;
	struct addrinfo hints, * res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	this->_SocType = SocketType::Client;
	if ((status = getaddrinfo(addr.c_str(), std::to_string(port).c_str(), &hints, &res) != 0))
	{
		CLog::error("%s > GetAddrInfo error:%s", this->_Name.c_str(), gai_strerror(status));
		return;
	}
	else
	{
		void* addr;
		char ipstr[INET_ADDRSTRLEN];
		struct sockaddr_in* socka = (struct sockaddr_in*)res->ai_addr;
		addr = &(socka->sin_addr);
		inet_ntop(res->ai_family, addr, ipstr, sizeof(ipstr));
		this->_Address = res;
		this->_Address->ai_protocol = IPPROTO_TCP;
		this->_Status = SocketStatus::HasAddresInfo;
	}
}

void SocketInstance::ConnectToServer()
{
	if (this->_Status != SocketStatus::HasAddresInfo)
	{
		CLog::error("%s > You are alowed to connect to server after specifing its adress", this->_Name.c_str());
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
			CLog::error("%s > Failed to connect to server %s, errno: %d", this->_Name.c_str(), ipstr, errno);
		}
		else
		{
			CLog::info("%s > Connected to %s", this->_Name.c_str(), ipstr);
			this->_Status = SocketStatus::Connected;
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
			PartSize = send(this->_Soc, ptr, size, 0);
			if (PartSize == SOCKET_ERROR)
			{
				CLog::error("%s > Failed to send data", this->_Name.c_str());
				return;
			}
			ptr += PartSize;
			length -= PartSize;
		}
		CLog::info("%s > Sent %d bytes", this->_Name.c_str(), size);
	}
	else
	{
		CLog::error("%s > You have to connect to server first.", this->_Name.c_str());
		return;
	}
}

void SocketInstance::RecieveTCPClient(void* dataBuffer, size_t bufferSize)
{
	if (this->_Status == SocketStatus::Connected)
	{
		int recievedBytes = recv(this->_Soc, (char*)dataBuffer, bufferSize, 0);
		if (recievedBytes > 0)
		{
			CLog::info("%s > Recieved %d bytes.", recievedBytes, this->_Name.c_str());
		}
		else if (recievedBytes == 0)
		{
			CLog::info("%s > Connection closed", this->_Name.c_str());
			this->_Status = SocketStatus::HasAddresInfo;
		}
		else
		{
			CLog::error("%s > Failed to recieve data", this->_Name.c_str());
		}
	}
}

void SocketInstance::CreateRecieveThread()
{
	if (this->ListeningThread.native_handle() == 0 && this->_SocType == SocketType::Client)
		this->ListeningThread = std::thread(&SocketInstance::RecieveFunc, this);
}

std::string SocketInstance::GetName()
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
	if (this->_Status == SocketStatus::Connected)
	{
		CLog::info("%s > Socket Closing %d", this->_Name.c_str(), this->_Soc);
		if (this->_Soc != 0)
			if ((result = shutdown(this->_Soc, SD_SEND)) == SOCKET_ERROR)
			{
				CLog::error("%s > Failed to close connection", this->_Name.c_str());
			}
			else
			{
				this->_Status = SocketStatus::HasAddresInfo;
			}
	}
	else if (this->_SocType == SocketType::Server && (this->_Status == SocketStatus::Binded ||this->_Status == SocketStatus::Listening))
	{
		CLog::info("%s > Server socket Closing %d", this->_Name.c_str(), this->_Soc);
#ifdef _WIN32
		if(closesocket(this->_Soc)!=0)
#else
		if (close(this->_Soc) != 0)
#endif
		{
			CLog::info("%s > Failed closing server socket %d", this->_Name.c_str(), this->_Soc);
		}
		this->_Status = SocketStatus::Initial;
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
	CLog::error("Failed to initalize WSA");
		WSACleanup();
		return;
	}
#endif // WIN
}


SocketWrap::~SocketWrap()
{
#ifdef _WIN32
	WSACleanup();
#endif // _WIN32
}

#ifndef _WIN32
std::shared_ptr<SocketInstance> SocketWrap::CreateSocket(std::string name, int proto)
#else
std::shared_ptr<SocketInstance> SocketWrap::CreateSocket(const char* name, IPPROTO proto)
#endif // !_WIN32
{
	SOCKET soc;
	if (proto == IPPROTO_TCP)
		soc = socket(AF_INET, SOCK_STREAM, proto);
	else// if (proto == IPPROTO_UDP)
		soc = socket(AF_INET, SOCK_DGRAM, proto);
	if (soc == INVALID_SOCKET)
	{
#ifdef _WIN32
		CLog::error("%s > Failed to create a socket. Error: %d", name.c_str(), WSAGetLastError());
#else
		CLog::error("%s > Failed to create a socket", name.c_str());
#endif // _WIN32
		return nullptr;
	}
	int ReuseAddr = 1;
	int ReuseAddrLen = sizeof(ReuseAddr);
	setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, (const char*)& ReuseAddr, ReuseAddrLen);

	unsigned long nonBlocking =0;
#ifdef _WIN32
	ioctlsocket(soc, FIONBIO, &NonBlocking);
#else
	if(nonBlocking){
		int flags = fcntl(soc, F_SETFL, fcntl(soc, F_GETFL, 0) | O_NONBLOCK);
		if(flags) CLog::error("%s > Failed to set socket mode to non blocking", name.c_str());
	}
#endif

	std::string finalName = name;
	auto mySocket = std::make_shared<SocketInstance>(finalName, soc, this);
	_Sockets.push_back(mySocket);
	return mySocket;
}

std::shared_ptr<SocketInstance> SocketWrap::CreateEmptySocket(std::string name)
{
	std::string finalName = name;
	auto mySocket = std::make_shared<SocketInstance>(std::string(name), this);
	_Sockets.push_back(mySocket);
	return mySocket;
}

std::shared_ptr<SocketInstance> SocketWrap::GetSocketByName(std::string name)
{
	for (auto a : this->_Sockets)
	{
		if (name.compare(a->GetName().c_str()) == 0)
		{
			return a;
		}
	}
	return nullptr;
}

void SocketWrap::CloseSocket(std::string name)
{
	for (int i = 0; i < this->_Sockets.size(); i++)
	{
		if (name.compare(this->_Sockets[i]->GetName().c_str()) == 0)
		{
			this->_Sockets.erase(this->_Sockets.begin() + i);
			return;
		}
	}
	CLog::error("Socket named %s not found", name.c_str());
}
