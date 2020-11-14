#ifndef CEFBROWSERCLIENT_HPP
#define CEFBROWSERCLIENT_HPP

#include <cstring>

#include "include/cef_client.h"
#include "include/cef_frame.h"
#include "include/cef_render_handler.h"
#include "include/cef_string_visitor.h"

// Perform CEF Client role, but don't do any render
// Also allow to run javascript and store last script result
class CefWindowlessBrowserClient : public CefClient, public CefLifeSpanHandler, public CefRenderHandler
{

public:
    CefWindowlessBrowserClient();
    virtual ~CefWindowlessBrowserClient();

    //For accessing script execution results
    std::string getScriptResult();
    bool haveScriptException();
    bool haveScriptResult();
    void resetScriptResultHaving();

    // CefClient methods
    virtual bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId source_process, CefRefPtr<CefProcessMessage> message) override;
    virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
    virtual CefRefPtr<CefRenderHandler> GetRenderHandler() override;

    // CefLifeSpanHandler methods
    virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

    // CefRenderHandler methods
    virtual void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects, const void* buffer, int width, int height) override;
    virtual void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;

    // Own methods
    CefRefPtr<CefBrowser> GetBrowser();

    IMPLEMENT_REFCOUNTING(CefWindowlessBrowserClient);

private:
    CefRefPtr<CefBrowser> m_browser;

    std::string m_scriptResult;
    bool m_hasException;
    bool m_haveResult;

    static constexpr int kPseudoWindowWidth = 1280;
    static constexpr int kPseudoWindowHeight = 720;
};

#endif /* CEFBROWSERCLIENT_HPP */
