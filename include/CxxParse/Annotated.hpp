
#pragma once 

#include <WIR/Stream.hpp>

#include <string>
#include <vector>

class AnnotatedSymbol
{
public:
    virtual ~AnnotatedSymbol();
    bool serialize(wir::Stream & toStream) const;
    bool deserialize(wir::Stream & fromStream);
    
    void addAnnotation(std::string const & newAnnotation);
    bool hasAnnotation(std::string const & reference);
    
protected:
    std::vector<std::string> m_annotations;
}; 
