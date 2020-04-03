
#pragma once

#include "CxxParse/HeaderFile.hpp"

#include <WIR/Async.hpp>

#include <cstdint>
#include <vector>
#include <string>


enum CppGenerateStatus : uint8_t
{
  GS_Invalid,
  GS_Completed,
  GS_Error
};

class CppGenerateTask : public wir::AsyncTask 
{
public:
  
  CppGenerateTask(std::string const & inputFile, std::string const & outputFile, std::vector<std::string> const & cxxFlags);
  
  virtual ~CppGenerateTask();
  
  virtual void execute() override;

  inline std::string const & getInputFile() const 
  {
    return m_inputFile;
  }

  inline HeaderFile const & getParsedHeader() const
  {
    return m_parsedHeader;
  }
  
  inline CppGenerateStatus getGeneratedStatus()
  {
    return (CppGenerateStatus)m_generatedStatus.load();
  }
  
  
protected:
  // Input
  std::string m_inputFile;
  std::string m_outputFile;
  std::vector<std::string> m_cxxFlags;
  
  // Output
  std::atomic_uint8_t m_generatedStatus { GS_Invalid };
  HeaderFile m_parsedHeader;
  
};

typedef std::shared_ptr<CppGenerateTask> CppGenerateTaskPtr;
