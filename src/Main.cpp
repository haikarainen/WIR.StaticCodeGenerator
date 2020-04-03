
#include "CxxParse/HeaderFile.hpp"
#include "CppGenerateTask.hpp"

#include <WIR/String.hpp>
#include <WIR/Error.hpp>
#include <WIR/Async.hpp>
#include <WIR/Filesystem.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <thread>

struct Parameter 
{
  Parameter(std::string _name, std::string _value = "true")
  {
    name = _name;
    value = _value;
  }
  
  std::string name;
  std::string value;
};

std::vector<Parameter> parseArguments(int argc, char **argv)
{
  std::vector<Parameter> returner;
  for(int32_t i = 0; i < argc; i++)
  {
    std::string currArg = wir::trim(argv[i]);
    if(currArg.size() < 4 || currArg.substr(0, 2) != "--")
    {
      returner.push_back({ currArg, "" });
      continue;
    }
    
    currArg = currArg.substr(2);
    std::string::size_type found = currArg.find_first_of("=");
    if(found == std::string::npos || found == currArg.size() - 1)
    {
      returner.push_back({currArg, "true"}); 
    }
    else 
    {
      returner.push_back({currArg.substr(0, found), currArg.substr(found+1)});
    }
  }
  
  return returner;
}

void generate(std::vector<Parameter> &parameters)
{
  if (parameters.size() < 2)
  {
    return;
  }

  std::vector<std::string> extraArgs;

  std::string outputPath = "./generated";
  std::string inputPath = "";

  bool skipFirst = false;
  for (auto param : parameters)
  {
    if (!skipFirst)
    {
      skipFirst = true;
      continue;
    }

    if (param.name == "inputPath")
    {
      inputPath = param.value;
    }
    if (param.name == "outputPath")
    {
      outputPath = param.value;
    }
    if (param.name == "include")
    {
      extraArgs.push_back("-I");
      extraArgs.push_back(param.value);
    }
    if (param.name == "define")
    {
      extraArgs.push_back("-D");
      extraArgs.push_back(param.value);
    }
  }

  if (inputPath.size() == 0)
  {
    Log("Please specify --inputPath=./include/");
    return;
  }

  outputPath = wir::Directory(outputPath).path();

  wir::Directory inputDir(inputPath);
  if (!inputDir.exist())
  {
    Log("the given input path does not exist");
    return;
  }

  std::vector<std::string> inputHeaders;
  inputDir.iterate(true, [&](wir::FilesystemEntry *entry)->void {
    wir::File *file = dynamic_cast<wir::File*>(entry);
    if (!file)
    {
      return;
    }

    if (file->extension() != ".hpp")
    {
      return;
    }

    inputHeaders.push_back(file->path());
  });

  if (inputHeaders.size() == 0)
  {
    Log("No input headers found");
    return;
  }
  std::recursive_mutex generateTasksMutex;
  std::set<CppGenerateTaskPtr> generateTasks;
  std::atomic_bool allJobsDone{ false };

  int32_t threadPoolSize = int32_t(std::thread::hardware_concurrency()) - 2;
  if (threadPoolSize < 1)
  {
    threadPoolSize = 1;
  }
  wir::AsyncContext generateContext(threadPoolSize);

  for (auto header : inputHeaders)
  {
    std::string relativePath = inputDir.relativePath(header);
    std::string outputFilePath = outputPath + "/" + relativePath;
    outputFilePath = outputFilePath.substr(0, outputFilePath.size() - 3);
    outputFilePath += "generated.cpp";
    wir::File outputFile(outputFilePath);
    wir::File inputFile(header);

    uint64_t lastChanged = inputFile.lastWriteTime();
    uint64_t lastGenerated = outputFile.exist() ? outputFile.lastWriteTime() : 0;

    if (lastChanged > lastGenerated)
    {
      CppGenerateTaskPtr newTask = std::make_shared<CppGenerateTask>(inputFile.path(), outputFile.path(), extraArgs);
      newTask->onCompleted += [newTask, &generateTasksMutex, &generateTasks, &allJobsDone]() {
        std::scoped_lock lock(generateTasksMutex);
        generateTasks.erase(newTask);
        if (generateTasks.empty())
        {
          allJobsDone.store(true);
        }
      };

      std::scoped_lock lock(generateTasksMutex);
      generateTasks.insert(newTask);
      generateContext.queueTask(newTask);
    }
  }

  if (generateTasks.size() == 0)
  {
    Log("No input headers needs update");
    return;
  }

  wir::Timer timer;
  while (true)
  {
    if (timer.seconds() <= 0.25)
    {
      continue;
    }

    generateContext.tick();

    timer.reset();
    if (allJobsDone.load())
    {
      break;
    }
  }


}
  

int main(int argc, char **argv)
{
  try
  {

    int32_t numThreads = std::thread::hardware_concurrency();

    Log("WIR Static Code Generator");
    Log("---");
    Log("Recognized %u hardware threads.", numThreads);
    Log("");

    auto parameters = parseArguments(argc, argv);
  
    Log("Runtime parameters:");
    for(auto param : parameters)
    {
      Log("%s = %s", param.name.c_str(), param.value.c_str());
    }
    Log("");
  
    generate(parameters);
  }
  catch (std::exception e)
  {
    LogError("Exception caught: %s", e.what());
    return 1;
  }

  return 0;
}
