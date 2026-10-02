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
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <memory>
#include <sstream>
#include <tuple>
#include <vector>
#include <array>

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
                    throw std::runtime_error("TISSU: Cannot register array because element type is not registered yet!'" + std::string(tag) + "'");
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
            
            void RegisterTypes() {
                RegisterType<int> ("i", [](std::ostream& o, const int& v) { writeNumberAsText(o, v); }, [](std::istream &i) { int v; i >> v; return v; });
                RegisterType<float> ("f", [](std::ostream& o, const float& v) { writeNumberAsText(o, v); }, [](std::istream &i) { float v; i >> v; return v; });
                RegisterType<double> ("d", [](std::ostream& o, const double& v) { writeNumberAsText(o, v); }, [](std::istream &i) { double v; i >> v; return v; });
                RegisterType<std::string>("str", [](std::ostream& o, const std::string& s) { o << s; }, [](std::istream &i) { 
                    i >> std::ws; 
                    std::string s; 
                    std::getline(i, s, ','); 
                    while(!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) { s.pop_back(); }
                    return s;
                });
                RegisterType<bool>("bool", [](std::ostream& o, const bool& b) {o << b; }, [](std::istream &i) {bool b; i >> b; return b; });
                RegisterStdVectorType<std::string>("stdvec_string");
                RegisterStdVectorType<int>("stdvec_int");
                RegisterStdVectorType<float>("stdvec_float");
                RegisterStdVectorType<double>("stdvec_double");
                RegisterStdArrayTypeAndCount<std::string, 3>("stdarr_string");
            }
    };

    class TissuSerializer {
        std::ofstream fileStream;
        int indentLevel = 0;
        const char* indentation = "    ";
        std::shared_ptr<TypeRegistry> registry;
        
        void writeIndent() {
            for (int i = 0; i < indentLevel; ++i) { fileStream << indentation; }
        }

        public:
            TissuSerializer(std::shared_ptr<TypeRegistry> reg = nullptr) : fileStream(), registry(reg ? reg : std::make_shared<TypeRegistry>()) {
                if(!reg) { registry->RegisterTypes(); }
            } // default constructor
        
            explicit TissuSerializer(const char* fileName, std::shared_ptr<TypeRegistry> reg = nullptr) : fileStream(fileName), registry(reg ? reg : std::make_shared<TypeRegistry>()) {
                if (!fileStream.is_open()) {
                    std::string msg = "TISSU: cannot open ";
                    msg.append(fileName);
                    throw std::runtime_error(msg);
                }

                if(!reg) { registry->RegisterTypes(); }       
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

            void Flush() {
                if(indentLevel == 0) { fileStream.flush(); }
            }

            // Write one "(tag)name: value" line. The type must be registered.
            template <class T>
            TissuSerializer& Member(std::string_view name, const T &value) {
                auto it = registry->byType.find(std::type_index(typeid(T)));
                if (it == registry->byType.end()) {
                    std::string msg = "TISSU: no identifier registered for member '";
                    msg.append(name).append("'");
                    throw std::runtime_error(msg);
                }

                writeIndent();
                fileStream << '(' << it->second.tag << ')' << name << ": ";
                it->second.write(fileStream, &value);
                fileStream << '\n';
                return *this;
            }

            // Write one "(tag)name: value" line. The type must be registered.
            template <class T>
            TissuSerializer& StdVectorOrArrayMember(std::string_view name, const T &value, std::size_t arraySize = 0) {
                auto it = registry->byType.find(std::type_index(typeid(T)));
                if (it == registry->byType.end()) {
                    std::string msg = "TISSU: no identifier registered for member '";
                    msg.append(name).append("'");
                    throw std::runtime_error(msg);
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
            TissuSerializer& ApplyForAllMembersVector(std::vector<std::tuple<std::string, T>> &items) {
                auto it = registry->byType.find(std::type_index(typeid(T)));
                if (it == registry->byType.end()) {
                    std::string msg = "TISSU: no identifier registered for member '";
                    msg.append(it->second.tag).append("'");
                    throw std::runtime_error(msg);
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

            template <class T, size_t Size>
            TissuSerializer& ApplyForAllMembersArray(std::array<std::tuple<std::string, T>, Size> &items) {
                auto it = registry->byType.find(std::type_index(typeid(T)));
                if (it == registry->byType.end()) {
                    throw std::runtime_error("TISUU: no identifier registered for member: ");
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
}

// Writes a member using the variable's own name as the field name:
//   IndentMember(s, playerPos)  ->  s.Member("playerPos", playerPos)
#define IndentMember(ser, var) (ser).Member(#var, var)
#define IndentArrMember(ser, var, arrSize) (ser).StdVectorOrArrayMember(#var, var, arrSize)
#define IndentAndApplyForAllVec(ser, vec) (ser).ApplyForAllMembersVector(vec)
#define IndentAndApplyForAllArr(ser, arr) (ser).ApplyForAllMembersArray(arr)

namespace TISSU_Deserialization {
    
    class TissuDeserializer {
        std::ifstream fileInput;
        std::shared_ptr<TISSU_Serialization::TypeRegistry> registry;

        std::string buffered;      // one-line lookahead
        bool hasBuffered = false;

        static std::string_view trim(std::string_view s) {
            const char* ws = " \t\r\n";
            auto b = s.find_first_not_of(ws);
            if (b == std::string_view::npos) { return {}; }
            auto e = s.find_last_not_of(ws);
            return s.substr(b, e - b + 1);
        }

        // Peek the next non-blank line without consuming it. '*' is NOT skipped here.
        bool peekLine(std::string& out) {
            if (hasBuffered) { out = buffered; return true; }
            std::string raw;
            while (std::getline(fileInput, raw)) {
                std::string_view v = trim(raw);
                if (v.empty()) { continue; }
                buffered = std::string(v);
                hasBuffered = true;
                out = buffered;
                return true;
            }
            return false;
        }

        void consume() { hasBuffered = false; }

        // Next line for a normal member: transparently skips a leading "*(tag)" marker.
        bool nextMemberLine(std::string& out) {
            if (!peekLine(out)) { return false; }
            if (out.front() == '*') {
                consume();
                if (!peekLine(out)) { return false; }
            }
            consume();
            return true;
        }

        struct Field { std::string name, value; };

        // returns name, value by splitting before colon and after colon 
        static Field parseField(const std::string& line) {
            std::string rest = line;
            if (!rest.empty() && rest.front() == '(') {
                auto close = rest.find(')');
                if (close == std::string::npos) { throw std::runtime_error("TISSU: malformed tag in '" + line + "'"); }
                rest = rest.substr(close + 1);
            }
            auto colon = rest.find(':');
            if (colon == std::string::npos) { throw std::runtime_error("TISSU: no ':' in '" + line + "'"); }
            return { std::string(trim(rest.substr(0, colon))), std::string(trim(rest.substr(colon + 1))) }; 
        }

        public:
            explicit TissuDeserializer(const char* fileName, std::shared_ptr<TISSU_Serialization::TypeRegistry> reg = nullptr)
            : fileInput(fileName), registry(reg ? reg : std::make_shared<TISSU_Serialization::TypeRegistry>()) {
                if (!fileInput.is_open()) { throw std::runtime_error(std::string("TISSU: cannot open ") + fileName); }
                if (!reg) { registry->RegisterTypes(); }
            }

            std::shared_ptr<TISSU_Serialization::TypeRegistry> GetRegistry() const { return registry; }

            TissuDeserializer& BeginStruct(std::string_view name) {
                std::string line;
                if (!nextMemberLine(line)) { throw std::runtime_error("TISSU: expected <" + std::string(name) + ":> but hit end of file"); }
                if (line.find('<') == std::string::npos || line.find(name) == std::string::npos) {
                    throw std::runtime_error("TISSU: expected struct '" + std::string(name) + "', got '" + line + "'");
                }
                return *this;
            }

            TissuDeserializer& EndStruct() {
                std::string line;
                if (!nextMemberLine(line)) { throw std::runtime_error("TISSU: expected '>' but hit end of file"); }
                if (line.find('>') == std::string::npos) { throw std::runtime_error("TISSU: expected '>', got '" + line + "'"); }
                return *this;
            }

            // read one member into "out" using the reader registered for T.
            template <class T>
            TissuDeserializer& Member(std::string_view name, T &out) {
                auto it = registry->byType.find(std::type_index(typeid(T)));
                if (it == registry->byType.end()) {
                        std::string msg = "TISSU: no identifier registered for member '";
                        msg.append(name).append("'");
                        throw std::runtime_error(msg); 
                    }

                std::string line;
                if (!nextMemberLine(line)) { throw std::runtime_error("TISSU: unexpected end of file reading '" + std::string(name) + "'"); }

                Field f = parseField(line);
                if (f.name != name) { throw std::runtime_error("TISSU: expected member '" + std::string(name) + "', found '" + f.name + "'"); }

                std::istringstream value(f.value);
                it->second.reader(value, static_cast<void*>(&out));
                return *this;
            }

            // Mirror of ApplyForAllMembersVector: consumes the "*(tag)" marker,
            // then reads every "key: value" line into pairs until the block ends
            // at the struct's '>' (or the next '*' section).
            template <class T>
            TissuDeserializer& ApplyForAll(std::vector<std::tuple<std::string, T>>& out) {
                auto it = registry->byType.find(std::type_index(typeid(T)));
                if (it == registry->byType.end()) { throw std::runtime_error("TISSU: no identifier registered for apply-for-all block"); }

                std::string line;
                if (!peekLine(line)) { throw std::runtime_error("TISSU: expected '*' apply-for-all marker, hit end of file"); }
                if (line.front() != '*') { throw std::runtime_error("TISSU: expected '*' apply-for-all marker, got '" + line + "'"); }
                consume(); // drop the "*(tag)" line — T already supplies the type

                out.clear();
                while (peekLine(line)) {
                    if (line.front() == '>' || line.front() == '*') { break; } // leave the '>' for EndStruct
                    consume();
                    Field f = parseField(line);
                    T value{};
                    std::istringstream vs(f.value);
                    it->second.reader(vs, static_cast<void*>(&value));
                    out.emplace_back(f.name, std::move(value));
                }
                return *this;
            }
    };
}

#define ReadMember(deser, var) (deser).Member(#var, var)
#define ReadApplyForAll(deser, var) (deser).ApplyForAll(var)