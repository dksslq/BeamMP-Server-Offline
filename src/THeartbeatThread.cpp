// BeamMP, the BeamNG.drive multiplayer mod.
// Copyright (C) 2024 BeamMP Ltd., BeamMP team and contributors.
//
// BeamMP Ltd. can be contacted by electronic mail via contact@beammp.com.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "THeartbeatThread.h"

#include "ChronoWrapper.h"
#include "Client.h"
#include "Common.h"
#include "Http.h"
// #include "SocketIO.h"
#include <nlohmann/json.hpp>
#include <sstream>

void THeartbeatThread::operator()() {
    RegisterThread("Heartbeat");
    // === OFFLINE MODE (BeamMP-Offline) ===
    // Heartbeats are purely a backend-communication mechanism used to register
    // this server on the public server list. In offline mode we never talk to
    // any backend. We keep the thread alive only to refresh THeartbeatThread::lastCall,
    // which is used to answer 'I' (information packet) requests over LAN.
    beammp_info("Offline mode: heartbeat thread running in local-only mode (no backend communication)");
    Application::SetSubsystemStatus("Heartbeat", Application::Status::Good);
    while (!Application::IsShuttingDown()) {
        // refresh the local information packet used by 'I' queries
        (void)GenerateCall();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

std::string THeartbeatThread::GenerateCall() {
    nlohmann::json Ret = {
        { "players", std::to_string(mServer.ClientCount()) },
        { "maxplayers", std::to_string(Application::Settings.getAsInt(Settings::Key::General_MaxPlayers)) },
        { "port", std::to_string(Application::Settings.getAsInt(Settings::Key::General_Port)) },
        { "map", Application::Settings.getAsString(Settings::Key::General_Map) },
        { "private", Application::Settings.getAsBool(Settings::Key::General_Private) ? "true" : "false" },
        { "version", Application::ServerVersionString() },
        { "clientversion", Application::ClientMinimumVersion().AsString() },
        { "name", Application::Settings.getAsString(Settings::Key::General_Name) },
        { "tags", Application::Settings.getAsString(Settings::Key::General_Tags) },
        { "guests", Application::Settings.getAsBool(Settings::Key::General_AllowGuests) ? "true" : "false" },
        { "modlist", mResourceManager.TrimmedList() },
        { "modstotalsize", std::to_string(mResourceManager.MaxModSize()) },
        { "modstotal", std::to_string(mResourceManager.ModsLoaded()) },
        { "playerslist", GetPlayers() },
        { "desc", Application::Settings.getAsString(Settings::Key::General_Description) }
    };

    lastCall = Ret.dump();

    // Add sensitive information here because value of lastCall is used for the information packet.
    Ret["uuid"] = Application::Settings.getAsString(Settings::Key::General_AuthKey);

    return Ret.dump();
}
THeartbeatThread::THeartbeatThread(TResourceManager& ResourceManager, TServer& Server)
    : mResourceManager(ResourceManager)
    , mServer(Server) {
    Application::SetSubsystemStatus("Heartbeat", Application::Status::Starting);
    Application::RegisterShutdownHandler([&] {
        Application::SetSubsystemStatus("Heartbeat", Application::Status::ShuttingDown);
        if (mThread.joinable()) {
            mThread.join();
        }
        Application::SetSubsystemStatus("Heartbeat", Application::Status::Shutdown);
    });
    Start();
}
std::string THeartbeatThread::GetPlayers() {
    std::string Return;
    mServer.ForEachClient([&](const std::weak_ptr<TClient>& ClientPtr) -> bool {
        ReadLock Lock(mServer.GetClientMutex());
        if (auto Client = ClientPtr.lock()) {
            Return += Client->GetName() + ";";
        }
        return true;
    });
    return Return;
}
/*THeartbeatThread::~THeartbeatThread() {
}*/
