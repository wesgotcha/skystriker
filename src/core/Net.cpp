#include "Net.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
// 非 Windows 平台编译占位：工程目前只在 Windows 上发布
typedef int SOCKET;
#endif

namespace sky::net {

namespace {

constexpr uint16_t kGamePort = 47800;
constexpr uint16_t kBeaconPort = 47801;
constexpr uint16_t kProtoVersion = 1;

enum MsgType {
    MT_HELLO = 1,
    MT_START = 2,
    MT_INPUT = 3,
    MT_PAUSE = 4,
    MT_CHECKSUM = 5,
    MT_BYE = 6,
    MT_PING = 7
};

Role  g_role = Role::None;
State g_state = State::Idle;
int   g_mode = 0;

int  g_myCraft = 0, g_mySkin = 0;
int  g_peerCraft = 0, g_peerSkin = 0;
bool g_peerReady = false;
std::string g_peerIp;
std::string g_localIp;

StartInfo g_start{};
bool      g_startRcvd = false;

std::map<uint32_t, uint16_t> g_remoteIn;
std::map<uint32_t, uint32_t> g_localSums;
bool g_pauseEdge = false, g_pauseVal = false;
bool g_desync = false;
bool g_disc = false;
bool g_failed = false;
char g_err[128] = {0};

std::vector<HostEntry> g_hosts;

bool g_inited = false;

#if defined(_WIN32)
SOCKET g_listen = INVALID_SOCKET;
SOCKET g_sock = INVALID_SOCKET;
SOCKET g_udp = INVALID_SOCKET;
sockaddr_in g_peerAddr{};
std::vector<uint8_t> g_inbuf, g_outbuf;

uint64_t g_lastRecv = 0;
uint64_t g_lastSend = 0;
uint64_t g_connectAt = 0;

uint64_t nowMs() { return (uint64_t)GetTickCount64(); }

void closeSock(SOCKET& s) {
    if (s != INVALID_SOCKET) {
        closesocket(s);
        s = INVALID_SOCKET;
    }
}

void setNonBlock(SOCKET s) {
    u_long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);
}

void setNoDelay(SOCKET s) {
    BOOL yes = TRUE;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&yes, sizeof yes);
}

void setBroadcast(SOCKET s) {
    BOOL yes = TRUE;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char*)&yes, sizeof yes);
}

void setError(const char* s) {
    snprintf(g_err, sizeof g_err, "%s", s);
    g_failed = true;
    g_state = State::Failed;
}

void resetSockets() {
    closeSock(g_listen);
    closeSock(g_sock);
    closeSock(g_udp);
    g_inbuf.clear();
    g_outbuf.clear();
}

void queueFrame(uint8_t type, const void* data, uint16_t len) {
    if (g_sock == INVALID_SOCKET) return;
    uint8_t h[5] = {'S', 'K', type, (uint8_t)(len & 0xFF), (uint8_t)(len >> 8)};
    g_outbuf.insert(g_outbuf.end(), h, h + 5);
    if (data && len > 0) {
        const uint8_t* p = (const uint8_t*)data;
        g_outbuf.insert(g_outbuf.end(), p, p + len);
    }
}

void flushOut() {
    if (g_sock == INVALID_SOCKET || g_outbuf.empty()) return;
    while (!g_outbuf.empty()) {
        int n = send(g_sock, (const char*)g_outbuf.data(), (int)g_outbuf.size(), 0);
        if (n > 0) {
            g_outbuf.erase(g_outbuf.begin(), g_outbuf.begin() + n);
            continue;
        }
        if (n == SOCKET_ERROR) {
            int e = WSAGetLastError();
            if (e == WSAEWOULDBLOCK) return;
            g_disc = true;
        }
        return;
    }
}

void sendHello() {
    uint8_t p[4] = {(uint8_t)g_myCraft, (uint8_t)g_mySkin, kProtoVersion, 0};
    queueFrame(MT_HELLO, p, sizeof p);
    flushOut();
}

