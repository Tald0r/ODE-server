//////////////////////////////////////////////////////////////////////////////
/// \file XML.cpp
/// \author excel96
/// \date 2003.7.25
///
/// \todo
/// \bug
/// \warning
//////////////////////////////////////////////////////////////////////////////

// #include "SFCPCH.h"
#include "SXml.h"

#include <charconv>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iosfwd>
#include <iostream>
#include <limits>

#include "Exception.h"

void itoa(int value, char* buf, int r) {
    if (r == 10) {
        snprintf(buf, 20, "%d", value);
    } else {
        snprintf(buf, 20, "%x", value);
    }
}

// using namespace std;

#include "tinyxml2.h"

//////////////////////////////////////////////////////////////////////////////
/// \class XMLUtil
/// \brief
//////////////////////////////////////////////////////////////////////////////

class XMLUtil {
public:
    static string trim(const string& str);
    static void filelog(const char* fmt, ...);
};


//////////////////////////////////////////////////////////////////////////////
/// \brief Copy one parsed tinyxml2 element into an XMLTree node.
///
/// Reproduces the tree shape the previous SAX handler was written to build:
///   - attribute names and values are trimmed, kept in document order
///   - child elements become child XMLTree nodes, in document order
///   - text runs are trimmed individually and appended to the enclosing
///     element, so <a>X<b>Y</b>Z</a> yields a."XZ" and b."Y"
///
/// Two deliberate differences from the xerces path it replaces:
///
/// 1. Text is actually captured. XMLTreeGenerator::characters() declared its
///    length parameter as `unsigned int`, but Xerces-C 3.x declares that
///    virtual as XMLSize_t (size_t). The signatures never matched, so the
///    override never bound and DefaultHandler's no-op ran instead: every
///    text node in every file was silently discarded. No caller reads
///    XMLTree::GetText() today, so nothing depended on the empty result.
///
/// 2. Bytes pass through untouched. tinyxml2 does no transcoding. The old
///    path ran every string through XMLString::transcode() to the *local
///    code page*, so results depended on the server's locale. The data files
///    declare iso-8859-1 while actually holding EUC-KR, so any conversion
///    here would be guessing; handing back the file's own bytes is
///    deterministic, and re-encoding is a separate decision that belongs
///    where the text is used.
//////////////////////////////////////////////////////////////////////////////
static void CopyElementToTree(const tinyxml2::XMLElement* pElement, XMLTree* pTree) {
    for (const tinyxml2::XMLAttribute* pAttr = pElement->FirstAttribute(); pAttr != NULL; pAttr = pAttr->Next()) {
        const char* pName = pAttr->Name();
        const char* pValue = pAttr->Value();
        pTree->AddAttribute(XMLUtil::trim(pName != NULL ? pName : ""), XMLUtil::trim(pValue != NULL ? pValue : ""));
    }

    for (const tinyxml2::XMLNode* pNode = pElement->FirstChild(); pNode != NULL; pNode = pNode->NextSibling()) {
        const tinyxml2::XMLText* pText = pNode->ToText();
        if (pText != NULL) {
            const char* pValue = pText->Value();
            if (pValue != NULL)
                pTree->SetText(pTree->GetText() + XMLUtil::trim(pValue));
            continue;
        }

        const tinyxml2::XMLElement* pChild = pNode->ToElement();
        if (pChild == NULL)
            continue; // comments, declarations, unknowns: ignored as before

        const char* pName = pChild->Name();
        const string name = XMLUtil::trim(pName != NULL ? pName : "");
        if (name.empty())
            continue;

        CopyElementToTree(pChild, pTree->AddChild(name));
    }
}

//////////////////////////////////////////////////////////////////////////////
/// \brief Populate an XMLTree from an already-parsed tinyxml2 document.
///
/// Parse failures are logged and leave the tree empty, matching the previous
/// behaviour of catching the xerces exception and carrying on.
//////////////////////////////////////////////////////////////////////////////
static void LoadTreeFromDocument(const tinyxml2::XMLDocument& document, XMLTree* pTree, const char* pWhat) {
    if (document.Error()) {
        const char* pDetail = document.ErrorStr();
        XMLUtil::filelog("\nError during parsing %s! %s: %s\n", pWhat, document.ErrorName(),
                         pDetail != NULL ? pDetail : "");
        return;
    }

    const tinyxml2::XMLElement* pRoot = document.RootElement();
    if (pRoot == NULL) {
        XMLUtil::filelog("\nNo root element while parsing %s\n", pWhat);
        return;
    }

    const char* pName = pRoot->Name();
    pTree->SetName(XMLUtil::trim(pName != NULL ? pName : ""));
    CopyElementToTree(pRoot, pTree);
}


