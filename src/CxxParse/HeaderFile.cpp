
#include "CxxParse/HeaderFile.hpp"
#include "CxxParse/EnumDeclaration.hpp"

#include <WIR/Error.hpp>
#include <WIR/Filesystem.hpp>
#include <WIR/Stream.hpp>

#include <clang-c/Index.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stack>

static std::string getCursorLocationPath(CXCursor &cursor)
{
  CXSourceLocation loc = clang_getCursorLocation(cursor);
  CXFile file;
  uint32_t lineno;
  uint32_t colno;
  uint32_t wutno;
  clang_getExpansionLocation(loc, &file, &lineno, &colno, &wutno);

  CXString filepathcl = clang_getFileName(file);
  return wir::File(clang_getCString(filepathcl)).path();
}

struct _VisitData
{
  HeaderFile *header = nullptr;
  std::stack<std::string> ns;
};

static bool isForwardDeclaration(CXCursor cursor)
{
  auto definition = clang_getCursorDefinition(cursor);

  // If the definition is null, then there is no definition in this translation
  // unit, so this cursor must be a forward declaration.
  if (clang_equalCursors(definition, clang_getNullCursor()))
    return true;

  // If there is a definition, then the forward declaration and the definition
  // are in the same translation unit. This cursor is the forward declaration if
  // it is _not_ the definition.
  return !clang_equalCursors(cursor, definition);
}

std::stack<std::string> getNamespaceFrom(CXCursor c)
{
  std::stack<std::string> ns;
  CXCursor p = clang_getCursorSemanticParent(c);
  while (!clang_Cursor_isNull(p) && clang_getCursorKind(p) != CXCursor_TranslationUnit)
  {
    ns.push(clang_getCString(clang_getCursorSpelling(p)));
    p = clang_getCursorSemanticParent(p);
  }

  return ns;
}

bool cleanFQNameFwdDecl(std::string cluttered, std::string &uncluttered)
{
  auto split = wir::split(wir::trim(cluttered), { ' ', '\t', '\n', '\r' });
  if (split.size() != 1)
  {
    if (split.size() == 0)
    {
      LogError("Invalid classname: \"%s\"", cluttered.c_str());
      return false;
    }
    else if (wir::trim(split[0]) == "struct" || wir::trim(split[0]) == "class")
    {
      uncluttered = wir::trim(wir::substring(cluttered, split[0].size()));
      return true;
    }
  }
  return true;
}

std::string getFQNameFrom(CXCursor c)
{
  std::string fq;
  auto ns = getNamespaceFrom(c);
  while (!ns.empty())
  {
    fq += ns.top() + "::";
    ns.pop();
  }

  fq += clang_getCString(clang_getCursorSpelling(c));

  // Handle forward declarations more gracefully
  if (!cleanFQNameFwdDecl(fq, fq))
  {
    LogError("Failed to clean possible forward declaration");
  }

  return fq;
}

static CXChildVisitResult _kcgHeader_visitAnnotations(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
  AnnotatedSymbol *annotated = (AnnotatedSymbol *)client_data;
  std::string name = clang_getCString(clang_getCursorSpelling(cursor));
  std::string parentName = clang_getCString(clang_getCursorSpelling(parent));
  CXCursorKind kind = clang_getCursorKind(cursor);

  if (kind == CXCursor_AnnotateAttr)
  {
    std::string newAnnotation = clang_getCString(clang_getCursorDisplayName(cursor));
    annotated->addAnnotation(newAnnotation);
  }

  return CXChildVisit_Continue;
}

static CXChildVisitResult _kcgHeader_visitEnumDecl(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
  EnumDeclaration *enumDecl = (EnumDeclaration *)client_data;
  std::string name = clang_getCString(clang_getCursorSpelling(cursor));
  CXCursorKind kind = clang_getCursorKind(cursor);

  if (kind == CXCursor_EnumConstantDecl)
  {
    enumDecl->addVariable(name, clang_getEnumConstantDeclValue(cursor));
  }

  return CXChildVisit_Recurse;
}

