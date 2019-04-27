#include "pch.h"
#include "SocketWrap.h"


SocketWrap::SocketWrap()
{
}


SocketWrap::~SocketWrap()
{
}

void SocketWrap::Initalize()
{
}

void SocketWrap::InitalizeWin()
{
	int error = WSAStartup(MAKEWORD(2, 2), &this->_WsaData);
	if (error != NO_ERROR)
	{
		Clog::PrintLog(LogTag::Error, "Failed to initalize WSA");
		WSACleanup();
		return;
	}
}

void SocketWrap::CreateSocketWin(const char* name, int proto)
{
	std::shared_ptr<MySocket> mySocket(new MySocket);
	SOCKET soc;
	if (proto == IPPROTO_TCP)
		soc = socket(AF_INET, SOCK_STREAM, proto);
	else if (proto == IPPROTO_UDP)
		soc = socket(AF_INET, SOCK_DGRAM, proto);
	if (soc == INVALID_SOCKET)
	{
		Clog::PrintLog(LogTag::Error, "Failed to create a socket. Error: %d",WSAGetLastError());
		return;
	}
	mySocket->name = name;
	mySocket->soc = soc;
	mySocket->First.sin_family = AF_INET;
	if (proto == IPPROTO_UDP)
	{
		mySocket->First.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	}
	_Sockets.push_back(mySocket);
}

void SocketWrap::ConnectionDataWin(const char* name,const char * ip, int port)
{
	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
	if (sock == nullptr)
	{
		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found Error: %d",name,WSAGetLastError());
		return;
	}
	if (sock->First.sin_addr.S_un.S_addr != htonl(INADDR_ANY))
	{
		sock->First.sin_addr.S_un.S_addr = inet_addr(ip);
	}
	sock->First.sin_port = htons(port);

	sock->Second.sin_addr.S_un.S_addr = inet_addr(ip);
	sock->Second.sin_port = htons(port);
	sock->Second.sin_family = AF_INET;
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
	int result = connect(sock->soc, (struct sockaddr*)&sock->First, sizeof(sock->First));
	if (result == SOCKET_ERROR)
	{
		Clog::PrintLog(LogTag::Error, "Failed to connect with socket named %s - Error: %d", name,WSAGetLastError());
		return;
	}
}

void SocketWrap::CloseConnectionWin(const char * name)
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

int SocketWrap::SendTCPWin(const char * name,const char * sendBuff)
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

int SocketWrap::RecvTCPWin(const char * name, char * recvBuff)
{
	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
	if (sock == nullptr)
	{
		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found Error: %d", name, WSAGetLastError());
		return -1;
	}

	int result = recv(sock->soc,recvBuff,(int)strlen(recvBuff),0);
	if (result == SOCKET_ERROR)
	{
		Clog::PrintLog(LogTag::Error, "Failed to recieve with socket named %s Error: %d", name, WSAGetLastError());
		return -1;
	}
	else if (result == 0)
	{
		Clog::PrintLog(LogTag::Log, "Connection closed");
	}
	printf("%d bytes recieved\n", result);
	return result;
}


int SocketWrap::RecvTCPServerWin(const char * name, int index, char * recvBuff)
{
	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
	if (sock == nullptr)
	{
		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found Error: %d", name, WSAGetLastError());
		return -1;
	}
	int result = recv(sock->ChildSockets[index],recvBuff,(int)strlen(recvBuff),0);
	if (result == SOCKET_ERROR)
	{
		Clog::PrintLog(LogTag::Error, "Failed to recieve with socket named %s Error: %d", name, WSAGetLastError());
		return -1;
	}
	printf("%d bytes recieved\n", result);
	return result;
}

int SocketWrap::SendUDPWin(const char * name, const char * sendBuff)
{
	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
	if (sock == nullptr)
	{
		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found Error: %d", name, WSAGetLastError());
		return -1;
	}
	int fromlen = sizeof(sock->Second);
	int result = sendto(sock->soc, sendBuff, strlen(sendBuff), 0, (sockaddr*)&sock->Second, fromlen);
	if (result == SOCKET_ERROR)
	{
		Clog::PrintLog(LogTag::Error, "Failed to send with socket named %s Error: %d", name, WSAGetLastError());
		return -1;
	}
	printf("%d bytes sent\n", result);
	return result;
}

