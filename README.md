# TISSU

**Typed, Indentation Separated Scripting Utility**

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![C++](https://img.shields.io/badge/C%2B%2B-20-00599C.svg)
![Header-only](https://img.shields.io/badge/header--only-yes-brightgreen.svg)

A small, header-only C++20 library for serializing typed data into a human-readable, indentation-based config/scripting format — and a format designed to be equally comfortable to read, hand-edit, and diff.

```tissu
<gamestate:
    (f)levelProgress: 12.5
    (d)money: 1048.55
    (i)achievements: 40
    (stdvec_string)inventory: assault rifle, pistol, grenade, knife
>
```

---

## Why TISSU?

 TISSU keeps the C++ type next to the value as a short tag, so the format is self-describing and round-trips back into the exact types you wrote it from. Structure comes from indentation and angle-bracket blocks rather than braces and quotes, which keeps saved game states and config files pleasant to read by hand.

## Features

- **Header-only** — drop in `tissu.hpp`, no build step, no dependencies beyond the standard library.
- **Type registry** — map any C++ type to a tag with a custom writer and reader. Built-ins are provided out of the box.
- **Containers** — first-class support for `std::vector<T>` and fixed-size `std::array<T, N>` of any registered element type.
- **Nested structs** — arbitrarily deep, indentation-tracked automatically.
- **Convenience macros** — write a member using the variable's own name as the field name.

## The TISSU format

| Syntax | Meaning |
| --- | --- |
| `<name:` … `>` | A **struct** block. Its members are indented one level. |
| `(tag)name: value` | A **member** — `tag` is the registered type identifier. |
| `(tag[N])name: value` | A member holding a fixed-size array of `N` elements. |
| `<&name: value>` | An **information block** (metadata, e.g. a version to match against). |
| `*(tag)` | **Apply-to-all** — every member below inherits `tag` until the block ends or another `*` appears. |
| `# comment` | A line comment. |

## Installation

TISSU is a single header. Copy `tissu.hpp` into your project and include it:

```cpp
#include "tissu.hpp"
```

Compile with a C++20-capable compiler:

```bash
g++ -std=c++20 save.cpp -o tissu.exe
```

## Usage example

The full program below is an example of TISSU usage (save_example.cpp). It covers everything: built-in types, a custom `Vector2` type, a `std::vector`, a `std::array`, and an apply-to-all settings block.

```cpp
#include "tissu.hpp"
#include <array>
using namespace TISSU_Serialization;

int main() {
    struct Vector2 { float x, y; };

    Vector2 position { 100.0f, 100.0f };
    float levelProgress { 12.5f };
    double money { 1048.55 };
    int achievements { 40 };
    std::string inGameTime { "12:55" };

    std::vector<std::string> inventory {
        "assault rifle", "pistol", "grenade", "knife", "smoke grenade", "bandage"
    };

    std::array<std::string, 3> choosenAbilities {
        "Freeze Ground", "Frozen Stance", "Frozen Spear"
    };

    std::vector<std::tuple<std::string, int>> settings = {
        { "shadowsLevel",        2 },
        { "textureQualityLevel", 3 },
        { "lightQualityLevel",   3 },
        { "worldDistanceLevel",  3 }
    };

    TissuSerializer tissu("config1.tissu");   // opens the file, registers built-ins

    // Register a custom type + a std::array. Registration is chainable.
    // (std::vectors are covered by the built-ins; std::arrays must be registered manually.)
    tissu.GetRegistry()->RegisterType<Vector2>("vec2",
        [](std::ostream& o, const Vector2& v) { o << v.x << ", " << v.y; },
        [](std::istream& i) {
            Vector2 v;
            i >> v.x;
            i >> std::ws;
            if (i.peek() == ',') { i.get(); }
            i >> v.y;
            return v;
        }
    ).RegisterStdArrayTypeAndCount<std::string, 3>("stdarr_string");

    tissu.BeginStruct("gamestate");
        IndentMember(tissu, position);        // uses the variable name as the field name
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
```

### Output

Running it writes `config1.tissu`:

```tissu
<gamestate:
    (vec2)position: 100, 100
    (f)levelProgress: 12.5
    (d)money: 1048.55
    (i)achievements: 40
    (str)inGameTime: 12:55
    (stdvec_string)inventory: assault rifle, pistol, grenade, knife, smoke grenade, bandage
    (stdarr_string[3])choosenAbilities: Freeze Ground, Frozen Stance, Frozen Spear
>
<settings:
    *(i)
    shadowsLevel: 2
    textureQualityLevel: 3
    lightQualityLevel: 3
    worldDistanceLevel: 3
>
```

### What the macros do

Each macro simply forwards the variable's own name as the field name, so you don't repeat yourself:

| Macro | Expands to |
| --- | --- |
| `IndentMember(ser, var)` | `ser.Member("var", var)` |
| `IndentArrMember(ser, var, size)` | `ser.StdVectorOrArrayMember("var", var, size)` |
| `IndentAndApplyForAll(ser, vec)` | `ser.ApplyForAllMembers(vec)` |

## Built-in types

| Tag | C++ type |
| --- | --- |
| `i` | `int` |
| `f` | `float` |
| `d` | `double` |
| `str` | `std::string` |
| `bool` | `bool` |
| `stdvec_int` / `stdvec_float` / `stdvec_double` / `stdvec_string` | `std::vector<…>` |

## Registering custom types

Any C++ type can be taught to TISSU by giving it a tag, a **writer**, and a **reader** — as with `Vector2` above. Because container registration reuses the element type's writer/reader, **register the element type first**:

```cpp
reg->RegisterType<Vector2>("vec2", /* writer */, /* reader */);
reg->RegisterStdVectorType<Vector2>("stdvec_vec2");        // needs vec2 first
reg->RegisterStdArrayTypeAndCount<Vector2, 3>("stdarr_vec2");
```

## API at a glance

**`TypeRegistry`**
- `RegisterType<T>(tag, writer, reader)` — map a type to a tag.
- `RegisterStdVectorType<T>(tag)` — register `std::vector<T>` (T must already be registered).
- `RegisterStdArrayTypeAndCount<T, N>(tag)` — register `std::array<T, N>`.
- `RegisterBuiltins()` — register the standard tags listed above.

**`TissuSerializer`**
- `BeginStruct(name)` / `EndStruct()` — open and close a struct block.
- `Member(name, value)` — write one typed line.
- `StdVectorOrArrayMember(name, value, arraySize)` — write a container line, optionally with the `[N]` size annotation.
- `ApplyForAllMembers(items)` — emit a `*(tag)` apply-to-all block from a list of `{name, value}` pairs.
- `GetRegistry()` — access the shared registry.

## Status & roadmap

- ✅ Serializer — types, containers, nested structs, apply-to-all.
- 🚧 **Deserializer** (`TISSU_Deserialization::TissuDeserializer`) — reader infrastructure is in place on the registry side; the file-parsing front end is a work in progress.
- 🚧 **tissu-gui** — a GUI editor for authoring and browsing `.tissu` files.

## License

Released under the [MIT License](https://opensource.org/licenses/MIT).

© 2026 Ondřej Slavík
