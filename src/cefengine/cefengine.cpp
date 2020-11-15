#include "cefengine.hpp"

#include <iostream>
#include <cassert>
#include <unistd.h>
#include "include/cef_app.h"
#include "cefbrowserapp.hpp"
#include "cefbrowserclient.hpp"

struct ChromiumWebEngineImpl
{
    bool isInitialized = false;
    CefRefPtr<CefWindowlessBrowserApp> app = nullptr;
    CefRefPtr<CefWindowlessBrowserClient> client = nullptr;
    CefRefPtr<CefBrowser> browser = nullptr;
};

ChromiumWebEngineImpl* ChromiumWebEngine::d = new ChromiumWebEngineImpl();


int ChromiumWebEngine::init(int argc, char** argv)
{
    CefMainArgs main_args(argc, argv);

    d->app = new CefWindowlessBrowserApp();
    int exit_code = CefExecuteProcess(main_args, d->app.get(), nullptr);
    if (exit_code >= 0) {
        return exit_code;
    }

    // Initialize the app without sandbox on for simplicity and page cache turned off
    CefSettings settings;
    settings.windowless_rendering_enabled = true;
    settings.log_severity = LOGSEVERITY_FATAL;
    //settings.single_process = true;
    settings.no_sandbox = true;
    settings.persist_session_cookies = true;
    CefString(&settings.user_agent).FromASCII("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/38.0.2125.104 Safari/537.36");
    CefString(&settings.cache_path).FromASCII("");
    CefInitialize(main_args, settings, d->app.get(), nullptr);

    // Create windowless browser host, disable also plugins that might be available on the system
    d->client = new CefWindowlessBrowserClient();
    CefWindowInfo info;
    CefWindowHandle nothing = 0UL;
    info.SetAsWindowless(nothing);
    CefBrowserSettings browser_settings;
    browser_settings.plugins = STATE_DISABLED;
    d->browser = CefBrowserHost::CreateBrowserSync(info, d->client, "http://www.google.com", browser_settings, NULL, NULL);
    while (!d->client->GetBrowser()) {
        usleep(1000);
    }

    d->isInitialized = true;
    return -1;
}

void ChromiumWebEngine::shutdown()
{
    d->isInitialized = false;

    d->browser->StopLoad();
    d->browser->GetMainFrame()->Delete();

    for (int i = 0; i < 10; i++)
        CefDoMessageLoopWork();

    d->browser->GetHost()->CloseBrowser(false);
    d->browser = nullptr;

    for (int i = 0; i < 10; i++)
        CefDoMessageLoopWork();

    for (int i = 0; i < 10; i++)
        CefDoMessageLoopWork();

    d->app = nullptr;
    d->client = nullptr;

    for (int i = 0; i < 10; i++)
        CefDoMessageLoopWork();

    CefShutdown();
}

bool ChromiumWebEngine::isLoadingFinished()
{
    assert(d->isInitialized);
    return !d->client->GetBrowser()->IsLoading();
}

std::string ChromiumWebEngine::getPageTitle()
{
    assert(d->isInitialized);
    return d->client->GetBrowser()->GetMainFrame()->GetName();
}

void ChromiumWebEngine::runScript(const std::string& script)
{
    assert(d->isInitialized);
    CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("javascript");
    CefRefPtr<CefListValue> args = msg->GetArgumentList();
    args->SetString(0, script);
    d->browser->GetMainFrame()->SendProcessMessage(PID_RENDERER, msg);
}

bool ChromiumWebEngine::isScriptResultReady()
{
    assert(d->isInitialized);
    return d->client->haveScriptResult();
}

std::string ChromiumWebEngine::scriptResult()
{
    assert(d->isInitialized);
    return d->client->getScriptResult();
}

bool ChromiumWebEngine::haveScriptException()
{
    assert(d->isInitialized);
    return d->client->haveScriptException();
}

void ChromiumWebEngine::handleEvents()
{
    assert(d->isInitialized);
    CefDoMessageLoopWork();
}

void ChromiumWebEngine::loadPage(const std::string& url)
{
    assert(d->isInitialized);
    d->client->resetScriptResultHaving();
    d->client->GetBrowser()->GetMainFrame()->LoadURL(url);
}
