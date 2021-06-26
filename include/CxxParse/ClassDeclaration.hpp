
#pragma once

#include "CxxParse/Annotated.hpp"

#include <WIR/Stream.hpp>

#include <cstdint>
#include <stack>
#include <string>
#include <vector>

enum AccessSpecifier : uint8_t
{
  AS_Public,
  AS_Protected,
  AS_Private
};

class MethodDeclaration : public AnnotatedSymbol, public wir::Serializable
{
public:
  MethodDeclaration();
  MethodDeclaration(std::string const &name, AccessSpecifier accessSpec = AS_Public, bool pureVirtual = false);

  std::string const &getName() const;

  void setName(std::string const &newValue);

  AccessSpecifier getAccessSpecifier() const;

  void setAccessSpecifier(AccessSpecifier newValue);

  bool isPureVirtual() const;

  void setPureVirtual(bool newValue);

  virtual bool serialize(wir::Stream &toStream) const override;
  virtual bool deserialize(wir::Stream &fromStream) override;

protected:
  std::string m_name;
  AccessSpecifier m_accessSpecifier = AS_Public;
  bool m_isPureVirtual = false;
};

class ClassDeclaration : public AnnotatedSymbol, public wir::Serializable
{
public:
  ClassDeclaration();
  ClassDeclaration(std::string const &newName, std::stack<std::string> ns, bool abstract);

  virtual bool serialize(wir::Stream &toStream) const override;
  virtual bool deserialize(wir::Stream &fromStream) override;

  bool isAbstract();

  std::string const &getName();

  void setName(std::string const &newValue);

  std::vector<MethodDeclaration> const &getMethodDeclarations();

  std::vector<std::string> const &getBaseClasses();

  void addMethodDeclaration(MethodDeclaration const &newDeclaration);

  void addBaseClass(std::string const &newBase);

  std::stack<std::string> getNamespace() const;

  std::string getFullyQualifiedName() const;

protected:
  std::string m_name;
  std::stack<std::string> m_namespace;
  std::vector<MethodDeclaration> m_methodDeclarations;
  std::vector<std::string> m_baseClasses;
  bool m_abstract = false;
};
