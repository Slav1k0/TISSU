/**
 * TISSU: Typed, Indentation Separated Scripting Utility
 * 
 * @author          Ondřej Slavík
 * @copyright       2026 (C) Ondřej Slavík
 * 
 * Released under the MIT License.
 * See: https://opensource.org/licenses/MIT
 */

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
#include <memory>


// A registered type: its tag string + how to write one value inline.
struct TypeEntry {
    std::string tag; 
    std::type_index typeId;
    std::function<void(std::ostream&, const void*)> write;
    std::function<void(std::istream&, void*)> reader;

    TypeEntry(std::string t, std::type_index id, 
              std::function<void(std::ostream&, const void*)> w, 
              std::function<void(std::istream&, void*)> r)
        : tag(std::move(t)), typeId(id), write(std::move(w)), reader(std::move(r)) {}
};

namespace TISSU_Serialization {

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

    class TypeRegistry {            
        public:
            std::unordered_map<std::type_index, TypeEntry> byType;
            std::unordered_map<std::string, TypeEntry> byTag;

            // Map a C++ type to a tag + an inline writer. Chainable.
            template <class T, class Writer, class Reader>
            TypeRegistry& RegisterType(std::string tag, Writer writer, Reader reader) {
                std::type_index idx = std::type_index(typeid(T));

                TypeEntry entry{
                    tag,
                    idx, 
                    [writer = std::move(writer)](std::ostream& os, const void* p) {
                        writer(os, *static_cast<const T*>(p));
                    },
                    [reader = std::move(reader)](std::istream& is, void *pTarget) {
                        *static_cast<T*>(pTarget) = reader(is); 
                    }
                };

                // Store in both lookup tables
                byType.insert_or_assign(idx, entry);
                byTag.insert_or_assign(std::move(tag), std::move(entry)); 

                return *this;
            }

            template <typename T>
            TypeRegistry& RegisterStdVectorType(std::string tag) {
                auto type = byType.find(std::type_index(typeid(T)));

                auto elementWriter = type->second.write;
                auto elementReader = type->second.reader;

                return RegisterType<std::vector<T>>(
                    std::move(tag), 
                    // writer
                    [elementWriter](std::ostream& o, const std::vector<T>& svec) {
                        bool first = true;
                        for(const auto& i : svec) {
                            if(!first) {
                                o << ", ";
                            }
                            first = false;
                            elementWriter(o, static_cast<const void*>(&i));    
                        }
                    },
                    // reader
                    [elementReader](std::istream & i) {
                        std::vector<T> vec;
                        while (i.good()) {
                            // Peek ahead to see if we should stop (e.g., if you use ending braces like ']')
                            if (i.peek() == '\n' || i.peek() == EOF) {
                                break;
                            }

                            vec.emplace_back();

                            elementReader(i, static_cast<void*>(&vec.back()));

                            // Handle the separating comma if there is one next in the stream
                            i >> std::ws; 
                            if (i.peek() == ',') {
                                i.get();
                            }
                        }
                        return vec;
                    }
                );

                return *this;
            }

            template <typename T, std::size_t Size>
            TypeRegistry& RegisterStdArrayTypeAndCount(std::string tag) {
                auto type = byType.find(std::type_index(typeid(T)));
                if (type == byType.end()) {
                    throw std::runtime_error("TISCL: Cannot register array because element type is not registered yet!'" + std::string(tag) + "'");
                }

                auto elementWriter = type->second.write;
                auto elementReader = type->second.reader;

                return RegisterType<std::array<T, Size>>(
                    std::move(tag), 
                    // writer
                    [elementWriter](std::ostream& o, const std::array<T, Size>& sarr) {
                        bool first = true;
                        for(const auto& i : sarr) {
                            if(!first) { o << ", "; }
                            first = false;
                            elementWriter(o, static_cast<const void*>(&i));    
                        }
                    },
                    // reader
                    [elementReader](std::istream & i) {
                        std::array<T, Size> sarr{};
                        for(size_t idx = 0; idx < Size; ++idx) {
                            elementReader(i, static_cast<void*>(&sarr[idx]));

                            i >> std::ws;
                            if(i.peek() == ',') { i.get(); }
                        }
                        return sarr;
                    }
                );

                return *this;
            }
            