void sendBeacon() {
    if (g_udp == INVALID_SOCKET) return;
    uint8_t b[8] = {'S', 'K', 'B', '1', (uint8_t)g_mode, (uint8_t)(kGamePort & 0xFF),
                    (uint8_t)(kGamePort >> 8), 0};
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(kBeaconPort);
    to.sin_addr.s_addr = INADDR_BROADCAST;
    sendto(g_udp, (const char*)b, sizeof b, 0, (sockaddr*)&to, sizeof to);
}

std::string detectLocalIp() {
    char name[256] = {0};
    if (gethostname(name, sizeof name) != 0) return "127.0.0.1";
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(name, nullptr, &hints, &res) == 0) {
        // 一台机器常有好几个 IPv4：虚拟网卡（198.18/15）、APIPA（169.254）都可能排在
        // 真局域网地址前面。按"像不像家用/办公网段"打分，挑最像的那个显示。
        auto scoreOf = [](const uint8_t* b) -> int {
            if (b[0] == 192 && b[1] == 168) return 100;
            if (b[0] == 10) return 90;
            if (b[0] == 172 && b[1] >= 16 && b[1] <= 31) return 80;
            if (b[0] == 127) return 0;
            if (b[0] == 169 && b[1] == 254) return 1;
            if (b[0] == 198 && (b[1] == 18 || b[1] == 19)) return 2;
            return 50;
        };
        std::string best = "127.0.0.1";
        int bestScore = -1;
        for (addrinfo* p = res; p; p = p->ai_next) {
            sockaddr_in* sa = (sockaddr_in*)p->ai_addr;
            const uint8_t* b = (const uint8_t*)&sa->sin_addr.s_addr;
            char buf[64] = {0};
            inet_ntop(AF_INET, &sa->sin_addr, buf, sizeof buf);
            int sc = scoreOf(b);
            if (sc > bestScore) {
                bestScore = sc;
                best = buf;
            }
        }
        freeaddrinfo(res);
        return best;
    }
    return "127.0.0.1";
}

void handleMsg(uint8_t type, const uint8_t* p, int n) {
    switch (type) {
        case MT_HELLO:
            if (n >= 2) {
                g_peerCraft = p[0] % 6;
                g_peerSkin = p[1] % 5;
                g_peerReady = true;
            }
            break;
        case MT_START:
            // 只认第一份：主机在转场期间可能重复发送，后面的种子不一致会直接把两端
            // 的随机序列打岔（敌机刷新/行为分叉）。
            if (!g_startRcvd && n >= 9) {
                memcpy(&g_start.seed, p, 4);
                g_start.mode = p[4] % 5;
                g_start.craft[0] = p[5] % 6;
                g_start.skin[0] = p[6] % 5;
                g_start.craft[1] = p[7] % 6;
                g_start.skin[1] = p[8] % 5;
                g_startRcvd = true;
            }
            break;
        case MT_INPUT:
            if (n >= 6) {
                uint32_t tick = 0;
                uint16_t bits = 0;
                memcpy(&tick, p, 4);
                memcpy(&bits, p + 4, 2);
                g_remoteIn[tick] = bits;
                if (getenv("SKY_NET_LOG")) {
                    static int dbg = 0;
                    if (dbg++ < 12) {
                        printf("[net] recv input tick=%u bits=%u role=%d\n", tick, bits, (int)g_role);
                        fflush(stdout);
                    }
                }
                if (g_remoteIn.size() > 300) g_remoteIn.erase(g_remoteIn.begin());
            }
            break;
        case MT_PAUSE:
            if (n >= 1) {
                g_pauseVal = p[0] != 0;
                g_pauseEdge = true;
            }
            break;
        case MT_CHECKSUM:
            if (n >= 8) {
                uint32_t tick = 0, sum = 0;
                memcpy(&tick, p, 4);
                memcpy(&sum, p + 4, 4);
                auto it = g_localSums.find(tick);
                if (it != g_localSums.end() && it->second != sum) g_desync = true;
                // 只保留最近两秒的校验值
                if (g_localSums.size() > 240) g_localSums.erase(g_localSums.begin());
            }
            break;
        case MT_BYE:
            g_disc = true;
            break;
        default:
            break;
    }
}

