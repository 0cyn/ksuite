//
// Created by kat on 5/31/23.
//

#include "binaryninjaapi.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

#ifndef KSUITE_METADATASERIALIZABLE_HPP
#define KSUITE_METADATASERIALIZABLE_HPP

#define MSS(name) store(#name, name)
#define MSS_CAST(name, type) store(#name, (type)name)
#define MSL(name) name = load(#name, name)
#define MSL_CAST(name, storedType, type) name = (type)load(#name, (storedType)name)

using namespace BinaryNinja;

class MetadataSerializable {
protected:
    struct SerialContext {
        rapidjson::Document doc;
        rapidjson::Document::AllocatorType allocator;
    };
    struct DeserContext {
        rapidjson::Document doc;
    };

    DeserContext* m_activeDeserContext = nullptr;
    SerialContext* m_activeContext = nullptr;

    void SetupSerContext(rapidjson::Document::AllocatorType* alloc = nullptr)
    {
        m_activeContext = new SerialContext();
        m_activeContext->doc.SetObject();
        m_activeContext->allocator = m_activeContext->doc.GetAllocator();
    }
    void S()
    {
        if (!m_activeContext)
            SetupSerContext();
    }
    rapidjson::Value& r(std::string k) // the r stands for really stupid thanks rapidjson
    {
        auto n = new rapidjson::Value(k.c_str(), m_activeContext->allocator);
        return *n;
    }
protected:
    void Serialize(std::string& name, uint8_t b)
    {
        S();
        m_activeContext->doc.AddMember(r(name), b, m_activeContext->allocator);
    }
    void Deserialize(std::string& name, uint8_t& b)
    {
        b = static_cast<uint8_t>(m_activeDeserContext->doc[name.c_str()].GetUint64());
    }
    void Serialize(std::string& name, uint16_t b)
    {
        S();
        m_activeContext->doc.AddMember(r(name), b, m_activeContext->allocator);
    }
    void Deserialize(std::string& name, uint16_t& b)
    {
        b = static_cast<uint16_t>(m_activeDeserContext->doc[name.c_str()].GetUint64());
    }
    void Serialize(std::string& name, uint32_t b)
    {
        S();
        m_activeContext->doc.AddMember(r(name), b, m_activeContext->allocator);
    }
    void Deserialize(std::string& name, uint32_t& b)
    {
        b = static_cast<uint32_t>(m_activeDeserContext->doc[name.c_str()].GetUint64());
    }
    void Serialize(std::string& name, uint64_t b)
    {
        S();
        m_activeContext->doc.AddMember(r(name), b, m_activeContext->allocator);
    }
    void Deserialize(std::string& name, uint64_t& b)
    {
        b = m_activeDeserContext->doc[name.c_str()].GetUint64();
    }
    void Serialize(std::string& name, int8_t b)
    {
        S();
        m_activeContext->doc.AddMember(r(name), b, m_activeContext->allocator);
    }
    void Deserialize(std::string& name, int8_t& b)
    {
        b = m_activeDeserContext->doc[name.c_str()].GetInt64();
    }
    void Serialize(std::string& name, int16_t b)
    {
        S();
        m_activeContext->doc.AddMember(r(name), b, m_activeContext->allocator);
    }
    void Deserialize(std::string& name, int16_t& b)
    {
        b = m_activeDeserContext->doc[name.c_str()].GetInt64();
    }
    void Serialize(std::string& name, int32_t b)
    {
        S();
        m_activeContext->doc.AddMember(r(name), b, m_activeContext->allocator);
    }
    void Deserialize(std::string& name, int32_t& b)
    {
        b = m_activeDeserContext->doc[name.c_str()].GetInt();
    }
    void Serialize(std::string& name, int64_t b)
    {
        S();
        m_activeContext->doc.AddMember(r(name), b, m_activeContext->allocator);
    }
    void Deserialize(std::string& name, int64_t& b)
    {
        b = m_activeDeserContext->doc[name.c_str()].GetInt64();
    }
    void Serialize(std::string& name, std::string b)
    {
        S();
        m_activeContext->doc.AddMember(r(name), b, m_activeContext->allocator);
    }
    void Deserialize(std::string& name, std::string& b)
    {
        b = m_activeDeserContext->doc[name.c_str()].GetString();
    }
    void Serialize(std::string& name, std::map<uint64_t, std::string> b)
    {
        S();
        rapidjson::Value bArr(rapidjson::kArrayType);
        for (auto i : b)
        {
            rapidjson::Value p(rapidjson::kArrayType);
            p.PushBack(i.first, m_activeContext->allocator);
            p.PushBack(i.second, m_activeContext->allocator);
            bArr.PushBack(p, m_activeContext->allocator);
        }
        m_activeContext->doc.AddMember(r(name), bArr, m_activeContext->allocator);
    }
    void Deserialize(std::string& name, std::map<uint64_t, std::string>& b)
    {
        for (auto& i : m_activeDeserContext->doc[r(name)].GetArray())
            b[i.GetArray()[0].GetUint64()] = i.GetArray()[1].GetString();
    }
    void Serialize(std::string& name, std::unordered_map<uint64_t, std::string> b)
    {
        S();
        rapidjson::Value bArr(rapidjson::kArrayType);
        for (auto i : b)
        {
            rapidjson::Value p(rapidjson::kArrayType);
            p.PushBack(i.first, m_activeContext->allocator);
            p.PushBack(i.second, m_activeContext->allocator);
            bArr.PushBack(p, m_activeContext->allocator);
        }
        m_activeContext->doc.AddMember(r(name), bArr, m_activeContext->allocator);
    }
    void Serialize(std::string& name, std::unordered_map<std::string, std::string> b)
    {
        S();
        rapidjson::Value bArr(rapidjson::kArrayType);
        for (auto i : b)
        {
            rapidjson::Value p(rapidjson::kArrayType);
            p.PushBack(i.first, m_activeContext->allocator);
            p.PushBack(i.second, m_activeContext->allocator);
            bArr.PushBack(p, m_activeContext->allocator);
        }
        m_activeContext->doc.AddMember(r(name), bArr, m_activeContext->allocator);
    }
    void Deserialize(std::string& name, std::unordered_map<uint64_t, std::string>& b)
    {
        for (auto& i : m_activeDeserContext->doc[r(name)].GetArray())
            b[i.GetArray()[0].GetUint64()] = i.GetArray()[1].GetString();
    }
    void Deserialize(std::string& name, std::unordered_map<std::string, std::string>& b)
    {
        for (auto& i : m_activeDeserContext->doc[r(name)].GetArray())
            b[i.GetArray()[0].GetString()] = i.GetArray()[1].GetString();
    }
    void Serialize(std::string &name, std::vector<std::string> b)
    {
        S();
        rapidjson::Value bArr(rapidjson::kArrayType);
        for (const auto& s : b)
            bArr.PushBack(s, m_activeContext->allocator);
        m_activeContext->doc.AddMember(r(name), bArr, m_activeContext->allocator);
    }
    void Deserialize(std::string& name, std::vector<std::string>& b)
    {
        for (auto& i : m_activeDeserContext->doc[r(name)].GetArray())
            b.emplace_back(i.GetString());
    }
    void Serialize(std::string& name, std::vector<std::pair<uint64_t, std::pair<uint64_t, uint64_t>>> b)
    {
        S();
        rapidjson::Value bArr(rapidjson::kArrayType);
        for (auto i : b)
        {
            rapidjson::Value segV(rapidjson::kArrayType);
            segV.PushBack(i.first, m_activeContext->allocator);
            segV.PushBack(i.second.first, m_activeContext->allocator);
            segV.PushBack(i.second.second, m_activeContext->allocator);
            bArr.PushBack(segV, m_activeContext->allocator);
        }
        m_activeContext->doc.AddMember(r(name), bArr, m_activeContext->allocator);
    }
    void Deserialize(std::string& name, std::vector<std::pair<uint64_t, std::pair<uint64_t, uint64_t>>>& b)
    {
        for (auto& i : m_activeDeserContext->doc[r(name)].GetArray())
        {
            std::pair<uint64_t, std::pair<uint64_t, uint64_t>> j;
            j.first = i.GetArray()[0].GetUint64();
            j.second.first = i.GetArray()[1].GetUint64();
            j.second.second = i.GetArray()[2].GetUint64();
            b.push_back(j);
        }
    }

