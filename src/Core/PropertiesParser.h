#ifndef DARKEDEN_PROPERTIES_PARSER_H
#define DARKEDEN_PROPERTIES_PARSER_H

#include <iosfwd>

class Properties;

namespace de {

// Read the existing key : value grammar from a borrowed stream, merging into
// properties with the last duplicate value winning. Comments start with '#'
// in column zero; spaces and tabs around keys and values are trimmed.
// Malformed input and stream error states throw IOException. Other exceptions
// from a caller-configured stream propagate. Successfully read earlier entries
// remain in properties; startup loads into an unpublished object.
void readProperties(std::istream& input, Properties& properties);

} // namespace de

#endif