static CXChildVisitResult _kcgHeader_visitClassDecl(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
  ClassDeclaration *classDecl = (ClassDeclaration *)client_data;
  std::string name = clang_getCString(clang_getCursorSpelling(cursor));
  std::string parentName = clang_getCString(clang_getCursorSpelling(parent));
  CXCursorKind kind = clang_getCursorKind(cursor);

  if (kind == CXCursor_CXXBaseSpecifier)
  {
    std::string className = getFQNameFrom(clang_getCursorDefinition(cursor));
    classDecl->addBaseClass(className);
  }
  else if (kind == CXCursor_CXXMethod)
  {
    const static std::map<CX_CXXAccessSpecifier, AccessSpecifier> accessMap = {{CX_CXXPublic, AS_Public}, {CX_CXXProtected, AS_Protected}, {CX_CXXPrivate, AS_Private}, {CX_CXXInvalidAccessSpecifier, AS_Public}};

    MethodDeclaration newMethod(clang_getCString(clang_getCursorSpelling(cursor)), accessMap.at(clang_getCXXAccessSpecifier(cursor)), clang_CXXMethod_isPureVirtual(cursor));
    clang_visitChildren(cursor, _kcgHeader_visitAnnotations, dynamic_cast<AnnotatedSymbol *>(&newMethod));

    classDecl->addMethodDeclaration(newMethod);
  }

  return CXChildVisit_Continue;
}

static CXChildVisitResult _kcgHeader_visitClassDecl_onlyBases(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
  _VisitData *visitData = (_VisitData *)client_data;
  HeaderFile *header = visitData->header;
  CXCursorKind kind = clang_getCursorKind(cursor);

  if (kind == CXCursor_CXXBaseSpecifier)
  {
    std::string className = clang_getCString(clang_getCursorSpelling(clang_getCursorDefinition(cursor)));

    header->registerBaseClass(getFQNameFrom(parent), getFQNameFrom(cursor));
  }

  return CXChildVisit_Continue;
}

static CXChildVisitResult _kcgHeader_visitUnit(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
  _VisitData *visitData = (_VisitData *)client_data;
  HeaderFile *header = visitData->header;
  std::string name = clang_getCString(clang_getCursorSpelling(cursor));
  CXCursorKind kind = clang_getCursorKind(cursor);
  std::string kindStr = clang_getCString(clang_getCursorKindSpelling(kind));
  CXSourceLocation loc = clang_getCursorLocation(cursor);
  CXFile file;
  uint32_t lineno;
  uint32_t colno;
  uint32_t wutno;
  clang_getExpansionLocation(loc, &file, &lineno, &colno, &wutno);

  CXString filepathcl = clang_getFileName(file);
  std::string filepath = wir::File(clang_getCString(filepathcl)).path();

  bool sameOrigin = filepath == header->getFilePath() && !isForwardDeclaration(cursor);

  std::stack<std::string> nss = visitData->ns;
  std::string ns;
  while (!nss.empty())
  {
    ns += "::" + nss.top();
    nss.pop();
  }

  if ((kind == CXCursor_ClassDecl || kind == CXCursor_StructDecl) && !isForwardDeclaration(cursor))
  {

    if (sameOrigin)
    {
      //Log("Class: %s in namespace %s", name.c_str(), ns.c_str());

      bool isAbstract = clang_CXXRecord_isAbstract(cursor) != 0;

      ClassDeclaration newDecl(name, visitData->ns, isAbstract);

      clang_visitChildren(cursor, _kcgHeader_visitClassDecl, &newDecl);
      clang_visitChildren(cursor, _kcgHeader_visitAnnotations, dynamic_cast<AnnotatedSymbol *>(&newDecl));

      for (auto base : newDecl.getBaseClasses())
      {
        header->registerBaseClass(newDecl.getFullyQualifiedName(), base);
      }

      header->addClassDeclaration(newDecl);
    }
    else
    {
      clang_visitChildren(cursor, _kcgHeader_visitClassDecl_onlyBases, visitData);
    }
  }
  else if (kind == CXCursor_EnumDecl)
  {
    if (sameOrigin)
    {

      //Log("Enum: %s in namespace %s", name.c_str(), ns.c_str());
      EnumDeclaration newDecl(name, visitData->ns);
      clang_visitChildren(cursor, _kcgHeader_visitEnumDecl, &newDecl);
      header->addEnumDeclaration(newDecl);
    }
  }
  else if (kind == CXCursor_Namespace)
  {
    visitData->ns.push(name);
    clang_visitChildren(cursor, _kcgHeader_visitUnit, visitData);
    visitData->ns.pop();
  }

  return CXChildVisit_Continue;
}

