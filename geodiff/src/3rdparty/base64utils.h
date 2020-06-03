#ifndef BASE64UTILS_H
#define BASE64UTILS_H

#include <string>

std::string base64_encode( unsigned char const *bytes_to_encode, unsigned int in_len );
std::string base64_decode( std::string const &encoded_string );


#endif // BASE64UTILS_H
