#include "tissu.hpp"
#include <array>
using namespace TISSU_Serialization;


// g++ -std=c++20 save_example.cpp -o tissu.exe

int main()
{
    struct Vector2
    {
        float x, y;
    };
    
    struct GameState
    {
        Vector2 playerPos { 100.0f, 100.0f };
        float levelProgress { 12.5f };

        double money {1048.55};
        int achievements { 40 };
        std::string inGameTime {"12:55"};
    };
    
    GameState gameState;
    const char* fileName = "config1.tissu";

    Vector2 position = gameState.playerPos;
    float levelProgress = gameState.levelProgress;

    double money = gameState.money;
    int achievements = gameState.achievements;
    std::string inGameTime = gameState.inGameTime;

    std::vector<std::string> inventory { "assault rifle", "pistol", "grenade", "knife", "smoke grenade", "bandage" };

    std::array<std::string, 3> choosenAbilities;
    choosenAbilities = { "Freeze Ground", "Frozen Stance", "Frozen Spear" };

    std::vector<std::tuple<std::string, int>> settings = {
        {"shadowsLevel", 2},
        {"textureQualityLevel", 3},
        {"lightQualityLevel", 3},
        {"worldDistanceLevel", 3}
    };

    TissuSerializer tissu(fileName);
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
        IndentAndApplyForAll(tissu, settings);
    tissu.EndStruct();


    return 0;
}