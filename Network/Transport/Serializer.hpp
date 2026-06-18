/*
    SlateX - 2026
*/
#pragma once
#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>
#include <cstdint>

// binary serializer for network packets
// write stuff in, send it, read it back out
// no fancy shit, just raw bytes

class Serializer {
public:
    void WriteString(const std::string& Val) {
        uint32_t Len = static_cast<uint32_t>(Val.size());
        WriteRaw(Len);
        m_buffer.insert(m_buffer.end(), Val.begin(), Val.end());
    }

    void WriteInt32(int32_t Val)   { WriteRaw(Val); }
    void WriteUInt32(uint32_t Val) { WriteRaw(Val); }
    void WriteInt64(int64_t Val)   { WriteRaw(Val); }
    void WriteUInt64(uint64_t Val) { WriteRaw(Val); }
    void WriteFloat(float Val)     { WriteRaw(Val); }
    void WriteDouble(double Val)   { WriteRaw(Val); }
    void WriteBool(bool Val)       { WriteRaw(Val); }
    void WriteByte(uint8_t Val)    { m_buffer.push_back(Val); }

    // writes length-prefixed blob
    void WriteBytes(const void* Data, size_t Size) {
        uint32_t Len = static_cast<uint32_t>(Size);
        WriteRaw(Len);
        const uint8_t* Ptr = reinterpret_cast<const uint8_t*>(Data);
        m_buffer.insert(m_buffer.end(), Ptr, Ptr + Size);
    }

    // appends raw buffer without length prefix
    void WriteRawBuffer(const std::vector<uint8_t>& Data) {
        m_buffer.insert(m_buffer.end(), Data.begin(), Data.end());
    }

    const std::vector<uint8_t>& GetBuffer() const { return m_buffer; }
    size_t Size() const { return m_buffer.size(); }
    void Clear() { m_buffer.clear(); }

private:
    template<typename T>
    void WriteRaw(const T& Val) {
        const uint8_t* Ptr = reinterpret_cast<const uint8_t*>(&Val);
        m_buffer.insert(m_buffer.end(), Ptr, Ptr + sizeof(T));
    }

    std::vector<uint8_t> m_buffer;
};

// reads back what Serializer wrote
// throws if you try to read past the end, da sucks

class Deserializer {
public:
    Deserializer(const uint8_t* Data, size_t Size)
        : m_data(Data), m_size(Size), m_offset(0) {}

    explicit Deserializer(const std::vector<uint8_t>& Buffer)
        : m_data(Buffer.data()), m_size(Buffer.size()), m_offset(0) {}

    std::string ReadString() {
        uint32_t Len = ReadRaw<uint32_t>();
        CheckBounds(Len);
        std::string Str(reinterpret_cast<const char*>(m_data + m_offset), Len);
        m_offset += Len;
        return Str;
    }

    int32_t  ReadInt32()   { return ReadRaw<int32_t>();  }
    uint32_t ReadUInt32()  { return ReadRaw<uint32_t>(); }
    int64_t  ReadInt64()   { return ReadRaw<int64_t>();  }
    uint64_t ReadUInt64()  { return ReadRaw<uint64_t>(); }
    float    ReadFloat()   { return ReadRaw<float>();    }
    double   ReadDouble()  { return ReadRaw<double>();   }
    bool     ReadBool()    { return ReadRaw<bool>();     }
    uint8_t  ReadByte()    { return ReadRaw<uint8_t>();  }

    // reads length-prefixed blob
    std::vector<uint8_t> ReadBytes() {
        uint32_t Len = ReadRaw<uint32_t>();
        CheckBounds(Len);
        std::vector<uint8_t> Out(m_data + m_offset, m_data + m_offset + Len);
        m_offset += Len;
        return Out;
    }

    bool   HasMore()    const { return m_offset < m_size; }
    size_t BytesLeft()  const { return m_size - m_offset; }

private:
    template<typename T>
    T ReadRaw() {
        CheckBounds(sizeof(T));
        T Val;
        std::memcpy(&Val, m_data + m_offset, sizeof(T));
        m_offset += sizeof(T);
        return Val;
    }

    void CheckBounds(size_t Needed) const {
        if (m_offset + Needed > m_size)
            throw std::runtime_error("Deserializer: buffer overflow, da sucks");
    }

    const uint8_t* m_data;
    size_t         m_size;
    size_t         m_offset;
};