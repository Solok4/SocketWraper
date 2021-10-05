// NetworkingTest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "SocketWrap.h"
#include <fstream>
#include <thread>

#ifndef _WIN32
	#include <unistd.h>
#endif

//#include <WinSock2.h>
//#include <WS2tcpip.h>
//#pragma comment(lib,"Ws2_32.lib")


void ServerTCP(int sleeptime)
{
	SocketWrap wrap;

	auto soc = wrap.CreateSocket("Server", IPPROTO_TCP);
	//soc->SetupClient("www.google.pl", "80");
	soc->SetupServer(27000);

	std::function<void(std::shared_ptr<SocketInstance>, char*, int)> ListenFunc = [&](std::shared_ptr<SocketInstance> a, char* b, int c)
	{
		CLog::info("%s > recieved :%.*s",a->GetName(), c, b);
	};
	soc->BindSocketFunction(ListenFunc, SocketFunctionTypes::Response);

	std::function<void(std::shared_ptr<SocketInstance>, char*, int)> WelcomeFunc = [&](std::shared_ptr<SocketInstance> a, char* b, int c)
	{
		const char* HelloMessage = "Hello";
		a->SendTCPClient((char*)HelloMessage, strlen(HelloMessage));
	};
	soc->BindSocketFunction(WelcomeFunc, SocketFunctionTypes::Welcome);
	soc->CreateListeningThread(std::thread::hardware_concurrency() >=2 ? std::thread::hardware_concurrency() - 1 : 1);
	std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(sleeptime));
	soc->StopListeningThread();
	soc->CloseConnection();
	wrap.CloseSocket("Server");
}

void ClientTCP(int sleeptime)
{
	SocketWrap wrap;
	auto soc = wrap.CreateSocket("Client", IPPROTO_TCP);
	soc->SetupClient("127.0.0.1", 27000);
	auto recvFunc = [&](std::shared_ptr<SocketInstance> a, char* b, int c)
	{
		CLog::info("%s recieved: %.*s", a->GetName(), c, b);
	};
	soc->BindSocketFunction(recvFunc, SocketFunctionTypes::Response);

	std::this_thread::sleep_for(std::chrono::duration<int,std::milli>(sleeptime/2));
	soc->ConnectToServer();
	soc->CreateRecieveThread();

	std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(sleeptime));
	soc->StopListeningThread();
	soc->CloseConnection();
	wrap.CloseSocket("Client");

}

int main()
{
	std::thread server(ServerTCP,5000);
	std::thread client1(ClientTCP, 5000);
	//std::thread client2(ClientTCP, 2300);
	//std::thread client3(ClientTCP, 2600);
	//std::thread client4(ClientTCP, 2900);


	server.join();
	client1.join();
	//client2.join();
	//client3.join();
	//client4.join();




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
