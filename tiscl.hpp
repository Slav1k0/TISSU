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

namespace TISCL {

    // Converts number to a text
    template <class T>
    inline void writeNumberAsText(std::ostream& os, T value) {
        char buf[32];
        auto [end, ec] = std::to_chars(buf, buf + sizeof buf, value);
        os.write(buf, end - buf);
    }

    inline void writeNumberAsText(std::ostream& os, float value) {
        char buf[32];
        auto [end, ec] = std::to_chars(buf, buf + sizeof buf, value, std::chars_format::fixed, 1);
        os.write(buf, end - buf);
    }

    class TisclSerializer {
        std::ofstream fileStream;
        int indentLevel = 0;
        const char* indentation = "    ";

        // A registered type: its tag string + how to write one value inline.
        struct TypeEntry {
            std::string tag; //
            std::function<void(std::ostream&, const void*)> write;
        };
        std::unordered_map<std::type_index, TypeEntry> registry;

        void writeIndent() {
            for (int i = 0; i < indentLevel; ++i) fileStream << indentation;
        }

    public:
        explicit TisclSerializer(const char* fileName) : fileStream(fileName) {
            if (!fileStream.is_open())
                throw std::runtime_error(std::string("TISCL: cannot open ") + fileName);
            registerBuiltins();
        }

        // Map a C++ type to a tag + an inline writer. Chainable.
        template <class T, class Writer>
        TisclSerializer& RegisterType(std::string tag, Writer writer) {
            registry[std::type_index(typeid(T))] = TypeEntry{
                std::move(tag),
                [writer = std::move(writer)](std::ostream& os, const void* p) {
                    writer(os, *static_cast<const T*>(p));
                }
            };
            return *this;
        }

        // Registers std::vector<T> using a generic vector printer
        template <typename T>
        TisclSerializer& RegisterStdVectorType(std::string tag) {
            auto type = registry.find(std::type_index(typeid(T)));
            auto writer = type->second.write;

            return RegisterType<std::vector<T>>(std::move(tag), [writer](std::ostream& o, const std::vector<T>& svec) {
                bool first = true;
                for(const auto& i : svec) {
                    if(!first) {
                        o << ", ";
                    }
                    first = false;
                    writer(o, static_cast<const void*>(&i));    
                }
            });
        }

        // register a std::array with its size
        template <typename T, std::size_t Size>
        TisclSerializer& RegisterStdArrayTypeAndCount(std::string tag) {
            auto type = registry.find(std::type_index(typeid(T)));
            if (type == registry.end()) {
                throw std::runtime_error("TISCL: Cannot register array because element type is not registered yet!'" + std::string(tag) + "'");
            }

            auto writer = type->second.write;

            return RegisterType<std::array<T, Size>>(std::move(tag), [writer](std::ostream& o, const std::array<T, Size> &arr) {
                bool first = true;
                for(const auto& i : arr) {
                    if(!first) {
                        o << ", ";
                    }
                    first = false;
                    writer(o, static_cast<const void*>(&i));    
                }
            });
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
        TisclSerializer& Member(std::string_view name, const T& value) {
            auto it = registry.find(std::type_index(typeid(T)));
            if (it == registry.end()) {
                throw std::runtime_error("TISCL: no identifier registered for member '" + std::string(name) + "'");
            }

            writeIndent();
            fileStream << '(' << it->second.tag << ')' << name << ": ";
            it->second.write(fileStream, &value);
            fileStream << '\n';
            return *this;
        }

        // Write one "(tag)name: value" line. The type must be registered.
        template <class T>
        TisclSerializer& StdVectorOrArrayMember(std::string_view name, const T& value, std::size_t arraySize = 0) {
            auto it = registry.find(std::type_index(typeid(T)));
            if (it == registry.end()) {
                throw std::runtime_error("TISCL: no identifier registered for member '" + std::string(name) + "'");
            }

            writeIndent();
            if(arraySize == 0) {
                fileStream << '(' << it->second.tag << ')' << name << ": ";
                it->second.write(fileStream, &value);
                fileStream << '\n';
            }
            else {
                fileStream << '(' << it->second.tag << "[" << arraySize << "]" << ')' << name << ": ";
                it->second.write(fileStream, &value);
                fileStream << '\n';
            }

            return *this;
        }

    private:
        // int / float / double / string work with no manual registration.
        void registerBuiltins() {
            RegisterType<int> ("i", [](std::ostream& o, const int& v) { writeNumberAsText(o, v); });
            RegisterType<float> ("f", [](std::ostream& o, const float& v) { writeNumberAsText(o, v); });
            RegisterType<double> ("d", [](std::ostream& o, const double& v) { writeNumberAsText(o, v); });
            RegisterType<std::string>("str", [](std::ostream& o, const std::string& s) { o << s; });
            RegisterType<bool>("bool", [](std::ostream& o, const bool& b) {o << b; });
            RegisterStdVectorType<std::string>("stdvec_string");
            RegisterStdVectorType<int>("stdvec_int");
            RegisterStdVectorType<float>("stdvec_float");
            RegisterStdVectorType<double>("stdvec_double");
        }
    };

} // namespace ISCL

// Writes a member using the variable's own name as the field name:
//   IndentMember(s, playerPos)  ->  s.Member("playerPos", playerPos)
#define IndentMember(ser, var) (ser).Member(#var, var)
#define IndentArrMember(ser, var, arrSize) (ser).StdVectorOrArrayMember(#var, var, arrSize)