void pumpRecv() {
    if (g_sock == INVALID_SOCKET) return;
    uint8_t buf[2048];
    for (;;) {
        int n = recv(g_sock, (char*)buf, sizeof buf, 0);
        if (n > 0) {
            g_inbuf.insert(g_inbuf.end(), buf, buf + n);
            g_lastRecv = nowMs();
            continue;
        }
        if (n == 0) {
            g_disc = true;
            return;
        }
        int e = WSAGetLastError();
        if (e != WSAEWOULDBLOCK) g_disc = true;
        break;
    }

    while (g_inbuf.size() >= 5) {
        if (g_inbuf[0] != 'S' || g_inbuf[1] != 'K') {
            g_inbuf.erase(g_inbuf.begin());
            continue;
        }
        uint8_t type = g_inbuf[2];
        uint16_t len = (uint16_t)g_inbuf[3] | ((uint16_t)g_inbuf[4] << 8);
        if (g_inbuf.size() < (size_t)5 + len) break;
        handleMsg(type, g_inbuf.data() + 5, len);
        g_inbuf.erase(g_inbuf.begin(), g_inbuf.begin() + 5 + len);
    }
}

#else
// 非 Windows：空实现，保证能编译
uint64_t nowMs() { return 0; }
void queueFrame(uint8_t, const void*, uint16_t) {}
void flushOut() {}
void sendHello() {}
void pumpRecv() {}
#endif

}  // namespace

// ---------------------------------------------------------------- 生命周期
bool init() {
#if defined(_WIN32)
    if (g_inited) return true;
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
    g_inited = true;
    g_localIp = detectLocalIp();
    return true;
#else
    return false;
#endif
}

void shutdown() {
#if defined(_WIN32)
    if (!g_inited) return;
    resetSockets();
    WSACleanup();
    g_inited = false;
#endif
}

void setLoadout(int craft, int skin) {
    g_myCraft = craft;
    g_mySkin = skin;
}

void cancel() {
#if defined(_WIN32)
    resetSockets();
#endif
    g_role = Role::None;
    g_state = State::Idle;
    g_peerReady = false;
    g_startRcvd = false;
    g_pauseEdge = false;
    g_desync = false;
    g_disc = false;
    g_failed = false;
    g_err[0] = 0;
    g_peerIp.clear();
    g_remoteIn.clear();
    g_localSums.clear();
    g_hosts.clear();
}

bool host(int mode) {
    cancel();
#if defined(_WIN32)
    if (!init()) { setError("主机不可用"); return false; }
    g_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listen == INVALID_SOCKET) { setError("主机不可用"); return false; }
    BOOL yes = TRUE;
    setsockopt(g_listen, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof yes);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(kGamePort);
    if (bind(g_listen, (sockaddr*)&addr, sizeof addr) != 0) {
        closeSock(g_listen);
        setError("主机不可用");
        return false;
    }
    listen(g_listen, 1);
    setNonBlock(g_listen);
    if (getenv("SKY_NET_LOG")) { printf("[net] hosting port=%d\n", (int)kGamePort); fflush(stdout); }

    g_udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_udp != INVALID_SOCKET) setBroadcast(g_udp);

    g_role = Role::Host;
    g_state = State::Hosting;
    g_mode = mode;
    g_lastRecv = nowMs();
    return true;
#else
    return false;
#endif
}

bool browse() {
    cancel();
#if defined(_WIN32)
    if (!init()) { setError("主机不可用"); return false; }
    g_udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_udp == INVALID_SOCKET) { setError("主机不可用"); return false; }
    BOOL yes = TRUE;
    setsockopt(g_udp, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof yes);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(kBeaconPort);
    if (bind(g_udp, (sockaddr*)&addr, sizeof addr) != 0) {
        closeSock(g_udp);
        setError("主机不可用");
        return false;
    }
    setNonBlock(g_udp);
    g_role = Role::Client;
    g_state = State::Browsing;
    return true;
#else
    return false;
#endif
}

