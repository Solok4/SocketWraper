// NetworkingTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include "SocketWrap.h"
#include <fstream>

//#include <WinSock2.h>
//#include <WS2tcpip.h>
//#pragma comment(lib,"Ws2_32.lib")


void ServerTCP(int sleep)
{
	SocketWrap wrap;

	auto soc = wrap.CreateSocket("Server", IPPROTO_TCP);
	//soc->SetupClient("www.google.pl", "80");
	soc->SetupServer("27000");
	soc->CreateListeningThread(16);
	Sleep(sleep);
	soc->StopListeningThread();
	/*wrap.ConnectionData("Server", "127.0.0.1", 27000);
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
	wrap.StopThread("Listen");*/
	wrap.CloseSocket("Server");
}

void ClientTCP(int sleep)
{
	SocketWrap wrap;
	auto soc = wrap.CreateSocket("Client", IPPROTO_TCP);
	soc->SetupClient("127.0.0.1", "27000");
	soc->ConnectToServer();
	Sleep(sleep);
	soc->CloseConnection();
	wrap.CloseSocket("Client");

}

int main()
{
	std::thread server(ServerTCP,2000);
	std::thread client(ClientTCP,1000);


	server.join();
	client.join();



	return 0;
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
