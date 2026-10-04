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