static const char* XML_ERROR_FILENAME = "__XMLError.log";

//////////////////////////////////////////////////////////////////////////////
/// \brief
/// \param str
/// \return string
//////////////////////////////////////////////////////////////////////////////

string XMLUtil::trim(const string& str) {
    if (str.size() == 0)
        return "";

    static const char* WhiteSpaces = " \t\n\r";
    size_t begin = str.find_first_not_of(WhiteSpaces);
    size_t end = str.find_last_not_of(WhiteSpaces);

    if (begin == string::npos) {
        if (end == string::npos)
            return "";
        else
            begin = 0;
    } else if (end == string::npos) {
        end = str.size();
    }

    return str.substr(begin, end - begin + 1);
}

//////////////////////////////////////////////////////////////////////////////
/// \brief
/// \param fmt
/// \param ...
//////////////////////////////////////////////////////////////////////////////
void XMLUtil::filelog(const char* fmt, ...) {
    ofstream file(XML_ERROR_FILENAME, ios::out | ios::app);
    if (file.is_open()) {
        va_list valist;
        va_start(valist, fmt);
        char message_buffer[30000] = {
            0,
        };
        int nchars = vsnprintf(message_buffer, 30000, fmt, valist);
        va_end(valist);
        if (nchars == -1 || nchars > 30000) {
            filelog("filelog buffer overflow!");
            throw Error("filelog() : more buffer size needed for log");
        }

        time_t now = time(0);
        char time_buffer[256] = {
            0,
        };
        snprintf(time_buffer, sizeof(time_buffer), "%s : ", ctime(&now));

        file.write(time_buffer, (streamsize)strlen(time_buffer));
        file.write(message_buffer, (streamsize)strlen(message_buffer));
        file.write("\n", (streamsize)strlen("\n"));
    }
}

//////////////////////////////////////////////////////////////////////////////
//
//	XMLAttribute
//
//////////////////////////////////////////////////////////////////////////////

XMLAttribute::XMLAttribute(const string& name, const string& value) : m_Name(name), m_Value(value) {}

XMLAttribute::~XMLAttribute() {}

void XMLAttribute::SetValue(string value) {
    m_Value = value;
}

const char* XMLAttribute::GetName() const {
    return m_Name.c_str();
}

const char* XMLAttribute::ToString() const {
    return m_Value.c_str();
}

const int XMLAttribute::ToInt() const {
    return atoi(m_Value.c_str());
}

const DWORD XMLAttribute::ToHex() const {
    return strtoul(m_Value.c_str(), NULL, 16);
}

const bool XMLAttribute::ToBool() const {
    return (m_Value == "true" || m_Value == "TRUE") ? true : false;
}

const double XMLAttribute::ToDouble() const {
    return atof(m_Value.c_str());
}

const float XMLAttribute::ToFloat() const {
    return (float)atof(m_Value.c_str());
}


//////////////////////////////////////////////////////////////////////////////
//
//	XMLTree
//
//////////////////////////////////////////////////////////////////////////////

XMLTree::XMLTree() : m_pParent(NULL) {}

XMLTree::XMLTree(const string& name) : m_Name(name), m_pParent(NULL) {}

XMLTree::XMLTree(const XMLTree& xmlTree) : m_Name(xmlTree.m_Name), m_Text(xmlTree.m_Text), m_pParent(NULL) {
    ATTRIBUTES_VECTOR::const_iterator aitr = xmlTree.m_AttributesVector.begin();

    for (; aitr != xmlTree.m_AttributesVector.end(); ++aitr) {
        AddAttribute((*aitr)->GetName(), (*aitr)->ToString());
    }

    CHILDREN_VECTOR::const_iterator citr = xmlTree.m_ChildrenVector.begin();

    for (; citr != xmlTree.m_ChildrenVector.end(); ++citr) {
        XMLTree* pNewChild = new XMLTree(**citr);
        AddChild(pNewChild);
    }
}

XMLTree::~XMLTree() {
    Release();
}

const string& XMLTree::GetName() const {
    return m_Name;
}

void XMLTree::SetName(const string& name) {
    m_Name = name;
}

const string& XMLTree::GetText() const {
    return m_Text;
}

void XMLTree::SetText(const string& text) {
    m_Text = text;
}

const XMLTree* XMLTree::GetParent() const {
    return m_pParent;
}

