
#include "CxxParse/ClassDeclaration.hpp"

bool ClassDeclaration::isAbstract()
{
  return m_abstract;
}

std::string const &ClassDeclaration::getName()
{
  return m_name;
}

void ClassDeclaration::setName(std::string const &newValue)
{
  m_name = newValue;
}

std::vector<MethodDeclaration> const &ClassDeclaration::getMethodDeclarations()
{
  return m_methodDeclarations;
}

std::vector<std::string> const &ClassDeclaration::getBaseClasses()
{
  return m_baseClasses;
}

bool ClassDeclaration::serialize(wir::Stream &toStream) const
{
  toStream << m_name;

  auto ns = m_namespace;
  toStream << (uint32_t)ns.size();
  while (!ns.empty())
  {
    toStream << ns.top();
    ns.pop();
  }

  toStream << (uint64_t)m_methodDeclarations.size();
  for (auto m : m_methodDeclarations)
  {
    toStream << m;
  }

  toStream << (uint64_t)m_baseClasses.size();
  for (auto s : m_baseClasses)
  {
    toStream << s;
  }

  toStream << m_abstract;

  if (!AnnotatedSymbol::serialize(toStream))
  {
    return false;
  }

  return true;
}

bool ClassDeclaration::deserialize(wir::Stream &fromStream)
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

  m_methodDeclarations.clear();
  uint64_t numMethods = 0;
  fromStream >> numMethods;
  for (uint64_t i = 0; i < numMethods; i++)
  {
    MethodDeclaration newDecl;
    fromStream >> newDecl;
    m_methodDeclarations.push_back(newDecl);
  }

  m_baseClasses.clear();
  uint64_t numBases = 0;
  fromStream >> numBases;
  for (uint64_t i = 0; i < numBases; i++)
  {
    std::string newBase;
    fromStream >> newBase;
    m_baseClasses.push_back(newBase);
  }

  fromStream >> m_abstract;

  if (!AnnotatedSymbol::deserialize(fromStream))
  {
    return false;
  }

  return true;
}

bool MethodDeclaration::serialize(wir::Stream &toStream) const
{
  toStream << m_name;
  toStream << (uint8_t)m_accessSpecifier;
  toStream << m_isPureVirtual;

  if (!AnnotatedSymbol::serialize(toStream))
  {
    return false;
  }

  return true;
}

bool MethodDeclaration::deserialize(wir::Stream &fromStream)
{
  fromStream >> m_name;
  uint8_t _as = 0;
  fromStream >> _as;
  m_accessSpecifier = (AccessSpecifier)_as;
  fromStream >> m_isPureVirtual;

  if (!AnnotatedSymbol::deserialize(fromStream))
  {
    return false;
  }

  return true;
}

void ClassDeclaration::addBaseClass(const std::string &newBase)
{
  m_baseClasses.push_back(newBase);
}

std::stack<std::string> ClassDeclaration::getNamespace() const
{
  return m_namespace;
}

std::string ClassDeclaration::getFullyQualifiedName() const
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

void ClassDeclaration::addMethodDeclaration(const MethodDeclaration &newDeclaration)
{
  m_methodDeclarations.push_back(newDeclaration);
}

MethodDeclaration::MethodDeclaration()
{
}

MethodDeclaration::MethodDeclaration(const std::string &name, AccessSpecifier accessSpec, bool pureVirtual)
{
  m_name = name;
  m_accessSpecifier = accessSpec;
  m_isPureVirtual = pureVirtual;
}

std::string const &MethodDeclaration::getName() const
{
  return m_name;
}

void MethodDeclaration::setName(std::string const &newValue)
{
  m_name = newValue;
}

AccessSpecifier MethodDeclaration::getAccessSpecifier() const
{
  return m_accessSpecifier;
}

void MethodDeclaration::setAccessSpecifier(AccessSpecifier newValue)
{
  m_accessSpecifier = newValue;
}

bool MethodDeclaration::isPureVirtual() const
{
  return m_isPureVirtual;
}

void MethodDeclaration::setPureVirtual(bool newValue)
{
  m_isPureVirtual = newValue;
}

ClassDeclaration::ClassDeclaration()
{
}

ClassDeclaration::ClassDeclaration(const std::string &newName, std::stack<std::string> ns, bool abs)
{
  m_name = newName;
  m_namespace = ns;
  m_abstract = abs;
}
