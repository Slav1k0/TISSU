#include "tiscl.hpp"
using namespace TISCL;

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
    const char* fileName = "config1.tiscl";

    Vector2 position = gameState.playerPos;
    float levelProgress = gameState.levelProgress;
    double money = gameState.money;
    int achievements = gameState.achievements;
    std::string inGameTime = gameState.inGameTime;

    std::vector<std::string> inventory { "assault rifle", "pistol", "grenade", "knife", "smoke grenade", "bandage" };

    std::array<std::string, 3> choosenAbilities;
    choosenAbilities = { "Freeze Ground", "Frozen Stance", "Frozen Spear" };

    TisclSerializer tiscl(fileName);
    tiscl.RegisterType<Vector2>("vec2", [](std::ostream &o, const Vector2 &v) { // registering user created struct
        o << v.x << ", " << v.y;
    }).RegisterStdArrayTypeAndCount<std::string, 3>("stdarr_string"); // std::arrays must be registered manually compared to std::vectors 

    tiscl.BeginStruct("gamestate");
        IndentMember(tiscl, position);
        IndentMember(tiscl, levelProgress);
        IndentMember(tiscl, money);
        IndentMember(tiscl, achievements);
        IndentMember(tiscl, inGameTime);
        IndentMember(tiscl, inventory);
        IndentArrMember(tiscl, choosenAbilities, choosenAbilities.size());
    tiscl.EndStruct();

    return 0;
}