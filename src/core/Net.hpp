#pragma once
// 局域网联机：TCP 传输入（锁步），UDP 广播做主机发现。
// 只在这里碰 winsock，游戏代码不直接 include windows.h。
#include <cstdint>
#include <string>
#include <vector>

namespace sky::net {

enum class Role { None, Host, Client };
enum class State { Idle, Hosting, Browsing, Connecting, Connected, Failed };

struct HostEntry {
    std::string ip;
    int         mode = 0;   // MatchMode
    uint32_t    seen = 0;   // 本机毫秒时间戳，用于过期
};

struct StartInfo {
    uint32_t seed = 0;
    int      mode = 0;
    int      craft[2] = {0, 0};
    int      skin[2] = {0, 0};
};

bool init();
void shutdown();

// 本机身份与外观（连接后随 HELLO 交换）
void setLoadout(int craft, int skin);

// 开房 / 找房 / 直连
bool host(int mode);
bool browse();
bool join(const std::string& ip);
void cancel();
void poll();  // 每帧调用：收包、接受连接、发心跳

State state();
Role  role();
bool  connected();
bool  failed();
const char* errorText();
std::string localIp();
std::string peerIp();

const std::vector<HostEntry>& hosts();
bool peerReady();
int  peerCraft();
int  peerSkin();

// 主机开局：把种子和双方机体发给客户端，同时自己记一份
bool sendStart(uint32_t seed, int mode, int c1, int s1, int c2, int s2);
bool startReceived();
const StartInfo& startInfo();

// 锁步
void beginMatch();
int  pendingInputs();  // 调试：已收到但还没被消费的对方输入数
void sendInput(uint32_t tick, uint16_t bits);
bool inputFor(uint32_t tick, uint16_t& bits);

// 暂停同步 / 校验 / 掉线
void sendPause(bool paused);
bool takePauseEvent(bool& paused);
bool sendChecksum(uint32_t tick, uint32_t sum);
bool takeDesync();
bool disconnected();
void disconnect();

int localSlot();  // 主机 0，客户端 1

}  // namespace sky::net
