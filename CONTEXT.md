# BeamMP-Server-Offline — Context / 项目记忆

> 项目完整背景请读主仓库的 [CONTEXT.md](https://github.com/dksslq/BeamMP-Offline/blob/main/CONTEXT.md)，
> 本文件是本仓库的简要记忆 + 合并手册。

## 本仓库是什么

BeamMP 官方服务端（BeamMP/BeamMP-Server）的**纯离线版**分支：
- 无需 keymaster AuthKey、不验证玩家 key、不向任何 backend 发心跳。
- 玩家用离线 Launcher（BeamMP-Launcher-Offline）连接，握手时把**玩家昵称**
  当作原来的"key"包发过来，服务器直接采用为玩家名（空 → Guest）。
- 其余所有功能（资源同步、Lua 插件、车辆同步等）与上游一致。

## 本仓库改动清单（合并上游时必须保护）

| 文件 | 离线改动 |
|---|---|
| `src/TNetwork.cpp` `TNetwork::Authentication()` | 无 backend 验证；身份包 = 玩家昵称（sanitize ≤32B）；空名 → Guest；≥40 位纯 hex（官方 Launcher 的公钥）→ 命名 `Player-<hex前6位>`（上游客户端兼容）；onPlayerAuth 事件保留 |
| `src/TNetwork.cpp` `TCPRcv()` + 空名字包 | **v1.0.2 起**：`TCPRcv` 新增第三参数 `bool* OutPacketValid`（成功收完一个包帧置 true，含"合法的 0 长度载荷"；连接关闭/超时/帧错误为 false）。`Authentication` 据此区分"客户端发来合法空名字包（→ Guest）"与"认证阶段连接断开（→ Connection closed during authentication）"。修复背景：旧离线 Launcher 对未命名玩家发送 0 字节名字包，v1.0.1 服务端把它误判为断线并踢出（用户报告的 "Client kicked: Connection closed during authentication"） |
| `src/TNetwork.cpp` 同名玩家处理 | **v1.0.1 起**：上游按"同名同 key=掉线重连"踢旧连接，离线无 key 会误伤同名真人。新语义（按 IP 区分）：同名+同 IP → 踢旧连接（僵尸重连，保留原名）；同名+不同 IP → 不同玩家，新客户端自动改名 `Name (2)/(3)…`（最小空闲 N，避开已有 "Name (2)"）。原始名存于客户端标识 `raw_name`，被改名的玩家重连仍能命中僵尸并取回原名。快照在 `GetClientMutex()` 读锁内收集、锁外决策 |
| `src/THeartbeatThread.cpp` `operator()` | 心跳线程只本地刷新 `lastCall`（每 5s），零网络请求 |
| `src/Common.cpp` `Application::CheckForUpdates()` | 空操作 |
| `src/TConsole.cpp` `Command_NetTest()` | 打印本地状态，不请求 Server Check API |
| `src/TConfig.cpp` | AuthKey 非空强制检查删除；配置头注释改为离线说明 |
| `.github/workflows/{linux,windows}.yml` | 触发分支加入 `main`（上游只触发 develop/minor；本仓库默认分支为 main） |

## 上游合并

```bash
git remote add upstream https://github.com/BeamMP/BeamMP-Server.git  # 一次即可
git fetch upstream
git merge upstream/minor      # 上游默认分支是 minor
```

冲突原则：
1. 上游新功能/修复全部接纳；
2. 上表中的离线语义必须保留；上游重写同区域时，把离线语义重新套到新实现上；
3. 合并后自检：`grep -rn "backend.beammp.com\|GetBackendUrlForAuth" src/` 只应出现在注释里；
   ServerConfig.toml 模板不要求 AuthKey；CI 绿灯。

## 官方客户端兼容（语义要点）

官方 Launcher/mod（联网状态）可直接 Direct Connect 加入本服务器：
握手协议未变；官方客户端发来的"身份包"是账号公钥（≥40 位纯 hex），服务器识别后
自动命名 `Player-<前6位>`。无互联网的官方客户端过不了官方登录流程，只能改用
本项目离线 Launcher（这正是本项目的意义）。

## 构建

`.github/workflows/{linux,windows,release}.yml`（上游自带）负责构建。
打 tag（如 `v1.0.0-offline`）触发 release 上传产物。

## 11. 安全审计记录（2026-10-04，v1.0.2-offline-server = 034d316）

- 全部离线 diff（8 commits/11 files）逐行审计：无后门/遥测/凭据外发。
- 已移除的外发路径：心跳（每 5-30s POST 服务器信息+AuthKey 到 backend）、版本检查、
  NetTest 外呼（check.beammp.com）、认证外呼（auth.beammp.com/pkToUser：玩家公钥+AuthKey+玩家IP）。
  Http:: 在 Http.cpp 之外零调用点，backend URL 常量为死代码；无 socket.io。
- 保留逻辑：AllowGuests 开关仍强制执行（TNetwork.cpp:606）；AllowGuests=false 时的
  踢出文案仍提 forum.beammp.com（纯文案残留，无网络行为，可后续润色）。
- 发布二进制验证：v1.0.2 BeamMP-Server.exe 中 auth/check/backend.beammp.com
  端点字符串完全消失（源码结论的二次确认）；其余 http:// 命中为静态链接库内嵌数据碎片。
- 供应链提示：release.yml 沿用上游 actions（部分为旧版 tag 引用），建议未来升级并按 SHA pin。

## 12. 已知上游行为：退出时 WSAEINTR 日志 + Windows appcrash（诊断于 2026-10-04，按用户指示不修复）

- **现象**：`exit` 命令优雅关闭走完 9 个子系统后，日志末尾出现
  `[ERROR] Failed to accept() new client: 一个封锁操作被对 WSACancelBlockingCall 的调用中断。`
  （= WSAEINTR/10004），且 Windows 每次退出都会生成一条 appcrash（WER 事件）记录。
- **证据链（全部为上游固有）**：
  1. accept 循环代码与 fork 基点逐字节相同（`git show f55931d:src/TNetwork.cpp` vs HEAD diff 为空）；
  2. fork 基点 f55931da739cb... 就是上游默认分支 `minor` 的最新 HEAD（2026-09-23，
     TNetwork.cpp sha1 51ec1fa1... 两边一致）——即上游至今未修；
  3. 上游 TNetwork 构造函数注册的关机 handler 显式 `mTCPThread.detach()` / `mUDPThread.detach()`
     （对应日志 Subsystem 2/9、3/9），并且每个客户端的 Identify 线程也是
     `ID.detach()` + 上游自己的 `TODO: Add to a queue and attempt to join periodically`。
- **机制**：优雅关闭结束后 main 返回 → 栈上 TNetwork/TServer(io_context) 析构 →
  acceptor/io_context 在阻塞的 accept() 下方被拆除 → 阻塞调用以 WSAEINTR 唤醒并打出该 ERROR；
  随后分离线程与对象析构/静态析构竞争 → 部分退出路径以访问违例收场 → WER 记 appcrash。
  main() 的 std::exit() 跳过部分 RAII 也参与其中。
- **影响评估**：无害。appcrash 发生在玩家已踢、配置已写、日志已刷盘之后，属退出尾声的外观性问题；
  不影响数据完整性与下次启动。
- **为何不修**（修 = 架构级改造，符合用户"改很多地方就先不修"的豁免条件）：
  需要线程注册表/收敛 join 语义（替换 3 处 detach）、acceptor 生命周期反转 + 关闭取消、
  main() 去掉 std::exit 并重排静态析构顺序——横跨 TNetwork/TServer/Common/main 四处。
- **对用户的话术**：可安全忽略；若在意 WER 记录，可用 Ctrl+C 直接强杀（等价结果），
  或等待未来 upstream sync 时一并处理。
