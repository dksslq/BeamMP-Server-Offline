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
| `src/TNetwork.cpp` `TNetwork::Authentication()` | 无 backend 验证；身份包 = 玩家昵称（sanitize ≤32B）；空名 → Guest；onPlayerAuth 事件保留 |
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

## 构建

`.github/workflows/{linux,windows,release}.yml`（上游自带）负责构建。
打 tag（如 `v1.0.0-offline`）触发 release 上传产物。