void XMLTree::SetParent(XMLTree* pParent) {
    m_pParent = pParent;
}

void XMLTree::AddAttribute(const string& name, const string& value) {
    ATTRIBUTES_MAP::iterator itr = m_AttributesMap.find(name);

    if (itr == m_AttributesMap.end()) {
        XMLAttribute* pAttr = new XMLAttribute(name, value);

        m_AttributesVector.push_back(pAttr);
        m_AttributesMap.insert(ATTRIBUTES_MAP::value_type(name, pAttr));
    } else {
        XMLAttribute* pAttr = itr->second;
        pAttr->SetValue(value);
    }
}

void XMLTree::AddAttribute(const string& name, const char* value) {
    AddAttribute(name, string(value));
}

void XMLTree::AddAttribute(const string& name, const int& value) {
    char szTemp[20];
    itoa(value, szTemp, 10);
    AddAttribute(name, string(szTemp));
}

void XMLTree::AddAttribute(const string& name, const unsigned int& value, const bool bHex) {
    char szTemp[20];
    if (bHex == true)
        itoa(value, szTemp, 16);
    else
        itoa(value, szTemp, 10);

    AddAttribute(name, string(szTemp));
}


void XMLTree::AddAttribute(const string& name, const unsigned long& value, const bool bHex) {
    char szTemp[20];
    if (bHex == true)
        itoa(value, szTemp, 16);
    else
        itoa(value, szTemp, 10);

    AddAttribute(name, string(szTemp));
}

void XMLTree::AddAttribute(const string& name, const float& value) {
    char szTemp[512];
    snprintf(szTemp, sizeof(szTemp), "%f", value);
    AddAttribute(name, string(szTemp));
}

void XMLTree::AddAttribute(const string& name, const double& value) {
    char szTemp[512];
    snprintf(szTemp, sizeof(szTemp), "%f", value);
    AddAttribute(name, string(szTemp));
}

void XMLTree::AddAttribute(const string& name, const bool& value) {
    if (value == true)
        AddAttribute(name, string("true"));
    else
        AddAttribute(name, string("false"));
}

XMLAttribute* XMLTree::GetAttribute(const string& name) const {
    if (m_AttributesMap.empty() == true)
        return NULL;

    ATTRIBUTES_MAP::const_iterator itr = m_AttributesMap.find(name);

    if (itr == m_AttributesMap.end())
        return NULL;

    return itr->second;
}

const bool XMLTree::GetAttribute(const string& name, string& value) {
    const XMLAttribute* pAttr = GetAttribute(name);

    if (pAttr == NULL)
        return false;

    value = pAttr->ToString();

    return true;
}

const bool XMLTree::GetAttribute(const string& name, int& value) {
    const XMLAttribute* pAttr = GetAttribute(name);

    if (pAttr == NULL)
        return false;

    value = pAttr->ToInt();

    return true;
}

namespace {
template <typename T> bool readNarrowAttribute(XMLTree& tree, const string& name, T& value) {
    const XMLAttribute* attribute = tree.GetAttribute(name);
    if (attribute == nullptr) {
        return false;
    }
    const char* begin = attribute->ToString();
    const char* end = begin + std::strlen(begin);
    unsigned int parsed = 0;
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end || parsed > std::numeric_limits<T>::max()) {
        return false;
    }
    value = static_cast<T>(parsed);
    return true;
}
} // namespace

bool XMLTree::GetAttribute(const string& name, WORD& value) {
    return readNarrowAttribute(*this, name, value);
}

bool XMLTree::GetAttribute(const string& name, BYTE& value) {
    return readNarrowAttribute(*this, name, value);
}

const bool XMLTree::GetAttribute(const string& name, unsigned int& value, const bool bHex) {
    const XMLAttribute* pAttr = GetAttribute(name);

    if (pAttr == NULL)
        return false;

    if (bHex == true)
        value = pAttr->ToHex();
    else
        value = pAttr->ToInt();

    return true;
}


const bool XMLTree::GetAttribute(const string& name, unsigned long& value, const bool bHex) {
    const XMLAttribute* pAttr = GetAttribute(name);

    if (pAttr == NULL)
        return false;

    if (bHex == true)
        value = pAttr->ToHex();
    else
        value = pAttr->ToInt();

    return true;
}

const bool XMLTree::GetAttribute(const string& name, float& value) {
    const XMLAttribute* pAttr = GetAttribute(name);

    if (pAttr == NULL)
        return false;

    value = pAttr->ToFloat();

    return true;
}

