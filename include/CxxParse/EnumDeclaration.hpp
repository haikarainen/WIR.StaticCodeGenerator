
#pragma once 

#include <WIR/Stream.hpp>

#include "CxxParse/Annotated.hpp"

#include <string>
#include <map>
#include <cstdint>
#include <stack>

class EnumDeclaration : public wir::Serializable, public AnnotatedSymbol
{
public:
  EnumDeclaration();
  EnumDeclaration(std::string const & name, std::stack<std::string> ns);
  
  std::string const & getName();
  
  void setName(std::string const & newName);
  
  std::map<std::string, int64_t> const & getVariables();
  
  void addVariable(std::string const & variableName, int64_t value);
  
  virtual bool serialize(wir::Stream & toStream) const override;
  virtual bool deserialize(wir::Stream & fromStream) override;
  
  
  std::string getFullyQualifiedName() const;

protected:
  std::string m_name;
  std::stack<std::string> m_namespace;
  std::map<std::string, int64_t> m_variables;
};