            void RegisterBuiltins() {
                RegisterType<int> ("i", [](std::ostream& o, const int& v) { writeNumberAsText(o, v); }, [](std::istream &i) { int v; i >> v; return v; });
                RegisterType<float> ("f", [](std::ostream& o, const float& v) { writeNumberAsText(o, v); }, [](std::istream &i) { float v; i >> v; return v; });
                RegisterType<double> ("d", [](std::ostream& o, const double& v) { writeNumberAsText(o, v); }, [](std::istream &i) { double v; i >> v; return v; });
                RegisterType<std::string>("str", [](std::ostream& o, const std::string& s) { o << s; }, [](std::istream &i) { std::string s; i >> s; return s; });
                RegisterType<bool>("bool", [](std::ostream& o, const bool& b) {o << b; }, [](std::istream &i) {bool b; i >> b; return b; });
                RegisterStdVectorType<std::string>("stdvec_string");
                RegisterStdVectorType<int>("stdvec_int");
                RegisterStdVectorType<float>("stdvec_float");
                RegisterStdVectorType<double>("stdvec_double");
            }
    };

    class TissuSerializer {
        std::ofstream fileStream;
        int indentLevel = 0;
        const char* indentation = "    ";
        std::shared_ptr<TypeRegistry> registry;
        

        void writeIndent() {
            for (int i = 0; i < indentLevel; ++i) fileStream << indentation;
        }

        public:
            TissuSerializer(std::shared_ptr<TypeRegistry> reg = nullptr) : fileStream(), registry(reg ? reg : std::make_shared<TypeRegistry>()) {
                if(!reg) {
                    registry->RegisterBuiltins();
                }
            } // default constructor
        
            explicit TissuSerializer(const char* fileName, std::shared_ptr<TypeRegistry> reg = nullptr) : fileStream(fileName), registry(reg ? reg : std::make_shared<TypeRegistry>()) {
                if (!fileStream.is_open()) {
                    throw std::runtime_error(std::string("TISCL: cannot open ") + fileName);
                }

                if(!reg) {
                    registry->RegisterBuiltins();
                }       
            }

            std::shared_ptr<TypeRegistry> GetRegistry() const {
                return registry;
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
            TissuSerializer& Member(std::string_view name, const T& value) {
                auto it = registry->byType.find(std::type_index(typeid(T)));
                if (it == registry->byType.end()) {
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
            TissuSerializer& StdVectorOrArrayMember(std::string_view name, const T& value, std::size_t arraySize = 0) {
                auto it = registry->byType.find(std::type_index(typeid(T)));
                if (it == registry->byType.end()) {
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

            template <class T>
            TissuSerializer& ApplyForAllMembers(std::vector<std::tuple<std::string, T>> items) {
                auto it = registry->byType.find(std::type_index(typeid(T)));
                if (it == registry->byType.end()) {
                    throw std::runtime_error("TISCL: no identifier registered for member '" + std::string("") + "'");
                }

                writeIndent();
                fileStream << '*' << '(' << it->second.tag << ')' << '\n';
                for(auto & [key, val] : items) {
                    writeIndent();
                    fileStream << key << ": ";
                    it->second.write(fileStream, &val);
                    fileStream << '\n';
                }
                return *this;
            }
    };

} // namespace ISCL

// Writes a member using the variable's own name as the field name:
//   IndentMember(s, playerPos)  ->  s.Member("playerPos", playerPos)
#define IndentMember(ser, var) (ser).Member(#var, var)
#define IndentArrMember(ser, var, arrSize) (ser).StdVectorOrArrayMember(#var, var, arrSize)
#define IndentAndApplyForAll(ser, vec) (ser).ApplyForAllMembers(vec)

namespace TISSU_Deserialization {
    
    class TissuDeserializer {
        std::ifstream fileInput;
        std::unordered_map<std::type_index, TypeEntry> byType;

        public:
            explicit TissuDeserializer(const char *fileName) : fileInput(fileName) {};

            TissuDeserializer& ReadFromFile(const std::string &fileName) {
                if (fileInput.is_open()) {
                    fileInput.close();
                }

                // 3. Clear any previous error flags (like EOF) from old stream operations
                fileInput.clear();

                // 4. Open the new file
                fileInput.open(fileName);

                // 5. Error handling
                if (!fileInput.is_open()) {
                    throw std::runtime_error("TISCL: cannot open " + fileName);
                }
            }
    };      
}