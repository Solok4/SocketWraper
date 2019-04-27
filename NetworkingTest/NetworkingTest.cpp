// NetworkingTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include "SocketWrap.h"
#include <fstream>

//#include <WinSock2.h>
//#include <WS2tcpip.h>
//#pragma comment(lib,"Ws2_32.lib")

void Client();
void Server();
void ClientTCP(int time);
void ServerTCP();

int main()
{
	//ServerTCP();
	//Client();
	std::thread ServerThread(ServerTCP);
	std::thread ClientThread(ClientTCP,1000);
	std::thread ClientThread2(ClientTCP,2000);

	ServerThread.join();
	ClientThread.join();
	ClientThread2.join();

	getchar();

	/*WSAData WsaData;

	int result = WSAStartup(MAKEWORD(2, 2),&WsaData);
	if (result != NO_ERROR)
	{
		std::cout << "Failed to start up\n";
		WSACleanup();
		return -1;
	}

	SOCKET MainSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (MainSocket == INVALID_SOCKET)
	{
		std::cout << "Failed to create a socket\n";
		WSACleanup();
		return -2;
	}
   

	addrinfo *service, hints;
	memset(&service, 0, sizeof(service));
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	result = getaddrinfo("127.0.0.1", "16000", &hints, &service);
	if (result != 0)
	{
		std::cout << "Failed to resolve ip\n";
		WSACleanup();
		return -3;

	}


	result = connect(MainSocket, service->ai_addr, (int)service->ai_addrlen);
	if (result == SOCKET_ERROR)
	{
		closesocket(MainSocket);
		std::cout << "Failed to connect\n";
		WSACleanup();
		return -4;
	}


	result = shutdown(MainSocket, SD_SEND);
	if (result == SOCKET_ERROR)
	{
		std::cout << "Failed to disconnect from server \n";
		closesocket(MainSocket);
		WSACleanup();
		return -5;
	}
	
	closesocket(MainSocket);
	WSACleanup();*/

	return 0;
}

void Client()
{
	char RecvBuff[1024];
	SocketWrap Wrap;
	/*std::fstream file;
	file.open("Log.txt", std::ios::out);
	if (!file.good()==true)
	{
		printf("Saving to log failed");
	}*/

	Wrap.InitalizeWin();
	Wrap.CreateSocketWin("Client", IPPROTO_UDP);
	Wrap.ConnectionDataWin("Client", "127.0.0.1", 27000);
	//Wrap.BindSocketWin("Client");
	//Wrap.ConnectWin("Client");
	//Wrap.SendWin("Client", (char*)"Hello\n");
	const char* request = "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\nST: ssdp:all\r\n\r\n";
	Wrap.SendUDPWin("Client",request);
	for (;;)
	{
		int messageLength = Wrap.RecvUDPWin("Client", RecvBuff);
		if (messageLength == 0) break;
		//file.write(RecvBuff, messageLength);
		printf("%.*s", messageLength, RecvBuff);
	}
	//file.close();
	//Wrap.CloseConnectionWin("Client");
	Wrap.CloseSocketWin("Client");
	Wrap.ShutdownWin();
}
void Server()
{
	char RecvBuff[1024];
	int msgLength;
	SocketWrap Wrap;

	Wrap.InitalizeWin();
	Wrap.CreateSocketWin("Server", IPPROTO_UDP);
	Wrap.ConnectionDataWin("Server", "127.0.0.1", 27000);
	Wrap.BindSocketWin("Server");
	//std::function<void()> ListenFunc = [&]() {
	//	char recvBuff[256];
	//	//auto Func = std::bind(&SocketWrap::RecvTCPWin, &Wrap, "Server", recvBuff);
	//	auto Func = std::bind(&SocketWrap::RecvUDPWin, &Wrap, "Server", recvBuff);
	//	int result = Func();
	//	printf_s("%d %s\n",result,recvBuff);
	//};
	//Wrap.BindListeningFunctionWin("Server", ListenFunc);
	//Wrap.CreateListeningThread("Listen", "Server");
	//Sleep(10000);
	//Wrap.StopThread("Listen");
	msgLength=Wrap.RecvUDPWin("Server", RecvBuff);
	printf("%.*s",msgLength, RecvBuff);
	Wrap.CloseSocketWin("Server");
	Wrap.ShutdownWin();
}
void ClientTCP(int time)
{
	char recvBuff[1024];
	SocketWrap wrap;

	wrap.InitalizeWin();
	wrap.CreateSocketWin("Client", IPPROTO_TCP);
	wrap.ConnectionDataWin("Client", "127.0.0.1", 27000);
	wrap.ConnectWin("Client");
	Sleep(time);
	wrap.SendTCPWin("Client", "Hello\n");
	int length = wrap.RecvTCPWin("Client", recvBuff);
	printf("[Client]: %.*s\n",length, recvBuff);
	wrap.CloseConnectionWin("Client");
	wrap.CloseSocketWin("Client");
	wrap.ShutdownWin();
}

void ServerTCP()
{
	SocketWrap wrap;

	wrap.InitalizeWin();
	wrap.CreateSocketWin("Server", IPPROTO_TCP);
	wrap.ConnectionDataWin("Server", "127.0.0.1", 27000);
	wrap.BindSocketWin("Server");
	//std::function<void()> ListenFunc = [&]() {
	//		char recvBuff[256];
	//		auto Func = std::bind(&SocketWrap::RecvTCPWin, &wrap, "Server", recvBuff);
	//		int result = Func();
	//		printf("%.*s\n",result,recvBuff);
	//	};
	//wrap.BindListeningFunctionWin("Server", ListenFunc);
	wrap.CreateListeningThread("Listen", "Server");
	Sleep(10000);
	wrap.StopThread("Listen");
	wrap.CloseSocketWin("Server");
	wrap.ShutdownWin();
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
