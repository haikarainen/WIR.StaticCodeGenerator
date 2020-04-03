
#include "CxxParse/EnumDeclaration.hpp"

EnumDeclaration::EnumDeclaration()
{
}

EnumDeclaration::EnumDeclaration(const std::string& name, std::stack<std::string> ns)
{
  m_name = name;
  m_namespace = ns;
}

std::string const & EnumDeclaration::getName()
{
  return m_name;
}

void EnumDeclaration::setName(std::string const & newName)
{
  m_name = newName;
}

std::map<std::string, int64_t> const & EnumDeclaration::getVariables()
{
  return m_variables;
}

void EnumDeclaration::addVariable(const std::string& variableName, int64_t value)
{
  m_variables[variableName] = value;
}


bool EnumDeclaration::serialize(wir::Stream & toStream) const
{
  toStream << m_name;

  auto ns = m_namespace;
  toStream << (uint32_t)ns.size();
  while (!ns.empty())
  {
    toStream << ns.top();
    ns.pop();
  }

  toStream << (uint64_t)m_variables.size();
  for(auto v : m_variables)
  {
    toStream << v.first;
    toStream << v.second;
  }
  
  if (!AnnotatedSymbol::serialize(toStream))
  {
    return false;
  }

  return true;
}

bool EnumDeclaration::deserialize(wir::Stream & fromStream)
{
  fromStream >> m_name;
  
  m_namespace = std::stack<std::string>();
  uint32_t nsLen = 0;
  fromStream >> nsLen;
  for (uint32_t i = 0; i < nsLen; i++)
  {
    std::string ns;
    fromStream >> ns;
    m_namespace.push(ns);
  }

  m_variables.clear();
  uint64_t numValues = 0;
  fromStream >> numValues;
  for(uint64_t i = 0; i < numValues; i++)
  {
    std::string newName;
    fromStream >> newName;
    
    int64_t newValue = 0;
    fromStream >> newValue;
    
    m_variables[newName] = newValue;
  }
  
  if (!AnnotatedSymbol::deserialize(fromStream))
  {
    return false;
  }

  return true;
}


std::string EnumDeclaration::getFullyQualifiedName() const
{
  std::string fqn;
  auto ns = m_namespace;
  while (!ns.empty())
  {
    fqn += ns.top() + "::";
    ns.pop();
  }

  fqn += m_name;

  return fqn;
}