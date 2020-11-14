#ifndef CEFENGINE_HPP
#define CEFENGINE_HPP

#include <cstring>
#include <functional>

struct  ChromiumWebEngineImpl;
class ChromiumWebEngine
{
public:
    static int init(int argc, char** argv);

    static void shutdown();

    static void loadPage(const std::string& url);

    static std::string getPageTitle();

    static bool isLoadingFinished();

    static void runScript(const std::string& script);

    static bool isScriptResultReady();

    static std::string scriptResult();

    static bool haveScriptException();

    static void handleEvents();

private:
    static ChromiumWebEngineImpl* d;
};

#endif // CEFENGINE_HPP