    template <typename T> void store(std::string x, T y)
    {
        Serialize(x, y);
    }
    template <typename T> T load(std::string x, T y)
    {
        T val;
        Deserialize(x, val);
        return val;
    }

    rapidjson::Document& GetDoc()
    {
        S();
        Store();
        return m_activeContext->doc;
    }

public:

    virtual void Store() = 0;
    virtual void Load() = 0;

    std::string AsString()
    {
        rapidjson::StringBuffer strbuf;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(strbuf);
        GetDoc().Accept(writer);

        std::string s = strbuf.GetString();
        return s;
    }
    rapidjson::Document& AsDocument()
    {
        return GetDoc();
    }
    void LoadFromString(const std::string& s)
    {
        auto d = new DeserContext();
        d->doc.Parse(s.c_str());
        m_activeDeserContext = d;
        Load();
    }
    void LoadFromValue(rapidjson::Value & s)
    {
        auto d = new DeserContext();
        d->doc.CopyFrom(s, d->doc.GetAllocator());
        m_activeDeserContext = d;
        Load();
    }
    Ref<Metadata> AsMetadata()
    {
        return new Metadata(AsString());
    }
    bool LoadFromMetadata(const Ref<Metadata>& meta)
    {
        if (!meta->IsString())
            return false;
        LoadFromString(meta->GetString());
        return true;
    }
};

#endif //KSUITE_METADATASERIALIZABLE_HPP
