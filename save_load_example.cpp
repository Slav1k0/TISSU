/**
 * TISSU: Typed, Indentation Separated Scripting Utility
 * 
 * @author          Ondřej Slavík
 * @copyright       2026 (C) Ondřej Slavík
 * 
 * Released under the MIT License.
 * See: https://opensource.org/licenses/MIT
 */

#include "tissu.hpp"
#include <iostream>
#include <array>
using namespace std;
using namespace TISSU_Deserialization;
using namespace TISSU_Serialization;

int main() {
    try
    {
        struct Vector2 { float x, y; };
        struct GameState {
            Vector2 playerPos{100.0f, 100.0f };
            float levelProgress{12.5f};
            double money{1048.55};
            int achievements{40};
            std::string inGameTime{"12:55"};
            std::vector<std::string> inventory { "assault rifle", "pistol", "grenade", "knife", "smoke grenade", "bandage" }; // inventory has 6 slots, but the space is upgradable
            std::array<std::string, 3> choosenAbilities { "Freeze Ground", "Frozen Stance", "Frozen Spear" };
        };

        struct Settings {
            int shadowsLevel{ 2 };
            int textureQualityLevel{ 3 };
            int lightQualityLevel{ 3 };
            int worldDistanceLevel{ 3 };
        };
        
        GameState gameState;
        Settings settings;
        const char* fileName = "config1.tissu";

        Vector2 &position = gameState.playerPos;
        float &levelProgress = gameState.levelProgress;
        double &money = gameState.money;
        int &achievements = gameState.achievements;
        std::string &inGameTime = gameState.inGameTime;
        std::vector<std::string> &inventory = gameState.inventory;
        std::array<std::string, 3> &choosenAbilities = gameState.choosenAbilities;

        std::array<std::tuple<std::string, int>, 4> settingsArray = {{
            {"shadowsLevel", settings.shadowsLevel},
            {"textureQualityLevel", settings.textureQualityLevel},
            {"lightQualityLevel", settings.lightQualityLevel},
            {"worldDistanceLevel", settings.worldDistanceLevel}
        }};

        TissuSerializer tissu(fileName);

        // registers both for serializer and deserializer
        tissu.GetRegistry()->RegisterType<Vector2>("vec2", 
        [](std::ostream &o, const Vector2 &v) { o << v.x << ", " << v.y; }, 
        [](std::istream &i) { 
            Vector2 v; 
            i >> v.x; 
            i >> std::ws; 
            if(i.peek() == ',') { i.get(); };
            i >> v.y; 
            return v;
        }).RegisterStdArrayTypeAndCount<std::string, 3>("stdarr_string"); // std::arrays must be registered manually opposed to std::vectors 

        // SAVE
        tissu.BeginStruct("gamestate");
        IndentMember(tissu, position);
        IndentMember(tissu, levelProgress);
        IndentMember(tissu, money);
        IndentMember(tissu, achievements);
        IndentMember(tissu, inGameTime);
        IndentMember(tissu, inventory);
        IndentArrMember(tissu, choosenAbilities, choosenAbilities.size());
        tissu.EndStruct();
        tissu.BeginStruct("settings");
            IndentAndApplyForAllArr(tissu, settingsArray);
        tissu.EndStruct();
        tissu.Flush();

        // NULLIFY
        gameState.playerPos.x = 0;
        gameState.playerPos.y = 0;
        gameState.levelProgress = 0;
        gameState.money = 0;
        gameState.achievements = 0;
        gameState.inGameTime = "";
        gameState.inventory = {""};
        gameState.choosenAbilities = {"", "", ""};

        cout << gameState.playerPos.x << "," << gameState.playerPos.y << "\n";
        cout << gameState.levelProgress << "\n";
        cout << gameState.money << "\n";
        cout << gameState.achievements << "\n";
        cout << gameState.inGameTime << "\n";
        
        for(auto &item : inventory) { cout << item << ","; }
        cout << "\n";

        for(auto &ability : choosenAbilities) { cout << ability << ","; }
        cout << "\n";
        
        //LOAD
        TissuDeserializer td(fileName, tissu.GetRegistry());

        td.BeginStruct("gamestate");
        ReadMember(td, position);
        ReadMember(td, levelProgress);
        ReadMember(td, money);
        ReadMember(td, achievements);
        ReadMember(td, inGameTime);
        ReadMember(td, inventory);
        ReadMember(td, choosenAbilities);
        td.EndStruct();

        gameState.playerPos.x = position.x;
        gameState.playerPos.y = position.y;
        gameState.levelProgress = levelProgress;
        gameState.money = money;
        gameState.achievements = achievements;
        gameState.inGameTime = inGameTime;
        gameState.inventory = inventory;
        gameState.choosenAbilities = choosenAbilities;
        
        cout << gameState.playerPos.x << "," << gameState.playerPos.y << "\n";
        cout << gameState.levelProgress << "\n";
        cout << gameState.money << "\n";
        cout << gameState.achievements << "\n";
        cout << gameState.inGameTime << "\n";
        
        for(auto &item : inventory) { cout << item << ","; }
        cout << "\n";

        for(auto &ability : choosenAbilities) { cout << ability << ","; }
        cout << "\n";
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }

    return 0;
}