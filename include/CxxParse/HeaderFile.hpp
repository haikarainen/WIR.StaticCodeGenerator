
#pragma once 

#include "ClassDeclaration.hpp"
#include "EnumDeclaration.hpp"

#include <WIR/Stream.hpp>

#include <string>
#include <vector>
#include <map>
#include <set>

enum HeaderMessageSeverity 
{
  MS_Fatal,
  MS_Error,
  MS_Warning,
  MS_Notice,
  MS_Ignored
};

struct HeaderMessage
{
  std::string prettyPrint();
  
  std::string message;
  std::string filename;
  uint32_t line = 0;
  uint32_t col = 0;
  HeaderMessageSeverity severity = MS_Ignored;
};

class HeaderFile : public wir::Serializable
{
public:
    HeaderFile();
    HeaderFile(std::string const & inHeaderFilePath, std::vector<std::string> const & cxxFlagsExtra);
    
    void registerBaseClass(std::string const & childClassName, std::string const & parentClassName);
    
    std::set<std::string> getInheritedClassesFor(std::string const & className, bool topLevelOnly = false) const;
    bool doesClassInherit(std::string const & className, std::string const & parentClass) const;
    bool doesAnyClassInherit(std::string const & parentClass) const;
    
    void addClassDeclaration(ClassDeclaration const & newDecl);
    void addEnumDeclaration(EnumDeclaration const & newDecl);
    
    inline std::string const & getFilePath() const
    {
      return m_filePath;
    }
    
    inline std::vector<ClassDeclaration> const & getClassDeclarations() const
    {
      return m_classDeclarations;
    }
    
    inline std::vector<EnumDeclaration> const & getEnumDeclarations() const
    {
      return m_enumDeclarations;
    }
    
    inline std::vector<HeaderMessage> const & getMessages() const
    {
      return m_messages;
    }
    
    inline bool isValid() const
    {
      return m_valid;
    }
    
    virtual bool serialize(wir::Stream & toStream) const override;
    virtual bool deserialize(wir::Stream & fromStream) override;
    
protected:
    // key inherits from value, includes external declarations
    // Used to generate a complete set of baseclasses for the classes
    std::map<std::string, std::set<std::string>> m_inheritMap;
    
    bool m_valid = false;
    std::string m_filePath;
    std::vector<ClassDeclaration> m_classDeclarations;
    std::vector<EnumDeclaration> m_enumDeclarations;
    std::vector<HeaderMessage> m_messages;
    
};
 