bool HeaderFile::doesAnyClassInherit(std::string const &parentClass) const
{
  for (auto c : m_classDeclarations)
  {
    if (doesClassInherit(c.getFullyQualifiedName(), parentClass))
    {
      return true;
    }
  }

  return false;
}

HeaderFile::HeaderFile()
{
}

HeaderFile::HeaderFile(std::string const &inHeaderFilePath, std::vector<std::string> const &cxxFlagsExtra)
{
  m_filePath = wir::File(inHeaderFilePath).path();
  m_valid = true;

  CXIndex clangIndex = clang_createIndex(0, 0);

  std::vector<std::string> cxxFlagsBase = {
      "-D", "__CODE_GENERATOR__", "-std=c++17"};

  std::vector<std::string> cxxFlagsAll;
  cxxFlagsAll.insert(cxxFlagsAll.end(), cxxFlagsBase.begin(), cxxFlagsBase.end());
  cxxFlagsAll.insert(cxxFlagsAll.end(), cxxFlagsExtra.begin(), cxxFlagsExtra.end());

  /*std::string flagStr;
	for(auto flag : cxxFlagsAll)
	{
	  flagStr.append(flag);
	  flagStr.append(" ");
	}

	KLog::print("Full command: clang %s %s\n", m_filePath.c_str(), flagStr.c_str());*/

  uint64_t numFlags = cxxFlagsAll.size();

  char **cxxFlags_c = new char *[numFlags];
  for (uint64_t i = 0; i < numFlags; i++)
  {
    uint64_t currSize = cxxFlagsAll[i].size() + 1;
    cxxFlags_c[i] = new char[currSize];
    snprintf(cxxFlags_c[i], currSize, "%s", cxxFlagsAll[i].c_str());
  }

  CXTranslationUnit translationUnit = nullptr;
  //CXErrorCode error = clang_parseTranslationUnit2(clangIndex, m_filePath.c_str(), cxxFlags_c, numFlags, nullptr, 0, CXTranslationUnit_None, &translationUnit);
  CXErrorCode error = clang_parseTranslationUnit2(clangIndex, m_filePath.c_str(), cxxFlags_c, numFlags, nullptr, 0, CXTranslationUnit_None, &translationUnit);
  //CXErrorCode error = clang_parseTranslationUnit2FullArgv(
  //    clangIndex, m_filePath.c_str(), cxxFlags_c, numFlags, nullptr, 0, CXTranslationUnit_None, &translationUnit
  //);

  for (uint64_t i = 0; i < numFlags; i++)
  {
    delete[] cxxFlags_c[i];
  }

  delete[] cxxFlags_c;

  if (error != CXError_Success)
  {
    m_valid = false;
    HeaderMessage newMessage;
    newMessage.message = "Failed to parse translation unit, possibly filesystem, permission or memory-related.";
    newMessage.filename = m_filePath;
    newMessage.severity = MS_Fatal;
    m_messages.push_back(newMessage);
  }

  if (m_valid)
  {
    // Fill the diagnostics messages
    uint32_t numDiagnostics = clang_getNumDiagnostics(translationUnit);
    for (uint32_t i = 0; i < numDiagnostics; i++)
    {
      HeaderMessage newMessage;

      CXDiagnostic currDiag = clang_getDiagnostic(translationUnit, i);
      CXDiagnosticSeverity severity = clang_getDiagnosticSeverity(currDiag);

      // Get the expansion location
      CXSourceLocation loc = clang_getDiagnosticLocation(currDiag);
      CXFile file;
      uint32_t _line;
      uint32_t _col;
      uint32_t wutno;

      clang_getExpansionLocation(loc, &file, &_line, &_col, &wutno);
      CXString clangFile = clang_getFileName(file);
      char const *cFile = clang_getCString(clangFile);

      if (cFile != nullptr)
      {
        newMessage.filename = wir::File(cFile).path();
      }
      else
      {
        newMessage.filename = "<Unknown_File>";
      }
      newMessage.message = clang_getCString(clang_getDiagnosticSpelling(currDiag));
      newMessage.line = _line;
      newMessage.col = _col;

      switch (severity)
      {
      case CXDiagnostic_Error:
        newMessage.severity = MS_Error;
        m_valid = false;
        break;
      case CXDiagnostic_Fatal:
        newMessage.severity = MS_Fatal;
        m_valid = false;
        break;
      case CXDiagnostic_Warning:
        newMessage.severity = MS_Warning;
        break;
      case CXDiagnostic_Note:
        newMessage.severity = MS_Notice;
        break;
      case CXDiagnostic_Ignored:
        newMessage.severity = MS_Ignored;
        break;
      }

      m_messages.push_back(newMessage);
    }

    CXCursor cursor = clang_getTranslationUnitCursor(translationUnit);
    _VisitData visitData{this};

    clang_visitChildren(cursor, _kcgHeader_visitUnit, &visitData);
  }

  // Dispose of the translation unit, this closes file handles and frees up memory
  clang_disposeTranslationUnit(translationUnit);
}