int SocketWrap::RecvUDPWin(const char * name, char * recvBuff)
{
	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
	if (sock == nullptr)
	{
		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found Error: %d", name, WSAGetLastError());
		return -1;
	}
	int fromLen = sizeof(sock->First);
	int result = recvfrom(sock->soc, recvBuff, strlen(recvBuff), 0, (sockaddr*)&sock->First, &fromLen);
	if (result == SOCKET_ERROR)
	{
		Clog::PrintLog(LogTag::Error, "Failed to recieve with socket named %s Error: %d", name, WSAGetLastError());
		return -1;
	}
	printf("%d bytes recieved\n", result);
	return result;
}

void SocketWrap::CloseSocketWin(const char * name)
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

void SocketWrap::ShutdownWin()
{
	for (auto x : _Sockets)
	{
		closesocket(x->soc);
	}
	WSACleanup();
}

void SocketWrap::BindSocketWin(const char * name)
{
	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
	if (sock == nullptr)
	{
		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found Error: %d", name, WSAGetLastError());
		return;
	}
	int result = bind(sock->soc, (struct sockaddr*)&sock->First, sizeof(sock->First));
	if (result == SOCKET_ERROR)
	{
		Clog::PrintLog(LogTag::Error, "Failed to bind socket named %s Error: %d", name, WSAGetLastError());
		return;
	}
}


void SocketWrap::BindListeningFunctionWin(const char * name, std::function<void()> fun)
{
	std::shared_ptr<MySocket> sock = SocketWrap::GetSocketByName(name);
	if (sock == nullptr)
	{
		Clog::PrintLog(LogTag::Error, "Socket with name of %s not found Error: %d", name, WSAGetLastError());
		return;
	}
	sock->fun = fun;
}

void SocketWrap::ListenSocketWin(std::shared_ptr<MySocket> sock, int numberOfClients)
{
	fd_set readfds;
	SOCKET maxSd=INVALID_SOCKET, sd = INVALID_SOCKET, activity,newSock= INVALID_SOCKET;
	int addrLen = sizeof(sock->First);
	char Buffer[1024];
	timeval tv;
	tv.tv_sec = 1;
	for (int i = 0; i < (sizeof(sock->ChildSockets) / sizeof(sock->ChildSockets[0])); i++)
	{
		sock->ChildSockets[i] = 0;
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
			sd = sock->ChildSockets[i];
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
			if (send(newSock, "Hallo\n", strlen("Hallo\n"), 0)!=strlen("Hallo\n"))
			{
				Clog::PrintLog(LogTag::Error, "Failed to send a message to socket %s Error: %d", sock->name,WSAGetLastError());
			}

			for (int i = 0; i < (sizeof(sock->ChildSockets) / sizeof(sock->ChildSockets[0])); i++)
			{
				if (sock->ChildSockets[i] == 0)
				{
					sock->ChildSockets[i] = newSock;
					break;
				}
			}

		}
		else
		{
			for (int i = 0; i < (sizeof(sock->ChildSockets) / sizeof(sock->ChildSockets[0])); i++)
			{
				sd = sock->ChildSockets[i];
				if (FD_ISSET(sd, &readfds))
				{
					int valread;
					if ((valread = recv(sd, Buffer, sizeof(Buffer) / sizeof(Buffer[0]), 0))==0)
					{
						//Ending connection prompt
						int AdressSize = sizeof(sock->Second);
						getpeername(sd, (sockaddr*)&sock->Second, &AdressSize);
						Clog::PrintLog(LogTag::Log, "Host disconected. IP: %s\n", inet_ntoa(sock->Second.sin_addr));
						closesocket(sd);
						sock->ChildSockets[i] = 0;
					}
					else if(valread>0)
					{
						//Process recieved data
						Clog::PrintLog(LogTag::Log, "[Server]: %.*s", valread, Buffer);
						int sentLength = send(sd, Buffer, valread, 0);
						if (sentLength != valread)
						{
							Clog::PrintLog(LogTag::Error, "Failed to send an echo message");
						}
					}
				}
			}
		}
		/*sock->ClientSocket = accept(sock->soc, NULL, NULL);
		if (sock->ClientSocket == INVALID_SOCKET)
		{
			Clog::PrintLog(LogTag::Error, "Failed to accept a socket. Error: %d", WSAGetLastError());
			return;
		}
		sock->fun();*/
	}
	printf("Ending thread");
}

void SocketWrap::CreateListeningThread(const char * ThreadName, const char * SocketName)
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
	ThrSock->thr = std::thread(&SocketWrap::ListenSocketWin,this,ThrSock->Socket,16);
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