bool join(const std::string& ip) {
    cancel();
#if defined(_WIN32)
    if (!init()) { setError("主机不可用"); return false; }
    g_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_sock == INVALID_SOCKET) { setError("主机不可用"); return false; }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kGamePort);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        closeSock(g_sock);
        setError("IP 不可用");
        return false;
    }
    setNonBlock(g_sock);
    if (getenv("SKY_NET_LOG")) { printf("[net] join %s\n", ip.c_str()); fflush(stdout); }
    connect(g_sock, (sockaddr*)&addr, sizeof addr);
    int e = WSAGetLastError();
    if (e != WSAEWOULDBLOCK && e != 0) {
        closeSock(g_sock);
        setError("连接失败");
        return false;
    }
    g_role = Role::Client;
    g_state = State::Connecting;
    g_peerIp = ip;
    g_connectAt = nowMs();
    g_lastRecv = nowMs();
    return true;
#else
    return false;
#endif
}

void poll() {
#if defined(_WIN32)
    if (!g_inited) return;
    uint64_t now = nowMs();

    if (g_state == State::Hosting) {
        if (g_udp != INVALID_SOCKET && now - g_lastSend > 500) {
            sendBeacon();
            g_lastSend = now;
        }
        if (g_sock == INVALID_SOCKET) {
            sockaddr_in from{};
            int flen = sizeof from;
            SOCKET s = accept(g_listen, (sockaddr*)&from, &flen);
            if (s != INVALID_SOCKET) {
                setNonBlock(s);
                setNoDelay(s);
                closeSock(g_listen);
                closeSock(g_udp);  // 房间已满，停止广播
                g_sock = s;
                g_peerAddr = from;
                char buf[64] = {0};
                inet_ntop(AF_INET, &from.sin_addr, buf, sizeof buf);
                g_peerIp = buf;
                g_state = State::Connected;
                g_lastRecv = now;
                g_lastSend = now;
                if (getenv("SKY_NET_LOG")) { printf("[net] accepted peer=%s\n", g_peerIp.c_str()); fflush(stdout); }
                sendHello();
            }
        }
    } else if (g_state == State::Browsing) {
        if (g_udp != INVALID_SOCKET) {
            uint8_t b[64];
            for (;;) {
                sockaddr_in from{};
                int flen = sizeof from;
                int n = recvfrom(g_udp, (char*)b, sizeof b, 0, (sockaddr*)&from, &flen);
                if (n == SOCKET_ERROR) break;
                if (n >= 8 && b[0] == 'S' && b[1] == 'K' && b[2] == 'B' && b[3] == '1') {
                    char ipbuf[64] = {0};
                    inet_ntop(AF_INET, &from.sin_addr, ipbuf, sizeof ipbuf);
                    std::string ip = ipbuf;
                    int mode = b[4] % 5;
                    bool found = false;
                    for (auto& h : g_hosts) {
                        if (h.ip == ip) {
                            h.mode = mode;
                            h.seen = (uint32_t)now;
                            found = true;
                            break;
                        }
                    }
                    if (!found) g_hosts.push_back({ip, mode, (uint32_t)now});
                }
            }
            g_hosts.erase(std::remove_if(g_hosts.begin(), g_hosts.end(),
                                         [&](const HostEntry& h) { return now - h.seen > 2600; }),
                          g_hosts.end());
        }
    } else if (g_state == State::Connecting) {
        fd_set wf;
        FD_ZERO(&wf);
        FD_SET(g_sock, &wf);
        timeval tv{0, 0};
        int r = select(0, nullptr, &wf, nullptr, &tv);
        if (r > 0) {
            int err = 0;
            int elen = sizeof err;
            getsockopt(g_sock, SOL_SOCKET, SO_ERROR, (char*)&err, &elen);
            if (err != 0) {
                setError("连接失败");
            } else {
                setNoDelay(g_sock);
                g_state = State::Connected;
                g_lastRecv = now;
                g_lastSend = now;
                if (getenv("SKY_NET_LOG")) { printf("[net] client connected\n"); fflush(stdout); }
                sendHello();
            }
        } else if (now - g_connectAt > 5000) {
            closeSock(g_sock);
            setError("连接超时");
        }
    }

    if (g_state == State::Connected) {
        pumpRecv();
        if (g_disc) return;
        if (now - g_lastRecv > 6000) {
            g_disc = true;
            return;
        }
        if (now - g_lastSend > 450) {
            queueFrame(MT_PING, nullptr, 0);
            flushOut();
            g_lastSend = now;
        }
    }
    flushOut();
#endif
}