void HeaderFile::addClassDeclaration(const ClassDeclaration &newDecl)
{
  m_classDeclarations.push_back(newDecl);
}

void HeaderFile::addEnumDeclaration(const EnumDeclaration &newDecl)
{
  m_enumDeclarations.push_back(newDecl);
}

void HeaderFile::registerBaseClass(std::string const &childClassName, std::string const &parentClassName)
{
  std::set<std::string> &bases = m_inheritMap[childClassName];
  bases.insert(parentClassName);
}

std::set<std::string> HeaderFile::getInheritedClassesFor(std::string const &className, bool topLevelOnly) const
{
  auto finder = m_inheritMap.find(className);
  if (finder == m_inheritMap.end())
  {
    return {};
  }

  std::set<std::string> returner;

  for (auto base : finder->second)
  {
    returner.insert(base);

    if (!topLevelOnly)
    {
      std::set<std::string> deepBases = getInheritedClassesFor(base, false);
      returner.merge(deepBases);
    }
  }

  return returner;
}

bool HeaderFile::doesClassInherit(std::string const &className, std::string const &parentClass) const
{
  auto v = getInheritedClassesFor(className, false);
  return (v.find(parentClass) != v.end());
}

bool HeaderFile::serialize(wir::Stream &toStream) const
{
  toStream << m_valid;
  toStream << m_filePath;
  toStream << (uint64_t)m_inheritMap.size();
  for (auto e : m_inheritMap)
  {
    toStream << e.first;
    toStream << (uint64_t)e.second.size();
    for (auto s : e.second)
    {
      toStream << s;
    }
  }

  toStream << (uint64_t)m_classDeclarations.size();
  for (auto c : m_classDeclarations)
  {
    toStream << c;
  }

  toStream << (uint64_t)m_enumDeclarations.size();
  for (auto e : m_enumDeclarations)
  {
    toStream << e;
  }

  return true;
}

bool HeaderFile::deserialize(wir::Stream &fromStream)
{
  fromStream >> m_valid;
  fromStream >> m_filePath;

  m_inheritMap.clear();
  uint64_t numInherit = 0;
  fromStream >> numInherit;
  for (uint64_t i = 0; i < numInherit; i++)
  {
    std::string inheritClass = "";
    fromStream >> inheritClass;

    uint64_t numBases = 0;
    fromStream >> numBases;

    for (uint64_t j = 0; j < numBases; j++)
    {
      std::string baseClass = "";
      fromStream >> baseClass;
      m_inheritMap[inheritClass].insert(baseClass);
    }
  }

  m_classDeclarations.clear();
  uint64_t numClasses;
  fromStream >> numClasses;
  for (uint64_t i = 0; i < numClasses; i++)
  {
    ClassDeclaration newDecl;
    fromStream >> newDecl;
    m_classDeclarations.push_back(newDecl);
  }

  m_enumDeclarations.clear();
  uint64_t numEnums;
  fromStream >> numEnums;
  for (uint64_t i = 0; i < numEnums; i++)
  {
    EnumDeclaration newDecl;
    fromStream >> newDecl;
    m_enumDeclarations.push_back(newDecl);
  }

  return true;
}

std::string HeaderMessage::prettyPrint()
{

  std::stringstream ss;
  switch (severity)
  {
  case MS_Fatal:
    ss << "Fatal: ";
    break;
  case MS_Error:
    ss << "Error: ";
    break;
  case MS_Warning:
    ss << "Warning: ";
    break;
  case MS_Notice:
    ss << "Notice: ";
    break;
  case MS_Ignored:
    ss << "Ignored: ";
    break;
  }

  ss << " in file \"" << filename << "\":" << line << ":" << col << ": " << message;
  return ss.str();
}
