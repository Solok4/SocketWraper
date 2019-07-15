// NetworkingTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include "SocketWrap.h"
#include <fstream>

//#include <WinSock2.h>
//#include <WS2tcpip.h>
//#pragma comment(lib,"Ws2_32.lib")

void ClientUDP();
void ServerUDP();
void ClientTCP(int time);
void ClientTCP2(int time);
void ServerTCP();

int main()
{
	//ServerTCP();
	//ClientUDP();
	std::thread ServerThread(ServerTCP);
	std::thread ClientThread(ClientTCP2,2000);
	//std::thread ClientThread2(ClientTCP2,2000);

	ServerThread.join();
	ClientThread.join();
	//ClientThread2.join();


	return 0;
}

void ClientUDP()
{
	SocketWrap Wrap;
	/*std::fstream file;
	file.open("Log.txt", std::ios::out);
	if (!file.good()==true)
	{
		printf("Saving to log failed");
	}*/

	Wrap.CreateSocket("Client", IPPROTO_UDP);
	Wrap.ConnectionData("Client", "239.255.255.250", 1900);
	std::function<void(int, char*, int)> ListenFunc = [&](int a, char* b, int c)
	{
		printf("[Client]: %.*s\n", c, b);
	};
	Wrap.BindSocketFunction("Client",SocketFunctionTypes::Response, ListenFunc);
	const char* request = "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\nST: ssdp:all\r\n\r\n";
	Wrap.SendUDP("Client",request);
	Wrap.CreateListeningClient("Client", "Listen", IPPROTO_UDP);
	Wrap.CloseSocket("Client");
	Wrap.Shutdown();
}

void ServerUDP()
{
	SocketWrap Wrap;

	Wrap.CreateSocket("Server", IPPROTO_UDP);
	Wrap.ConnectionData("Server", "127.0.0.1", 27000);
	Wrap.BindSocket("Server");
	Wrap.CloseSocket("Server");
	Wrap.Shutdown();
}

void ClientTCP(int time)
{
	SocketWrap wrap;

	wrap.CreateSocket("Client", IPPROTO_TCP);
	//wrap.ConnectionDataWin("Client", "127.0.0.1", 27000);
	wrap.ConnectionData("Client", "216.58.209.14", 80);
	wrap.ConnectWin("Client");
	std::function<void(int, char*, int)> ListenFunc = [&](int a, char* b, int c)
	{
		printf("[Client]:Sock num %d %.*s\n",a, c, b);
	};
	wrap.BindSocketFunction("Client",SocketFunctionTypes::Response, ListenFunc);
	wrap.CreateListeningClient("Client", "Listen", IPPROTO_TCP);

	wrap.SendTCP("Client", "GET / HTTP/1.1\r\nhost:www.google.com\r\n\r\n");
	Sleep(time);
	wrap.StopThread("Listen");
	wrap.CloseConnection("Client");
	wrap.CloseSocket("Client");
	wrap.Shutdown();
}

void ClientTCP2(int time)
{
	SocketWrap wrap;

	wrap.CreateSocket("Client", IPPROTO_TCP);
	wrap.ConnectionData("Client", "127.0.0.1", 27000);
	//wrap.ConnectionData("Client", "216.58.209.14", 80);
	wrap.ConnectWin("Client");
	std::function<void(int, char*, int)> ListenFunc = [&](int a, char* b, int c)
	{
		printf("[Client]:Sock num %d %.*s\n", a, c, b);
	};
	wrap.BindSocketFunction("Client", SocketFunctionTypes::Response, ListenFunc);
	wrap.CreateListeningClient("Client", "Listen", IPPROTO_TCP);

	//wrap.SendTCPWin("Client", "Hello\n");
	wrap.SendTCP("Client", "GET / HTTP/1.1\r\nhost:www.google.com\r\n\r\n");
	Sleep(time);
	wrap.StopThread("Listen");
	wrap.CloseConnection("Client");
	wrap.CloseSocket("Client");
	wrap.Shutdown();
}

void ServerTCP()
{
	SocketWrap wrap;

	wrap.CreateSocket("Server", IPPROTO_TCP);
	wrap.ConnectionData("Server", "127.0.0.1", 27000);
	wrap.BindSocket("Server");
	std::function<void(int,char*, int)> ListenFunc = [&](int a,char* b, int c) {
			printf("[Server]: %.*s\n",c,b);
		};
	wrap.BindSocketFunction("Server",SocketFunctionTypes::Response, ListenFunc);
	std::function<void(int, char*, int)> WelcomeFunc = [&](int a, char* b, int c) {
		wrap.SendTCP(a,"MOTD");
	};
	wrap.BindSocketFunction("Server", SocketFunctionTypes::Welcome, WelcomeFunc);
	wrap.CreateListeningServerThread("Listen", "Server");
	Sleep(10000);
	wrap.StopThread("Listen");
	wrap.CloseSocket("Server");
	wrap.Shutdown();
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
