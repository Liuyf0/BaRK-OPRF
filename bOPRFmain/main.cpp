#include <iostream>

using namespace std;
#include "Common/Defines.h"
#include "PSI/BopPsiReceiver.h"
#include "PSI/BopPsiSender.h"
#include "Network/BtEndpoint.h" 
#include <math.h>
#include "Common/Log.h"
#include "Common/Timer.h"
#include "Crypto/PRNG.h"
using namespace bOPRF;
#include <fstream>
#include <dirent.h>

void BopSender(string ipAddressPort, int senderSize, int recverSize, int senderes)
{
	u64 numThreads = 1;

	std::string name("psi");

	BtIOService ios(0);

	BtEndpoint ep0(ios, ipAddressPort,  true, name);

	std::vector<Channel*> sendChls(numThreads);
	for (u64 i = 0; i < numThreads; ++i)
		sendChls[i] = &ep0.addChannel(name + std::to_string(i), name + std::to_string(i));
	
	// 读取文件夹内所有worker location
	std::vector<std::vector<block>> sendSets;
    std::string folderPath = "cloudserver"; // 文件夹路径
    DIR* dir;
    struct dirent* entry;
    if ((dir = opendir(folderPath.c_str())) != nullptr) {
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type == DT_REG) {
				std::vector<block> sendSet;
				// we will use this to hash large inputs
				std::hash<std::string> szHash;
				std::string path = folderPath + "/" + entry->d_name;
				std::ifstream file(path, std::ios::in);
				if (file.is_open() == false)
					throw std::runtime_error("failed to open file: " + path);
				std::string buffer;
				while (std::getline(file, buffer))
				{
					size_t hashVal = szHash(buffer);
					sendSet.push_back(_mm_set_epi32(hashVal,hashVal,hashVal,hashVal));
				}
				sendSets.push_back(sendSet);
            }
        }
        closedir(dir);
    } else {
        std::cerr << "Error opening directory." << std::endl;
    }
	u64 psiSecParam = 40;

	u64 offlineTimeTot(0);
	u64 onlineTimeTot(0);

	SSOtExtSender OTSender0;

	BopPsiSender sendPSIs;

	//SimpleHasher mBins;
    SimpleHasher *mBins = new SimpleHasher[senderes];
	u8 dumm[1];
	sendChls[0]->asyncSend(dumm, 1);
	sendChls[0]->recv(dumm, 1);
	sendChls[0]->asyncSend(dumm, 1);

	gTimer.reset();
	for (int i = 0; i < senderes; ++i) {mBins[i].init(recverSize, senderSize);}
	sendPSIs.init(senderSize, recverSize, psiSecParam, *sendChls[0], OTSender0, OneBlock, mBins);

	sendPSIs.sendInput(sendSets, sendChls, mBins);
	u64 otIdx = 0;

	for (u64 i = 0; i < numThreads; ++i)
	{
		sendChls[i]->close();
	}

	ep0.stop();
	ios.stop();
	delete [] mBins;
}

void BopRecv(string ipAddressPort, int senderSize, int recverSize, int senderes)
{
	u64 numThreads = 1;

	std::string name("psi");

	BtIOService ios(0);
	BtEndpoint ep1(ios, ipAddressPort,false, name);


	std::vector<Channel*> recvChls(numThreads);
	for (u64 i = 0; i < numThreads; ++i)
		recvChls[i] = &ep1.addChannel(name + std::to_string(i), name + std::to_string(i));

	std::vector<block> recvSet;
	// we will use this to hash large inputs
	std::hash<std::string> szHash;
	std::string path{"requester.txt"};
	std::ifstream file(path, std::ios::in);
	if (file.is_open() == false)
		throw std::runtime_error("failed to open file: " + path);
	std::string buffer;
	while (std::getline(file, buffer))
	{
		size_t hashVal = szHash(buffer);
		recvSet.push_back(_mm_set_epi32(hashVal,hashVal,hashVal,hashVal));
	}
	u64 psiSecParam = 40;
	u64 offlineTimeTot(0);
	u64 onlineTimeTot(0);

	SSOtExtReceiver OTRecver0;
	BopPsiReceiver recvPSIs;

	u8 dumm[1];
	recvChls[0]->recv(dumm, 1);
	recvChls[0]->asyncSend(dumm, 1);
	recvChls[0]->recv(dumm, 1);

	//gTimer.reset();
	Timer timer;
	timer.setTimePoint("start");
	auto start = timer.setTimePoint("start");
	recvPSIs.init(senderSize, recverSize, psiSecParam, recvChls, OTRecver0, ZeroBlock);
	auto mid = timer.setTimePoint("init");
	recvPSIs.sendInput(recvSet, recvChls, senderes);
	auto end = timer.setTimePoint("done");

	auto offlineTime = std::chrono::duration_cast<std::chrono::milliseconds>(mid - start).count();
	auto online = std::chrono::duration_cast<std::chrono::milliseconds>(end - mid).count();
	offlineTimeTot += offlineTime;
	onlineTimeTot += online;

	std::cout << recverSize << " vs " << senderSize << "-- Online Avg Time: " << onlineTimeTot << " ms " << "\n";
	std::cout << recverSize << " vs " << senderSize << "-- Offline Avg Time: " << offlineTimeTot << " ms " << "\n";
	std::cout << recverSize << " vs " << senderSize << "-- Total Avg Time: " << (offlineTimeTot + onlineTimeTot) << " ms " << "\n";

	std::string outpath = "./output.txt";
    std::ofstream outputFile(outpath, std::ios::app);
	outputFile << "Grid Size: " << senderSize << "\tworker Number: " << senderes;
	outputFile << "\tTotal Time: "<< (offlineTimeTot + onlineTimeTot) << " ms " << std::endl;
	outputFile << "----------------------------------------" << std::endl;
    outputFile.close();

	for (u64 i = 0; i < numThreads; ++i)
	{
		recvChls[i]->close();
	}

	ep1.stop();
	ios.stop();
}

int main(int argc, char** argv)
{
	if (argc == 8 && argv[1][0] == '-' && argv[1][1] == 'r' && atoi(argv[2]) == 0 && argv[3][0] == '-' && argv[3][1] == 'i'&& argv[3][2] == 'p') {
		string ipAddr = argv[4];
		int senderSize= atoi(argv[5]);
		int recverSize= atoi(argv[6]);
		int senderes = atoi(argv[7]);
		BopSender(ipAddr, senderSize, recverSize, senderes);

	}
	else if (argc == 8 && argv[1][0] == '-' && argv[1][1] == 'r' && atoi(argv[2]) == 1 && argv[3][0] == '-' && argv[3][1] == 'i'&& argv[3][2] == 'p') {
		string ipAddr = argv[4];
		int senderSize= atoi(argv[5]);
		int recverSize= atoi(argv[6]);
		int senderes = atoi(argv[7]);
		BopRecv(ipAddr, senderSize, recverSize, senderes);
	}
	else 
		std::cout << "Use:\n\t./Release/bOPRFmain.exe -r 0/1 -ip <ipAdrress:portNumber> <senderSize> <recverSize> <senderes>" << std::endl;
	return 0;
}
