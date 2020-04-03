
#include "CppGenerateTask.hpp"

#include "WIR/Error.hpp"
#include "WIR/Filesystem.hpp"

#include <fstream>

CppGenerateTask::CppGenerateTask(std::string const & inputFile, std::string const & outputFile, std::vector<std::string> const & cxxFlags)
{
  m_inputFile = wir::File(inputFile).path();
  m_outputFile = wir::File(outputFile).path();
  m_generatedStatus = GS_Invalid;
  m_cxxFlags = cxxFlags;
}

CppGenerateTask::~CppGenerateTask()
{
}

void CppGenerateTask::execute()
{
  wir::Timer timer;
  std::string flagsStr;
  for(auto f : m_cxxFlags)
  {
    flagsStr.append(f);
    flagsStr.append(" ");
  }
  
  std::string inputFilename = wir::File(m_inputFile).name();
  std::string outputFilename = wir::File(m_outputFile).name();

  Log("Generating %s -> %s", inputFilename.c_str(), outputFilename.c_str());
  
  // Parse the header 
  m_parsedHeader = HeaderFile(m_inputFile, m_cxxFlags);
  
  if(!m_parsedHeader.isValid())
  {
    std::string ss;
    auto msgs = m_parsedHeader.getMessages();
    ss = wir::formatString("%u Errors when parsing %s:\n", msgs.size(), inputFilename.c_str());

    for(auto msg : msgs)
    {
      ss += wir::formatString("\t%s\n", msg.prettyPrint().c_str());
    }
    
    Log("%s", ss.c_str());

    m_generatedStatus = GS_Error;
    return;
  }
  
  auto parsedClasses = m_parsedHeader.getClassDeclarations();
  auto parsedEnums = m_parsedHeader.getEnumDeclarations();
  
  // If we parsed OK, generate the source file
  wir::File(m_outputFile).createPath();
  std::ofstream outputFile(m_outputFile, std::ios_base::binary);
  if(!outputFile.is_open())
  {
    LogError("Generation failed, could not open file for write (%s)", m_outputFile.c_str());
    m_generatedStatus = GS_Error;
  }
  
  bool writtenHeader = false;
  
  uint64_t i = 0;
  for(auto parsedClass : parsedClasses)
  {
    if(!m_parsedHeader.doesClassInherit(parsedClass.getFullyQualifiedName(), "wir::Class"))
    {
      continue;
    }
    
    if(!writtenHeader)
    {
      outputFile << "\n/* File is automatically generated by WIR, any changes manually made will be lost. */\n\n";
      outputFile << "#include \"" << wir::File(m_inputFile).path() << "\"\n";

      outputFile << "#include <WIR/Class.hpp>\n";
      outputFile << "#include <functional>\n";
      outputFile << "#include <memory>\n";
      writtenHeader = true;
    }
      
    std::string bases;
    std::string basesTemplate;  
    
    bool f = true;
    for(auto b : m_parsedHeader.getInheritedClassesFor(parsedClass.getFullyQualifiedName(), true))
    {
      
      if(f)
      {
        bases += "\"" + b + "\"";
        f = false;
      }
      else
      {
        bases += ", \"" + b + "\"";
      }
    }
    
    f = true;
    for(auto b : m_parsedHeader.getInheritedClassesFor(parsedClass.getFullyQualifiedName(), false))
    {
      if(f)
      {
        basesTemplate += b;
        f = false;
      }
      else
      {
        basesTemplate += ", " + b;
      }
    }
    
    outputFile << "namespace\n";
    outputFile << "{\n";
    outputFile << "  wir::ClassInfo *classInfo" << i << " = nullptr;\n";
    outputFile << "}\n";
    outputFile << "\n";   
    outputFile << "wir::ClassInfo * " << parsedClass.getFullyQualifiedName() << "::classInfo()\n";
    outputFile << "{\n";
    outputFile << "  if (::classInfo" << i <<")\n";
    outputFile << "  {\n";
    outputFile << "    return ::classInfo" << i << ";\n";
    outputFile << "  }\n";
    outputFile << "\n";
    outputFile << "  ::classInfo" << i << " = wir::Class::classInfo(\"" << parsedClass.getFullyQualifiedName() << "\");\n";
    outputFile << "  return ::classInfo" << i << ";\n";
    outputFile << "}\n";
    outputFile << "\n";   
    outputFile << "void " << parsedClass.getFullyQualifiedName() << "::initializeClass()\n";
    outputFile << "{\n";
    if(parsedClass.isAbstract())
    {
      outputFile << "  ::classInfo" << i << " = wir::Class::registerClass(\"" << parsedClass.getFullyQualifiedName() << "\", { " << bases << " }, [](wir::DynamicArguments const &args)->wir::Class*{ LogWarning(\"Attempted to construct pure virtual class instance " << parsedClass.getFullyQualifiedName() << "\"); return nullptr; }, [](wir::DynamicArguments const &args)->wir::ClassPtr { LogWarning(\"Attempted to construct pure virtual class instance " << parsedClass.getFullyQualifiedName() << "\"); return nullptr;} , [](wir::Class *c)->void{ LogWarning(\"Attempted to destruct pure virtual class " << parsedClass.getFullyQualifiedName() << "\"); });\n";
    }
    else
    {
      outputFile << "  ::classInfo" << i << " = wir::Class::registerClass(\"" << parsedClass.getFullyQualifiedName() << "\", { " << bases << " }, [](wir::DynamicArguments const &args)->wir::Class*{ return new " << parsedClass.getFullyQualifiedName() << "(args); }, [](wir::DynamicArguments const & args)->wir::ClassPtr{ return std::make_shared<" << parsedClass.getFullyQualifiedName() << ">(args); } , [](wir::Class *c)->void{ delete c; });\n";
    }

    outputFile << "}\n";

    i++;
  }
  
  outputFile.close();
  
  m_generatedStatus = GS_Completed;
  
  auto seconds = timer.seconds();
  Log("Generated %s in %.00f seconds", outputFilename.c_str(), seconds);
}


