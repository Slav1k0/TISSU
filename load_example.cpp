#include "tissu.hpp"
#include <iostream>
using namespace std;
using namespace TISSU_Deserialization;

int main () {
    struct Vector2 { float x, y; };

    struct GameState {
        Vector2 playerPos{};
        float levelProgress{};
        double money{};
        int achievements{};
        std::string inGameTime{};
        std::vector<std::string> inventory{};
        std::array<std::string, 3> choosenAbilities{};
    };

    GameState gs;
    const char *fileName = "config1.tissu";

    TissuDeserializer td(fileName);

    Vector2 position;
    float levelProgress;
    double money;
    int achievements;
    std::string inGameTime;
    std::vector<std::string> inventory; 
    std::array<std::string, 3> choosenAbilities;

    td.GetRegistry()->RegisterType<Vector2>("vec2", 
    [](std::ostream &o, const Vector2 &v) { o << v.x << ", " << v.y; }, 
    [](std::istream &i) { 
        Vector2 v; 
        i >> v.x; 
        i >> std::ws; 
        if(i.peek() == ',') { i.get(); };
        i >> v.y; 
        return v;
    }).RegisterStdArrayTypeAndCount<std::string, 3>("stdarr_string");

    td.BeginStruct("gamestate");
    ReadMember(td, position);
    ReadMember(td, levelProgress);
    ReadMember(td, money);
    ReadMember(td, achievements);
    ReadMember(td, inGameTime);
    ReadMember(td, inventory);
    ReadMember(td, choosenAbilities);
    td.EndStruct();

    gs.playerPos.x = position.x;
    gs.playerPos.y = position.y;
    gs.levelProgress = levelProgress;
    gs.money = money;
    gs.achievements = achievements;
    gs.inGameTime = inGameTime;
    gs.inventory = inventory;
    gs.choosenAbilities = choosenAbilities;
    
    cout << gs.playerPos.x << "," << gs.playerPos.y << "\n";
    cout << gs.levelProgress << "\n";
    cout << gs.money << "\n";
    cout << gs.achievements << "\n";
    cout << gs.inGameTime << "\n";
    
    for(auto &item : inventory) { cout << item << ","; }
    cout << "\n";

    for(auto &ability : choosenAbilities) { cout << ability << ","; }
    cout << "\n";
    return 0;
}