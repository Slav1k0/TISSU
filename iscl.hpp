#pragma once

#include <charconv>
#include <fstream>
#include <functional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>

namespace ISCL {

    // Round-trip-safe number -> text: emits the shortest string that reads
    // back to the exact same value. Use this inside custom writers too.
    template <class T>
    inline void writeNumber(std::ostream& os, T value) {
        char buf[32];
        auto [end, ec] = std::to_chars(buf, buf + sizeof buf, value);
        os.write(buf, end - buf);
    }

    class IsclSerializer {
        std::ofstream fileStream;
        int indentLevel = 0;
        const char* indentation = "    ";

        // A registered type: its tag string + how to write one value inline.
        struct TypeEntry {
            std::string tag;
            std::function<void(std::ostream&, const void*)> write;
        };
        std::unordered_map<std::type_index, TypeEntry> registry;

        void writeIndent() {
            for (int i = 0; i < indentLevel; ++i) fileStream << indentation;
        }

        // int / float / double / string work with no manual registration.
        void registerBuiltins() {
            RegisterType<int> ("i", [](std::ostream& o, const int& v) { writeNumber(o, v); });
            RegisterType<float> ("f", [](std::ostream& o, const float& v) { writeNumber(o, v); });
            RegisterType<double> ("d", [](std::ostream& o, const double& v) { writeNumber(o, v); });
            RegisterType<std::string>("str", [](std::ostream& o, const std::string& s) { o << s; });
        }

    public:
        explicit IsclSerializer(const char* fileName) : fileStream(fileName) {
            if (!fileStream.is_open())
                throw std::runtime_error(std::string("ISCL: cannot open ") + fileName);
            registerBuiltins();
        }

        // Map a C++ type to a tag + an inline writer. Chainable.
        template <class T, class Writer>
        IsclSerializer& RegisterType(std::string tag, Writer writer) {
            registry[std::type_index(typeid(T))] = TypeEntry{
                std::move(tag),
                [writer = std::move(writer)](std::ostream& os, const void* p) {
                    writer(os, *static_cast<const T*>(p));
                }
            };
            return *this;
        }

        void BeginStruct(std::string_view name) {
            writeIndent();
            fileStream << '<' << name << ":\n";
            ++indentLevel;
        }

        void EndStruct() {
            --indentLevel;
            writeIndent();
            fileStream << ">\n";
        }

        // Write one "(tag)name: value" line. The type must be registered.
        template <class T>
        IsclSerializer& Member(std::string_view name, const T& value) {
            auto it = registry.find(std::type_index(typeid(T)));
            if (it == registry.end())
                throw std::runtime_error("ISCL: no identifier registered for member '"
                                         + std::string(name) + "'");
            writeIndent();
            fileStream << '(' << it->second.tag << ')' << name << ": ";
            it->second.write(fileStream, &value);
            fileStream << '\n';
            return *this;
        }
    };

} // namespace ISCL

// Writes a member using the variable's own name as the field name:
//   IndentMember(s, playerPos)  ->  s.Member("playerPos", playerPos)
#define IndentMember(ser, var) (ser).Member(#var, var)