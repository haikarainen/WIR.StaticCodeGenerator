#include "CxxParse/Annotated.hpp"

AnnotatedSymbol::~AnnotatedSymbol()
{
}

bool AnnotatedSymbol::hasAnnotation(std::string const &reference)
{
  for (auto annotation : m_annotations)
  {
    if (annotation == reference)
    {
      return true;
    }
  }

  return false;
}

void AnnotatedSymbol::addAnnotation(const std::string &newAnnotation)
{
  if (hasAnnotation(newAnnotation))
  {
    return;
  }

  m_annotations.push_back(newAnnotation);
}

bool AnnotatedSymbol::serialize(wir::Stream &toStream) const
{
  toStream << (uint64_t)m_annotations.size();
  for (auto a : m_annotations)
  {
    toStream << a;
  }

  return true;
}

bool AnnotatedSymbol::deserialize(wir::Stream &fromStream)
{
  m_annotations.clear();
  uint64_t numAnnotations = 0;
  fromStream >> numAnnotations;
  for (uint64_t i = 0; i < numAnnotations; i++)
  {
    std::string newName;
    fromStream >> newName;
    m_annotations.push_back(newName);
  }

  return true;
}
