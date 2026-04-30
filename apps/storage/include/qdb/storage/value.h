#ifndef QUASARDB_VALUE_H
#define QUASARDB_VALUE_H

#include <string>
#include <variant>
#include <stdexcept>
#include <vector>
#include <cstdint>
#include <cstring>
#include "sql_bool.h"

class Value final {
private:
    std::variant<std::nullptr_t, int32_t, std::string> _data;

public:
    enum class Type {
        NULL_TYPE,
        INT,
        STRING,
    };
    
    Value() : _data(nullptr) {}
    Value(int32_t val) : _data(val) {}
    Value(std::string val) : _data(std::move(val)) {}
    Value(std::nullptr_t) : _data(nullptr) {}
    
    bool is_null() const {
        return std::holds_alternative<std::nullptr_t>(_data);
    }

    bool is_int() const {
        return std::holds_alternative<int32_t>(_data);
    }

    bool is_string() const {
        return std::holds_alternative<std::string>(_data);
    }

    uint32_t serialized_size() const {
        if (is_null()) {
            return 1;
        }
        if (is_int()) {
            return sizeof(as_int());
        }
        if (is_string()) {
            uint32_t len = static_cast<uint32_t>(as_string().size());
            return sizeof(len) + len;
        }
        assert(false && "Unexpected type.");
    }

    std::vector<uint8_t> serialize() const {
        if (is_null()) {
            return std::vector<uint8_t>(1, 0);
        }
        if (is_int()) {
            auto value = as_int();
            auto data_v = std::vector<uint8_t>(sizeof(value));
            std::memcpy(data_v.data(), &value, sizeof(value));
            return data_v;
        }
        if (is_string()) {
            uint32_t str_len = as_string().size();
            auto data_v = std::vector<uint8_t>();
            data_v.resize(sizeof(str_len) + str_len);
            std::memcpy(data_v.data(), &str_len, sizeof(str_len));
            std::memcpy(data_v.data() + sizeof(str_len), as_string().data(), str_len);
            return data_v;
        }
        assert(false && "Unexpected type.");
    }

    void append_to_buffer(std::vector<uint8_t>& buffer) const {
        if (is_null()) {
            buffer.push_back(0); 
        } else if (is_int()) {
            auto value = as_int();
            size_t offset = buffer.size();
            buffer.resize(offset + sizeof(value));
            std::memcpy(buffer.data() + offset, &value, sizeof(value));
        } else if (is_string()) {
            const auto& value = as_string();
            uint32_t len = static_cast<uint32_t>(value.size());
            size_t offset = buffer.size();
            buffer.resize(offset + sizeof(len) + len);
            std::memcpy(buffer.data() + offset, &len, sizeof(len));
            std::memcpy(buffer.data() + offset + sizeof(len), value.data(), len);
        } else {
            assert(false && "Unexpected type.");
        }
    }

    static Value from_binary(uint8_t*& data, Type type) {
        if (type == Type::NULL_TYPE) {
            uint8_t value;
            std::memcpy(&value, data, sizeof(value));
            if (value != 0) {
                throw std::runtime_error("Cannot read NULL.");
            }
            data += 1;
            return Value();
        }
        if (type == Type::INT) {
            int32_t value;
            std::memcpy(&value, data, sizeof(value));
            data += sizeof(value);
            return Value(value);
        }
        if (type == Type::STRING) {
            uint32_t len;
            std::memcpy(&len, data, sizeof(len));
            data += sizeof(len);
            std::string value;
            value.resize(len);
            std::memcpy(value.data(), data, len);
            data += len;
            return Value(value);
        }
        assert(false && "Unexpected type.");
    }

    Type get_type() const {
        if (is_null()) {
            return Type::NULL_TYPE;
        }
        if (is_int()) {
            return Type::INT;
        }
        if (is_string()) {
            return Type::STRING;
        }
        assert(false && "Unexpected type.");
    }

    std::string type_name() const {
        if (is_null()) {
            return "NULL";
        }
        if (is_int()) {
            return "INT";
        }
        if (is_string()) {
            return "STRING";
        }
        assert(false && "Unexpected type.");
    }
    
    int32_t as_int() const {
        if (!is_int())
            throw std::runtime_error("Value is not an int");
        return std::get<int32_t>(_data);
    }
    
    const std::string& as_string() const {
        if (!is_string())
            throw std::runtime_error("Value is not a string");
        return std::get<std::string>(_data);
    }

    int32_t& as_int() {
        if (!is_int())
            throw std::runtime_error("Value is not an int");
        return std::get<int32_t>(_data);
    }
    
    std::string& as_string() {
        if (!is_string())
            throw std::runtime_error("Value is not a string");
        return std::get<std::string>(_data);
    }
    
    std::string to_string() const {
        if (is_null()) {
            return "NULL";
        }
        if (is_int()) {
            return std::to_string(as_int());
        }
        if (is_string()) {
            return as_string();
        }
        assert(false && "Unexpected type.");
    }

    bool StrictEq(const Value& other) const {
        if (is_null() && other.is_null()) {
            return true;
        }
        if (get_type() != other.get_type()) {
            return false;
        }
        if (is_int()) {
            return as_int() == other.as_int();
        }
        if (is_string()) {
            return as_string() == other.as_string();
        }
        assert(false && "Unexpected type."); 
    }
    
    SqlBool operator==(const Value& other) const {
        if (is_null() || other.is_null()) {
            return SqlBool::UNKNOWN;
        }
        if (get_type() != other.get_type()) {
            throw std::runtime_error("Compare different type");
        }
        if (is_int()) {
            return as_int() == other.as_int() ? SqlBool::TRUE : SqlBool::FALSE;
        }
        if (is_string()) {
            return as_string() == other.as_string() ? SqlBool::TRUE : SqlBool::FALSE;
        }
        assert(false && "Unexpected type.");
    }
    
    SqlBool operator<(const Value& other) const {
        if (is_null() || other.is_null()) {
            return SqlBool::UNKNOWN;
        }
        if (get_type() != other.get_type()) {
            throw std::runtime_error("Compare different type");
        }
        if (is_int()) {
            return as_int() < other.as_int() ? SqlBool::TRUE : SqlBool::FALSE;
        }
        if (is_string()) {
            return as_string() < other.as_string() ? SqlBool::TRUE : SqlBool::FALSE;
        }
        assert(false && "Unexpected type.");
    }
    
    SqlBool operator<=(const Value& other) const {
        return (*this < other) || (*this == other);
    }
    
    SqlBool operator>(const Value& other) const {
        return !(*this <= other);
    }
    
    SqlBool operator>=(const Value& other) const {
        return !(*this < other);
    }
    
    SqlBool operator!=(const Value& other) const {
        return !(*this == other);
    }
};

#endif  // QUASARDB_VALUE_H