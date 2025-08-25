#include "test_comprehensive.h"
#include "../include/market_data_client.h"
#include "../include/runtime_config.h"
#include "../include/message_buffer.h"
#include "../include/metrics.h"
#include <thread>
#include <vector>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

class BackpressureTests : public TestBase {
public:
    BackpressureTests() : TestBase("Backpressure Hysteresis") {}

    struct ServerConn { int fd; };

    class MiniServer {
        int listenFd{-1};
        std::thread th; std::atomic<bool> running{false};
    public:
        uint16_t port; std::vector<int> clientFds; std::atomic<int> accepted{0};
        explicit MiniServer(uint16_t p): port(p) {}
        bool start() {
            listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
            if (listenFd<0) return false;
            int opt=1; setsockopt(listenFd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
            sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_addr.s_addr=INADDR_ANY; addr.sin_port=htons(port);
            if (bind(listenFd,(sockaddr*)&addr,sizeof(addr))<0) return false;
            if (listen(listenFd,8)<0) return false;
            running=true; th=std::thread([this]{ loop(); });
            return true;
        }
        void loop() {
            while(running) {
                sockaddr_in ca{}; socklen_t cl=sizeof(ca);
                int c = accept(listenFd,(sockaddr*)&ca,&cl);
                if (c>=0) { accepted++; clientFds.push_back(c); }
            }
        }
        void sendBurst(size_t bytes) {
            if (clientFds.empty()) return;
            std::vector<char> buf(bytes,'Q');
            for (int fd: clientFds) {
                ssize_t off=0; while (off < (ssize_t)bytes) {
                    ssize_t s = ::send(fd, buf.data()+off, bytes-off, 0);
                    if (s<=0) break; off+=s;
                }
            }
        }
        void stop(){ running=false; if(listenFd>=0){shutdown(listenFd,SHUT_RDWR);close(listenFd);} if(th.joinable()) th.join(); for(int fd:clientFds) close(fd); }
        ~MiniServer(){ stop(); }
    };

    bool waitConnected(MarketDataClient& c,int ms=1000){ auto start=std::chrono::steady_clock::now(); while(!c.isConnected()){ if(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count()>ms) return false; std::this_thread::sleep_for(std::chrono::milliseconds(10)); } return true; }

    void runAll() override {
        runTest("Multi-connection hard pause & resume", [this](std::string& details){
            setenv("VWAP_RECV_SOFT_WM_PCT","10",1);
            setenv("VWAP_RECV_HARD_WM_PCT","15",1); // very low
            setenv("VWAP_HARD_RESUME_DELTA","5",1); // resume below 10%
            setenv("VWAP_HARD_ACTION","PAUSE",1);
            runtimeConfig().loadFromEnv();

            MiniServer srv(19100);
            if(!srv.start()) { details="Server start failed"; return false; }

            MarketDataClient c1("127.0.0.1",19100); MarketDataClient c2("127.0.0.1",19100);
            if(!c1.connect()||!c2.connect()) { details="Connect failed"; return false; }
            if(!waitConnected(c1)||!waitConnected(c2)) { details="Connect timeout"; return false; }

            std::vector<uint8_t> filler(2000, 0xFF);
            for (int burst=0; burst<20; ++burst) {
                srv.sendBurst(filler.size());
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                c1.processIncomingData();
                c2.processIncomingData();
            }

            auto snap = MetricsSnapshot::capture(g_systemMetrics);
            if (snap.hardWatermarkEvents == 0) { details="No hard watermark events recorded"; return false; }

            if(!c1.isConnected() || !c2.isConnected()) { details="Connection unexpectedly closed under PAUSE"; return false; }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            srv.sendBurst(10); // tiny
            c1.processIncomingData();
            c2.processIncomingData();

            if(!c1.isConnected() || !c2.isConnected()) { details="Connection lost after resume attempt"; return false; }
            return true;
        });
    }
};

extern "C" void runBackpressureTests(){ BackpressureTests t; t.runAll(); t.printSummary(); }
