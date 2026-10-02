#include "PropertiesParser.h"

#include <istream>
#include <string>

#include "Properties.h"

namespace de {

void readProperties(std::istream& input, Properties& properties) {
    while (true) {
        std::string line;
        try {
            std::getline(input, line);
        } catch (const std::ios_base::failure&) {
            // EOF can throw when the caller enables stream exceptions. A
            // successfully extracted final line still needs to be parsed.
            if (input.bad() || !input.eof())
                throw IOException("error reading properties");
        }
        if (input.bad() || (input.fail() && !input.eof()))
            throw IOException("error reading properties");
        if (input.fail())
            break;

        if (line.empty() || line[0] == Properties::Comment)
            continue;
        const auto keyBegin = line.find_first_not_of(Properties::WhiteSpaces);
        if (keyBegin == std::string::npos)
            continue;
        const auto separator = line.find(Properties::Separator, keyBegin);
        if (separator == std::string::npos)
            throw IOException("missing separator");
        if (separator == keyBegin)
            throw IOException("missing key");
        const auto keyEnd = line.find_last_not_of(Properties::WhiteSpaces, separator - 1);
        const auto valueBegin = line.find_first_not_of(Properties::WhiteSpaces, separator + 1);
        if (valueBegin == std::string::npos)
            throw IOException("missing value");
        const auto valueEnd = line.find_last_not_of(Properties::WhiteSpaces);
        properties.setProperty(line.substr(keyBegin, keyEnd - keyBegin + 1),
                               line.substr(valueBegin, valueEnd - valueBegin + 1));
    }
}

} // namespace de