State state() { return g_state; }
Role role() { return g_role; }
bool connected() { return g_state == State::Connected; }
bool failed() { return g_state == State::Failed || g_failed; }
const char* errorText() { return g_err[0] ? g_err : "连接失败"; }
std::string localIp() { return g_localIp.empty() ? "127.0.0.1" : g_localIp; }
std::string peerIp() { return g_peerIp; }
const std::vector<HostEntry>& hosts() { return g_hosts; }
bool peerReady() { return g_peerReady; }
int peerCraft() { return g_peerCraft; }
int peerSkin() { return g_peerSkin; }

bool sendStart(uint32_t seed, int mode, int c1, int s1, int c2, int s2) {
    if (!connected()) return false;
    g_start.seed = seed;
    g_start.mode = mode;
    g_start.craft[0] = c1;
    g_start.skin[0] = s1;
    g_start.craft[1] = c2;
    g_start.skin[1] = s2;
    uint8_t p[9];
    memcpy(p, &seed, 4);
    p[4] = (uint8_t)mode;
    p[5] = (uint8_t)c1;
    p[6] = (uint8_t)s1;
    p[7] = (uint8_t)c2;
    p[8] = (uint8_t)s2;
    queueFrame(MT_START, p, sizeof p);
    flushOut();
    return true;
}

int pendingInputs() { return (int)g_remoteIn.size(); }

bool startReceived() { return g_startRcvd; }
const StartInfo& startInfo() { return g_start; }

void beginMatch() {
    // 注意：**不能**清 g_remoteIn。先进入战斗的一端（主机）会立刻把 tick 0 起的
    // 输入发过来，此时另一端还在 0.6s 的转场里；清掉就等于把对方开局那几帧吞了，
    // 两边会各自卡在"等对方第 0 帧"上。
    g_localSums.clear();
    g_pauseEdge = false;
    g_desync = false;
    g_disc = false;
    g_startRcvd = false;
    if (getenv("SKY_NET_LOG")) {
        printf("[net] beginMatch role=%d state=%d pending=%d\n", (int)g_role, (int)g_state,
               (int)g_remoteIn.size());
        fflush(stdout);
    }
}

void sendInput(uint32_t tick, uint16_t bits) {
    if (!connected()) return;
    uint8_t p[6];
    memcpy(p, &tick, 4);
    memcpy(p + 4, &bits, 2);
    queueFrame(MT_INPUT, p, sizeof p);
    flushOut();
}

bool inputFor(uint32_t tick, uint16_t& bits) {
    auto it = g_remoteIn.find(tick);
    if (it == g_remoteIn.end()) {
        if (getenv("SKY_NET_LOG")) {
            static int miss = 0;
            if (miss++ < 8) {
                printf("[net] miss tick=%u pending=%d first=%u\n", tick, (int)g_remoteIn.size(),
                       g_remoteIn.empty() ? 0u : g_remoteIn.begin()->first);
                fflush(stdout);
            }
        }
        return false;
    }
    bits = it->second;
    g_remoteIn.erase(it);
    return true;
}

void sendPause(bool paused) {
    if (!connected()) return;
    uint8_t v = paused ? 1 : 0;
    queueFrame(MT_PAUSE, &v, 1);
    flushOut();
}

bool takePauseEvent(bool& paused) {
    if (!g_pauseEdge) return false;
    g_pauseEdge = false;
    paused = g_pauseVal;
    return true;
}

bool sendChecksum(uint32_t tick, uint32_t sum) {
    if (!connected()) return false;
    g_localSums[tick] = sum;
    uint8_t p[8];
    memcpy(p, &tick, 4);
    memcpy(p + 4, &sum, 4);
    queueFrame(MT_CHECKSUM, p, sizeof p);
    flushOut();
    return true;
}

bool takeDesync() {
    bool v = g_desync;
    g_desync = false;
    return v;
}

bool disconnected() { return g_disc; }
void disconnect() {
#if defined(_WIN32)
    if (g_state == State::Connected) {
        queueFrame(MT_BYE, nullptr, 0);
        flushOut();
    }
#endif
    cancel();
}

int localSlot() { return g_role == Role::Host ? 0 : 1; }

}  // namespace sky::net
