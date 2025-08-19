#pragma once
#include <string>
namespace Tso {
    class Entity;
    class Scene;

    

    class ByteStream {
    public:
//        struct Header
//        {
//            uint8_t protocol = 0;
//            size_t dataLength = 0;
//        };
        static constexpr size_t HEADER_SIZE = sizeof(uint32_t) + sizeof(uint16_t);
    public:
        ByteStream(const uint8_t* data, size_t size) : buffer(data, data + size), pos(0) {}

        // д���������
        template<typename T>
        void write(const T& value){
            SERVER_ASSERT(std::is_arithmetic_v<T> || std::is_enum_v<T>,
                "Only arithmetic types and enums allowed");
            const char* data = reinterpret_cast<const char*>(&value);
            buffer.insert(buffer.end(), data, data + sizeof(T));
        }
        //
        template<typename T>
        void writeFront(const T& value);

        // д���ַ���
        void writeString(const std::string& str){
            write<uint32_t>(static_cast<uint32_t>(str.size()));
            buffer.insert(buffer.end(), str.begin(), str.end());
        }

        // ��ȡ��������
        template<typename T>
        T read()const {
            static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T>,
                "Only arithmetic types and enums allowed");
            if (pos + sizeof(T) > buffer.size()) {
                throw std::runtime_error("Read out of bounds");
            }
            T value;
            memcpy(&value, buffer.data() + pos, sizeof(T));
            pos += sizeof(T);
            return value;
        }

        // ��ȡ�ַ���
        std::string readString()const;

        //ByteStream static SeriealizeEntity(Entity& entity, const uint8_t protocolID);
        void DeseriealizeEntity(Ref<Scene> scene);

//        void static PackHeader(ByteStream& byte, const Header& header);

        ByteStream(std::vector<uint8_t> buffer);
        ByteStream() = default;

        const std::vector<uint8_t>& getBuffer() const { return buffer; }
        const void* getRawBuffer() { return static_cast<void*>(buffer.data()); }
        const void ResetRead() { pos = 0; }
        size_t getRawBufferLength()& { return buffer.size(); }
        void SetClientId(uint32_t id) { m_ClientId = id; }
        uint32_t GetClientId() const { return m_ClientId; }
        void setBuffer(const std::vector<uint8_t>& newBuffer);
        
        bool operator==(ByteStream& other);
    private:
        std::vector<uint8_t> buffer;
        mutable size_t pos = 0;
        uint32_t m_ClientId = 0;

    };
}
