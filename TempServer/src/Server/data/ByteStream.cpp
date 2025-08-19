#include "SPch.h"
#include "ByteStream.h"
namespace Tso {

    //ByteStream ByteStream::SeriealizeEntity(Entity& entity, const uint8_t protocolID) {
    //    //for network 
    //    ByteStream entityByte;

    //    /*entityByte.write(protocolID);*/

    //    if (entity.HasComponent<TagComponent>())
    //    {
    //        entityByte.write(ComponentID::TagComponent);
    //        entityByte.writeString(entity.GetComponent<TagComponent>().GetTagName());
    //    }

    //    if (entity.HasComponent<ScriptComponent>())
    //    {
    //        //TODO(Suchan): script maybe not necessary
    //    }

    //    if (entity.HasComponent<TransformComponent>())
    //    {
    //        entityByte.write(ComponentID::TransformComponent);
    //        for (int i = 0; i < 3; i++) entityByte.write(entity.GetComponent<TransformComponent>().m_Translation[i]);
    //        for (int i = 0; i < 3; i++) entityByte.write(entity.GetComponent<TransformComponent>().m_Scale[i]);
    //        for (int i = 0; i < 3; i++) entityByte.write(entity.GetComponent<TransformComponent>().m_Rotation[i]);
    //    }

    //    if (entity.HasComponent<CameraComponent>())
    //    {
    //        //TODO(Suchan): camera maybe does not sync maybe
    //    }

    //    if (entity.HasComponent<Renderable>())
    //    {
    //        //TODO(Suchan):no need for network
    //    }


    //    if (entity.HasComponent<Rigidbody2DComponent>())
    //    {
    //        //TODO(Suchan):no need for network
    //    }

    //    if (entity.HasComponent<BoxCollider2DComponent>())
    //    {
    //        //TODO(Suchan):no need for network
    //    }

    //    if (entity.HasComponent<TextComponent>())
    //    {
    //        entityByte.write(ComponentID::TextComponent);
    //        entityByte.writeString(entity.GetComponent<TextComponent>().Text);
    //    }

    //    if (entity.HasComponent<IDComponent>())
    //    {
    //        entityByte.write(ComponentID::IDComponent);
    //        entityByte.write((uint64_t)(entity.GetComponent<IDComponent>().ID));
    //    }

    //    PackHeader(entityByte, { protocolID , entityByte.getRawBufferLength() + ByteStream::HEADER_SIZE });
    //    return entityByte;
    //}

    void ByteStream::DeseriealizeEntity(Ref<Scene> scene)
    {
        vec3 translate;
        vec3 scale;
        vec3 rotation;
        std::string tag;
        std::string text;
        UUID uuid;
        bool transformTag = false;

        while (pos < buffer.size()) {
            ComponentID CompID = (ComponentID)read<uint16_t>();
            if (CompID == ComponentID::TagComponent) {
                tag = readString();
            }
            if (CompID == ComponentID::TransformComponent) {
                for (int i = 0; i < 3; i++) translate[i] = read<float>();
                for (int i = 0; i < 3; i++) scale[i] = read<float>();
                for (int i = 0; i < 3; i++) rotation[i] = read<float>();
                transformTag = true;
            }
            if (CompID == ComponentID::TextComponent) {
                text = readString();
            }
            if (CompID == ComponentID::IDComponent) {
                uuid = read<uint64_t>();
            }
            else {
                SERVER_ASSERT(false, "unknown componentID:{}", (uint16_t)CompID);
            }
        }
        /*if (scene->HasEntity(uuid)) {
            auto entity = scene->GetEntityByUUID(uuid);
            if (transformTag) {
                auto& transform = entity.GetComponent<TransformComponent>();
                transform.m_Translation = translate;
                transform.m_Scale = scale;
                transform.m_Rotation = rotation;
            }
            if (!tag.empty()) {
                auto& tagcomp = entity.GetComponent<TagComponent>();
                tagcomp.m_Name = tag;
            }
            if (!text.empty()) {
                auto& textcomp = entity.GetComponent<TextComponent>();
                textcomp.Text = text;
            }
        }*/
    }

//    void ByteStream::PackHeader(ByteStream& byte, const ByteStream::Header& header)
//    {
//        byte.writeFront(header.dataLength);
//        byte.writeFront(header.protocol);
//    }

    ByteStream::ByteStream(std::vector<uint8_t> buffer) : buffer(buffer)
    {
    }



    template<typename T>
    void ByteStream::writeFront(const T& value) {
        SERVER_ASSERT(std::is_arithmetic_v<T> || std::is_enum_v<T>,
            "Only arithmetic types and enums allowed");
        const char* data = reinterpret_cast<const char*>(&value);
        buffer.insert(buffer.begin(), data, data + sizeof(T));
    }


    // 读取基本类型


    // 读取字符串
    std::string ByteStream::readString()const {
        uint32_t len = read<uint32_t>();
        if (pos + len > buffer.size()) {
            throw std::runtime_error("Read out of bounds");
        }
        std::string str(buffer.begin() + pos, buffer.begin() + pos + len);
        pos += len;
        return str;
    }

    //const std::vector<uint8_t>& ByteStream::getBuffer() const { return buffer; }

    void ByteStream::setBuffer(const std::vector<uint8_t>& newBuffer) {
        buffer = newBuffer;
        pos = 0;
    }

    bool ByteStream::operator==(ByteStream& other) {
        if (buffer.size() != other.buffer.size()) return false;
        for (int i = 0; i < buffer.size(); i++)
            if (buffer[i] != other.buffer[i]) return false;
        return true;
    }
}