const bool XMLTree::GetAttribute(const string& name, double& value) {
    const XMLAttribute* pAttr = GetAttribute(name);

    if (pAttr == NULL)
        return false;

    value = pAttr->ToDouble();

    return true;
}

const bool XMLTree::GetAttribute(const string& name, bool& value) {
    const XMLAttribute* pAttr = GetAttribute(name);

    if (pAttr == NULL)
        return false;

    value = pAttr->ToBool();

    return true;
}

XMLTree* XMLTree::AddChild(const string& name) {
    CHILDREN_MAP::iterator itr = m_ChildrenMap.find(name);

    XMLTree* pTree = new XMLTree(name);

    pTree->SetParent(this);
    m_ChildrenVector.push_back(pTree);

    if (itr == m_ChildrenMap.end()) {
        m_ChildrenMap.insert(CHILDREN_MAP::value_type(name, pTree));
    }

    return pTree;
}

XMLTree* XMLTree::AddChild(XMLTree* pChild) {
    pChild->SetParent(this);
    m_ChildrenVector.push_back(pChild);
    m_ChildrenMap[pChild->GetName()] = pChild;
    return pChild;
}

XMLTree* XMLTree::GetChild(const string& name) const {
    CHILDREN_MAP::const_iterator itr = m_ChildrenMap.find(name);

    if (itr == m_ChildrenMap.end())
        return NULL;

    return itr->second;
}

XMLAttribute* XMLTree::GetAttribute(size_t index) const {
    return ((index < m_AttributesVector.size()) ? m_AttributesVector[index] : NULL);
}
const size_t XMLTree::GetAttributeCount() const {
    return m_AttributesVector.size();
}

XMLTree* XMLTree::GetChild(size_t index) const {
    return ((index < m_ChildrenVector.size()) ? m_ChildrenVector[index] : NULL);
}

const size_t XMLTree::GetChildCount() const {
    return m_ChildrenVector.size();
}

void XMLTree::Release() {
    ATTRIBUTES_VECTOR::iterator itr = m_AttributesVector.begin();
    ATTRIBUTES_VECTOR::iterator endItr = m_AttributesVector.end();

    while (itr != endItr) {
        delete (*itr);
        itr++;
    }

    m_AttributesMap.clear();
    m_AttributesVector.clear();

    CHILDREN_VECTOR::iterator itr2 = m_ChildrenVector.begin();
    CHILDREN_VECTOR::iterator endItr2 = m_ChildrenVector.end();

    while (itr2 != endItr2) {
        delete (*itr2);
        itr2++;
    }

    m_ChildrenMap.clear();
    m_ChildrenVector.clear();
}

void XMLTree::LoadFromFile(const char* pFilename) {
    tinyxml2::XMLDocument document;
    document.LoadFile(pFilename);
    LoadTreeFromDocument(document, this, pFilename);
}

void XMLTree::LoadFromMem(const char* pBuffer) {
    tinyxml2::XMLDocument document;
    document.Parse(pBuffer);
    LoadTreeFromDocument(document, this, "<memory buffer>");
}

void XMLTree::SaveToFile(const char* pFilename) {
    ofstream file(pFilename, ios::out | ios::trunc);

    if (!file)
        return;

    file << "<?xml version=\"1.0\" encoding=\"EUC-KR\"?>" << endl;

    Save(file, 0);
}

void XMLTree::Save(ofstream& file, size_t indent) {
    for (size_t i = 0; i < indent; i++)
        file << "\t";

    file << "<" << m_Name;

    ATTRIBUTES_VECTOR::iterator itr = m_AttributesVector.begin();
    ATTRIBUTES_VECTOR::iterator endItr = m_AttributesVector.end();

    while (itr != endItr) {
        file << " " << (*itr)->GetName() << "='" << (*itr)->ToString() << "'";

        itr++;
    }

    if (m_ChildrenVector.empty() == true && GetText().empty() == true) {
        file << "/>" << endl;
    } else {
        file << ">" << GetText();

        if (m_ChildrenVector.empty() == true) {
            file << "</" << m_Name << ">" << endl;
        } else {
            file << endl;

            CHILDREN_VECTOR::iterator itr = m_ChildrenVector.begin();
            CHILDREN_VECTOR::iterator endItr = m_ChildrenVector.end();

            while (itr != endItr) {
                (*itr)->Save(file, indent + 1);

                itr++;
            }

            for (size_t i = 0; i < indent; i++)
                file << "\t";

            file << "</" << m_Name << ">" << endl;
        }
    }
}
